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
    masterPreviewBuffer.setSize (2, 44100 * 4); // Default 4-second preview buffer
    masterPreviewBuffer.clear();
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

void EXAudioProcessor::runAdvancedAiAudioAnalysis (int layerIdx)
{
    auto& track = tracks[layerIdx];
    if (track.buffer.getNumSamples() == 0) return;

    int total = track.buffer.getNumSamples();
    const float* data = track.buffer.getReadPointer (0);

    // Exact bar-interval beat tracking
    double lengthSeconds = total / 44100.0;
    int candidates[] = { 4, 8, 16, 2 };
    int bestBpm = 130;
    double minDiff = 1e9;

    for (int bars : candidates)
    {
        double calculatedBpm = (bars * 4.0 * 60.0) / lengthSeconds;
        while (calculatedBpm < 70.0) calculatedBpm *= 2.0;
        while (calculatedBpm > 180.0) calculatedBpm /= 2.0;

        double diff = std::abs (calculatedBpm - std::round (calculatedBpm));
        if (diff < minDiff)
        {
            minDiff = diff;
            bestBpm = (int) std::round (calculatedBpm);
        }
    }
    track.sourceBpm = juce::jlimit (70, 180, bestBpm);

    // 100Hz - 800Hz Pitch Analysis for Clean Fundamental Extraction
    double chroma[12] = { 0.0 };
    int checkLen = std::min (44100 * 4, total);

    for (int lag = 70; lag < 450; lag += 2)
    {
        double freq = 44100.0 / (double) lag;
        if (freq >= 80.0 && freq <= 800.0)
        {
            int midi = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
            int pClass = ((midi % 12) + 12) % 12;

            float corr = 0.0f;
            for (int i = 0; i < checkLen - lag; i += 32)
                corr += data[i] * data[i + lag];

            chroma[pClass] += std::max (0.0f, corr);
        }
    }

    double bestScore = -1e9;
    int bestIdx = 12;

    for (int pIdx = 0; pIdx < 24; ++pIdx)
    {
        double score = 0.0;
        for (int note = 0; note < 12; ++note)
            score += chroma[note] * AiMatrixData::KrumhanslHarmonics[pIdx][note];

        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = pIdx;
        }
    }

    if (bestIdx < 12)
    {
        track.sourceKey = bestIdx;
        track.sourceScaleMode = 1;
    }
    else
    {
        track.sourceKey = bestIdx - 12;
        track.sourceScaleMode = 0;
    }
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

        if (layerIdx == 0)
        {
            masterTargetBpm = track.sourceBpm;
            masterTargetKey = track.sourceKey;
            masterTargetScaleMode = track.sourceScaleMode;
        }

        runCoordinatedAiGeneration();
        isPlayingPattern.store (false);
        isVoiceActive = false;
        playheadPosition = 0.0;
    }
}

