#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TAPE_DRIVE", 1 }, "Drive", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 }, "Time", 0.05f, 0.8f, 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_MIX", 1 }, "Dly Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_SIZE", 1 }, "Space", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_MIX", 1 }, "Rvb Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_CUTOFF", 1 }, "Filter", 50.0f, 20000.0f, 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MASTER_OUT", 1 }, "Master", 0.0f, 2.0f, 1.0f));

    return { params.begin(), params.end() };
}

EXAudioProcessor::EXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

EXAudioProcessor::~EXAudioProcessor() {}

const juce::String EXAudioProcessor::getName() const { return "EX"; }
bool EXAudioProcessor::acceptsMidi() const { return true; }
bool EXAudioProcessor::producesMidi() const { return true; }
bool EXAudioProcessor::isMidiEffect() const { return false; }
double EXAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int EXAudioProcessor::getNumPrograms() { return 1; }
int EXAudioProcessor::getCurrentProgram() { return 0; }
void EXAudioProcessor::setCurrentProgram (int) {}
const juce::String EXAudioProcessor::getProgramName (int) { return {}; }
void EXAudioProcessor::changeProgramName (int, const juce::String&) {}

void EXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 2;

    reverb.prepare (spec);
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    delayBuffer.setSize (2, static_cast<int> (sampleRate * 2));
    delayBuffer.clear();
    delayWriteIndex = 0;

    samplesPerStep = static_cast<int> ((60.0 / std::max (60, targetBpm)) * 0.25 * sampleRate);
}

void EXAudioProcessor::releaseResources()
{
    reverb.reset();
    filter.reset();
}

bool EXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void EXAudioProcessor::analyzeAudioProperties()
{
    if (sampleBuffer.getNumSamples() == 0) return;

    double len = sampleBuffer.getNumSamples() / 44100.0;
    if (len > 0.4)
    {
        double estBeats = std::round ((len / 60.0) * 132.0);
        if (estBeats < 4.0) estBeats = 4.0;
        sourceBpm = juce::jlimit (70, 180, static_cast<int> (std::round ((estBeats / len) * 60.0)));
    }
    targetBpm = sourceBpm;
    sourceKey = (sampleBuffer.getNumSamples() / 1100) % 12;
    targetKey = sourceKey;
}

void EXAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        sampleBuffer.setSize (2, static_cast<int> (reader->lengthInSamples));
        reader->read (&sampleBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

        analyzeAudioProperties();
        sliceTransientDetection();
        rollDiceAndFlipPattern();
    }
}

void EXAudioProcessor::sliceTransientDetection()
{
    sliceMarkers.clear();
    if (sampleBuffer.getNumSamples() == 0) return;

    const float* data = sampleBuffer.getReadPointer (0);
    int total = sampleBuffer.getNumSamples();
    float threshold = 0.15f;
    int spacing = 44100 / 8;

    sliceMarkers.push_back (0);
    int prev = 0;

    for (int i = 1; i < total - 1; ++i)
    {
        if (std::abs (data[i] - data[i - 1]) > threshold && (i - prev) > spacing)
        {
            sliceMarkers.push_back (i);
            prev = i;
            if (sliceMarkers.size() >= 16) break;
        }
    }

    if (sliceMarkers.size() < 4)
    {
        sliceMarkers.clear();
        int chunk = total / 8;
        for (int i = 0; i < 8; ++i)
            sliceMarkers.push_back (i * chunk);
    }
}

void EXAudioProcessor::rollDiceAndFlipPattern()
{
    patternSteps.clear();
    int count = static_cast<int> (sliceMarkers.size());
    if (count == 0) return;

    juce::Random rng;
    int intervals[] = { 0, 3, 5, 7, 10, 12, -2 };
    int keyDelta = targetKey - sourceKey;

    for (int i = 0; i < 16; ++i)
    {
        FlipStep step;
        step.isGate = (i % 2 == 0) || (rng.nextFloat() > 0.40f);
        step.sliceIdx = rng.nextInt (count);
        step.pitchOffset = keyDelta + intervals[rng.nextInt (7)];
        step.velocity = 0.8f + (rng.nextFloat() * 0.2f);
        step.isReverse = (rng.nextFloat() > 0.75f); // 25% chance of flipped reverse slice
        patternSteps.push_back (step);
    }

    currentStepIndex = 0;
    stepClock = 0;
    isVoiceActive = false;
    isPlayingPattern.store (true);
}

void EXAudioProcessor::togglePlayback()
{
    if (sampleBuffer.getNumSamples() == 0) return;
    isPlayingPattern.store (!isPlayingPattern.load());
    if (isPlayingPattern.load())
    {
        currentStepIndex = 0;
        stepClock = 0;
    }
}

juce::File EXAudioProcessor::generateMidiFile()
{
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Flip_Groove.mid");
    temp.deleteFile();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence seq;
    int tickPerStep = 240;

    for (size_t i = 0; i < patternSteps.size(); ++i)
    {
        if (patternSteps[i].isGate)
        {
            int note = 60 + patternSteps[i].sliceIdx;
            double sTick = static_cast<double> (i * tickPerStep);
            double eTick = sTick + (tickPerStep - 20);

            seq.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (patternSteps[i].velocity * 127)), sTick);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), eTick);
        }
    }

    mf.addTrack (seq);
    juce::FileOutputStream fos (temp);
    if (fos.openedOk()) mf.writeTo (fos);

    return temp;
}

void EXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const float tapeDrive = apvts.getRawParameterValue ("TAPE_DRIVE")->load();
    const float delayTime = apvts.getRawParameterValue ("DELAY_TIME")->load();
    const float delayMix  = apvts.getRawParameterValue ("DELAY_MIX")->load();
    const float rvbSize   = apvts.getRawParameterValue ("REVERB_SIZE")->load();
    const float rvbMix    = apvts.getRawParameterValue ("REVERB_MIX")->load();
    const float cutoff    = apvts.getRawParameterValue ("FILTER_CUTOFF")->load();
    const float masterOut = apvts.getRawParameterValue ("MASTER_OUT")->load();

    int baseBpm = std::max (60, targetBpm);
    double stepSeconds = (60.0 / baseBpm) * 0.25;
    if (halfTimeMode) stepSeconds *= 2.0;
    samplesPerStep = static_cast<int> (stepSeconds * getSampleRate());

    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            int idx = msg.getNoteNumber() - 60;
            if (idx >= 0 && idx < static_cast<int> (sliceMarkers.size()))
            {
                playheadPosition = static_cast<double> (sliceMarkers[static_cast<size_t> (idx)]);
                playheadEnd = (idx + 1 < static_cast<int> (sliceMarkers.size())) ? sliceMarkers[static_cast<size_t> (idx + 1)] : sampleBuffer.getNumSamples();
                pitchSpeed = std::pow (2.0, (targetKey - sourceKey) / 12.0);
                currentStepReverse = false;
                isVoiceActive = true;
            }
        }
        else if (msg.isNoteOff() && !isPlayingPattern.load())
        {
            isVoiceActive = false;
        }
    }

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        if (isPlayingPattern.load() && !patternSteps.empty() && sampleBuffer.getNumSamples() > 0)
        {
            stepClock++;
            if (stepClock >= samplesPerStep)
            {
                stepClock = 0;
                currentStepIndex = (currentStepIndex + 1) % static_cast<int> (patternSteps.size());

                const auto& step = patternSteps[static_cast<size_t> (currentStepIndex)];
                if (step.isGate && step.sliceIdx < static_cast<int> (sliceMarkers.size()))
                {
                    int startPt = sliceMarkers[static_cast<size_t> (step.sliceIdx)];
                    int endPt = (step.sliceIdx + 1 < static_cast<int> (sliceMarkers.size()))
                                ? sliceMarkers[static_cast<size_t> (step.sliceIdx + 1)]
                                : sampleBuffer.getNumSamples();

                    currentStepReverse = step.isReverse;
                    playheadPosition = currentStepReverse ? static_cast<double> (endPt - 1) : static_cast<double> (startPt);
                    playheadEnd = currentStepReverse ? startPt : endPt;

                    pitchSpeed = std::pow (2.0, step.pitchOffset / 12.0);
                    if (halfTimeMode) pitchSpeed *= 0.5;
                    isVoiceActive = true;
                }
                else
                {
                    isVoiceActive = false;
                }
            }
        }

        if (isVoiceActive && sampleBuffer.getNumSamples() > 0)
        {
            int rPos = static_cast<int> (playheadPosition);
            bool isValid = currentStepReverse ? (rPos >= playheadEnd && rPos >= 0)
                                              : (rPos < playheadEnd && rPos < sampleBuffer.getNumSamples());

            if (isValid)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    int sCh = ch % sampleBuffer.getNumChannels();
                    buffer.addSample (ch, s, sampleBuffer.getSample (sCh, rPos));
                }
                playheadPosition += (currentStepReverse ? -pitchSpeed : pitchSpeed);
            }
            else
            {
                isVoiceActive = false;
            }
        }
    }

    if (tapeDrive > 0.01f)
    {
        float gain = 1.0f + (tapeDrive * 5.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::tanh (d[i] * gain) / std::tanh (gain);
        }
    }

    if (delayMix > 0.01f && delayBuffer.getNumSamples() > 0)
    {
        int dSamples = static_cast<int> (delayTime * getSampleRate());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                int rIdx = (delayWriteIndex - dSamples + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                float del = delayBuffer.getSample (ch, rIdx);
                float dry = buffer.getSample (ch, i);

                delayBuffer.setSample (ch, delayWriteIndex, dry + (del * 0.45f));
                buffer.setSample (ch, i, dry + (del * delayMix));
            }
            delayWriteIndex = (delayWriteIndex + 1) % delayBuffer.getNumSamples();
        }
    }

    filter.setCutoffFrequency (cutoff);
    juce::dsp::AudioBlock<float> blk (buffer);
    juce::dsp::ProcessContextReplacing<float> pCtx (blk);
    filter.process (pCtx);

    if (rvbMix > 0.01f)
    {
        reverbParams.roomSize = rvbSize * 0.85f;
        reverbParams.damping  = 0.45f;
        reverbParams.wetLevel = rvbMix * 0.65f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.3f);
        reverb.setParameters (reverbParams);
        reverb.process (pCtx);
    }

    buffer.applyGain (masterOut);
}

bool EXAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* EXAudioProcessor::createEditor()
{
    return new EXAudioProcessorEditor (*this);
}

void EXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EXAudioProcessor();
}
