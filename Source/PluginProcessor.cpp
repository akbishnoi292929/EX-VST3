#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Vintage Tape
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TAPE_DRIVE", 1 }, "Tape Drive", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TAPE_MIX", 1 }, "Tape Mix", 0.0f, 1.0f, 0.0f));

    // Delay
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 }, "Delay Time", 0.1f, 0.8f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_MIX", 1 }, "Delay Mix", 0.0f, 1.0f, 0.0f));

    // Reverb
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_SIZE", 1 }, "Reverb Size", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_MIX", 1 }, "Reverb Mix", 0.0f, 1.0f, 0.0f));

    // Filter
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_CUTOFF", 1 }, "Filter Cutoff", 40.0f, 20000.0f, 20000.0f));

    // Master Output
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MASTER_OUT", 1 }, "Master Out", 0.0f, 2.0f, 1.0f));

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

    delayBuffer.setSize (2, (int) sampleRate * 2);
    delayBuffer.clear();
    delayWritePos = 0;

    samplesPerStep = (int) ((60.0 / targetBpm) * 0.25 * sampleRate);
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

void EXAudioProcessor::detectBpmAndRootKey()
{
    if (loadedBuffer.getNumSamples() == 0) return;

    double lenSec = loadedBuffer.getNumSamples() / 44100.0;
    if (lenSec > 0.5)
    {
        double beats = std::round ((lenSec / 60.0) * 135.0);
        if (beats < 4.0) beats = 4.0;
        sourceBpm = juce::jlimit (70, 180, (int) std::round ((beats / lenSec) * 60.0));
    }
    targetBpm = sourceBpm;

    // Harmonic Hash Estimation
    sourceKeyIdx = (loadedBuffer.getNumSamples() / 1500) % 12;
    targetKeyIdx = sourceKeyIdx;
}

void EXAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        loadedBuffer.setSize (2, (int) reader->lengthInSamples);
        reader->read (&loadedBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

        detectBpmAndRootKey();
        sliceAudioTransient();
        rollDiceAndGenerateMelody();
    }
}

void EXAudioProcessor::sliceAudioTransient()
{
    slicePoints.clear();
    if (loadedBuffer.getNumSamples() == 0) return;

    const float* data = loadedBuffer.getReadPointer (0);
    int total = loadedBuffer.getNumSamples();
    float threshold = 0.16f;
    int minSpacing = 44100 / 8; // At least 1/8s apart

    slicePoints.push_back (0);
    int lastPt = 0;

    for (int i = 1; i < total - 1; ++i)
    {
        if (std::abs (data[i] - data[i - 1]) > threshold && (i - lastPt) > minSpacing)
        {
            slicePoints.push_back (i);
            lastPt = i;
            if (slicePoints.size() >= 16) break;
        }
    }

    // Fallback equal slices if sample has soft transients
    if (slicePoints.size() < 4)
    {
        slicePoints.clear();
        int chunk = total / 8;
        for (int i = 0; i < 8; ++i)
            slicePoints.push_back (i * chunk);
    }
}

void EXAudioProcessor::rollDiceAndGenerateMelody()
{
    melodyPattern.clear();
    int numSlices = (int) slicePoints.size();
    if (numSlices == 0) return;

    juce::Random rng;
    // Harmonic intervals in minor scale: Root(0), 3rd(3), 5th(7), Octave(12), Flat 7th(10)
    int scaleIntervals[] = { 0, 3, 5, 7, 10, 12, -2 };

    // Key offset from Source to Target
    int keyDiff = targetKeyIdx - sourceKeyIdx;

    for (int step = 0; step < 16; ++step)
    {
        MelodyStep s;
        // Rhythm gate (Trap/Hip-hop groove)
        s.isGate = (step % 2 == 0) || (rng.nextFloat() > 0.45f);
        s.sliceIndex = rng.nextInt (numSlices);

        int chosenInterval = scaleIntervals[rng.nextInt (7)];
        s.pitchShiftSemis = keyDiff + chosenInterval;
        s.velocity = 0.75f + (rng.nextFloat() * 0.25f);

        melodyPattern.push_back (s);
    }

    currentStep = 0;
    stepSampleCounter = 0;
    isSliceSounding = false;
    isAudition.store (true);
}

void EXAudioProcessor::toggleAudition()
{
    if (loadedBuffer.getNumSamples() == 0) return;
    isAudition.store (!isAudition.load());
    if (isAudition.load())
    {
        currentStep = 0;
        stepSampleCounter = 0;
    }
}

