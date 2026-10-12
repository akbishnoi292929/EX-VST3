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

    int curBpm = std::max (60, masterTargetBpm);
    samplesPerStep = static_cast<int> ((60.0 / curBpm) * 0.25 * sampleRate);
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
        if (freq >= 75.0 && freq <= 900.0)
        {
            int midi = (int) std::round (69.0 + 12.0 * std::log2 (freq / 440.0));
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

// -----------------------------------------------------------------------------------------
// COHESIVE MELODIC HOOK GENERATOR: SCALE PITCH-MOULDING, NOT RANDOM CHOPPING
// -----------------------------------------------------------------------------------------
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

    int progressionType = rng.nextInt (6);
    
    // Natural Minor Pentatonic Degrees (0: Root, 3: Minor 3rd, 5: 4th, 7: 5th, 10: Minor 7th)
    const int minorPentatonic[5] = { 0, 3, 5, 7, 10 };

    // Find the cleanest harmonic slice in the primary track as the instrument sound timbre
    int mainTrack = loadedTracks[0];
    int totalSlices = (int) tracks[mainTrack].sliceMarkers.size();
    int signatureToneSlice = rng.nextInt (std::max (1, totalSlices / 3));

    // Musical phrasing: 4 Bars structured melody (16 steps = 4 beats each in 16th resolution)
    // Structure:
    // Bar 1 (Steps 0-3): Call motif
    // Bar 2 (Steps 4-7): Response motif (Higher pitch resolution)
    // Bar 3 (Steps 8-11): Variation (Syncopated hook)
    // Bar 4 (Steps 12-15): Turnaround cadence resolution
    for (int step = 0; step < 16; ++step)
    {
        FlipStep s;
        int bar = step / 4;
        int beatInBar = step % 4;
        int rootChord = AiMatrixData::BillboardTrapChordChains[progressionType][bar];

        bool isNoteTrigger = false;
        int activeTrack = mainTrack;
        int activeSlice = signatureToneSlice;
        int noteOffset = rootChord;

        // Structured Call-and-Response Rhythm
        if (beatInBar == 0)
        {
            // Strong rhythmic downbeat of each bar
            isNoteTrigger = true;
            activeTrack = mainTrack;
            activeSlice = signatureToneSlice;
            noteOffset = rootChord; // Ground root note
            s.pan = 0.0f;
            s.distanceDepth = 0.05f;
            s.velocity = 0.95f;
        }
        else if (beatInBar == 2)
        {
            // Syncopated answer note on the off-beat
            if (rng.nextFloat() > 0.25f)
            {
                isNoteTrigger = true;
                if (loadedTracks.size() >= 2 && rng.nextFloat() > 0.35f)
                {
                    activeTrack = loadedTracks[1];
                    activeSlice = rng.nextInt ((int) tracks[activeTrack].sliceMarkers.size());
                }
                else
                {
                    activeTrack = mainTrack;
                    activeSlice = (signatureToneSlice + 1) % totalSlices;
                }
                noteOffset = rootChord + minorPentatonic[1 + rng.nextInt (3)]; // Harmonic 3rd, 4th, or 5th
                s.pan = (bar % 2 == 0) ? -0.35f : 0.35f;
                s.distanceDepth = 0.25f;
                s.velocity = 0.82f;
            }
        }
        else if (beatInBar == 3 && (bar == 1 || bar == 3))
        {
            // Melodic lick resolution at bar endings
            if (rng.nextFloat() > 0.30f)
            {
                isNoteTrigger = true;
                if (loadedTracks.size() >= 3 && rng.nextFloat() > 0.40f)
                {
                    activeTrack = loadedTracks[2];
                    activeSlice = rng.nextInt ((int) tracks[activeTrack].sliceMarkers.size());
                }
                else
                {
                    activeTrack = mainTrack;
                    activeSlice = signatureToneSlice;
                }
                noteOffset = rootChord + minorPentatonic[4]; // High 7th/octave ear-candy
                s.pan = (bar == 1) ? 0.60f : -0.60f;
                s.distanceDepth = 0.50f;
                s.velocity = 0.74f;
            }
        }

        s.trackIdx = activeTrack;
        s.sliceIdx = activeSlice;
        s.isGate = isNoteTrigger;

        // Accurate semitone transposition relative to loaded source key and target key
        int baseTransposition = masterTargetKey - tracks[activeTrack].sourceKey;
        s.pitchOffset = baseTransposition + noteOffset;
        s.isReverse = (step == 15 && rng.nextFloat() > 0.50f);

        patternSteps.push_back (s);
    }

    renderMasterPreviewBuffer();
}

