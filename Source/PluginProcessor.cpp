#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TAPE_DRIVE", 1 }, "Tape Drive", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_TIME", 1 }, "Delay Time", 0.05f, 0.8f, 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY_MIX", 1 }, "Delay Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_SIZE", 1 }, "Space Size", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "REVERB_MIX", 1 }, "Reverb Mix", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER_CUTOFF", 1 }, "Filter Cutoff", 50.0f, 20000.0f, 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MASTER_OUT", 1 }, "Master Gain", 0.0f, 2.0f, 1.0f));

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

    samplesPerStep = static_cast<int> ((60.0 / std::max (60, masterTargetBpm)) * 0.25 * sampleRate);
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

// Cognitive Chromagram & Spectral Flux Detection
void EXAudioProcessor::analyzeAudioBpmAndKey (int layerIdx)
{
    auto& track = tracks[layerIdx];
    if (track.buffer.getNumSamples() == 0) return;

    int totalSamples = track.buffer.getNumSamples();
    const float* data = track.buffer.getReadPointer (0);

    // 1. Accurate Energy-Envelope BPM Detection
    int stepSize = 441; // 10ms window
    int numFrames = totalSamples / stepSize;
    std::vector<float> energyEnv (numFrames, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        float sum = 0.0f;
        for (int i = 0; i < stepSize; ++i)
        {
            float s = data[f * stepSize + i];
            sum += s * s;
        }
        energyEnv[f] = std::sqrt (sum / stepSize);
    }

    float maxCorr = 0.0f;
    int bestLagFrames = 44; // ~135 BPM fallback

    for (int lag = 33; lag <= 100; ++lag) // 60 BPM to 180 BPM
    {
        float corr = 0.0f;
        for (size_t f = 0; f < energyEnv.size() - lag; ++f)
            corr += energyEnv[f] * energyEnv[f + lag];

        if (corr > maxCorr)
        {
            maxCorr = corr;
            bestLagFrames = lag;
        }
    }

    double beatDur = (bestLagFrames * stepSize) / 44100.0;
    if (beatDur > 0.25)
        track.sourceBpm = juce::jlimit (70, 180, (int) std::round (60.0 / beatDur));

    // 2. 12-Bin Pitch Chromagram for Musical Key
    double chroma[12] = { 0.0 };
    int maxAnalyze = std::min (44100 * 3, totalSamples);

    for (int lag = 80; lag < 450; lag += 2)
    {
        double freq = 44100.0 / lag;
        if (freq >= 65.4 && freq <= 523.25)
        {
            int midiNote = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
            int pitchClass = ((midiNote % 12) + 12) % 12;

            float corr = 0.0f;
            for (int i = 0; i < maxAnalyze - lag; i += 24)
                corr += data[i] * data[i + lag];

            chroma[pitchClass] += std::max (0.0f, corr);
        }
    }

    int bestKey = 0;
    double maxChroma = 0.0;
    for (int k = 0; k < 12; ++k)
    {
        if (chroma[k] > maxChroma)
        {
            maxChroma = chroma[k];
            bestKey = k;
        }
    }

    track.sourceKey = bestKey;
    track.sourceScaleMode = 0; // Minor
}

void EXAudioProcessor::loadAudioFileToLayer (int layerIdx, const juce::File& file)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        track.buffer.setSize (2, static_cast<int> (reader->lengthInSamples));
        reader->read (&track.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

        track.hasAudio = true;
        track.fileName = file.getFileNameWithoutExtension().substring (0, 14);

        analyzeAudioBpmAndKey (layerIdx);
        applyGridSlicingToLayer (layerIdx, currentGrid);
        rollDiceAndGenerateMasterPattern();
    }
}

void EXAudioProcessor::clearLayer (int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    tracks[layerIdx].buffer.setSize (2, 0);
    tracks[layerIdx].sliceMarkers.clear();
    tracks[layerIdx].hasAudio = false;
    tracks[layerIdx].fileName = "EMPTY SLOT";
}

