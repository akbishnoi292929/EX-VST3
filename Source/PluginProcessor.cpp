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

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "VOL_LAYER_1", 1 }, "Layer 1 Vol", 0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "VOL_LAYER_2", 1 }, "Layer 2 Vol", 0.0f, 2.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "VOL_LAYER_3", 1 }, "Layer 3 Vol", 0.0f, 2.0f, 1.0f));

    return { params.begin(), params.end() };
}

EXAudioProcessor::EXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();
    masterPreviewBuffer.setSize (2, 44100 * 16);
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
    spec.sampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    spec.maximumBlockSize = static_cast<juce::uint32> (std::max (32, samplesPerBlock));
    spec.numChannels = 2;

    reverb.prepare (spec);
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    delayBuffer.setSize (2, static_cast<int> (spec.sampleRate * 2));
    delayBuffer.clear();
    delayWriteIndex = 0;

    int curBpm = std::max (60, masterTargetBpm);
    samplesPerStep = static_cast<int> ((60.0 / curBpm) * 0.25 * spec.sampleRate);
}

void EXAudioProcessor::releaseResources()
{
    reverb.reset();
    filter.reset();
    delayBuffer.clear();
}

bool EXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Pure Synth/Generator: Only Stereo Output supported
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
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

void EXAudioProcessor::analyzeLayerSlicesWithNeuralEngine (int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    if (track.buffer.getNumSamples() == 0) return;

    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    track.timbreProfile = neuralEngine.extractAcousticTimbre (track.buffer.getReadPointer (0),
                                                              std::min (track.buffer.getNumSamples(), 44100 * 2),
                                                              sRate);
}

