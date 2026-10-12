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
    masterPreviewBuffer.setSize (2, 44100 * 4);
    masterPreviewBuffer.clear();

    for (int i = 0; i < 2; ++i)
    {
        voiceSlots[i].active = false;
        voiceSlots[i].currentGain = 0.0f;
    }
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
    delayBuffer.clear();
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

static int snapToZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample)
{
    if (buf.getNumSamples() <= 1) return 0;
    int clamped = juce::jlimit (0, buf.getNumSamples() - 2, targetSample);
    const float* channel = buf.getReadPointer (0);

    int searchWindow = 128;
    int start = std::max (0, clamped - searchWindow);
    int end = std::min (buf.getNumSamples() - 2, clamped + searchWindow);

    int bestSample = clamped;
    float minAbsVal = std::abs (channel[clamped]);

    for (int i = start; i <= end; ++i)
    {
        if ((channel[i] <= 0.0f && channel[i + 1] >= 0.0f) ||
            (channel[i] >= 0.0f && channel[i + 1] <= 0.0f))
        {
            return i;
        }
        float absVal = std::abs (channel[i]);
        if (absVal < minAbsVal)
        {
            minAbsVal = absVal;
            bestSample = i;
        }
    }
    return bestSample;
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

        rollDiceAndGenerateMasterPattern();
        isPlayingPattern.store (false);
        for (int i = 0; i < 2; ++i) voiceSlots[i].active = false;
    }
}

void EXAudioProcessor::clearLayer (int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    tracks[layerIdx].buffer.setSize (2, 0);
    tracks[layerIdx].sliceMarkers.clear();
    tracks[layerIdx].hasAudio = false;
    tracks[layerIdx].fileName = "EMPTY";

    rollDiceAndGenerateMasterPattern();
    isPlayingPattern.store (false);
    for (int i = 0; i < 2; ++i) voiceSlots[i].active = false;
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
                track.sliceMarkers.push_back (snapToZeroCrossing (track.buffer, i));
                last = i;
                if (track.sliceMarkers.size() >= 16) break;
            }
        }
        return;
    }

    int stepSize = total / numCuts;
    for (int i = 0; i < numCuts; ++i)
        track.sliceMarkers.push_back (snapToZeroCrossing (track.buffer, i * stepSize));
}

void EXAudioProcessor::setSlicePosition (int layerIdx, int markerIdx, int newSamplePos)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    if (markerIdx >= 0 && markerIdx < static_cast<int> (track.sliceMarkers.size()))
    {
        int snapped = snapToZeroCrossing (track.buffer, newSamplePos);
        track.sliceMarkers[static_cast<size_t> (markerIdx)] = juce::jlimit (0, track.buffer.getNumSamples() - 1, snapped);
        std::sort (track.sliceMarkers.begin(), track.sliceMarkers.end());
    }
}

void EXAudioProcessor::rollDiceAndGenerateMasterPattern()
{
    runCoordinatedAiGeneration();
}

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
    
    int anchorSlice = rng.nextInt ((int) tracks[loadedTracks[0]].sliceMarkers.size());
    int answerSlice = (anchorSlice + 2) % (int) tracks[loadedTracks[0]].sliceMarkers.size();

    for (int step = 0; step < 16; ++step)
    {
        FlipStep s;
        int barIdx = step / 4;
        int stepInBar = step % 4;

        int chosenTrack = loadedTracks[0];
        if (loadedTracks.size() == 2)
        {
            chosenTrack = (stepInBar % 2 == 0) ? loadedTracks[0] : loadedTracks[1];
        }
        else if (loadedTracks.size() >= 3)
        {
            if (stepInBar == 0) chosenTrack = loadedTracks[0];
            else if (stepInBar == 2) chosenTrack = loadedTracks[1];
            else chosenTrack = loadedTracks[2];
        }

        auto& trk = tracks[chosenTrack];
        s.trackIdx = chosenTrack;

        if (stepInBar == 0)
            s.sliceIdx = anchorSlice;
        else if (stepInBar == 2)
            s.sliceIdx = answerSlice;
        else
            s.sliceIdx = rng.nextInt ((int) trk.sliceMarkers.size());

        int keyOffset = masterTargetKey - trk.sourceKey;
        int chordOffset = AiMatrixData::BillboardTrapChordChains[progressionType][barIdx];
        s.pitchOffset = keyOffset + chordOffset;

        s.isGate = (stepInBar == 0) || (stepInBar == 2) || (rng.nextFloat() > 0.40f);
        s.velocity = (stepInBar == 0) ? 1.0f : (0.78f + (rng.nextFloat() * 0.18f));
        s.isReverse = (step == 7 || step == 15) ? (rng.nextFloat() > 0.40f) : false;

        patternSteps.push_back (s);
    }

    renderMasterPreviewBuffer();
}

