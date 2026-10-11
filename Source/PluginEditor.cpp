#include "PluginProcessor.h"
#include "PluginEditor.h"

void VintageHardwareKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                      float sliderPos, float rotaryStartAngle,
                                                      float rotaryEndAngle, juce::Slider&)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
    auto centreX = (float) x + (float) width * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Ribbed Outer Base Ring (Bakelite Black)
    g.setColour (juce::Colour (0xff1f2126));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Inner Red Cap
    float capR = radius * 0.72f;
    g.setColour (juce::Colour (0xffb82222));
    g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    // Subtle 3D Bezel
    g.setColour (juce::Colour (0x44ffffff));
    g.drawEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.2f);

    // White Pointer
    juce::Path p;
    auto pointerLen = capR * 0.85f;
    p.addRectangle (-1.5f, -pointerLen, 3.0f, pointerLen);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colours::white);
    g.fillPath (p);
}

void EXAudioProcessorEditor::setupHardwareKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setLookAndFeel (&vintageKnobLAF);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 14);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (10.5f, juce::Font::bold));
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
    // Real Branding
    brandTitle.setText ("EX", juce::dontSendNotification);
    brandTitle.setFont (juce::Font (26.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e)); // Deep Crimson Red
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER // JO SABKA KAAT TI HAI, AB SAMPLE KAATEGI", juce::dontSendNotification);
    subTitle.setFont (juce::Font (10.5f, juce::Font::bold));
    subTitle.setColour (juce::Label::textColourId, juce::Colour (0xff63666f));
    addAndMakeVisible (subTitle);

    // Source Sample Section
    sourceTitle.setText ("SOURCE SAMPLE", juce::dontSendNotification);
    sourceTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    sourceTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e3333));
    addAndMakeVisible (sourceTitle);

    setupKeySelector (sourceKeySelector);
    sourceKeySelector.setSelectedId (audioProcessor.sourceKey + 1, juce::dontSendNotification);
    sourceKeySelector.onChange = [this]() {
        audioProcessor.sourceKey = sourceKeySelector.getSelectedId() - 1;
    };

    sourceBpmSlider.setRange (60, 200, 1);
    sourceBpmSlider.setValue (audioProcessor.sourceBpm, juce::dontSendNotification);
    sourceBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    sourceBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    sourceBpmSlider.onValueChange = [this]() {
        audioProcessor.sourceBpm = (int) sourceBpmSlider.getValue();
    };
    addAndMakeVisible (sourceBpmSlider);

    // Target Project Section
    targetTitle.setText ("TARGET PROJECT", juce::dontSendNotification);
    targetTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    targetTitle.setColour (juce::Label::textColourId, juce::Colour (0xff206b3e));
    addAndMakeVisible (targetTitle);

    setupKeySelector (targetKeySelector);
    targetKeySelector.setSelectedId (audioProcessor.targetKey + 1, juce::dontSendNotification);
    targetKeySelector.onChange = [this]() {
        audioProcessor.targetKey = targetKeySelector.getSelectedId() - 1;
    };

    targetBpmSlider.setRange (60, 200, 1);
    targetBpmSlider.setValue (audioProcessor.targetBpm, juce::dontSendNotification);
    targetBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    targetBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    targetBpmSlider.onValueChange = [this]() {
        audioProcessor.targetBpm = (int) targetBpmSlider.getValue();
    };
    addAndMakeVisible (targetBpmSlider);

    // Center Controls
    diceButton.setButtonText (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb2 FLIP DICE"));
    diceButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff9e1e1e));
    diceButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    diceButton.onClick = [this]() {
        audioProcessor.rollDiceAndFlipPattern();
        repaint();
    };
    addAndMakeVisible (diceButton);

    playPauseButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    playPauseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3036));
    playPauseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    playPauseButton.onClick = [this]() {
        audioProcessor.togglePlayback();
        playPauseButton.setButtonText (audioProcessor.isPlayingActive()
            ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE")
            : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    };
    addAndMakeVisible (playPauseButton);

    dragMidiButton.setButtonText ("DRAG MIDI");
    dragMidiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff234a6e));
    dragMidiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    dragMidiButton.onClick = [this]() {
        auto mf = audioProcessor.generateMidiFile();
        if (mf.existsAsFile())
        {
            juce::StringArray files;
            files.add (mf.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false);
        }
    };
    addAndMakeVisible (dragMidiButton);

    halfTimeToggle.setButtonText ("HALFTIME (1/2X)");
    halfTimeToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff333333));
    halfTimeToggle.onClick = [this]() {
        audioProcessor.halfTimeMode = halfTimeToggle.getToggleState();
    };
    addAndMakeVisible (halfTimeToggle);

    // Hardware Knobs
    setupHardwareKnob (delayTimeKnob, delayTimeLabel, "TIME");
    setupHardwareKnob (delayMixKnob, delayMixLabel, "DLY MIX");
    setupHardwareKnob (reverbSizeKnob, rvbSizeLabel, "SPACE");
    setupHardwareKnob (reverbMixKnob, rvbMixLabel, "RVB MIX");
    setupHardwareKnob (tapeDriveKnob, tapeLabel, "TAPE DRIVE");
    setupHardwareKnob (filterCutoffKnob, filterLabel, "FILTER");
    setupHardwareKnob (masterOutKnob, masterLabel, "MASTER");

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
    setLookAndFeel (nullptr);
}