void EXAudioProcessor::applyGridSlicingToLayer (int layerIdx, GridDivision div)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    track.sliceMarkers.clear();
    int total = track.buffer.getNumSamples();
    if (total == 0) return;

    if (div == GridDivision::Transient)
    {
        const float* d = track.buffer.getReadPointer (0);
        track.sliceMarkers.push_back (0);

        int minDistance = total / 24;
        int window = 384;

        std::vector<float> bodyEnergyProfile;
        float prevLowPass = 0.0f;

        for (int i = 0; i < total - window; i += window)
        {
            float energy = 0.0f;
            for (int k = 0; k < window; ++k)
            {
                float smp = d[i + k];
                prevLowPass = prevLowPass + 0.18f * (smp - prevLowPass);
                energy += (prevLowPass * prevLowPass);
            }
            bodyEnergyProfile.push_back (std::sqrt (energy / static_cast<float> (window)));
        }

        int lastCut = 0;
        for (size_t p = 1; p < bodyEnergyProfile.size() - 1; ++p)
        {
            int samplePos = static_cast<int> (p) * window;
            float surge = bodyEnergyProfile[p] - bodyEnergyProfile[p - 1];

            if (surge > 0.028f && bodyEnergyProfile[p] > 0.035f && (samplePos - lastCut) > minDistance)
            {
                track.sliceMarkers.push_back (snapToZeroCrossing (track.buffer, samplePos));
                lastCut = samplePos;
            }
        }

        if (track.sliceMarkers.size() < 12)
        {
            track.sliceMarkers.clear();
            int stepSize = total / 16;
            for (int i = 0; i < 16; ++i)
                track.sliceMarkers.push_back (snapToZeroCrossing (track.buffer, i * stepSize));
        }

        analyzeLayerSlicesWithNeuralEngine (layerIdx);
        return;
    }

    int numCuts = 16;
    if (div == GridDivision::Quarter)        numCuts = 4;
    else if (div == GridDivision::Eighth)    numCuts = 8;
    else if (div == GridDivision::Sixteenth) numCuts = 16;
    else if (div == GridDivision::Triplet1_3) numCuts = 6;
    else if (div::Triplet1_6 == div)         numCuts = 12;

    int stepSize = total / numCuts;
    for (int i = 0; i < numCuts; ++i)
        track.sliceMarkers.push_back (snapToZeroCrossing (track.buffer, i * stepSize));

    analyzeLayerSlicesWithNeuralEngine (layerIdx);
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
            bestBpm = static_cast<int> (std::round (calculatedBpm));
        }
    }
    track.sourceBpm = juce::jlimit (70, 180, bestBpm);

    double chroma[12] = { 0.0 };
    int checkLen = std::min (44100 * 4, total);

    for (int lag = 70; lag < 450; lag += 2)
    {
        double freq = 44100.0 / static_cast<double> (lag);
        if (freq >= 80.0 && freq <= 800.0)
        {
            int midi = static_cast<int> (std::round (69.0 + 12.0 * std::log2 (freq / 440.0)));
            int pClass = ((midi % 12) + 12) % 12;

            float corr = 0.0f;
            for (int i = 0; i < checkLen - lag; i += 24)
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
        applyGridSlicingToLayer (layerIdx, GridDivision::Transient);

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

void EXAudioProcessor::setSlicePosition (int layerIdx, int markerIdx, int newSamplePos)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& track = tracks[layerIdx];
    if (markerIdx >= 0 && markerIdx < static_cast<int> (track.sliceMarkers.size()))
    {
        int snapped = snapToZeroCrossing (track.buffer, newSamplePos);
        track.sliceMarkers[static_cast<size_t> (markerIdx)] = juce::jlimit (0, track.buffer.getNumSamples() - 1, snapped);
        std::sort (track.sliceMarkers.begin(), track.sliceMarkers.end());
        analyzeLayerSlicesWithNeuralEngine (layerIdx);
    }
}

void EXAudioProcessor::rollDiceAndGenerateMasterPattern()
{
    runCoordinatedAiGeneration();
}

static int quantizeToMasterScale (int rawPitch, int targetRootKey, int targetMode)
{
    static const int scaleIntervals[4][7] = {
        { 0, 2, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 5, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 9, 10 },
        { 0, 1, 3, 5, 7, 8, 10 }
    };

    int modeIdx = juce::jlimit (0, 3, targetMode);
    const int* validDegrees = scaleIntervals[modeIdx];

    int pitchClass = (rawPitch - targetRootKey) % 12;
    if (pitchClass < 0) pitchClass += 12;

    int bestDegree = validDegrees[0];
    int minDistance = 999;
    for (int d = 0; d < 7; ++d)
    {
        int dist = std::abs (pitchClass - validDegrees[d]);
        if (dist < minDistance)
        {
            minDistance = dist;
            bestDegree = validDegrees[d];
        }
    }

    int octaves = (rawPitch - targetRootKey - pitchClass);
    return targetRootKey + octaves + bestDegree;
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

    for (int i = 0; i < 64; ++i)
    {
        FlipStep blankStep;
        blankStep.stepIndex = i;
        blankStep.isGate = false;
        patternSteps.push_back (blankStep);
    }

    int mainTrack = loadedTracks[0];
    const auto& trk = tracks[mainTrack];
    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;

    auto timbre = neuralEngine.extractAcousticTimbre (trk.buffer.getReadPointer (0),
                                                      std::min (trk.buffer.getNumSamples(), 44100 * 2),
                                                      sRate);

    int seed = rng.nextInt();
    auto composedNotes = neuralEngine.composeInspiredMelody (timbre, masterTargetScaleMode, seed);

    for (const auto& note : composedNotes)
    {
        if (note.stepIndex < 0 || note.stepIndex >= 64) continue;

        FlipStep s;
        s.stepIndex = note.stepIndex;
        s.trackIdx = mainTrack;
        s.sliceIdx = rng.nextInt (std::max (1, static_cast<int> (trk.sliceMarkers.size())));
        s.isGate = true;
        s.velocity = note.velocity;
        s.pan = note.pan;
        s.distanceDepth = 0.1f;
        s.isReverse = false;

        int targetMidi = 60 + masterTargetKey + note.scaleDegree;
        int quantizedMidi = quantizeToMasterScale (targetMidi, masterTargetKey, masterTargetScaleMode);

        int pitchOffset = quantizedMidi - (60 + trk.sourceKey);
        while (pitchOffset < -8) pitchOffset += 12;
        while (pitchOffset > 8)  pitchOffset -= 12;

        s.pitchOffset = pitchOffset;
        patternSteps[static_cast<size_t> (note.stepIndex)] = s;
    }

    renderMasterPreviewBuffer();
}

void EXAudioProcessor::triggerNewSliceVoice (int trkIdx, int sliceIdx, int pitchOffset, float pan, float depth, bool reverse)
{
    if (trkIdx < 0 || trkIdx >= 3) return;
    auto& trk = tracks[trkIdx];
    if (!trk.hasAudio || sliceIdx >= static_cast<int> (trk.sliceMarkers.size()) || trk.buffer.getNumSamples() <= 0) return;

    int oldIdx = activeVoiceIndex;
    int newIdx = 1 - activeVoiceIndex;
    activeVoiceIndex = newIdx;

    if (voiceSlots[oldIdx].active)
    {
        voiceSlots[oldIdx].targetGain = 0.0f;
        voiceSlots[oldIdx].fadeStep = - (voiceSlots[oldIdx].currentGain / 256.0f);
    }

    int startPt = trk.sliceMarkers[static_cast<size_t> (sliceIdx)];
    int endPt = (sliceIdx + 1 < static_cast<int> (trk.sliceMarkers.size())) ? trk.sliceMarkers[static_cast<size_t> (sliceIdx + 1)] : trk.buffer.getNumSamples();

    auto& v = voiceSlots[newIdx];
    v.trackIdx = trkIdx;
    v.isReverse = reverse;
    v.playheadStart = startPt;
    v.playheadEnd = endPt;
    v.playheadPosition = static_cast<double> (startPt);
    v.speedRatio = std::pow (2.0, pitchOffset / 12.0);
    if (halfTimeMode) v.speedRatio *= 0.5;

    float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    v.panL = std::cos (panAngle);
    v.panR = std::sin (panAngle);
    v.distance = depth;

    v.active = true;
    v.currentGain = 0.0f;
    v.targetGain = 1.0f;
    v.fadeStep = 1.0f / 256.0f;
}

void EXAudioProcessor::renderMasterPreviewBuffer()
{
    int curBpm = std::max (60, masterTargetBpm);
    double stepSec = (60.0 / curBpm) * 0.25;
    if (halfTimeMode) stepSec *= 2.0;
    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    int curSamplesPerStep = static_cast<int> (stepSec * sRate);
    int total = curSamplesPerStep * 64;

    juce::AudioBuffer<float> tempBuffer (2, total);
    tempBuffer.clear();

    if (patternSteps.empty())
    {
        masterPreviewBuffer.setSize (2, total);
        masterPreviewBuffer.clear();
        return;
    }

    const float vol1 = apvts.getRawParameterValue ("VOL_LAYER_1")->load();
    const float vol2 = apvts.getRawParameterValue ("VOL_LAYER_2")->load();
    const float vol3 = apvts.getRawParameterValue ("VOL_LAYER_3")->load();
    const float layerVols[3] = { vol1, vol2, vol3 };

    for (int step = 0; step < 64; ++step)
    {
        const auto& s = patternSteps[static_cast<size_t> (step)];
        if (s.isGate && s.trackIdx >= 0 && s.trackIdx < 3)
        {
            const auto& trk = tracks[s.trackIdx];
            if (trk.hasAudio && !trk.isMute && s.sliceIdx < static_cast<int> (trk.sliceMarkers.size()) && trk.buffer.getNumSamples() > 0)
            {
                int start = trk.sliceMarkers[static_cast<size_t> (s.sliceIdx)];
                int end = (s.sliceIdx + 1 < static_cast<int> (trk.sliceMarkers.size())) ? trk.sliceMarkers[static_cast<size_t> (s.sliceIdx + 1)] : trk.buffer.getNumSamples();

                double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
                if (halfTimeMode) pRatio *= 0.5;

                int writeStart = step * curSamplesPerStep;
                int renderLength = std::min (curSamplesPerStep * 5, total - writeStart);
                double rPos = start;

                float panAngle = (s.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                float gainL = std::cos (panAngle);
                float gainR = std::sin (panAngle);
                float lGain = layerVols[s.trackIdx];

                for (int i = 0; i < renderLength; ++i)
                {
                    int curR = static_cast<int> (rPos);
                    if (curR < end && curR < trk.buffer.getNumSamples() - 1)
                    {
                        if (writeStart + i < total)
                        {
                            float env = 1.0f;
                            if (i < 160)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / 160.0f));
                            else if (renderLength - i < 1200)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (renderLength - i) / 1200.0f));

                            float rawL = trk.buffer.getSample (0, curR);
                            float rawR = (trk.buffer.getNumChannels() > 1) ? trk.buffer.getSample (1, curR) : rawL;

                            float smpVal = ((rawL + rawR) * 0.5f) * trk.volume * lGain * env * s.velocity * 0.85f;
                            tempBuffer.addSample (0, writeStart + i, smpVal * gainL);
                            tempBuffer.addSample (1, writeStart + i, smpVal * gainR);
                        }
                        rPos += pRatio;
                    }
                    else break;
                }
            }
        }
    }

    const float tapeDrive = apvts.getRawParameterValue ("TAPE_DRIVE")->load();
    const float delayTime = apvts.getRawParameterValue ("DELAY_TIME")->load();
    const float delayMix  = apvts.getRawParameterValue ("DELAY_MIX")->load();
    const float rvbMix    = apvts.getRawParameterValue ("REVERB_MIX")->load();
    const float masterOut = apvts.getRawParameterValue ("MASTER_OUT")->load();

    if (delayMix > 0.01f)
    {
        int dSamples = static_cast<int> (delayTime * sRate);
        if (dSamples > 0 && dSamples < total)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = dSamples; i < total; ++i)
                {
                    float dry = tempBuffer.getSample (ch, i);
                    float wet = tempBuffer.getSample (ch, i - dSamples);
                    tempBuffer.setSample (ch, i, dry + (wet * delayMix * 0.35f));
                }
            }
        }
    }

    if (rvbMix > 0.01f)
    {
        int rvbOffset = static_cast<int> (0.045 * sRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = rvbOffset; i < total; ++i)
            {
                float dry = tempBuffer.getSample (ch, i);
                float tail = tempBuffer.getSample (ch, i - rvbOffset);
                tempBuffer.setSample (ch, i, dry + (tail * rvbMix * 0.30f));
            }
        }
    }

    float driveGain = 1.0f + (tapeDrive * 1.5f);
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = tempBuffer.getWritePointer (ch);
        for (int i = 0; i < total; ++i)
        {
            float v = d[i] * masterOut;
            if (tapeDrive > 0.01f)
                v = std::tanh (v * driveGain) / std::tanh (driveGain);
            if (std::abs (v) > 0.95f)
                v = std::copysign (0.95f, v);
            d[i] = v;
        }
    }

    masterPreviewBuffer.setSize (2, total, false, true, true);
    for (int ch = 0; ch < 2; ++ch)
        masterPreviewBuffer.copyFrom (ch, 0, tempBuffer, ch, 0, total);
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