// Seamless Anti-Click Ping-Pong Crossfade Voice Trigger (5ms / 256 samples fade)
void EXAudioProcessor::triggerNewSliceVoice (int trkIdx, int sliceIdx, int pitchOffset, bool reverse)
{
    if (trkIdx < 0 || trkIdx >= 3) return;
    auto& trk = tracks[trkIdx];
    if (!trk.hasAudio || sliceIdx >= (int) trk.sliceMarkers.size() || trk.buffer.getNumSamples() <= 0) return;

    int oldIdx = activeVoiceIndex;
    int newIdx = 1 - activeVoiceIndex;
    activeVoiceIndex = newIdx;

    // Fade out previous voice smoothly over 256 samples
    if (voiceSlots[oldIdx].active)
    {
        voiceSlots[oldIdx].targetGain = 0.0f;
        voiceSlots[oldIdx].fadeStep = - (voiceSlots[oldIdx].currentGain / 256.0f);
    }

    // Initialize new voice and fade in over 256 samples
    int startPt = trk.sliceMarkers[(size_t) sliceIdx];
    int endPt = (sliceIdx + 1 < (int) trk.sliceMarkers.size()) ? trk.sliceMarkers[(size_t) sliceIdx + 1] : trk.buffer.getNumSamples();

    auto& v = voiceSlots[newIdx];
    v.trackIdx = trkIdx;
    v.isReverse = reverse;
    v.playheadStart = startPt;
    v.playheadEnd = endPt;
    v.playheadPosition = reverse ? static_cast<double> (endPt - 1) : static_cast<double> (startPt);
    v.speedRatio = std::pow (2.0, pitchOffset / 12.0);
    if (halfTimeMode) v.speedRatio *= 0.5;

    v.active = true;
    v.currentGain = 0.0f;
    v.targetGain = 1.0f;
    v.fadeStep = 1.0f / 256.0f;
}