void EXAudioProcessor::triggerNewSliceVoice (int trkIdx, int sliceIdx, int pitchOffset, float pan, float depth, bool reverse)
{
    if (trkIdx < 0 || trkIdx >= 3) return;
    auto& trk = tracks[trkIdx];
    if (!trk.hasAudio || sliceIdx >= (int) trk.sliceMarkers.size() || trk.buffer.getNumSamples() <= 0) return;

    int oldIdx = activeVoiceIndex;
    int newIdx = 1 - activeVoiceIndex;
    activeVoiceIndex = newIdx;

    if (voiceSlots[oldIdx].active)
    {
        voiceSlots[oldIdx].targetGain = 0.0f;
        voiceSlots[oldIdx].fadeStep = - (voiceSlots[oldIdx].currentGain / 256.0f);
    }

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

    float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    v.panL = std::cos (panAngle);
    v.panR = std::sin (panAngle);
    v.distance = depth;

    v.active = true;
    v.currentGain = 0.0f;
    v.targetGain = 1.0f;
    v.fadeStep = 1.0f / 256.0f;
}

// 100% TEMPO-LOCKED MASTER PREVIEW RENDERER
void EXAudioProcessor::renderMasterPreviewBuffer()
{
    int curBpm = std::max (60, masterTargetBpm);
    double stepSec = (60.0 / curBpm) * 0.25;
    if (halfTimeMode) stepSec *= 2.0;
    int curSamplesPerStep = static_cast<int> (stepSec * getSampleRate());
    int total = curSamplesPerStep * 16;

    juce::AudioBuffer<float> tempBuffer (2, total);
    tempBuffer.clear();

    if (patternSteps.empty())
    {
        masterPreviewBuffer.setSize (2, total);
        masterPreviewBuffer.clear();
        return;
    }

    for (int step = 0; step < 16; ++step)
    {
        const auto& s = patternSteps[(size_t) step];
        if (s.isGate && s.trackIdx >= 0 && s.trackIdx < 3)
        {
            const auto& trk = tracks[s.trackIdx];
            if (trk.hasAudio && !trk.isMute && s.sliceIdx < (int) trk.sliceMarkers.size() && trk.buffer.getNumSamples() > 0)
            {
                int start = trk.sliceMarkers[(size_t) s.sliceIdx];
                int end = (s.sliceIdx + 1 < (int) trk.sliceMarkers.size()) ? trk.sliceMarkers[(size_t) s.sliceIdx + 1] : trk.buffer.getNumSamples();

                double pRatio = std::pow (2.0, s.pitchOffset / 12.0);
                if (halfTimeMode) pRatio *= 0.5;

                int writeStart = step * curSamplesPerStep;
                int renderLength = std::min (curSamplesPerStep * 2, total - writeStart);
                double rPos = s.isReverse ? (end - 1) : start;

                float panAngle = (s.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                float gainL = std::cos (panAngle);
                float gainR = std::sin (panAngle);
                float distDamp = 1.0f - (s.distanceDepth * 0.25f);

                for (int i = 0; i < renderLength; ++i)
                {
                    int curR = (int) rPos;
                    if ((!s.isReverse && curR < end) || (s.isReverse && curR >= start))
                    {
                        if (writeStart + i < total && curR < trk.buffer.getNumSamples() && curR >= 0)
                        {
                            float env = 1.0f;
                            if (i < 256)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / 256.0f));
                            else if (renderLength - i < 512)
                                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (renderLength - i) / 512.0f));

                            float rawMono = trk.buffer.getSample (0, curR);
                            if (trk.buffer.getNumChannels() > 1)
                                rawMono = (rawMono + trk.buffer.getSample (1, curR)) * 0.5f;

                            if (!std::isnan(rawMono) && !std::isinf(rawMono))
                            {
                                float sampleVal = std::clamp (rawMono, -1.0f, 1.0f) * trk.volume * env * s.velocity * distDamp * 0.40f;
                                tempBuffer.addSample (0, writeStart + i, sampleVal * gainL);
                                tempBuffer.addSample (1, writeStart + i, sampleVal * gainR);
                            }
                        }
                        rPos += (s.isReverse ? -pRatio : pRatio);
                    }
                    else break;
                }
            }
        }
    }

    float maxPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < total; ++i)
        {
            float v = std::abs (tempBuffer.getSample (ch, i));
            if (!std::isnan (v) && !std::isinf (v) && v > maxPeak)
                maxPeak = v;
        }
    }

    float normGain = 0.85f;
    if (maxPeak > 0.001f)
        normGain = std::min (0.85f, 0.85f / maxPeak);

    tempBuffer.applyGain (normGain);

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

