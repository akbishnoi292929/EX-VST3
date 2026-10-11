#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

struct FlipStep
{
    int sliceIdx = 0;
    int pitchOffset = 0;
    float velocity = 1.0f;
    bool isReverse = false;
    bool isGate = true;
};

enum class GridDivision
{
    Quarter = 0, // 1/4
    Eighth,      // 1/8
    Sixteenth,   // 1/16
    Triplet1_3,  // 1/3 Triplet
    Triplet1_6,  // 1/6 Triplet
    Transient    // Onset Detection
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
    void applyGridSlicing (GridDivision div);
    void setSlicePosition (int markerIndex, int newSamplePosition);
    void rollDiceAndFlipPattern();

    juce::File exportCurrentAsAudioWav();
    juce::File exportCurrentAsMidi();

    void togglePlayback();
    bool isPlayingActive() const { return isPlayingPattern.load(); }

    juce::AudioSampleBuffer& getAudioBuffer() { return sampleBuffer; }
    const std::vector<int>& getSlicePoints() const { return sliceMarkers; }

    int sourceBpm = 130;
    int sourceKey = 0;
    int sourceScaleMode = 0; // 0: Minor, 1: Major

    int targetBpm = 140;
    int targetKey = 0;
    int targetScaleMode = 0; // 0: Minor, 1: Major, 2: Dorian, 3: Phrygian
    bool halfTimeMode = false;
    GridDivision currentGrid = GridDivision::Eighth;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void analyzeAudioBpmAndKey();

    juce::AudioSampleBuffer sampleBuffer;
    std::vector<int> sliceMarkers;
    std::vector<FlipStep> patternSteps;

    std::atomic<bool> isPlayingPattern { false };
    int currentStepIndex = 0;
    int samplesPerStep = 11025;
    int stepClock = 0;

    double playheadPosition = 0.0;
    int playheadEnd = 0;
    double pitchSpeed = 1.0;
    bool isVoiceActive = false;
    bool currentStepReverse = false;

    // DSP Processors
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::StateVariableTPTFilter<float> filter;

    juce::AudioBuffer<float> delayBuffer;
    int delayWriteIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessor)
};