void EXAudioProcessor::clearLayer (int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    tracks[layerIdx].buffer.setSize (2, 0);
    tracks[layerIdx].sliceMarkers.clear();
    tracks[layerIdx].hasAudio = false;
    tracks[layerIdx].fileName = "EMPTY";

    runCoordinatedAiGeneration();
    isPlayingPattern.store (false);
    isVoiceActive = false;
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

// Multi-Agent Collaboration AI: Evaluates all layers and generates melodic pocket
void EXAudioProcessor::runCoordinatedAiGeneration()
{
    patternSteps.clear();

    std::vector<int> loadedTracks;
    for (int i = 0; i < 3; ++i)
        if (tracks[i].hasAudio && !tracks[i].sliceMarkers.empty() && tracks[i].buffer.getNumSamples() > 0)
            loadedTracks.push_back (i);

    if (loadedTracks.empty())
    {
        masterPreviewBuffer.clear();
        return;
    }

    isDiceRolling.store (true);
    diceRollProgress = 0.0f;
    juce::Random rng;
    activeDiceNumber = 1 + rng.nextInt (6);

    int progressionType = rng.nextInt (4);

    for (int step = 0; step < 16; ++step)
    {
        FlipStep s;
        int barIdx = step / 4;

        int chosenTrack = loadedTracks[0];
        if (loadedTracks.size() == 2)
        {
            chosenTrack = (step % 2 == 0) ? loadedTracks[0] : loadedTracks[1];
        }
        else if (loadedTracks.size() >= 3)
        {
            if (step % 4 == 0) chosenTrack = loadedTracks[0];      // Foundation Layer
            else if (step % 4 == 2) chosenTrack = loadedTracks[1]; // Harmonic Layer
            else chosenTrack = loadedTracks[2];                    // Counter-Melody Layer
        }

        auto& trk = tracks[chosenTrack];
        s.trackIdx = chosenTrack;
        s.sliceIdx = rng.nextInt ((int) trk.sliceMarkers.size());

        int keyOffset = masterTargetKey - trk.sourceKey;
        int chordOffset = AiMatrixData::BillboardTrapChordChains[progressionType][barIdx];
        s.pitchOffset = keyOffset + chordOffset;

        s.isGate = (step % 2 == 0) || (rng.nextFloat() > 0.35f);
        s.velocity = (step % 4 == 0) ? 1.0f : (0.80f + (rng.nextFloat() * 0.18f));
        s.isReverse = (step == 7 || step == 15) ? (rng.nextFloat() > 0.40f) : false;

        patternSteps.push_back (s);
    }

    // Immediately render preview waveform for Layer 4
    renderMasterPreviewBuffer();
}

// Live Offline Render of Layer 4 Preview Track
void EXAudioProcessor::renderMasterPreviewBuffer()
{
    int baseBpm = std::max (60, masterTargetBpm);
    double stepSec = (60.0 / baseBpm) * 0.25;
    if (halfTimeMode) stepSec *= 2.0;
    int curSamplesPerStep = static_cast<int> (stepSec * getSampleRate());
    int total = curSamplesPerStep * 16;

    masterPreviewBuffer.setSize (2, total);
    masterPreviewBuffer.clear();

    if (patternSteps.empty()) return;

    for (int step = 0; step < 16; ++step)
    {
        const auto& s = patternSteps[(size_t) step];
        if (s.trackIdx >= 0 && s.trackIdx < 3)
        {
            const auto& trk = tracks[s.trackIdx];
            if (trk.hasAudio && !trk.isMute && s.isGate && s.sliceIdx < (int) trk.sliceMarkers.size() && trk.buffer.getNumSamples() > 0)
            {
                int start = trk.sliceMarkers[(size_t) s.sliceIdx];
                int end = (s.sliceIdx + 1 < (int) trk.sliceMarkers.size()) ? trk.sliceMarkers[(size_t) s.sliceIdx + 1] : trk.buffer.getNumSamples();

                double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
                if (halfTimeMode) pRatio *= 0.5;

                int writeStart = step * curSamplesPerStep;
                double rPos = s.isReverse ? (end - 1) : start;

                for (int i = 0; i < curSamplesPerStep; ++i)
                {
                    int curR = (int) rPos;
                    if ((!s.isReverse && curR < end) || (s.isReverse && curR >= start))
                    {
                        if (writeStart + i < total && curR < trk.buffer.getNumSamples() && curR >= 0)
                        {
                            // Anti-click Cosine Window at slice boundaries
                            float sliceEnv = 1.0f;
                            int dist = s.isReverse ? (curR - start) : (end - curR);
                            if (i < 128) sliceEnv = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / 128.0f));
                            else if (dist < 128) sliceEnv = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * dist / 128.0f));

                            for (int ch = 0; ch < 2; ++ch)
                                masterPreviewBuffer.setSample (ch, writeStart + i, trk.buffer.getSample (ch % trk.buffer.getNumChannels(), curR) * trk.volume * sliceEnv * 0.60f);
                        }
                        rPos += (s.isReverse ? -pRatio : pRatio);
                    }
                    else break;
                }
            }
        }
    }
}

void EXAudioProcessor::togglePlayback()
{
    bool willPlay = !isPlayingPattern.load();
    isPlayingPattern.store (willPlay);

    if (!willPlay)
    {
        isVoiceActive = false;
        playheadPosition = 0.0;
        stepClock = 0;
    }
    else
    {
        currentStepIndex = 0;
        stepClock = 0;
    }
}

