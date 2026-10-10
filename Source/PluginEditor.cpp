#include "PluginProcessor.h"
#include "PluginEditor.h"

void EXAudioProcessorEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe63946)); // Crimson Red
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffdddddd));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (12.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff999999));
    addAndMakeVisible (label);
}

void EXAudioProcessorEditor::setupButton (juce::TextButton& btn, const juce::String& text)
{
    btn.setButtonText (text);
    btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff25262c));
    btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (btn);
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    titleLabel.setText ("EX // SMART SAMPLER & WORKSTATION", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    setupButton (chop1_4Button, "1/4 CHOP");
    setupButton (chop1_8Button, "1/8 CHOP");
    setupButton (chopTransientButton, "TRANSIENT");
    setupButton (stemSplitButton, "AI STEM SPLIT (4-WAY)");

    chop1_4Button.onClick = [this]() {
        audioProcessor.sliceAudio (4);
        repaint();
    };

    chop1_8Button.onClick = [this]() {
        audioProcessor.sliceAudio (8);
        repaint();
    };

    chopTransientButton.onClick = [this]() {
        audioProcessor.detectTransientsAndSlice();
        repaint();
    };

    setupKnob (driveSlider, driveLabel, "DRIVE");
    setupKnob (crushSlider, crushLabel, "BIT CRUSH");
    setupKnob (spaceSlider, spaceLabel, "SPACE");
    setupKnob (gainSlider, gainLabel, "MASTER");

    driveAttach = std::make_unique<SliderAttachment> (audioProcessor.apvts, "DRIVE", driveSlider);
    crushAttach = std::make_unique<SliderAttachment> (audioProcessor.apvts, "CRUSH", crushSlider);
    spaceAttach = std::make_unique<SliderAttachment> (audioProcessor.apvts, "SPACE", spaceSlider);
    gainAttach  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "GAIN",  gainSlider);

    setSize (720, 480);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor() {}

bool EXAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto file : files)
    {
        if (file.endsWithIgnoreCase (".wav") || file.endsWithIgnoreCase (".mp3") || file.endsWithIgnoreCase (".aif"))
            return true;
    }
    return false;
}

void EXAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0)
    {
        audioProcessor.loadAudioFile (juce::File (files[0]));
        repaint();
    }
}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101114)); // Deep matte dark

    // Waveform Container
    auto waveArea = juce::Rectangle<int> (24, 60, getWidth() - 48, 160);
    g.setColour (juce::Colour (0xff18191f));
    g.fillRoundedRectangle (waveArea.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff2d2f38));
    g.drawRoundedRectangle (waveArea.toFloat(), 8.0f, 1.5f);

    auto& buffer = audioProcessor.getAudioBuffer();
    if (buffer.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xff00f5d4)); // Neon cyan wave
        auto numSamples = buffer.getNumSamples();
        auto width = waveArea.getWidth();
        auto midY = waveArea.getCentreY();
        auto maxH = waveArea.getHeight() / 2 - 10;

        for (int x = 0; x < width; ++x)
        {
            int sampleIdx = (int) ((x / (float) width) * numSamples);
            float sampleVal = buffer.getSample (0, sampleIdx);
            g.drawLine ((float) waveArea.getX() + x, midY - (sampleVal * maxH),
                        (float) waveArea.getX() + x, midY + (sampleVal * maxH), 1.0f);
        }

        // Slice Markers
        g.setColour (juce::Colour (0xffe63946)); // Red slice lines
        for (int slicePos : audioProcessor.getSlicePoints())
        {
            float lineX = waveArea.getX() + ((slicePos / (float) numSamples) * width);
            g.drawLine (lineX, (float) waveArea.getY(), lineX, (float) waveArea.getBottom(), 2.0f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff666a7a));
        g.setFont (15.0f);
        g.drawText ("DRAG & DROP AUDIO SAMPLE HERE (.WAV / .MP3)", waveArea, juce::Justification::centred);
    }
}

void EXAudioProcessorEditor::resized()
{
    titleLabel.setBounds (0, 16, getWidth(), 26);

    // Chops Buttons
    int btnY = 235;
    int btnW = 100;
    int btnH = 32;
    chop1_4Button.setBounds (24, btnY, btnW, btnH);
    chop1_8Button.setBounds (134, btnY, btnW, btnH);
    chopTransientButton.setBounds (244, btnY, 110, btnH);
    stemSplitButton.setBounds (getWidth() - 24 - 190, btnY, 190, btnH);

    // FX Knobs
    int startY = 300;
    int spacing = 160;
    int knobW = 95;
    int knobH = 95;
    int startX = (getWidth() - (spacing * 4)) / 2 + 30;

    juce::Slider* knobs[] = { &driveSlider, &crushSlider, &spaceSlider, &gainSlider };
    juce::Label* labels[] = { &driveLabel, &crushLabel, &spaceLabel, &gainLabel };

    for (int i = 0; i < 4; ++i)
    {
        knobs[i]->setBounds (startX + (i * spacing), startY, knobW, knobH);
        labels[i]->setBounds (startX + (i * spacing), startY + knobH + 4, knobW, 18);
    }
}
