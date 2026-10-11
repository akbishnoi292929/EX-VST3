#include "PluginProcessor.h"
#include "PluginEditor.h"

void EXAudioProcessorEditor::setupVintageKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 15);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffb82525)); // Deep Vintage Crimson
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff36383e)); // Bakelite Dark
    s.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff0f0f0));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (11.0f, juce::Font::bold));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff4a4c52));
    addAndMakeVisible (l);
}

void EXAudioProcessorEditor::setupKeySelector (juce::ComboBox& box)
{
    const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        box.addItem (keys[i], i + 1);

    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffe8e6df));
    box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1f2124));
    box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffb8b4a8));
    addAndMakeVisible (box);
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    brandTitle.setText ("SYNTH SHAPER // EX", juce::dontSendNotification);
    brandTitle.setFont (juce::Font (18.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff26282e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HARMONIC GENERATIVE SAMPLE FLIPPER", juce::dontSendNotification);
    subTitle.setFont (juce::Font (10.0f, juce::Font::plain));
    subTitle.setColour (juce::Label::textColourId, juce::Colour (0xff7a7c85));
    addAndMakeVisible (subTitle);

    // Source Controls
    sourceSectionTitle.setText ("INPUT SAMPLE", juce::dontSendNotification);
    sourceSectionTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    sourceSectionTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e3333));
    addAndMakeVisible (sourceSectionTitle);

    setupKeySelector (sourceKeySelector);
    sourceKeySelector.setSelectedId (audioProcessor.sourceKeyIdx + 1, juce::dontSendNotification);
    sourceKeySelector.onChange = [this]() {
        audioProcessor.sourceKeyIdx = sourceKeySelector.getSelectedId() - 1;
    };

    sourceBpmSlider.setRange (60, 200, 1);
    sourceBpmSlider.setValue (audioProcessor.sourceBpm, juce::dontSendNotification);
    sourceBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    sourceBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    sourceBpmSlider.onValueChange = [this]() {
        audioProcessor.sourceBpm = (int) sourceBpmSlider.getValue();
    };
    addAndMakeVisible (sourceBpmSlider);

    // Target Controls
    targetSectionTitle.setText ("PROJECT TARGET", juce::dontSendNotification);
    targetSectionTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    targetSectionTitle.setColour (juce::Label::textColourId, juce::Colour (0xff206b3e));
    addAndMakeVisible (targetSectionTitle);

    setupKeySelector (targetKeySelector);
    targetKeySelector.setSelectedId (audioProcessor.targetKeyIdx + 1, juce::dontSendNotification);
    targetKeySelector.onChange = [this]() {
        audioProcessor.targetKeyIdx = targetKeySelector.getSelectedId() - 1;
    };

    targetBpmSlider.setRange (60, 200, 1);
    targetBpmSlider.setValue (audioProcessor.targetBpm, juce::dontSendNotification);
    targetBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    targetBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    targetBpmSlider.onValueChange = [this]() {
        audioProcessor.targetBpm = (int) targetBpmSlider.getValue();
    };
    addAndMakeVisible (targetBpmSlider);

    // Center Big Interactive 3D Dice Button
    diceButton.setButtonText (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb2 FLIP DICE"));
    diceButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffa62424));
    diceButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    diceButton.onClick = [this]() {
        audioProcessor.rollDiceAndGenerateMelody();
        repaint();
    };
    addAndMakeVisible (diceButton);

    playPauseButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    playPauseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff32353b));
    playPauseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    playPauseButton.onClick = [this]() {
        audioProcessor.toggleAudition();
        playPauseButton.setButtonText (audioProcessor.isAuditionPlaying()
            ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE")
            : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    };
    addAndMakeVisible (playPauseButton);

    dragMidiButton.setButtonText ("DRAG MIDI");
    dragMidiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff244b70));
    dragMidiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    dragMidiButton.onClick = [this]() {
        auto mf = audioProcessor.exportCurrentSequenceAsMidi();
        if (mf.existsAsFile())
        {
            juce::StringArray files;
            files.add (mf.getFullPathName());
            performExternalDragDropOfFiles (files, false);
        }
    };
    addAndMakeVisible (dragMidiButton);

    // FX Knobs
    setupVintageKnob (delayTimeKnob, delayTimeLabel, "TIME");
    setupVintageKnob (delayMixKnob, delayMixLabel, "DLY MIX");
    setupVintageKnob (reverbSizeKnob, rvbSizeLabel, "SPACE");
    setupVintageKnob (reverbMixKnob, rvbMixLabel, "RVB MIX");
    setupVintageKnob (tapeDriveKnob, tapeLabel, "TAPE DRIVE");
    setupVintageKnob (filterCutoffKnob, filterLabel, "FILTER");
    setupVintageKnob (masterOutKnob, masterLabel, "MASTER");

    delayTimeAtt = std::make_unique<Attachment> (audioProcessor.apvts, "DELAY_TIME", delayTimeKnob);
    delayMixAtt  = std::make_unique<Attachment> (audioProcessor.apvts, "DELAY_MIX",  delayMixKnob);
    rvbSizeAtt   = std::make_unique<Attachment> (audioProcessor.apvts, "REVERB_SIZE", reverbSizeKnob);
    rvbMixAtt    = std::make_unique<Attachment> (audioProcessor.apvts, "REVERB_MIX",  reverbMixKnob);
    tapeAtt      = std::make_unique<Attachment> (audioProcessor.apvts, "TAPE_DRIVE",  tapeDriveKnob);
    filterAtt    = std::make_unique<Attachment> (audioProcessor.apvts, "FILTER_CUTOFF", filterCutoffKnob);
    masterAtt    = std::make_unique<Attachment> (audioProcessor.apvts, "MASTER_OUT", masterOutKnob);

    setSize (840, 560);
    startTimerHz (30);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor()
{
    stopTimer();
}