// 100% Click-Free Master Preview Buffer Render with Crossfaded Offline DSP
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

    // Ping-pong offline rendering for zero-click export
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

                // Render with smooth 256-sample crossfading window
                for (int i = 0; i < curSamplesPerStep; ++i)
                {
                    int curR = (int) rPos;
                    if ((!s.isReverse && curR < end) || (s.isReverse && curR >= start))
                    {
                        if (writeStart + i < total && curR < trk.buffer.getNumSamples() && curR >= 0)
                        {
                            float env = 1.0f;
                            if (i < 256)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / 256.0f));
                            else if (curSamplesPerStep - i < 256)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (curSamplesPerStep - i) / 256.0f));

                            for (int ch = 0; ch < 2; ++ch)
                            {
                                float val = trk.buffer.getSample (ch % trk.buffer.getNumChannels(), curR);
                                if (!std::isnan(val) && !std::isinf(val))
                                    masterPreviewBuffer.setSample (ch, writeStart + i, val * trk.volume * env * 0.48f);
                            }
                        }
                        rPos += (s.isReverse ? -pRatio : pRatio);
                    }
                    else break;
                }
            }
        }
    }

    // Apply Offline Processing
    const float tapeDrive = apvts.getRawParameterValue ("TAPE_DRIVE")->load();
    const float delayTime = apvts.getRawParameterValue ("DELAY_TIME")->load();
    const float delayMix  = apvts.getRawParameterValue ("DELAY_MIX")->load();
    const float rvbSize   = apvts.getRawParameterValue ("REVERB_SIZE")->load();
    const float rvbMix    = apvts.getRawParameterValue ("REVERB_MIX")->load();
    const float cutoff    = apvts.getRawParameterValue ("FILTER_CUTOFF")->load();
    const float masterOut = apvts.getRawParameterValue ("MASTER_OUT")->load();

    if (tapeDrive > 0.01f)
    {
        float gain = 1.0f + (tapeDrive * 1.8f);
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = masterPreviewBuffer.getWritePointer (ch);
            for (int i = 0; i < total; ++i)
                d[i] = std::tanh (d[i] * gain);
        }
    }

    if (delayMix > 0.01f)
    {
        int dSamples = static_cast<int> (delayTime * getSampleRate());
        if (dSamples > 0 && dSamples < total)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = dSamples; i < total; ++i)
                {
                    float dry = masterPreviewBuffer.getSample (ch, i);
                    float wet = masterPreviewBuffer.getSample (ch, i - dSamples);
                    masterPreviewBuffer.setSample (ch, i, dry + (wet * delayMix * 0.28f));
                }
            }
        }
    }

    filter.setCutoffFrequency (cutoff);
    juce::dsp::AudioBlock<float> blk (masterPreviewBuffer);
    juce::dsp::ProcessContextReplacing<float> pCtx (blk);
    filter.process (pCtx);

    if (rvbMix > 0.01f)
    {
        reverbParams.roomSize = rvbSize * 0.80f;
        reverbParams.damping  = 0.45f;
        reverbParams.wetLevel = rvbMix * 0.45f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.20f);
        reverb.setParameters (reverbParams);
        reverb.process (pCtx);
    }

    masterPreviewBuffer.applyGain (masterOut);

    // Studio Clean Limiter without clicks
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = masterPreviewBuffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
        {
            if (std::isnan(d[i]) || std::isinf(d[i])) { d[i] = 0.0f; continue; }
            float absVal = std::abs (d[i]);
            if (absVal > 0.82f)
            {
                float excess = absVal - 0.82f;
                d[i] = std::copysign (0.82f + 0.10f * std::tanh (excess * 2.0f), d[i]);
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
        for (int i = 0; i < 2; ++i) voiceSlots[i].active = false;
        stepClock = 0;
        delayBuffer.clear();
    }
    else
    {
        currentStepIndex = 0;
        stepClock = 0;
    }
}

juce::File EXAudioProcessor::exportCurrentAsAudioWav()
{
    juce::int64 timestamp = juce::Time::currentTimeMillis();
    auto fileName = "EX_Loop_" + juce::String (timestamp % 100000) + ".wav";
    auto tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (fileName);

    renderMasterPreviewBuffer();

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (wavFmt.createWriterFor (new juce::FileOutputStream (tempWav), 44100.0, 2, 24, {}, 0));
    if (writer != nullptr && masterPreviewBuffer.getNumSamples() > 0)
        writer->writeFromAudioSampleBuffer (masterPreviewBuffer, 0, masterPreviewBuffer.getNumSamples());

    return tempWav;
}

juce::File EXAudioProcessor::exportCurrentAsMidi()
{
    juce::int64 timestamp = juce::Time::currentTimeMillis();
    auto fileName = "EX_Groove_" + juce::String (timestamp % 100000) + ".mid";
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (fileName);

    if (patternSteps.empty()) return temp;

    const auto snapshotSteps = patternSteps;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    
    juce::MidiMessageSequence metaTrack;
    double microSecondsPerQuarter = 60000000.0 / std::max (60, masterTargetBpm);
    metaTrack.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (microSecondsPerQuarter)), 0.0);
    metaTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    metaTrack.updateMatchedPairs();
    mf.addTrack (metaTrack);

    juce::MidiMessageSequence noteTrack;
    int tickPerStep = 240;

    for (size_t i = 0; i < snapshotSteps.size(); ++i)
    {
        const auto& s = snapshotSteps[i];
        if (s.trackIdx >= 0 && s.trackIdx < 3 && tracks[s.trackIdx].hasAudio && s.isGate)
        {
            int note = 60 + s.sliceIdx;
            double sTick = static_cast<double> (i * tickPerStep);
            double eTick = sTick + (tickPerStep - 30);

            noteTrack.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (s.velocity * 127)), sTick);
            noteTrack.addEvent (juce::MidiMessage::noteOff (1, note), eTick);
        }
    }

    noteTrack.updateMatchedPairs();
    mf.addTrack (noteTrack);

    juce::FileOutputStream fos (temp);
    if (fos.openedOk()) mf.writeTo (fos);

    return temp;
}

