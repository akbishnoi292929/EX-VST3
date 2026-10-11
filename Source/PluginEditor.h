#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class EXAudioProcessorEditor : public juce::AudioProcessorEditor,
                               public juce::FileDragAndDropTarget,
                               public juce::Timer
{
public:
    explicit EXAudioProcessorEditor (EXAudioProcessor&);
    ~EXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    EXAudioProcessor& audioProcessor;

    // Header & Titles
    juce::Label brandTitle;
    juce::Label subTitle;

    // Source Sample Controls (Detected + Manual Override)
    juce::Label sourceSectionTitle;
    juce::ComboBox sourceKeySelector;
    juce::Slider sourceBpmSlider;

    // Target Project Controls (Your Song Key & Tempo)
    juce::Label targetSectionTitle;
    juce::ComboBox targetKeySelector;
    juce::Slider targetBpmSlider;

    // Center Circular Oscilloscope / Radar + 3D Dice
    juce::TextButton diceButton;
    juce::TextButton playPauseButton;
    juce::TextButton dragMidiButton;

    // Vintage Knobs (Red Fluted Hardware Style)
    juce::Slider tapeDriveKnob;
    juce::Slider delayTimeKnob;
    juce::Slider delayMixKnob;
    juce::Slider reverbSizeKnob;
    juce::Slider reverbMixKnob;
    juce::Slider filterCutoffKnob;
    juce::Slider masterOutKnob;

    juce::Label tapeLabel, delayTimeLabel, delayMixLabel, rvbSizeLabel, rvbMixLabel, filterLabel, masterLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> tapeAtt, delayTimeAtt, delayMixAtt, rvbSizeAtt, rvbMixAtt, filterAtt, masterAtt;

    void setupVintageKnob (juce::Slider& s, juce::Label& l, const juce::String& name);
    void setupKeySelector (juce::ComboBox& box);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessorEditor)
};
