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

void EXAudioProcessor::analyzeAudioBpmAndKey()
{
    if (sampleBuffer.getNumSamples() == 0) return;

    int totalSamples = sampleBuffer.getNumSamples();
    const float* data = sampleBuffer.getReadPointer (0);

    // 1. Accurate Energy-Envelope BPM Beat Tracker
    int stepSize = 441; // 10ms window
    int numFrames = totalSamples / stepSize;
    std::vector<float> energyEnvelope (numFrames, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        float sum = 0.0f;
        for (int i = 0; i < stepSize; ++i)
        {
            float s = data[f * stepSize + i];
            sum += s * s;
        }
        energyEnvelope[f] = std::sqrt (sum / stepSize);
    }

    // Auto-correlate energy envelope for tempo lag (60 to 180 BPM)
    float maxCorr = 0.0f;
    int bestLagFrames = 44; // ~135 BPM default

    for (int lag = 33; lag <= 100; ++lag) // ~60 BPM to 180 BPM
    {
        float corr = 0.0f;
        for (size_t f = 0; f < energyEnvelope.size() - lag; ++f)
            corr += energyEnvelope[f] * energyEnvelope[f + lag];

        if (corr > maxCorr)
        {
            maxCorr = corr;
            bestLagFrames = lag;
        }
    }

    double beatDurationSeconds = (bestLagFrames * stepSize) / 44100.0;
    if (beatDurationSeconds > 0.25)
    {
        sourceBpm = juce::jlimit (70, 180, (int) std::round (60.0 / beatDurationSeconds));
    }
    targetBpm = sourceBpm;

    // 2. 12-Bin Musical Chromagram Key Detection
    double chromaEnergy[12] = { 0.0 };
    int maxAnalyze = std::min (44100 * 3, totalSamples);

    for (int lag = 80; lag < 450; lag += 2)
    {
        double freq = 44100.0 / lag;
        if (freq >= 65.4 && freq <= 523.25) // C2 to C5
        {
            int midiNote = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
            int pitchClass = ((midiNote % 12) + 12) % 12;

            float corr = 0.0f;
            for (int i = 0; i < maxAnalyze - lag; i += 24)
                corr += data[i] * data[i + lag];

            chromaEnergy[pitchClass] += std::max (0.0f, corr);
        }
    }

    int bestKey = 0;
    double maxChroma = 0.0;
    for (int k = 0; k < 12; ++k)
    {
        if (chromaEnergy[k] > maxChroma)
        {
            maxChroma = chromaEnergy[k];
            bestKey = k;
        }
    }

    sourceKey = bestKey;
    sourceScaleMode = 0; // Minor
    targetKey = sourceKey;
    targetScaleMode = sourceScaleMode;
}

void EXAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        sampleBuffer.setSize (2, static_cast<int> (reader->lengthInSamples));
        reader->read (&sampleBuffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

        analyzeAudioBpmAndKey();
        applyGridSlicing (currentGrid);
        rollDiceAndFlipPattern();
    }
}

void EXAudioProcessor::applyGridSlicing (GridDivision div)
{
    currentGrid = div;
    sliceMarkers.clear();
    int total = sampleBuffer.getNumSamples();
    if (total == 0) return;

    int numCuts = 16;
    if (div == GridDivision::Quarter) numCuts = 4;
    else if (div == GridDivision::Eighth) numCuts = 8;
    else if (div == GridDivision::Sixteenth) numCuts = 16;
    else if (div == GridDivision::Triplet1_3) numCuts = 6;
    else if (div == GridDivision::Triplet1_6) numCuts = 12;
    else if (div == GridDivision::Transient)
    {
        const float* d = sampleBuffer.getReadPointer (0);
        int spacing = total / 24;
        sliceMarkers.push_back (0);
        int last = 0;
        for (int i = 1; i < total - 1; ++i)
        {
            if (std::abs (d[i] - d[i - 1]) > 0.12f && (i - last) > spacing)
            {
                sliceMarkers.push_back (i);
                last = i;
                if (sliceMarkers.size() >= 16) break;
            }
        }
        return;
    }

    int stepSize = total / numCuts;
    for (int i = 0; i < numCuts; ++i)
        sliceMarkers.push_back (i * stepSize);
}

void EXAudioProcessor::setSlicePosition (int markerIndex, int newSamplePosition)
{
    if (markerIndex >= 0 && markerIndex < static_cast<int> (sliceMarkers.size()))
    {
        sliceMarkers[static_cast<size_t> (markerIndex)] = juce::jlimit (0, sampleBuffer.getNumSamples() - 1, newSamplePosition);
        std::sort (sliceMarkers.begin(), sliceMarkers.end());
    }
}