void EXAudioProcessor::applyGridSlicingToLayer (int layerIdx, GridDivision div)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    track.sliceMarkers.clear();
    int total = track.buffer.getNumSamples();
    if (total == 0) return;

    int numCuts = 16;
    if (div == GridDivision::Quarter) numCuts = 4;
    else if (div == GridDivision::Eighth) numCuts = 8;
    else if (div == GridDivision::Sixteenth) numCuts = 16;
    else if (div == GridDivision::Triplet1_3) numCuts = 6;
    else if (div == GridDivision::Triplet1_6) numCuts = 12;
    else if (div == GridDivision::Transient)
    {
        const float* d = track.buffer.getReadPointer (0);
        int spacing = total / 24;
        track.sliceMarkers.push_back (0);
        int last = 0;
        for (int i = 1; i < total - 1; ++i)
        {
            if (std::abs (d[i] - d[i - 1]) > 0.12f && (i - last) > spacing)
            {
                track.sliceMarkers.push_back (i);
                last = i;
                if (track.sliceMarkers.size() >= 16) break;
            }
        }
        return;
    }

    int stepSize = total / numCuts;
    for (int i = 0; i < numCuts; ++i)
        track.sliceMarkers.push_back (i * stepSize);
}

void EXAudioProcessor::setSlicePosition (int layerIdx, int markerIdx, int newSamplePos)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    if (markerIdx >= 0 && markerIdx < static_cast<int> (track.sliceMarkers.size()))
    {
        track.sliceMarkers[static_cast<size_t> (markerIdx)] = juce::jlimit (0, track.buffer.getNumSamples() - 1, newSamplePos);
        std::sort (track.sliceMarkers.begin(), track.sliceMarkers.end());
    }
}

// Master Generative AI/Harmonic Flip Engine
void EXAudioProcessor::rollDiceAndGenerateMasterPattern()
{
    patternSteps.clear();

    std::vector<int> loadedTracks;
    for (int i = 0; i < 3; ++i)
        if (tracks[i].hasAudio && !tracks[i].sliceMarkers.empty())
            loadedTracks.push_back (i);

    if (loadedTracks.empty()) return;

    // Trigger 3D Dice Roll Animation
    isDiceRolling.store (true);
    diceRollProgress = 0.0f;
    juce::Random rng;
    activeDiceNumber = 1 + rng.nextInt (6);

    // Master Scale Intervals
    std::vector<int> targetIntervals;
    if (masterTargetScaleMode == 1)      // Major
        targetIntervals = { 0, 4, 7, 9, 12, 16, 2 };
    else if (masterTargetScaleMode == 2) // Dorian
        targetIntervals = { 0, 3, 5, 7, 9, 10, 14 };
    else if (masterTargetScaleMode == 3) // Phrygian
        targetIntervals = { 0, 1, 5, 7, 8, 10, 12 };
    else                                 // Natural Minor
        targetIntervals = { 0, 3, 5, 7, 8, 10, 12 };

    // Musical 16-Step Arranger blending all active tracks seamlessly
    for (int step = 0; step < 16; ++step)
    {
        FlipStep s;
        // Select track: Call & Response distribution
        int tIdx = loadedTracks[(size_t) rng.nextInt ((int) loadedTracks.size())];
        if (loadedTracks.size() > 1)
        {
            if (step % 4 == 0) tIdx = loadedTracks[0]; // Anchor track
            else if (step % 4 == 2 && loadedTracks.size() > 1) tIdx = loadedTracks[1]; // Counter phrase
        }

        auto& selTrack = tracks[tIdx];
        s.trackIdx = tIdx;
        s.sliceIdx = rng.nextInt ((int) selTrack.sliceMarkers.size());

        // Strict Harmonic Offset: Snap track source key directly to Master Target Key
        int keyOffset = masterTargetKey - selTrack.sourceKey;
        int interval = targetIntervals[(size_t) rng.nextInt ((int) targetIntervals.size())];
        s.pitchOffset = keyOffset + interval;

        // Hip-hop / Drill syncopated gating
        s.isGate = (step % 2 == 0) || (rng.nextFloat() > 0.40f);
        s.velocity = 0.82f + (rng.nextFloat() * 0.18f);
        s.isReverse = (rng.nextFloat() > 0.82f);

        patternSteps.push_back (s);
    }

    currentStepIndex = 0;
    stepClock = 0;
    isVoiceActive = false;
    isPlayingPattern.store (true);
}

void EXAudioProcessor::togglePlayback()
{
    isPlayingPattern.store (!isPlayingPattern.load());
    if (isPlayingPattern.load())
    {
        currentStepIndex = 0;
        stepClock = 0;
    }
}

