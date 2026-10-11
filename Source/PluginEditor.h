#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VintageHardwareKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
};

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

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    EXAudioProcessor& audioProcessor;
    VintageHardwareKnobLookAndFeel vintageKnobLAF;

    juce::Label brandTitle;
    juce::Label subTitle;

    // Controls
    juce::Label sourceTitle;
    juce::ComboBox sourceKeySelector;
    juce::ComboBox sourceModeSelector;
    juce::Slider sourceBpmSlider;

    juce::Label targetTitle;
    juce::ComboBox targetKeySelector;
    juce::ComboBox targetModeSelector;
    juce::Slider targetBpmSlider;

    // Slicing Grid Buttons
    juce::TextButton btnQuarter;
    juce::TextButton btnEighth;
    juce::TextButton btnSixteenth;
    juce::TextButton btnTriplet1_3;
    juce::TextButton btnTriplet1_6;
    juce::TextButton btnTransient;

    // Center Controls & Exporters
    juce::TextButton diceButton;
    juce::TextButton playPauseButton;
    juce::TextButton dragMidiButton;
    juce::TextButton dragAudioButton;
    juce::ToggleButton halfTimeToggle;

    // Knobs
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

    float radarAngle = 0.0f;
    int draggingMarkerIdx = -1;
    juce::Rectangle<float> waveformArea;

    void setupHardwareKnob (juce::Slider& s, juce::Label& l, const juce::String& name);
    void setupKeySelector (juce::ComboBox& box);
    void setupModeSelector (juce::ComboBox& box);
    void draw3DDice (juce::Graphics& g, float cx, float cy, float size);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessorEditor)
};