juce::StringArray EXAudioProcessor::exportCurrentAsStems()
{
    juce::StringArray stemFiles;
    juce::int64 timestamp = juce::Time::currentTimeMillis();

    int curBpm = std::max (60, masterTargetBpm);
    double stepSec = (60.0 / curBpm) * 0.25;
    if (halfTimeMode) stepSec *= 2.0;
    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    int curSamplesPerStep = static_cast<int> (stepSec * sRate);
    int total = curSamplesPerStep * 64;

    for (int l = 0; l < 3; ++l)
    {
        if (!tracks[l].hasAudio) continue;

        auto fileName = "EX_Stem_L" + juce::String (l + 1) + "_" + juce::String (timestamp % 10000) + ".wav";
        auto tempWav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (fileName);

        juce::AudioBuffer<float> stemBuf (2, total);
        stemBuf.clear();

        for (int step = 0; step < 64; ++step)
        {
            const auto& s = patternSteps[static_cast<size_t> (step)];
            if (s.isGate && s.trackIdx == l)
            {
                const auto& trk = tracks[l];
                if (s.sliceIdx < static_cast<int> (trk.sliceMarkers.size()) && trk.buffer.getNumSamples() > 0)
                {
                    int start = trk.sliceMarkers[static_cast<size_t> (s.sliceIdx)];
                    int end = (s.sliceIdx + 1 < static_cast<int> (trk.sliceMarkers.size())) ? trk.sliceMarkers[static_cast<size_t> (s.sliceIdx + 1)] : trk.buffer.getNumSamples();

                    double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
                    if (halfTimeMode) pRatio *= 0.5;

                    int writeStart = step * curSamplesPerStep;
                    int renderLength = std::min (curSamplesPerStep * 5, total - writeStart);
                    double rPos = start;

                    for (int i = 0; i < renderLength; ++i)
                    {
                        int curR = static_cast<int> (rPos);
                        if (curR < end && curR < trk.buffer.getNumSamples() - 1 && writeStart + i < total)
                        {
                            float env = 1.0f;
                            if (i < 160) env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / 160.0f));
                            float rawL = trk.buffer.getSample (0, curR);
                            float rawR = (trk.buffer.getNumChannels() > 1) ? trk.buffer.getSample (1, curR) : rawL;
                            float smp = ((rawL + rawR) * 0.5f) * trk.volume * env * s.velocity * 0.85f;
                            stemBuf.addSample (0, writeStart + i, smp);
                            stemBuf.addSample (1, writeStart + i, smp);
                            rPos += pRatio;
                        }
                        else break;
                    }
                }
            }
        }

        juce::WavAudioFormat wavFmt;
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFmt.createWriterFor (new juce::FileOutputStream (tempWav), 44100.0, 2, 24, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (stemBuf, 0, total);

        stemFiles.add (tempWav.getFullPathName());
    }

    return stemFiles;
}

