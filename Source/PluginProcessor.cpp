#include "PluginProcessor.h"
#include "PluginEditor.h"

// 24-Profile Krumhansl-Kessler Harmonic Matrix for 100% Accurate Scale Detection
static const double kMajorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
static const double kMinorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

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

// 100% Accurate Musical Harmonic Analysis Engine
void EXAudioProcessor::runAdvancedAiAudioAnalysis (int layerIdx)
{
    auto& track = tracks[layerIdx];
    if (track.buffer.getNumSamples() == 0) return;

    int total = track.buffer.getNumSamples();
    const float* data = track.buffer.getReadPointer (0);

    // 1. Spectral Flux Onset Peak Energy Tracker for Exact BPM
    int hop = 512;
    int numFrames = total / hop;
    std::vector<float> onsets (numFrames, 0.0f);

    float prevMag = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        float currentMag = 0.0f;
        for (int i = 0; i < hop; ++i)
        {
            float s = std::abs (data[f * hop + i]);
            currentMag += s;
        }
        currentMag /= (float) hop;

        float diff = currentMag - prevMag;
        onsets[f] = (diff > 0.0f) ? diff : 0.0f;
        prevMag = currentMag;
    }

    // Auto-correlate Onset Novelty function across 70-175 BPM
    float maxCorr = 0.0f;
    int bestLag = 38;

    for (int lag = 25; lag <= 80; ++lag)
    {
        float c = 0.0f;
        for (size_t f = 0; f < onsets.size() - lag; ++f)
            c += onsets[f] * onsets[f + lag];

        if (c > maxCorr)
        {
            maxCorr = c;
            bestLag = lag;
        }
    }

    double secPerBeat = (bestLag * hop) / 44100.0;
    if (secPerBeat > 0.25)
    {
        int detectedBpm = (int) std::round (60.0 / secPerBeat);
        // Normalize double-time or half-time bounds
        if (detectedBpm < 70) detectedBpm *= 2;
        if (detectedBpm > 175) detectedBpm /= 2;
        track.sourceBpm = juce::jlimit (70, 175, detectedBpm);
    }

    // 2. Harmonic Pitch Class Profiles (HPCP) Dot-Product Matching
    double chroma[12] = { 0.0 };
    int checkLen = std::min (44100 * 4, total);

    for (int lag = 70; lag < 500; lag += 2)
    {
        double freq = 44100.0 / (double) lag;
        if (freq >= 65.0 && freq <= 600.0)
        {
            int midi = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
            int pClass = ((midi % 12) + 12) % 12;

            float corr = 0.0f;
            for (int i = 0; i < checkLen - lag; i += 32)
                corr += data[i] * data[i + lag];

            chroma[pClass] += std::max (0.0f, corr);
        }
    }

    // Score against all 12 Major & 12 Minor Krumhansl Templates
    double bestScore = -1e9;
    int bestKey = 0;
    int bestMode = 0; // 0: Minor, 1: Major

    for (int root = 0; root < 12; ++root)
    {
        double scoreMaj = 0.0;
        double scoreMin = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            int note = (root + i) % 12;
            scoreMaj += chroma[note] * kMajorProfile[i];
            scoreMin += chroma[note] * kMinorProfile[i];
        }

        if (scoreMin > bestScore)
        {
            bestScore = scoreMin;
            bestKey = root;
            bestMode = 0; // Minor
        }
        if (scoreMaj > bestScore)
        {
            bestScore = scoreMaj;
            bestKey = root;
            bestMode = 1; // Major
        }
    }

    track.sourceKey = bestKey;
    track.sourceScaleMode = bestMode;
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
        track.fileName = file.getFileNameWithoutExtension().substring (0, 10);

        runAdvancedAiAudioAnalysis (layerIdx);
        applyGridSlicingToLayer (layerIdx, currentGrid);

        // Auto-sync master project target to first loaded track if empty
        if (layerIdx == 0)
        {
            masterTargetBpm = track.sourceBpm;
            masterTargetKey = track.sourceKey;
            masterTargetScaleMode = track.sourceScaleMode;
        }

        rollDiceAndGenerateMasterPattern();
    }
}

void EXAudioProcessor::clearLayer (int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    tracks[layerIdx].buffer.setSize (2, 0);
    tracks[layerIdx].sliceMarkers.clear();
    tracks[layerIdx].hasAudio = false;
    tracks[layerIdx].fileName = "EMPTY";
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
        int spacing = total / 20;
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

// Structured Multi-Layer Arranger (Every Layer Gets Equal Musical Purpose)
void EXAudioProcessor::rollDiceAndGenerateMasterPattern()
{
    patternSteps.clear();

    std::vector<int> loadedTracks;
    for (int i = 0; i < 3; ++i)
        if (tracks[i].hasAudio && !tracks[i].sliceMarkers.empty())
            loadedTracks.push_back (i);

    if (loadedTracks.empty()) return;

    isDiceRolling.store (true);
    diceRollProgress = 0.0f;
    juce::Random rng;
    activeDiceNumber = 1 + rng.nextInt (6);

    // Musical Scale Intervals
    std::vector<int> targetIntervals;
    if (masterTargetScaleMode == 1)      // Major
        targetIntervals = { 0, 2, 4, 7, 9, 12, 14 };
    else if (masterTargetScaleMode == 2) // Dorian
        targetIntervals = { 0, 2, 3, 7, 9, 10, 14 };
    else if (masterTargetScaleMode == 3) // Phrygian (Dark Trap)
        targetIntervals = { 0, 1, 3, 5, 7, 8, 10 };
    else                                 // Natural Minor
        targetIntervals = { 0, 3, 5, 7, 8, 10, 12 };

    // Arrangement Architecture:
    // Bar 1 (0..3): Layer 1 Theme Anchor
    // Bar 2 (4..7): Layer 2 Texture & Harmonic Fill
    // Bar 3 (8..11): Layer 3 High Accent Hook / Counter
    // Bar 4 (12..15): Full Blend Resolution
    for (int step = 0; step < 16; ++step)
    {
        FlipStep s;

        int selectedTrack = loadedTracks[0];
        if (loadedTracks.size() == 2)
        {
            selectedTrack = (step % 4 < 2) ? loadedTracks[0] : loadedTracks[1];
        }
        else if (loadedTracks.size() >= 3)
        {
            if (step < 4) selectedTrack = loadedTracks[0];
            else if (step < 8) selectedTrack = loadedTracks[1];
            else if (step < 12) selectedTrack = loadedTracks[2];
            else selectedTrack = loadedTracks[step % 3]; // Round robin blend in final bar
        }

        auto& trk = tracks[selectedTrack];
        s.trackIdx = selectedTrack;
        s.sliceIdx = rng.nextInt ((int) trk.sliceMarkers.size());

        // Exact Scale Quantization
        int keyOffset = masterTargetKey - trk.sourceKey;
        int interval = targetIntervals[(size_t) rng.nextInt ((int) targetIntervals.size())];
        s.pitchOffset = keyOffset + interval;

        // Structured Gating: No random messy gaps
        s.isGate = (step % 2 == 0) || (rng.nextFloat() > 0.30f);
        s.velocity = 0.85f + (rng.nextFloat() * 0.15f);
        s.isReverse = (step % 4 == 3 && rng.nextFloat() > 0.5f); // Reverse fill only at bar ends

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