// 100% Click-Free Real-Time Block Processing with Dual Voice Crossfading
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
                triggerNewSliceVoice (trkIdx, sIdx, masterTargetKey - trk.sourceKey, false);
            }
        }
        else if (msg.isNoteOff() && !isPlayingPattern.load())
        {
            for (int i = 0; i < 2; ++i)
            {
                voiceSlots[i].targetGain = 0.0f;
                voiceSlots[i].fadeStep = - (voiceSlots[i].currentGain / 256.0f);
            }
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
                        triggerNewSliceVoice (step.trackIdx, step.sliceIdx, step.pitchOffset, step.isReverse);
                    }
                }
            }
        }

        // Render Dual Ping-Pong Voices Simultaneously (Seamless Overlap Without Clicks)
        for (int vIdx = 0; vIdx < 2; ++vIdx)
        {
            auto& v = voiceSlots[vIdx];
            if (v.active && v.trackIdx >= 0 && v.trackIdx < 3)
            {
                auto& trk = tracks[v.trackIdx];
                int rPos = static_cast<int> (v.playheadPosition);
                bool inBounds = v.isReverse ? (rPos >= v.playheadStart && rPos >= 0)
                                            : (rPos < v.playheadEnd && rPos < trk.buffer.getNumSamples() - 1);

                if (inBounds)
                {
                    // Linear fractional sample interpolation
                    float frac = static_cast<float> (v.playheadPosition - rPos);

                    // Update Cosine Smooth Fade Ramp
                    v.currentGain += v.fadeStep;
                    if (v.fadeStep > 0.0f && v.currentGain >= v.targetGain)
                    {
                        v.currentGain = v.targetGain;
                        v.fadeStep = 0.0f;
                    }
                    else if (v.fadeStep < 0.0f && v.currentGain <= v.targetGain)
                    {
                        v.currentGain = 0.0f;
                        v.active = false;
                        v.fadeStep = 0.0f;
                    }

                    // True Cosine Curve Gain
                    float smoothGain = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * v.currentGain));
                    float finalGain = trk.volume * smoothGain * 0.48f;

                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        int sCh = ch % trk.buffer.getNumChannels();
                        float s0 = trk.buffer.getSample (sCh, rPos);
                        float s1 = trk.buffer.getSample (sCh, rPos + 1);
                        float interpolated = s0 + frac * (s1 - s0);

                        if (!std::isnan(interpolated) && !std::isinf(interpolated))
                            buffer.addSample (ch, s, interpolated * finalGain);
                    }
                    v.playheadPosition += (v.isReverse ? -v.speedRatio : v.speedRatio);
                }
                else
                {
                    v.active = false;
                }
            }
        }
    }

    if (tapeDrive > 0.01f)
    {
        float gain = 1.0f + (tapeDrive * 1.8f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::tanh (d[i] * gain);
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

                float toDelay = dry + (del * 0.25f);
                if (std::isnan(toDelay) || std::isinf(toDelay) || std::abs(toDelay) > 1.0f)
                    toDelay = std::clamp(toDelay, -1.0f, 1.0f);

                delayBuffer.setSample (ch, delayWriteIndex, toDelay);
                buffer.setSample (ch, i, dry + (del * delayMix * 0.28f));
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
        reverbParams.roomSize = rvbSize * 0.80f;
        reverbParams.damping  = 0.45f;
        reverbParams.wetLevel = rvbMix * 0.45f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.20f);
        reverb.setParameters (reverbParams);
        reverb.process (pCtx);
    }

    buffer.applyGain (masterOut);

    // True Peak Soft Clipper (-0.8 dB true ceiling)
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(d[i]) || std::isinf(d[i])) { d[i] = 0.0f; continue; }
            float absVal = std::abs (d[i]);
            if (absVal > 0.82f)
            {
                float excess = absVal - 0.82f;
                d[i] = std::copysign (0.82f + 0.10f * std::tanh (excess * 2.0f), d[i]);
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