juce::File EXAudioProcessor::exportCurrentAsMidi()
{
    juce::int64 timestamp = juce::Time::currentTimeMillis();
    auto fileName = "EX_Groove_" + juce::String (timestamp % 100000) + ".mid";
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (fileName);

    if (patternSteps.empty()) return temp;

    const auto snapshotSteps = patternSteps;
    juce::MidiFile mf;
    const int ticksPerQuarter = 960;
    mf.setTicksPerQuarterNote (ticksPerQuarter);

    int exportBpm = std::max (60, masterTargetBpm);
    double microSecondsPerQuarter = 60000000.0 / exportBpm;

    juce::MidiMessageSequence trackSeq;
    trackSeq.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (microSecondsPerQuarter)), 0.0);
    trackSeq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    const int ticksPerStep = ticksPerQuarter / 4;

    for (size_t i = 0; i < snapshotSteps.size(); ++i)
    {
        const auto& s = snapshotSteps[i];
        if (s.isGate && s.trackIdx >= 0 && s.trackIdx < 3 && tracks[s.trackIdx].hasAudio)
        {
            int note = 60 + s.pitchOffset;
            while (note < 48) note += 12;
            while (note > 84) note -= 12;

            double startTick = static_cast<double> (i * ticksPerStep);
            double endTick = startTick + (ticksPerStep * 4 - 20);

            int midiPan = juce::jlimit (0, 127, static_cast<int> ((s.pan + 1.0f) * 63.5f));
            trackSeq.addEvent (juce::MidiMessage::controllerEvent (1, 10, midiPan), startTick);

            trackSeq.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (juce::jlimit (1, 127, static_cast<int> (s.velocity * 124)))), startTick);
            trackSeq.addEvent (juce::MidiMessage::noteOff (1, note), endTick);
        }
    }

    double totalLoopTicks = static_cast<double> (64 * ticksPerStep);
    trackSeq.addEvent (juce::MidiMessage::endOfTrack(), totalLoopTicks);
    trackSeq.updateMatchedPairs();
    mf.addTrack (trackSeq);

    juce::FileOutputStream fos (temp);
    if (fos.openedOk()) mf.writeTo (fos);

    return temp;
}

void EXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (position->getBpm().hasValue())
            {
                int hostBpm = static_cast<int> (*position->getBpm());
                if (hostBpm >= 60 && hostBpm <= 220)
                    masterTargetBpm = hostBpm;
            }
        }
    }

    const float tapeDrive = apvts.getRawParameterValue ("TAPE_DRIVE")->load();
    const float delayTime = apvts.getRawParameterValue ("DELAY_TIME")->load();
    const float delayMix  = apvts.getRawParameterValue ("DELAY_MIX")->load();
    const float rvbSize   = apvts.getRawParameterValue ("REVERB_SIZE")->load();
    const float rvbMix    = apvts.getRawParameterValue ("REVERB_MIX")->load();
    const float cutoff    = apvts.getRawParameterValue ("FILTER_CUTOFF")->load();
    const float masterOut = apvts.getRawParameterValue ("MASTER_OUT")->load();

    const float vol1 = apvts.getRawParameterValue ("VOL_LAYER_1")->load();
    const float vol2 = apvts.getRawParameterValue ("VOL_LAYER_2")->load();
    const float vol3 = apvts.getRawParameterValue ("VOL_LAYER_3")->load();
    const float layerVols[3] = { vol1, vol2, vol3 };

    int curBpm = std::max (60, masterTargetBpm);
    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    double stepSeconds = (60.0 / curBpm) * 0.25;
    if (halfTimeMode) stepSeconds *= 2.0;
    samplesPerStep = static_cast<int> (stepSeconds * sRate);

    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            int raw = msg.getNoteNumber() - 60;
            int trkIdx = juce::jlimit (0, 2, raw / 12);
            int sIdx = raw % 12;

            auto& trk = tracks[trkIdx];
            if (trk.hasAudio && sIdx < static_cast<int> (trk.sliceMarkers.size()))
            {
                triggerNewSliceVoice (trkIdx, sIdx, masterTargetKey - trk.sourceKey, 0.0f, 0.0f, false);
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
                if (step.isGate && step.trackIdx >= 0 && step.trackIdx < 3)
                {
                    auto& trk = tracks[step.trackIdx];
                    if (trk.hasAudio && !trk.isMute && step.sliceIdx < static_cast<int> (trk.sliceMarkers.size()))
                    {
                        triggerNewSliceVoice (step.trackIdx, step.sliceIdx, step.pitchOffset, step.pan, step.distanceDepth, step.isReverse);
                    }
                }
            }
        }

        for (int vIdx = 0; vIdx < 2; ++vIdx)
        {
            auto& v = voiceSlots[vIdx];
            if (v.active && v.trackIdx >= 0 && v.trackIdx < 3)
            {
                auto& trk = tracks[v.trackIdx];
                int rPos = static_cast<int> (v.playheadPosition);
                bool inBounds = (rPos >= v.playheadStart && rPos < v.playheadEnd && rPos < trk.buffer.getNumSamples() - 1);

                if (inBounds)
                {
                    float frac = static_cast<float> (v.playheadPosition - rPos);

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

                    float smoothGain = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * v.currentGain));
                    float finalGain = trk.volume * layerVols[v.trackIdx] * smoothGain * 0.75f;

                    float s0 = trk.buffer.getSample (0, rPos);
                    float s1 = trk.buffer.getSample (0, rPos + 1);
                    float interpolated = s0 + frac * (s1 - s0);

                    if (trk.buffer.getNumChannels() > 1)
                    {
                        float s0_R = trk.buffer.getSample (1, rPos);
                        float s1_R = trk.buffer.getSample (1, rPos + 1);
                        interpolated = (interpolated + (s0_R + frac * (s1_R - s0_R))) * 0.5f;
                    }

                    if (!std::isnan(interpolated) && !std::isinf(interpolated))
                    {
                        float val = std::clamp (interpolated, -1.0f, 1.0f) * finalGain;
                        buffer.addSample (0, s, val * v.panL);
                        buffer.addSample (1, s, val * v.panR);
                    }
                    v.playheadPosition += v.speedRatio;
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
        float gain = 1.0f + (tapeDrive * 1.5f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::tanh (d[i] * gain);
        }
    }

    if (delayMix > 0.01f && delayBuffer.getNumSamples() > 0)
    {
        int dSamples = static_cast<int> (delayTime * sRate);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                int rIdx = (delayWriteIndex - dSamples + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                float del = delayBuffer.getSample (ch, rIdx);
                float dry = buffer.getSample (ch, i);

                float toDelay = dry + std::clamp (del * 0.20f, -0.5f, 0.5f);
                toDelay = std::clamp (toDelay, -1.0f, 1.0f);

                delayBuffer.setSample (ch, delayWriteIndex, toDelay);
                buffer.setSample (ch, i, dry + (del * delayMix * 0.25f));
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
        reverbParams.wetLevel = rvbMix * 0.40f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.20f);
        reverb.setParameters (reverbParams);
        reverb.process (pCtx);
    }

    buffer.applyGain (masterOut);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(d[i]) || std::isinf(d[i]))
            {
                d[i] = 0.0f;
                continue;
            }
            float absVal = std::abs (d[i]);
            if (absVal > 0.90f)
            {
                float excess = absVal - 0.90f;
                d[i] = std::copysign (0.90f + 0.08f * std::tanh (excess * 2.0f), d[i]);
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
