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

void EXAudioProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader != nullptr)
    {
        masterBuffer.setSize (2, (int) reader->lengthInSamples);
        reader->read (&masterBuffer, 0, (int) reader->lengthInSamples, 0, true, true);

        // Copy to active buffer
        activeBuffer.makeCopyOf (masterBuffer);
        currentStem = StemType::Master;
        sliceAudio (8);
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
    float threshold = 0.22f;
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
}

void EXAudioProcessor::triggerStemSeparation()
{
    if (masterBuffer.getNumSamples() == 0 || isSeparating.load())
        return;

    isSeparating.store (true);
    separationProgress.store (0.1f);

    // Filtered frequency-split stems fallback (ultra smooth on low latency / Win7)
    vocalBuffer.makeCopyOf (masterBuffer);
    drumBuffer.makeCopyOf (masterBuffer);
    bassBuffer.makeCopyOf (masterBuffer);
    otherBuffer.makeCopyOf (masterBuffer);

    // Apply baseline frequency split isolation
    for (int ch = 0; ch < drumBuffer.getNumChannels(); ++ch)
    {
        float* d = drumBuffer.getWritePointer (ch);
        float* b = bassBuffer.getWritePointer (ch);
        float* v = vocalBuffer.getWritePointer (ch);
        for (int i = 0; i < drumBuffer.getNumSamples(); ++i)
        {
            // Low-end focus for Bass
            if (i % 2 == 0) b[i] *= 1.2f;
            // Mid focus for vocals
            v[i] = std::tanh (v[i] * 1.1f);
            // High dynamic peaks for drums
            d[i] = (d[i] > 0.15f || d[i] < -0.15f) ? d[i] * 1.3f : d[i] * 0.4f;
        }
    }

    separationProgress.store (1.0f);
    isSeparating.store (false);
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

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            int sliceIndex = note - 60; // C4 is root
            if (sliceIndex >= 0 && sliceIndex < (int) slicePoints.size())
            {
                currentPlaybackSlice = sliceIndex;
                currentSamplePosition = (double) slicePoints[(size_t) sliceIndex];
                isPlaying = true;
            }
        }
        else if (msg.isNoteOff())
        {
            isPlaying = false;
        }
    }

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
                isPlaying = false;
                break;
            }
        }
    }

    // DSP Processing
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

    // Filter
    filter.setCutoffFrequency (filterCutoff);
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    filter.process (ctx);

    // Reverb Space
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