void EXAudioProcessor::rollDiceAndFlipPattern()
{
    patternSteps.clear();
    int count = static_cast<int> (sliceMarkers.size());
    if (count == 0) return;

    // Trigger Dice 3D Roll Animation
    isDiceRolling.store (true);
    diceRollProgress = 0.0f;
    juce::Random rng;
    activeDiceNumber = 1 + rng.nextInt (6);

    // Musical Scale Palette
    std::vector<int> chords;
    if (targetScaleMode == 1)      // Major: 1, 3, 5, 6, 8, 10
        chords = { 0, 4, 7, 9, 12, 16, 2 };
    else if (targetScaleMode == 2) // Dorian: 1, b3, 4, 5, 6, b7
        chords = { 0, 3, 5, 7, 9, 10, 14 };
    else if (targetScaleMode == 3) // Phrygian (Dark Trap): 1, b2, 4, 5, b6, b7
        chords = { 0, 1, 5, 7, 8, 10, 12 };
    else                           // Natural Minor: 1, b3, 4, 5, b6, b7
        chords = { 0, 3, 5, 7, 8, 10, 12 };

    int keyDelta = targetKey - sourceKey;

    // Call-and-Response Loop Progression (Bar 1: Intro phrase, Bar 2: Response, Bar 3: High Tension, Bar 4: Drop resolution)
    int rootSlice = rng.nextInt (std::max (1, count / 2));
    int bounceSlice = (rootSlice + 2) % count;

    for (int i = 0; i < 16; ++i)
    {
        FlipStep s;
        // Hip-hop syncopation (Steps 0, 3, 6, 8, 10, 12, 14)
        if (i == 0 || i == 6 || i == 8 || i == 12)
        {
            s.isGate = true;
            s.sliceIdx = rootSlice;
            s.pitchOffset = keyDelta + chords[0]; // Anchor Root
            s.velocity = 1.0f;
            s.isReverse = false;
        }
        else if (i == 3 || i == 10 || i == 14)
        {
            s.isGate = true;
            s.sliceIdx = bounceSlice;
            s.pitchOffset = keyDelta + chords[rng.nextInt ((int) chords.size())];
            s.velocity = 0.85f;
            s.isReverse = (rng.nextFloat() > 0.65f); // Reverse fill
        }
        else
        {
            s.isGate = (rng.nextFloat() > 0.50f);
            s.sliceIdx = rng.nextInt (count);
            s.pitchOffset = keyDelta + chords[rng.nextInt ((int) chords.size())];
            s.velocity = 0.75f;
            s.isReverse = false;
        }
        patternSteps.push_back (s);
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

juce::File EXAudioProcessor::exportCurrentAsAudioWav()
{
    auto tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Rendered_Loop.wav");
    tempWav.deleteFile();

    if (sampleBuffer.getNumSamples() == 0 || patternSteps.empty())
        return tempWav;

    int totalSamples = samplesPerStep * 16;
    juce::AudioBuffer<float> renderBuf (2, totalSamples);
    renderBuf.clear();

    for (int step = 0; step < 16; ++step)
    {
        const auto& s = patternSteps[(size_t) step];
        if (s.isGate && s.sliceIdx < (int) sliceMarkers.size())
        {
            int start = sliceMarkers[(size_t) s.sliceIdx];
            int end = (s.sliceIdx + 1 < (int) sliceMarkers.size()) ? sliceMarkers[(size_t) s.sliceIdx + 1] : sampleBuffer.getNumSamples();
            double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
            if (halfTimeMode) pRatio *= 0.5;

            int writeStart = step * samplesPerStep;
            double rPos = s.isReverse ? (end - 1) : start;

            for (int i = 0; i < samplesPerStep; ++i)
            {
                int curR = (int) rPos;
                if ((!s.isReverse && curR < end) || (s.isReverse && curR >= start))
                {
                    if (writeStart + i < totalSamples && curR < sampleBuffer.getNumSamples() && curR >= 0)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            renderBuf.setSample (ch, writeStart + i, sampleBuffer.getSample (ch % sampleBuffer.getNumChannels(), curR));
                    }
                    rPos += (s.isReverse ? -pRatio : pRatio);
                }
                else break;
            }
        }
    }

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (wavFmt.createWriterFor (new juce::FileOutputStream (tempWav), 44100.0, 2, 24, {}, 0));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (renderBuf, 0, totalSamples);

    return tempWav;
}

juce::File EXAudioProcessor::exportCurrentAsMidi()
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
                float gainEnv = 1.0f;
                int distFromEdge = currentStepReverse ? (rPos - playheadEnd) : (playheadEnd - rPos);
                if (distFromEdge < 64)
                    gainEnv = static_cast<float> (distFromEdge) / 64.0f;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    int sCh = ch % sampleBuffer.getNumChannels();
                    buffer.addSample (ch, s, sampleBuffer.getSample (sCh, rPos) * gainEnv);
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