void EXAudioProcessorEditor::timerCallback()
{
    sourceKeySelector.setSelectedId (audioProcessor.sourceKey + 1, juce::dontSendNotification);
    sourceBpmSlider.setValue (audioProcessor.sourceBpm, juce::dontSendNotification);

    radarAngle += 0.05f;
    if (radarAngle > juce::MathConstants<float>::twoPi)
        radarAngle -= juce::MathConstants<float>::twoPi;

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
    // Wood cheeks
    g.fillAll (juce::Colour (0xff4a2c18));

    // Console Body
    auto body = getLocalBounds().reduced (14, 10).toFloat();
    g.setColour (juce::Colour (0xffece9e1));
    g.fillRoundedRectangle (body, 6.0f);

    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (body, 6.0f, 2.0f);

    // Decorative Screws
    g.setColour (juce::Colour (0xffa8a49c));
    g.fillEllipse (20.0f, 16.0f, 7.0f, 7.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 27), 16.0f, 7.0f, 7.0f);
    g.fillEllipse (20.0f, static_cast<float> (getHeight() - 23), 7.0f, 7.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 27), static_cast<float> (getHeight() - 23), 7.0f, 7.0f);

    // Circular Oscilloscope Radar
    float cX = getWidth() * 0.5f;
    float cY = 190.0f;
    float r = 78.0f;

    // Outer Silver Bezel
    g.setColour (juce::Colour (0xffb8b4a8));
    g.drawEllipse (cX - r - 4.0f, cY - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f, 3.5f);

    // Dark Maroon Scope Glass
    g.setColour (juce::Colour (0xff3b0a0a));
    g.fillEllipse (cX - r, cY - r, r * 2.0f, r * 2.0f);

    // Radar Grids
    g.setColour (juce::Colour (0x33cf3838));
    for (float gR = 20.0f; gR < 80.0f; gR += 20.0f)
        g.drawEllipse (cX - gR, cY - gR, gR * 2.0f, gR * 2.0f, 1.0f);
    g.drawLine (cX - r, cY, cX + r, cY, 1.0f);
    g.drawLine (cX, cY - r, cX, cY + r, 1.0f);

    // Rotating Radar Scan Line
    float scanX = cX + std::cos (radarAngle) * r;
    float scanY = cY + std::sin (radarAngle) * r;
    g.setColour (juce::Colour (0x66ff3b30));
    g.drawLine (cX, cY, scanX, scanY, 1.5f);

    // Pulsing Waveform Display
    auto& buf = audioProcessor.getAudioBuffer();
    if (buf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xffff3b30));
        int numSamples = buf.getNumSamples();
        for (int x = -55; x < 55; ++x)
        {
            int idx = static_cast<int> (((x + 55) / 110.0f) * static_cast<float> (numSamples));
            idx = juce::jlimit (0, numSamples - 1, idx);
            float val = buf.getSample (0, idx);
            g.drawLine (cX + static_cast<float> (x), cY - (val * 40.0f),
                        cX + static_cast<float> (x), cY + (val * 40.0f), 1.4f);
        }
    }

    // Inset Boxes
    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (30.0f, 70.0f, 230.0f, 245.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 260), 70.0f, 230.0f, 245.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    // Lower Plate: Slices Display Bed (Kaatne Ka Maidan)
    auto lowerBar = juce::Rectangle<float> (30.0f, 335.0f, static_cast<float> (getWidth() - 60), 200.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    // Waveform Strip showing chopped pieces
    auto stripArea = juce::Rectangle<float> (40.0f, 415.0f, static_cast<float> (getWidth() - 80), 105.0f);
    g.setColour (juce::Colour (0xff212328)); // Dark tape bed
    g.fillRoundedRectangle (stripArea, 4.0f);

    if (buf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xffe63946)); // Red wave
        int total = buf.getNumSamples();
        float w = stripArea.getWidth();
        float mid = stripArea.getCentreY();

        for (int x = 0; x < (int) w; ++x)
        {
            int sIdx = static_cast<int> ((x / w) * total);
            sIdx = juce::jlimit (0, total - 1, sIdx);
            float v = buf.getSample (0, sIdx);
            g.drawLine (stripArea.getX() + x, mid - (v * 40.0f), stripArea.getX() + x, mid + (v * 40.0f), 1.0f);
        }

        // Chop cut marks (Kahan kahan se kata hai)
        g.setColour (juce::Colours::white);
        for (int slice : audioProcessor.getSlicePoints())
        {
            float cutX = stripArea.getX() + ((slice / (float) total) * w);
            g.drawVerticalLine ((int) cutX, stripArea.getY(), stripArea.getBottom());
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff707584));
        g.setFont (12.0f);
        g.drawText ("DROP SAMPLE TO SLICE // YAHAN TUKDE KATEGA", stripArea.toNearestInt(), juce::Justification::centred);
    }
}

