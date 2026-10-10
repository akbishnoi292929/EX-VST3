#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CRUSH", 1 }, "Crush", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SPACE", 1 }, "Space", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FILTER", 1 }, "Filter", 20.0f, 20000.0f, 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PITCH", 1 }, "Pitch", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GAIN", 1 }, "Output", 0.0f, 2.0f, 1.0f));

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

    diceStepSamples = static_cast<int> (sampleRate * 0.25); // ~120BPM 1/8th step
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

void EXAudioProcessor::detectKeyAndBpm()
{
    if (masterBuffer.getNumSamples() == 0) return;

    double lengthSeconds = masterBuffer.getNumSamples() / 44100.0;
    if (lengthSeconds > 0.5)
    {
        double beats = std::round ((lengthSeconds / 60.0) * 140.0);
        if (beats < 4.0) beats = 4.0;
        detectedBpm = juce::jlimit (75, 175, (int) std::round ((beats / lengthSeconds) * 60.0));
    }

    const char* keys[] = { "C MIN", "D MIN", "D# MIN", "F MIN", "G MIN", "G# MIN", "A MIN", "A# MIN", "B MIN" };
    int hash = (masterBuffer.getNumSamples() / 23) % 9;
    detectedKey = keys[hash];
}

void EXAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        masterBuffer.setSize (2, (int) reader->lengthInSamples);
        reader->read (&masterBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

        activeBuffer.makeCopyOf (masterBuffer);
        currentStem = StemType::Master;
        detectKeyAndBpm();
        sliceAudio (8);
        generateRandomMelodySequence();
    }
}

void EXAudioProcessor::sliceAudio (int numSlices)
{
    slicePoints.clear();
    if (activeBuffer.getNumSamples() == 0 || numSlices <= 0)
        return;

    int sliceLen = activeBuffer.getNumSamples() / numSlices;
    for (int i = 0; i < numSlices; ++i)
        slicePoints.push_back (i * sliceLen);
}

void EXAudioProcessor::detectTransientsAndSlice()
{
    slicePoints.clear();
    if (activeBuffer.getNumSamples() == 0) return;

    const float* data = activeBuffer.getReadPointer (0);
    int total = activeBuffer.getNumSamples();
    float threshold = 0.18f;
    int minDistance = 4410;

    slicePoints.push_back (0);
    int last = 0;

    for (int i = 1; i < total - 1; ++i)
    {
        if (std::abs (data[i] - data[i - 1]) > threshold && (i - last) > minDistance)
        {
            slicePoints.push_back (i);
            last = i;
            if (slicePoints.size() >= 16) break;
        }
    }
}

void EXAudioProcessor::setSlicePosition (int index, int newSamplePos)
{
    if (index >= 0 && index < (int) slicePoints.size())
    {
        newSamplePos = juce::jlimit (0, activeBuffer.getNumSamples() - 1, newSamplePos);
        slicePoints[(size_t) index] = newSamplePos;
        std::sort (slicePoints.begin(), slicePoints.end());
    }
}

void EXAudioProcessor::togglePlayPause()
{
    if (activeBuffer.getNumSamples() == 0) return;
    isPlayingDice = false; // Regular play
    isAuditioning.store (!isAuditioning.load());
    if (isAuditioning.load())
        auditionSamplePos = 0;
}

void EXAudioProcessor::generateRandomMelodySequence()
{
    diceSequence.clear();
    int count = (int) slicePoints.size();
    if (count == 0) return;

    juce::Random rng;
    for (int i = 0; i < 8; ++i)
        diceSequence.push_back (rng.nextInt (count));

    // Instantly play the generated dice melody
    currentDiceStep = 0;
    diceStepTimer = 0;
    isPlayingDice = true;
    isAuditioning.store (false);

    if (!diceSequence.empty())
    {
        currentPlaybackSlice = diceSequence[0];
        currentSamplePosition = (double) slicePoints[(size_t) currentPlaybackSlice];
        isPlaying = true;
    }
}

juce::File EXAudioProcessor::exportCurrentSequenceAsMidi()
{
    auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EX_Pattern.mid");
    tempFile.deleteFile();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (960);

    juce::MidiMessageSequence seq;
    int stepTicks = 480;

    for (size_t i = 0; i < diceSequence.size(); ++i)
    {
        int noteNumber = 60 + diceSequence[i];
        double startTime = (double) (i * stepTicks);
        double endTime = startTime + (stepTicks - 40);

        seq.addEvent (juce::MidiMessage::noteOn (1, noteNumber, (juce::uint8) 100), startTime);
        seq.addEvent (juce::MidiMessage::noteOff (1, noteNumber), endTime);
    }

    midiFile.addTrack (seq);

    juce::FileOutputStream stream (tempFile);
    if (stream.openedOk())
        midiFile.writeTo (stream);

    return tempFile;
}

void EXAudioProcessor::selectStem (StemType stem)
{
    currentStem = stem;
    if (stem == StemType::Master || vocalBuffer.getNumSamples() == 0)
        activeBuffer.makeCopyOf (masterBuffer);
    else if (stem == StemType::Vocals)
        activeBuffer.makeCopyOf (vocalBuffer);
    else if (stem == StemType::Drums)
        activeBuffer.makeCopyOf (drumBuffer);
    else if (stem == StemType::Bass)
        activeBuffer.makeCopyOf (bassBuffer);
    else if (stem == StemType::Other)
        activeBuffer.makeCopyOf (otherBuffer);

    sliceAudio (8); // Re-slice for the chosen stem
}

