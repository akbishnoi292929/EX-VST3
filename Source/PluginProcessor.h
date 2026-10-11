#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

struct MelodyStep
{
    int sliceIndex = 0;
    int pitchShiftSemis = 0; // Relative scale transposition
    float velocity = 1.0f;
    bool isGate = true;
};

class EXAudioProcessor : public juce::AudioProcessor
{
public:
    EXAudioProcessor();
    ~EXAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadAudioFile (const juce::File& file);
    void sliceAudioTransient();
    void rollDiceAndGenerateMelody();
    juce::File exportCurrentSequenceAsMidi();

    void toggleAudition();
    bool isAuditionPlaying() const { return isAudition.load(); }

    juce::AudioSampleBuffer& getAudioBuffer() { return loadedBuffer; }
    const std::vector<int>& getSlicePoints() const { return slicePoints; }

    // Source vs Target Parameters
    int sourceBpm = 130;
    int sourceKeyIdx = 0;
    int targetBpm = 140;
    int targetKeyIdx = 0; // 0: C, 1: C#, 2: D...
    int targetScaleMode = 0; // 0: Minor, 1: Major

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void detectBpmAndRootKey();

    juce::AudioSampleBuffer loadedBuffer;
    std::vector<int> slicePoints;
    std::vector<MelodyStep> melodyPattern;

    // Playback Engine
    std::atomic<bool> isAudition { false };
    int currentStep = 0;
    int samplesPerStep = 11025; // 1/16 note default
    int stepSampleCounter = 0;
    double activeSliceReadPos = 0.0;
    int activeSliceEnd = 0;
    double currentPitchFactor = 1.0;
    bool isSliceSounding = false;

    // DSP Rack
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::StateVariableTPTFilter<float> filter;

    // Delay line buffer
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessor)
};