juce::File EXAudioProcessor::exportCurrentAsAudioWav()
{
    auto tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Rendered_Workstation_Loop.wav");
    tempWav.deleteFile();

    if (patternSteps.empty()) return tempWav;

    int totalSamples = samplesPerStep * 16;
    juce::AudioBuffer<float> renderBuf (2, totalSamples);
    renderBuf.clear();

    for (int step = 0; step < 16; ++step)
    {
        const auto& s = patternSteps[(size_t) step];
        auto& trk = tracks[s.trackIdx];
        if (trk.hasAudio && s.isGate && s.sliceIdx < (int) trk.sliceMarkers.size())
        {
            int start = trk.sliceMarkers[(size_t) s.sliceIdx];
            int end = (s.sliceIdx + 1 < (int) trk.sliceMarkers.size()) ? trk.sliceMarkers[(size_t) s.sliceIdx + 1] : trk.buffer.getNumSamples();

            double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
            if (halfTimeMode) pRatio *= 0.5;

            int writeStart = step * samplesPerStep;
            double rPos = s.isReverse ? (end - 1) : start;

            for (int i = 0; i < samplesPerStep; ++i)
            {
                int curR = (int) rPos;
                if ((!s.isReverse && curR < end) || (s.isReverse && curR >= start))
                {
                    if (writeStart + i < totalSamples && curR < trk.buffer.getNumSamples() && curR >= 0)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            renderBuf.setSample (ch, writeStart + i, trk.buffer.getSample (ch % trk.buffer.getNumChannels(), curR) * trk.volume);
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
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Workstation_Groove.mid");
    temp.deleteFile();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence seq;
    int tickPerStep = 240;

    for (size_t i = 0; i < patternSteps.size(); ++i)
    {
        if (patternSteps[i].isGate)
        {
            int note = 60 + patternSteps[i].sliceIdx + (patternSteps[i].trackIdx * 12);
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

    int baseBpm = std::max (60, masterTargetBpm);
    double stepSeconds = (60.0 / baseBpm) * 0.25;
    if (halfTimeMode) stepSeconds *= 2.0;
    samplesPerStep = static_cast<int> (stepSeconds * getSampleRate());

    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            int raw = msg.getNoteNumber() - 60;
            int trkIdx = juce::jlimit (0, 2, raw / 12);
            int sIdx = raw % 12;

            auto& trk = tracks[trkIdx];
            if (trk.hasAudio && sIdx < (int) trk.sliceMarkers.size())
            {
                activeTrackIdx = trkIdx;
                playheadPosition = static_cast<double> (trk.sliceMarkers[(size_t) sIdx]);
                playheadEnd = (sIdx + 1 < (int) trk.sliceMarkers.size()) ? trk.sliceMarkers[(size_t) sIdx + 1] : trk.buffer.getNumSamples();
                pitchSpeed = std::pow (2.0, (masterTargetKey - trk.sourceKey) / 12.0);
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
        if (isPlayingPattern.load() && !patternSteps.empty())
        {
            stepClock++;
            if (stepClock >= samplesPerStep)
            {
                stepClock = 0;
                currentStepIndex = (currentStepIndex + 1) % static_cast<int> (patternSteps.size());

                const auto& step = patternSteps[static_cast<size_t> (currentStepIndex)];
                auto& trk = tracks[step.trackIdx];

                if (trk.hasAudio && !trk.isMute && step.isGate && step.sliceIdx < static_cast<int> (trk.sliceMarkers.size()))
                {
                    activeTrackIdx = step.trackIdx;
                    int startPt = trk.sliceMarkers[static_cast<size_t> (step.sliceIdx)];
                    int endPt = (step.sliceIdx + 1 < static_cast<int> (trk.sliceMarkers.size()))
                                ? trk.sliceMarkers[static_cast<size_t> (step.sliceIdx + 1)]
                                : trk.buffer.getNumSamples();

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

        if (isVoiceActive)
        {
            auto& trk = tracks[activeTrackIdx];
            if (trk.hasAudio && trk.buffer.getNumSamples() > 0)
            {
                int rPos = static_cast<int> (playheadPosition);
                bool isValid = currentStepReverse ? (rPos >= playheadEnd && rPos >= 0)
                                                  : (rPos < playheadEnd && rPos < trk.buffer.getNumSamples());

                if (isValid)
                {
                    float gainEnv = 1.0f;
                    int distFromEdge = currentStepReverse ? (rPos - playheadEnd) : (playheadEnd - rPos);
                    if (distFromEdge < 64)
                        gainEnv = static_cast<float> (distFromEdge) / 64.0f;

                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        int sCh = ch % trk.buffer.getNumChannels();
                        buffer.addSample (ch, s, trk.buffer.getSample (sCh, rPos) * trk.volume * gainEnv);
                    }
                    playheadPosition += (currentStepReverse ? -pitchSpeed : pitchSpeed);
                }
                else
                {
                    isVoiceActive = false;
                }
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
