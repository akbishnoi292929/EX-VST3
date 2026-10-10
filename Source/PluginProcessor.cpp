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
        juce::ParameterID { "GAIN", 1 }, "Output", 0.0f, 1.5f, 1.0f));

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
}

void EXAudioProcessor::releaseResources()
{
    reverb.reset();
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
        loadedBuffer.setSize (2, (int) reader->lengthInSamples);
        reader->read (&loadedBuffer, 0, (int) reader->lengthInSamples, 0, true, true);
        sliceAudio (8); // Default 8 slices
    }
}

void EXAudioProcessor::sliceAudio (int numSlices)
{
    slicePoints.clear();
    if (loadedBuffer.getNumSamples() == 0 || numSlices <= 0)
        return;

    int sliceLength = loadedBuffer.getNumSamples() / numSlices;
    for (int i = 0; i < numSlices; ++i)
    {
        slicePoints.push_back (i * sliceLength);
    }
}

void EXAudioProcessor::detectTransientsAndSlice()
{
    slicePoints.clear();
    if (loadedBuffer.getNumSamples() == 0)
        return;

    const float* channelData = loadedBuffer.getReadPointer (0);
    int totalSamples = loadedBuffer.getNumSamples();
    float threshold = 0.25f;
    int minDistance = 4410; // 100ms at 44.1kHz

    slicePoints.push_back (0);
    int lastSlice = 0;

    for (int i = 1; i < totalSamples - 1; ++i)
    {
        float diff = std::abs (channelData[i] - channelData[i - 1]);
        if (diff > threshold && (i - lastSlice) > minDistance)
        {
            slicePoints.push_back (i);
            lastSlice = i;
            if (slicePoints.size() >= 16) break; // Maximum 16 slices
        }
    }
}

void EXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Handle MIDI Note-On events for triggering slices
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber(); // Note 60 = C4 (Slice 0)
            int sliceIndex = note - 60;
            if (sliceIndex >= 0 && sliceIndex < (int) slicePoints.size())
            {
                currentPlaybackSlice = sliceIndex;
                currentSamplePosition = slicePoints[(size_t) sliceIndex];
                isPlaying = true;
            }
        }
        else if (msg.isNoteOff())
        {
            isPlaying = false;
        }
    }

    // Playback Slices into buffer
    if (isPlaying && loadedBuffer.getNumSamples() > 0)
    {
        int nextSlicePoint = loadedBuffer.getNumSamples();
        if (currentPlaybackSlice + 1 < (int) slicePoints.size())
            nextSlicePoint = slicePoints[(size_t) currentPlaybackSlice + 1];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (currentSamplePosition < nextSlicePoint && currentSamplePosition < loadedBuffer.getNumSamples())
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    int loadedCh = ch % loadedBuffer.getNumChannels();
                    buffer.addSample (ch, sample, loadedBuffer.getSample (loadedCh, currentSamplePosition));
                }
                currentSamplePosition++;
            }
            else
            {
                isPlaying = false;
                break;
            }
        }
    }

    // DSP FX Processing
    const float driveAmount = apvts.getRawParameterValue ("DRIVE")->load();
    const float crushAmount = apvts.getRawParameterValue ("CRUSH")->load();
    const float spaceAmount = apvts.getRawParameterValue ("SPACE")->load();
    const float gainAmount  = apvts.getRawParameterValue ("GAIN")->load();

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float s = channelData[i];

            if (driveAmount > 0.01f)
            {
                float driveGain = 1.0f + (driveAmount * 4.0f);
                s = std::tanh (s * driveGain) / std::tanh (driveGain);
            }

            if (crushAmount > 0.01f)
            {
                float steps = std::max (2.0f, 64.0f * (1.0f - crushAmount));
                s = std::round (s * steps) / steps;
            }

            channelData[i] = s;
        }
    }

    if (spaceAmount > 0.01f)
    {
        reverbParams.roomSize = spaceAmount * 0.85f;
        reverbParams.damping  = 0.4f;
        reverbParams.wetLevel = spaceAmount * 0.6f;
        reverbParams.dryLevel = 1.0f - (spaceAmount * 0.3f);
        reverbParams.width    = 1.0f;
        reverb.setParameters (reverbParams);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);
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
