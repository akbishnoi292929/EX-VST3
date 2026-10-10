#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class EXAudioProcessorEditor : public juce::AudioProcessorEditor,
                               public juce::FileDragAndDropTarget
{
public:
    explicit EXAudioProcessorEditor (EXAudioProcessor&);
    ~EXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    EXAudioProcessor& audioProcessor;

    juce::Label titleLabel;

    // Controls
    juce::TextButton chop1_4Button;
    juce::TextButton chop1_8Button;
    juce::TextButton chopTransientButton;
    juce::TextButton stemSplitButton;

    // FX Knobs
    juce::Slider driveSlider;
    juce::Slider crushSlider;
    juce::Slider spaceSlider;
    juce::Slider gainSlider;

    juce::Label driveLabel;
    juce::Label crushLabel;
    juce::Label spaceLabel;
    juce::Label gainLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttach;
    std::unique_ptr<SliderAttachment> crushAttach;
    std::unique_ptr<SliderAttachment> spaceAttach;
    std::unique_ptr<SliderAttachment> gainAttach;

    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupButton (juce::TextButton& btn, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessorEditor)
};