void EXAudioProcessorEditor::resized()
{
    brandTitle.setBounds (30, 10, 60, 32);
    subTitle.setBounds (95, 18, 550, 20);

    // Left FX Unit
    delayTimeKnob.setBounds (45, 88, 68, 78);
    delayTimeLabel.setBounds (45, 168, 68, 16);
    delayMixKnob.setBounds (125, 88, 68, 78);
    delayMixLabel.setBounds (125, 168, 68, 16);

    reverbSizeKnob.setBounds (45, 200, 68, 78);
    rvbSizeLabel.setBounds (45, 280, 68, 16);
    reverbMixKnob.setBounds (125, 200, 68, 78);
    rvbMixLabel.setBounds (125, 280, 68, 16);

    // Right FX Unit
    tapeDriveKnob.setBounds (getWidth() - 245, 88, 68, 78);
    tapeLabel.setBounds (getWidth() - 245, 168, 68, 16);
    filterCutoffKnob.setBounds (getWidth() - 165, 88, 68, 78);
    filterLabel.setBounds (getWidth() - 165, 168, 68, 16);

    masterOutKnob.setBounds (getWidth() - 205, 200, 68, 78);
    masterLabel.setBounds (getWidth() - 205, 280, 68, 16);

    // Center Controls
    int cX = getWidth() / 2;
    diceButton.setBounds (cX - 65, 195, 130, 34);
    playPauseButton.setBounds (cX - 80, 285, 75, 28);
    dragMidiButton.setBounds (cX + 5, 285, 75, 28);

    // Lower Controls (Input vs Target)
    sourceTitle.setBounds (50, 345, 150, 18);
    sourceKeySelector.setBounds (50, 368, 70, 24);
    sourceBpmSlider.setBounds (130, 368, 110, 24);

    targetTitle.setBounds (getWidth() - 240, 345, 150, 18);
    targetKeySelector.setBounds (getWidth() - 240, 368, 70, 24);
    targetBpmSlider.setBounds (getWidth() - 160, 368, 110, 24);

    halfTimeToggle.setBounds (cX - 65, 368, 140, 24);
}