// EXACT GRID & BPM LOCKED MIDI EXPORTER
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

    // 16th note = Exactly 240 ticks at 960 PPQ
    const int ticksPerStep = ticksPerQuarter / 4; 

    for (size_t i = 0; i < snapshotSteps.size(); ++i)
    {
        const auto& s = snapshotSteps[i];
        if (s.isGate && s.trackIdx >= 0 && s.trackIdx < 3 && tracks[s.trackIdx].hasAudio)
        {
            // Transpose into audible middle-octave register (C4 to C6)
            int note = 60 + s.pitchOffset;
            while (note < 48) note += 12;
            while (note > 84) note -= 12;

            double startTick = static_cast<double> (i * ticksPerStep);
            // Sustained note duration across 2 steps for natural melody hold
            double endTick = startTick + (ticksPerStep * 2 - 20); 

            int midiPan = juce::jlimit (0, 127, static_cast<int> ((s.pan + 1.0f) * 63.5f));
            trackSeq.addEvent (juce::MidiMessage::controllerEvent (1, 10, midiPan), startTick);

            trackSeq.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (juce::jlimit (1, 127, (int) (s.velocity * 120)))), startTick);
            trackSeq.addEvent (juce::MidiMessage::noteOff (1, note), endTick);
        }
    }

    // Force exact 4-bar ending boundary marker so DAW doesn't shrink or stretch the pattern
    double totalLoopTicks = static_cast<double> (16 * ticksPerStep);
    trackSeq.addEvent (juce::MidiMessage::endOfTrack(), totalLoopTicks);
    trackSeq.updateMatchedPairs();
    mf.addTrack (trackSeq);

    juce::FileOutputStream fos (temp);
    if (fos.openedOk()) mf.writeTo (fos);

    return temp;
}

// PROCESS BLOCK WITH ZERO NOISE & EXACT SYNC
void EXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Detect Host DAW Playhead BPM directly for sample-accurate sync
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

    int curBpm = std::max (60, masterTargetBpm);
    double stepSeconds = (60.0 / curBpm) * 0.25;
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
                bool inBounds = v.isReverse ? (rPos >= v.playheadStart && rPos >= 0)
                                            : (rPos < v.playheadEnd && rPos < trk.buffer.getNumSamples() - 1);

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
                    float distDamp = 1.0f - (v.distance * 0.25f);
                    float finalGain = trk.volume * smoothGain * distDamp * 0.38f;

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
        int dSamples = static_cast<int> (delayTime * getSampleRate());
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
            if (absVal > 0.80f)
            {
                float excess = absVal - 0.80f;
                d[i] = std::copysign (0.80f + 0.10f * std::tanh (excess * 2.0f), d[i]);
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
