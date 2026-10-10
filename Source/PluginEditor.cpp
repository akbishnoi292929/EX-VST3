#include "PluginProcessor.h"
#include "PluginEditor.h"

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("EX // AK BISHNOI", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    setSize (600, 380);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor() {}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214)); // Dark background
    g.setColour (juce::Colour (0xff242428));
    g.drawRect (getLocalBounds(), 2);
}

void EXAudioProcessorEditor::resized()
{
    titleLabel.setBounds (0, 20, getWidth(), 40);
}
