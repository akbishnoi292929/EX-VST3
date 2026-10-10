#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class EXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EXAudioProcessorEditor (EXAudioProcessor&);
    ~EXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EXAudioProcessor& audioProcessor;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EXAudioProcessorEditor)
};