void EXAudioProcessorEditor::timerCallback()
{
    sourceKeySelector.setSelectedId (audioProcessor.sourceKeyIdx + 1, juce::dontSendNotification);
    sourceBpmSlider.setValue (audioProcessor.sourceBpm, juce::dontSendNotification);

    if (audioProcessor.isAuditionPlaying())
        repaint();
}

bool EXAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".mp3")) return true;
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
    // 1. Vintage Wooden Side Cheeks (Reference Image)
    g.fillAll (juce::Colour (0xff57331d)); // Dark walnut wood

    // 2. Off-White / Cream Console Body Plate
    auto body = getLocalBounds().reduced (14, 10).toFloat();
    g.setColour (juce::Colour (0xffeae7de));
    g.fillRoundedRectangle (body, 6.0f);

    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (body, 6.0f, 2.0f);

    // Decorative Screws in 4 Corners
    g.setColour (juce::Colour (0xff9e9a91));
    g.fillEllipse (20, 16, 7, 7);
    g.fillEllipse (getWidth() - 27, 16, 7, 7);
    g.fillEllipse (20, getHeight() - 23, 7, 7);
    g.fillEllipse (getWidth() - 27, getHeight() - 23, 7, 7);

    // Top Header Plate
    g.setColour (juce::Colour (0xffded9cd));
    g.drawHorizontalLine (56, body.getX(), body.getRight());

    // 3. Center Circular Oscilloscope / Radar (Exact Reference)
    float centerX = getWidth() * 0.5f;
    float centerY = 190.0f;
    float radius = 80.0f;

    // Outer Bezel
    g.setColour (juce::Colour (0xffb5b0a3));
    g.drawEllipse (centerX - radius - 4, centerY - radius - 4, (radius + 4) * 2, (radius + 4) * 2, 3.0f);

    // Dark Red Radar Glass
    g.setColour (juce::Colour (0xff400d0d));
    g.fillEllipse (centerX - radius, centerY - radius, radius * 2, radius * 2);

    // Radar Grid Lines
    g.setColour (juce::Colour (0x33b83232));
    for (int r = 20; r < 80; r += 20)
        g.drawEllipse (centerX - r, centerY - r, r * 2, r * 2, 1.0f);
    g.drawLine (centerX - radius, centerY, centerX + radius, centerY, 1.0f);
    g.drawLine (centerX, centerY - radius, centerX, centerY + radius, 1.0f);

    // Neon Red/Orange Oscilloscope Waveform inside radar
    auto& buf = audioProcessor.getAudioBuffer();
    if (buf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xffff3b30));
        int numSamples = buf.getNumSamples();
        for (int x = -60; x < 60; ++x)
        {
            int idx = (int) (((x + 60) / 120.0f) * numSamples);
            float val = buf.getSample (0, idx);
            g.drawLine (centerX + x, centerY - (val * 45.0f), centerX + x, centerY + (val * 45.0f), 1.2f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xffa65353));
        g.setFont (12.0f);
        g.drawText ("DROP SAMPLE", centerX - 50, centerY - 25, 100, 20, juce::Justification::centred);
    }

    // 4. Panel Insets for Left & Right FX Units
    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (30, 75, 230, 240);
    auto rightBox = juce::Rectangle<float> (getWidth() - 260, 75, 230, 240);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    // 5. Lower Sample & Scale Control Bar
    auto lowerBar = juce::Rectangle<float> (30, 330, getWidth() - 60, 205);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);
}

void EXAudioProcessorEditor::resized()
{
    brandTitle.setBounds (30, 14, 250, 22);
    subTitle.setBounds (30, 36, 250, 16);

    // Left FX Unit (Delay & Space)
    delayTimeKnob.setBounds (45, 95, 65, 75);
    delayTimeLabel.setBounds (45, 172, 65, 16);
    delayMixKnob.setBounds (125, 95, 65, 75);
    delayMixLabel.setBounds (125, 172, 65, 16);

    reverbSizeKnob.setBounds (45, 205, 65, 75);
    rvbSizeLabel.setBounds (45, 282, 65, 16);
    reverbMixKnob.setBounds (125, 205, 65, 75);
    rvbMixLabel.setBounds (125, 282, 65, 16);

    // Right FX Unit (Tape, Filter, Master)
    tapeDriveKnob.setBounds (getWidth() - 245, 95, 65, 75);
    tapeLabel.setBounds (getWidth() - 245, 172, 65, 16);
    filterCutoffKnob.setBounds (getWidth() - 165, 95, 65, 75);
    filterLabel.setBounds (getWidth() - 165, 172, 65, 16);

    masterOutKnob.setBounds (getWidth() - 205, 205, 65, 75);
    masterLabel.setBounds (getWidth() - 205, 282, 65, 16);

    // Center Controls (Below Radar)
    int cX = getWidth() / 2;
    diceButton.setBounds (cX - 65, 200, 130, 34);
    playPauseButton.setBounds (cX - 80, 285, 75, 28);
    dragMidiButton.setBounds (cX + 5, 285, 75, 28);

    // Lower Controls: Input Sample vs Target Project
    sourceSectionTitle.setBounds (50, 345, 150, 18);
    sourceKeySelector.setBounds (50, 370, 70, 24);
    sourceBpmSlider.setBounds (130, 370, 110, 24);

    targetSectionTitle.setBounds (getWidth() - 240, 345, 150, 18);
    targetKeySelector.setBounds (getWidth() - 240, 370, 70, 24);
    targetBpmSlider.setBounds (getWidth() - 160, 370, 110, 24);
}