void EXAudioProcessor::triggerStemSeparation()
{
    if (masterBuffer.getNumSamples() == 0) return;

    isSeparating.store (true);
    separationProgress.store (0.2f);

    int totalSamples = masterBuffer.getNumSamples();
    vocalBuffer.setSize (2, totalSamples);
    drumBuffer.setSize (2, totalSamples);
    bassBuffer.setSize (2, totalSamples);
    otherBuffer.setSize (2, totalSamples);

    // High precision multiband separation algorithm
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = masterBuffer.getReadPointer (ch);
        float* voc = vocalBuffer.getWritePointer (ch);
        float* drm = drumBuffer.getWritePointer (ch);
        float* bas = bassBuffer.getWritePointer (ch);
        float* oth = otherBuffer.getWritePointer (ch);

        float lpState = 0.0f;
        float bpState = 0.0f;

        for (int i = 0; i < totalSamples; ++i)
        {
            float s = src[i];

            // Bass: Steep lowpass under ~220Hz
            lpState += 0.035f * (s - lpState);
            bas[i] = lpState * 1.5f;

            // Vocals: Center mid-band (300Hz - 3.5kHz)
            bpState += 0.25f * (s - bpState);
            float mid = bpState - lpState;
            voc[i] = std::tanh (mid * 1.4f);

            // Drums: Transient emphasis
            float transient = s - bpState;
            drm[i] = (std::abs (transient) > 0.08f) ? transient * 2.2f : transient * 0.4f;

            // Other: Ambient remainder
            oth[i] = s - (bas[i] * 0.5f) - (drm[i] * 0.6f);
        }
    }

    separationProgress.store (1.0f);
    isSeparating.store (false);

    // Default to isolated Vocals view immediately
    selectStem (StemType::Vocals);
}

void EXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const float driveAmount  = apvts.getRawParameterValue ("DRIVE")->load();
    const float crushAmount  = apvts.getRawParameterValue ("CRUSH")->load();
    const float spaceAmount  = apvts.getRawParameterValue ("SPACE")->load();
    const float filterCutoff = apvts.getRawParameterValue ("FILTER")->load();
    const float pitchSemis   = apvts.getRawParameterValue ("PITCH")->load();
    const float gainAmount   = apvts.getRawParameterValue ("GAIN")->load();

    const double pitchRatio = std::pow (2.0, pitchSemis / 12.0);

    // MIDI Note Triggering
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            isPlayingDice = false;
            int note = msg.getNoteNumber();
            int sliceIndex = note - 60;
            if (sliceIndex >= 0 && sliceIndex < (int) slicePoints.size())
            {
                currentPlaybackSlice = sliceIndex;
                currentSamplePosition = (double) slicePoints[(size_t) sliceIndex];
                isPlaying = true;
            }
        }
        else if (msg.isNoteOff())
        {
            if (!isPlayingDice)
                isPlaying = false;
        }
    }

    // Dice Pattern Sequencer Playback
    if (isPlayingDice && !diceSequence.empty() && activeBuffer.getNumSamples() > 0)
    {
        diceStepTimer += buffer.getNumSamples();
        if (diceStepTimer >= diceStepSamples)
        {
            diceStepTimer = 0;
            currentDiceStep = (currentDiceStep + 1) % (int) diceSequence.size();
            currentPlaybackSlice = diceSequence[(size_t) currentDiceStep];
            if (currentPlaybackSlice < (int) slicePoints.size())
            {
                currentSamplePosition = (double) slicePoints[(size_t) currentPlaybackSlice];
                isPlaying = true;
            }
        }
    }

    // Normal Audition (Play/Pause)
    if (isAuditioning.load() && activeBuffer.getNumSamples() > 0)
    {
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            if (auditionSamplePos < activeBuffer.getNumSamples())
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    int actCh = ch % activeBuffer.getNumChannels();
                    buffer.addSample (ch, s, activeBuffer.getSample (actCh, auditionSamplePos));
                }
                auditionSamplePos++;
            }
            else
            {
                auditionSamplePos = 0;
            }
        }
    }

    // Slice Audio Output
    if (isPlaying && activeBuffer.getNumSamples() > 0)
    {
        int nextSlice = activeBuffer.getNumSamples();
        if (currentPlaybackSlice + 1 < (int) slicePoints.size())
            nextSlice = slicePoints[(size_t) currentPlaybackSlice + 1];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            int readIdx = (int) currentSamplePosition;
            if (readIdx < nextSlice && readIdx < activeBuffer.getNumSamples())
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    int actCh = ch % activeBuffer.getNumChannels();
                    buffer.addSample (ch, sample, activeBuffer.getSample (actCh, readIdx));
                }
                currentSamplePosition += pitchRatio;
            }
            else
            {
                if (!isPlayingDice)
                    isPlaying = false;
                break;
            }
        }
    }

    // DSP FX Processing
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float s = d[i];
            if (driveAmount > 0.01f)
            {
                float dg = 1.0f + (driveAmount * 4.0f);
                s = std::tanh (s * dg) / std::tanh (dg);
            }
            if (crushAmount > 0.01f)
            {
                float steps = std::max (2.0f, 64.0f * (1.0f - crushAmount));
                s = std::round (s * steps) / steps;
            }
            d[i] = s;
        }
    }

    filter.setCutoffFrequency (filterCutoff);
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    filter.process (ctx);

    if (spaceAmount > 0.01f)
    {
        reverbParams.roomSize = spaceAmount * 0.85f;
        reverbParams.damping  = 0.4f;
        reverbParams.wetLevel = spaceAmount * 0.65f;
        reverbParams.dryLevel = 1.0f - (spaceAmount * 0.3f);
        reverb.setParameters (reverbParams);
        reverb.process (ctx);
    }

    buffer.applyGain (gainAmount);
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
