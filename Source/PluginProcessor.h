#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

enum class StemType { Master = 0, Vocals, Drums, Bass, Other };

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
    void sliceAudio (int numSlices);
    void detectTransientsAndSlice();
    void setSlicePosition (int index, int newSamplePos);
    void triggerStemSeparation();

    // Playback, Dice & MIDI
    void togglePlayPause();
    bool getIsAuditioning() const { return isAuditioning.load(); }
    void generateRandomMelodySequence();
    juce::File exportCurrentSequenceAsMidi();

    juce::AudioSampleBuffer& getAudioBuffer() { return activeBuffer; }
    const std::vector<int>& getSlicePoints() const { return slicePoints; }
    void selectStem (StemType stem);
    StemType getCurrentStem() const { return currentStem; }

    std::atomic<bool> isSeparating { false };
    std::atomic<float> separationProgress { 0.0f };
    juce::String detectedKey { "A MIN" };
    int detectedBpm { 140 };

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void detectKeyAndBpm();

    juce::AudioSampleBuffer masterBuffer;
    juce::AudioSampleBuffer vocalBuffer;
    juce::AudioSampleBuffer drumBuffer;
    juce::AudioSampleBuffer bassBuffer;
    juce::AudioSampleBuffer otherBuffer;
    juce::AudioSampleBuffer activeBuffer;

    StemType currentStem = StemType::Master;
    std::vector<int> slicePoints;
    std::vector<int> diceSequence; // Random pattern indices

    int currentPlaybackSlice = -1;
    double currentSamplePosition = 0.0;
    bool isPlaying = false;
    std::atomic<bool> isAuditioning { false };
    int auditionSamplePos = 0;

    // DSP Processors
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::StateVariableTPTFilter<float> filter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessor)
};
