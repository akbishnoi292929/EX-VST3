#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "AiWeightsMatrix.h"

struct FlipStep
{
    int trackIdx = 0;
    int sliceIdx = 0;
    int pitchOffset = 0;
    float velocity = 1.0f;
    bool isReverse = false;
    bool isGate = true;
};

enum class GridDivision
{
    Quarter = 0,
    Eighth,
    Sixteenth,
    Triplet1_3,
    Triplet1_6,
    Transient
};

struct AudioLayerTrack
{
    juce::AudioSampleBuffer buffer;
    std::vector<int> sliceMarkers;
    int sourceBpm = 120;
    int sourceKey = 0;
    int sourceScaleMode = 0;
    float volume = 1.0f;
    bool isMute = false;
    bool hasAudio = false;
    juce::String fileName = "EMPTY";
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

    void loadAudioFileToLayer (int layerIndex, const juce::File& file);
    void applyGridSlicingToLayer (int layerIndex, GridDivision div);
    void setSlicePosition (int layerIndex, int markerIndex, int newSamplePosition);
    void clearLayer (int layerIndex);

    void rollDiceAndGenerateMasterPattern();
    juce::File exportCurrentAsAudioWav();
    juce::File exportCurrentAsMidi();

    void togglePlayback();
    bool isPlayingActive() const { return isPlayingPattern.load(); }

    AudioLayerTrack& getTrack (int index) { return tracks[juce::jlimit (0, 2, index)]; }

    int masterTargetBpm = 130;
    int masterTargetKey = 0;
    int masterTargetScaleMode = 0;
    bool halfTimeMode = false;
    GridDivision currentGrid = GridDivision::Eighth;

    std::atomic<bool> isDiceRolling { false };
    float diceRollProgress = 0.0f;
    int activeDiceNumber = 5;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void runAdvancedAiAudioAnalysis (int layerIndex);

    AudioLayerTrack tracks[3];
    std::vector<FlipStep> patternSteps;

    std::atomic<bool> isPlayingPattern { false };
    int currentStepIndex = 0;
    int samplesPerStep = 11025;
    int stepClock = 0;

    int activeTrackIdx = 0;
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
