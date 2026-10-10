#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class EXAudioProcessorEditor : public juce::AudioProcessorEditor,
                               public juce::FileDragAndDropTarget,
                               public juce::DragAndDropContainer,
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

    juce::Label headerLabel;
    juce::Label infoBadgeLabel;

    // Transport & Smart Buttons
    juce::TextButton playPauseBtn;
    juce::TextButton diceBtn;
    juce::TextButton dragMidiBtn;

    // Chops & AI Controls
    juce::TextButton chop1_4Btn;
    juce::TextButton chop1_8Btn;
    juce::TextButton chop1_16Btn;
    juce::TextButton chopTransientBtn;
    juce::TextButton stemSplitBtn;

    // Stem View Switcher Tabs
    juce::TextButton masterTab;
    juce::TextButton vocalTab;
    juce::TextButton drumTab;
    juce::TextButton bassTab;
    juce::TextButton otherTab;

    // 6-Knob Studio Rack
    juce::Slider driveKnob;
    juce::Slider crushKnob;
    juce::Slider spaceKnob;
    juce::Slider filterKnob;
    juce::Slider pitchKnob;
    juce::Slider gainKnob;

    juce::Label driveLabel, crushLabel, spaceLabel, filterLabel, pitchLabel, gainLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> driveAtt, crushAtt, spaceAtt, filterAtt, pitchAtt, gainAtt;

    int draggedSliceIndex = -1;
    juce::Rectangle<int> waveArea;

    void styleKnob (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix = "");
    void styleButton (juce::TextButton& b, juce::Colour col);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessorEditor)
};