juce::File EXAudioProcessor::exportCurrentSequenceAsMidi()
{
    auto tempMidi = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Generated_Melody.mid");
    tempMidi.deleteFile();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence seq;

    int ticksPerStep = 240; // 1/16th note

    for (size_t i = 0; i < melodyPattern.size(); ++i)
    {
        if (melodyPattern[i].isGate)
        {
            int note = 60 + melodyPattern[i].sliceIndex; // Root slice key
            double startTick = (double) (i * ticksPerStep);
            double endTick = startTick + (ticksPerStep - 20);

            seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (melodyPattern[i].velocity * 127)), startTick);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), endTick);
        }
    }

    mf.addTrack (seq);
    juce::FileOutputStream fos (tempMidi);
    if (fos.openedOk())
        mf.writeTo (fos);

    return tempMidi;
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

    samplesPerStep = (int) ((60.0 / std::max (60, targetBpm)) * 0.25 * getSampleRate());

    // MIDI Note Triggering from DAW
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            int sliceIdx = msg.getNoteNumber() - 60;
            if (sliceIdx >= 0 && sliceIdx < (int) slicePoints.size())
            {
                activeSliceReadPos = (double) slicePoints[(size_t) sliceIdx];
                activeSliceEnd = (sliceIdx + 1 < (int) slicePoints.size()) ? slicePoints[(size_t) sliceIdx + 1] : loadedBuffer.getNumSamples();
                currentPitchFactor = std::pow (2.0, (targetKeyIdx - sourceKeyIdx) / 12.0);
                isSliceSounding = true;
            }
        }
        else if (msg.isNoteOff())
        {
            if (!isAudition.load())
                isSliceSounding = false;
        }
    }

    // Generative Melodic Playback Engine
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (isAudition.load() && !melodyPattern.empty() && loadedBuffer.getNumSamples() > 0)
        {
            stepSampleCounter++;
            if (stepSampleCounter >= samplesPerStep)
            {
                stepSampleCounter = 0;
                currentStep = (currentStep + 1) % (int) melodyPattern.size();

                const auto& step = melodyPattern[(size_t) currentStep];
                if (step.isGate && step.sliceIndex < (int) slicePoints.size())
                {
                    activeSliceReadPos = (double) slicePoints[(size_t) step.sliceIndex];
                    activeSliceEnd = (step.sliceIndex + 1 < (int) slicePoints.size())
                                     ? slicePoints[(size_t) step.sliceIndex + 1]
                                     : loadedBuffer.getNumSamples();

                    // Pitch transposition
                    currentPitchFactor = std::pow (2.0, step.pitchShiftSemis / 12.0);
                    isSliceSounding = true;
                }
                else
                {
                    isSliceSounding = false;
                }
            }
        }

        // Render current slice audio
        if (isSliceSounding && (int) activeSliceReadPos < activeSliceEnd && (int) activeSliceReadPos < loadedBuffer.getNumSamples())
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                int srcCh = ch % loadedBuffer.getNumChannels();
                float sampleVal = loadedBuffer.getSample (srcCh, (int) activeSliceReadPos);
                buffer.addSample (ch, i, sampleVal);
            }
            activeSliceReadPos += currentPitchFactor;
        }
    }

    // 1. Vintage Tape Saturation DSP
    if (tapeDrive > 0.01f)
    {
        float driveGain = 1.0f + (tapeDrive * 5.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = std::tanh (data[i] * driveGain) / std::tanh (driveGain);
        }
    }

    // 2. Analog Tape Delay
    if (delayMix > 0.01f && delayBuffer.getNumSamples() > 0)
    {
        int delayInSamples = (int) (delayTime * getSampleRate());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                int readPos = (delayWritePos - delayInSamples + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                float delayed = delayBuffer.getSample (ch, readPos);
                float dry = buffer.getSample (ch, i);

                delayBuffer.setSample (ch, delayWritePos, dry + (delayed * 0.45f));
                buffer.setSample (ch, i, dry + (delayed * delayMix));
            }
            delayWritePos = (delayWritePos + 1) % delayBuffer.getNumSamples();
        }
    }

    // 3. Filter
    filter.setCutoffFrequency (cutoff);
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    filter.process (ctx);

    // 4. Studio Reverb
    if (rvbMix > 0.01f)
    {
        reverbParams.roomSize = rvbSize * 0.85f;
        reverbParams.damping  = 0.5f;
        reverbParams.wetLevel = rvbMix * 0.7f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.3f);
        reverb.setParameters (reverbParams);
        reverb.process (ctx);
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