juce::File EXAudioProcessor::exportCurrentAsAudioWav()
{
    auto tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_FL_Drop_Loop.wav");
    tempWav.deleteFile();

    if (masterPreviewBuffer.getNumSamples() == 0)
        renderMasterPreviewBuffer();

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (wavFmt.createWriterFor (new juce::FileOutputStream (tempWav), 44100.0, 2, 24, {}, 0));
    if (writer != nullptr && masterPreviewBuffer.getNumSamples() > 0)
        writer->writeFromAudioSampleBuffer (masterPreviewBuffer, 0, masterPreviewBuffer.getNumSamples());

    return tempWav;
}

juce::File EXAudioProcessor::exportCurrentAsMidi()
{
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_FL_Drop_Groove.mid");
    temp.deleteFile();

    if (patternSteps.empty()) return temp;

    const auto snapshotSteps = patternSteps;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence seq;
    int tickPerStep = 240;

    for (size_t i = 0; i < snapshotSteps.size(); ++i)
    {
        const auto& s = snapshotSteps[i];
        if (s.trackIdx >= 0 && s.trackIdx < 3 && tracks[s.trackIdx].hasAudio && s.isGate)
        {
            int note = 60 + s.sliceIdx + (s.trackIdx * 12);
            double sTick = static_cast<double> (i * tickPerStep);
            double eTick = sTick + (tickPerStep - 20);

            seq.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (s.velocity * 127)), sTick);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), eTick);
        }
    }

    mf.addTrack (seq);
    juce::FileOutputStream fos (temp);
    if (fos.openedOk()) mf.writeTo (fos);

    return temp;
}

// Zero-Distortion Anti-Click ProcessBlock
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
                playbackSpeedRatio = std::pow (2.0, (masterTargetKey - trk.sourceKey) / 12.0);
                currentStepReverse = false;
                declickFadeGain = 0.0f;
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
                if (step.trackIdx >= 0 && step.trackIdx < 3)
                {
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

                        playbackSpeedRatio = std::pow (2.0, step.pitchOffset / 12.0);
                        if (halfTimeMode) playbackSpeedRatio *= 0.5;
                        declickFadeGain = 0.0f;
                        isVoiceActive = true;
                    }
                    else
                    {
                        isVoiceActive = false;
                    }
                }
            }
        }

        if (isVoiceActive && isPlayingPattern.load())
        {
            auto& trk = tracks[activeTrackIdx];
            if (trk.hasAudio && trk.buffer.getNumSamples() > 0)
            {
                int rPos = static_cast<int> (playheadPosition);
                bool isValid = currentStepReverse ? (rPos >= playheadEnd && rPos >= 0)
                                                  : (rPos < playheadEnd && rPos < trk.buffer.getNumSamples() - 1);

                if (isValid)
                {
                    float frac = static_cast<float> (playheadPosition - rPos);
                    
                    // Smooth S-Curve Anti-Click Crossfade at boundaries
                    if (declickFadeGain < 1.0f)
                        declickFadeGain += (1.0f / 128.0f); // 128 samples attack ramp

                    int distFromEdge = currentStepReverse ? (rPos - playheadEnd) : (playheadEnd - rPos);
                    float edgeGain = 1.0f;
                    if (distFromEdge < 128)
                        edgeGain = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * distFromEdge / 128.0f));

                    float voiceGain = trk.volume * declickFadeGain * edgeGain * 0.55f;

                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        int sCh = ch % trk.buffer.getNumChannels();
                        float s0 = trk.buffer.getSample (sCh, rPos);
                        float s1 = trk.buffer.getSample (sCh, rPos + 1);
                        float interpolated = s0 + frac * (s1 - s0);

                        buffer.addSample (ch, s, interpolated * voiceGain);
                    }
                    playheadPosition += (currentStepReverse ? -playbackSpeedRatio : playbackSpeedRatio);
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
        float gain = 1.0f + (tapeDrive * 2.5f);
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

                delayBuffer.setSample (ch, delayWriteIndex, dry + (del * 0.35f));
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
        reverbParams.wetLevel = rvbMix * 0.50f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.20f);
        reverb.setParameters (reverbParams);
        reverb.process (pCtx);
    }

    buffer.applyGain (masterOut);

    // True Analog Brickwall Limiter (-0.5 dB True Ceiling with Smooth Knee)
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float absVal = std::abs (d[i]);
            if (absVal > 0.85f)
            {
                float excess = absVal - 0.85f;
                float compressed = 0.85f + 0.10f * std::tanh (excess * 3.0f);
                d[i] = std::copysign (compressed, d[i]);
            }
        }
    }
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
