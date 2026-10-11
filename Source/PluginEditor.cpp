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

    g.setColour (juce::Colour (0xff1f2126));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    float capR = radius * 0.72f;
    g.setColour (juce::Colour (0xffb82222));
    g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (juce::Colour (0x44ffffff));
    g.drawEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.2f);

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

void EXAudioProcessorEditor::setupModeSelector (juce::ComboBox& box)
{
    box.addItem ("MINOR", 1);
    box.addItem ("MAJOR", 2);
    box.addItem ("DORIAN", 3);
    box.addItem ("PHRYGIAN", 4);

    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffe8e6df));
    box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1f2124));
    box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffb8b4a8));
    addAndMakeVisible (box);
}

void EXAudioProcessorEditor::draw3DDice (juce::Graphics& g, float cx, float cy, float size)
{
    auto r = juce::Rectangle<float> (cx - size * 0.5f, cy - size * 0.5f, size, size);
    g.setColour (juce::Colour (0xffd92b2b));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colour (0xff8f1616));
    g.drawRoundedRectangle (r, 6.0f, 2.0f);

    // 5-dot Dice layout
    g.setColour (juce::Colours::white);
    float dot = size * 0.16f;
    g.fillEllipse (cx - dot * 0.5f, cy - dot * 0.5f, dot, dot); // center
    g.fillEllipse (cx - size * 0.28f, cy - size * 0.28f, dot, dot); // top left
    g.fillEllipse (cx + size * 0.28f - dot, cy - size * 0.28f, dot, dot); // top right
    g.fillEllipse (cx - size * 0.28f, cy + size * 0.28f - dot, dot, dot); // bottom left
    g.fillEllipse (cx + size * 0.28f - dot, cy + size * 0.28f - dot, dot, dot); // bottom right
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    brandTitle.setText ("EX", juce::dontSendNotification);
    brandTitle.setFont (juce::Font (26.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER", juce::dontSendNotification);
    subTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    subTitle.setColour (juce::Label::textColourId, juce::Colour (0xff63666f));
    addAndMakeVisible (subTitle);

    // Source Controls
    sourceTitle.setText ("SOURCE SAMPLE", juce::dontSendNotification);
    sourceTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    sourceTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e3333));
    addAndMakeVisible (sourceTitle);

    setupKeySelector (sourceKeySelector);
    setupModeSelector (sourceModeSelector);
    sourceKeySelector.setSelectedId (audioProcessor.sourceKey + 1, juce::dontSendNotification);
    sourceModeSelector.setSelectedId (audioProcessor.sourceScaleMode + 1, juce::dontSendNotification);
    sourceKeySelector.onChange = [this]() { audioProcessor.sourceKey = sourceKeySelector.getSelectedId() - 1; };
    sourceModeSelector.onChange = [this]() { audioProcessor.sourceScaleMode = sourceModeSelector.getSelectedId() - 1; };

    sourceBpmSlider.setRange (60, 200, 1);
    sourceBpmSlider.setValue (audioProcessor.sourceBpm, juce::dontSendNotification);
    sourceBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
    sourceBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    sourceBpmSlider.onValueChange = [this]() { audioProcessor.sourceBpm = (int) sourceBpmSlider.getValue(); };
    addAndMakeVisible (sourceBpmSlider);

    // Target Controls
    targetTitle.setText ("TARGET PROJECT", juce::dontSendNotification);
    targetTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    targetTitle.setColour (juce::Label::textColourId, juce::Colour (0xff206b3e));
    addAndMakeVisible (targetTitle);

    setupKeySelector (targetKeySelector);
    setupModeSelector (targetModeSelector);
    targetKeySelector.setSelectedId (audioProcessor.targetKey + 1, juce::dontSendNotification);
    targetModeSelector.setSelectedId (audioProcessor.targetScaleMode + 1, juce::dontSendNotification);
    targetKeySelector.onChange = [this]() { audioProcessor.targetKey = targetKeySelector.getSelectedId() - 1; };
    targetModeSelector.onChange = [this]() { audioProcessor.targetScaleMode = targetModeSelector.getSelectedId() - 1; };

    targetBpmSlider.setRange (60, 200, 1);
    targetBpmSlider.setValue (audioProcessor.targetBpm, juce::dontSendNotification);
    targetBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
    targetBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    targetBpmSlider.onValueChange = [this]() { audioProcessor.targetBpm = (int) targetBpmSlider.getValue(); };
    addAndMakeVisible (targetBpmSlider);

    // Grid Slicing Buttons
    auto setupGridBtn = [this](juce::TextButton& b, const juce::String& text, GridDivision div) {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2e35));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.onClick = [this, div]() { audioProcessor.applyGridSlicing (div); repaint(); };
        addAndMakeVisible (b);
    };

    setupGridBtn (btnQuarter, "1/4", GridDivision::Quarter);
    setupGridBtn (btnEighth, "1/8", GridDivision::Eighth);
    setupGridBtn (btnSixteenth, "1/16", GridDivision::Sixteenth);
    setupGridBtn (btnTriplet1_3, "1/3T", GridDivision::Triplet1_3);
    setupGridBtn (btnTriplet1_6, "1/6T", GridDivision::Triplet1_6);
    setupGridBtn (btnTransient, "TRANS", GridDivision::Transient);

    // Center Controls
    diceButton.setButtonText ("");
    diceButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    diceButton.onClick = [this]() { audioProcessor.rollDiceAndFlipPattern(); repaint(); };
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
        auto mf = audioProcessor.exportCurrentAsMidi();
        if (mf.existsAsFile())
        {
            juce::StringArray files;
            files.add (mf.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false);
        }
    };
    addAndMakeVisible (dragMidiButton);

    dragAudioButton.setButtonText ("DRAG AUDIO");
    dragAudioButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b633b));
    dragAudioButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    dragAudioButton.onClick = [this]() {
        auto wf = audioProcessor.exportCurrentAsAudioWav();
        if (wf.existsAsFile())
        {
            juce::StringArray files;
            files.add (wf.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false);
        }
    };
    addAndMakeVisible (dragAudioButton);

    halfTimeToggle.setButtonText ("HALFTIME (1/2X)");
    halfTimeToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff333333));
    halfTimeToggle.onClick = [this]() { audioProcessor.halfTimeMode = halfTimeToggle.getToggleState(); };
    addAndMakeVisible (halfTimeToggle);

    // Knobs
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

void EXAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    auto& buf = audioProcessor.getAudioBuffer();
    if (buf.getNumSamples() == 0 || !waveformArea.contains (e.position)) return;

    float clickX = e.position.x - waveformArea.getX();
    float samplePerPx = (float) buf.getNumSamples() / waveformArea.getWidth();

    draggingMarkerIdx = -1;
    for (size_t i = 0; i < audioProcessor.getSlicePoints().size(); ++i)
    {
        float markerPx = audioProcessor.getSlicePoints()[i] / samplePerPx;
        if (std::abs (clickX - markerPx) < 7.0f)
        {
            draggingMarkerIdx = (int) i;
            break;
        }
    }
}

void EXAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto& buf = audioProcessor.getAudioBuffer();
    if (draggingMarkerIdx >= 0 && buf.getNumSamples() > 0)
    {
        float curX = juce::jlimit (waveformArea.getX(), waveformArea.getRight(), e.position.x) - waveformArea.getX();
        float samplePerPx = (float) buf.getNumSamples() / waveformArea.getWidth();
        audioProcessor.setSlicePosition (draggingMarkerIdx, (int) (curX * samplePerPx));
        repaint();
    }
}

void EXAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    draggingMarkerIdx = -1;
}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff4a2c18));

    auto body = getLocalBounds().reduced (14, 10).toFloat();
    g.setColour (juce::Colour (0xffece9e1));
    g.fillRoundedRectangle (body, 6.0f);

    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (body, 6.0f, 2.0f);

    g.setColour (juce::Colour (0xffa8a49c));
    g.fillEllipse (20.0f, 16.0f, 7.0f, 7.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 27), 16.0f, 7.0f, 7.0f);
    g.fillEllipse (20.0f, static_cast<float> (getHeight() - 23), 7.0f, 7.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 27), static_cast<float> (getHeight() - 23), 7.0f, 7.0f);

    // Center Circular Radar
    float cX = getWidth() * 0.5f;
    float cY = 190.0f;
    float r = 78.0f;

    g.setColour (juce::Colour (0xffb8b4a8));
    g.drawEllipse (cX - r - 4.0f, cY - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f, 3.5f);

    g.setColour (juce::Colour (0xff3b0a0a));
    g.fillEllipse (cX - r, cY - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colour (0x33cf3838));
    for (float gR = 20.0f; gR < 80.0f; gR += 20.0f)
        g.drawEllipse (cX - gR, cY - gR, gR * 2.0f, gR * 2.0f, 1.0f);
    g.drawLine (cX - r, cY, cX + r, cY, 1.0f);
    g.drawLine (cX, cY - r, cX, cY + r, 1.0f);

    float scanX = cX + std::cos (radarAngle) * r;
    float scanY = cY + std::sin (radarAngle) * r;
    g.setColour (juce::Colour (0x66ff3b30));
    g.drawLine (cX, cY, scanX, scanY, 1.5f);

    // Draw Real 3D Dice inside Radar
    draw3DDice (g, cX, cY, 44.0f);

    // Inset Boxes
    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (30.0f, 70.0f, 230.0f, 245.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 260), 70.0f, 230.0f, 245.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    // Lower Plate: Slices Display Bed
    auto lowerBar = juce::Rectangle<float> (30.0f, 335.0f, static_cast<float> (getWidth() - 60), 200.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    // Waveform Strip
    waveformArea = juce::Rectangle<float> (40.0f, 415.0f, static_cast<float> (getWidth() - 80), 105.0f);
    g.setColour (juce::Colour (0xff212328));
    g.fillRoundedRectangle (waveformArea, 4.0f);

    auto& buf = audioProcessor.getAudioBuffer();
    if (buf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xffe63946));
        int total = buf.getNumSamples();
        float w = waveformArea.getWidth();
        float mid = waveformArea.getCentreY();

        for (int x = 0; x < (int) w; ++x)
        {
            int sIdx = static_cast<int> ((x / w) * total);
            sIdx = juce::jlimit (0, total - 1, sIdx);
            float v = buf.getSample (0, sIdx);
            g.drawLine (waveformArea.getX() + x, mid - (v * 40.0f), waveformArea.getX() + x, mid + (v * 40.0f), 1.0f);
        }

        // Draggable Slice Divider Lines with Handles
        for (int slice : audioProcessor.getSlicePoints())
        {
            float cutX = waveformArea.getX() + ((slice / (float) total) * w);
            g.setColour (juce::Colours::white);
            g.drawVerticalLine ((int) cutX, waveformArea.getY(), waveformArea.getBottom());
            g.setColour (juce::Colour (0xffffcc00));
            g.fillEllipse (cutX - 3.0f, waveformArea.getY(), 6.0f, 6.0f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff707584));
        g.setFont (12.0f);
        g.drawText ("DROP SAMPLE", waveformArea.toNearestInt(), juce::Justification::centred);
    }
}

void EXAudioProcessorEditor::resized()
{
    brandTitle.setBounds (30, 10, 60, 32);
    subTitle.setBounds (95, 18, 400, 20);

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
    diceButton.setBounds (cX - 35, 168, 70, 70); // Over the 3D dice
    playPauseButton.setBounds (cX - 80, 285, 75, 28);
    dragMidiButton.setBounds (cX + 5, 285, 75, 28);

    // Lower Controls: Input vs Target
    sourceTitle.setBounds (45, 345, 120, 18);
    sourceKeySelector.setBounds (45, 368, 50, 24);
    sourceModeSelector.setBounds (98, 368, 68, 24);
    sourceBpmSlider.setBounds (170, 368, 85, 24);

    targetTitle.setBounds (getWidth() - 245, 345, 120, 18);
    targetKeySelector.setBounds (getWidth() - 245, 368, 50, 24);
    targetModeSelector.setBounds (getWidth() - 192, 368, 68, 24);
    targetBpmSlider.setBounds (getWidth() - 120, 368, 85, 24);

    // Grid Slicing Selectors
    int gX = 265;
    int gY = 368;
    int gW = 38;
    btnQuarter.setBounds (gX, gY, gW, 24);
    btnEighth.setBounds (gX + 42, gY, gW, 24);
    btnSixteenth.setBounds (gX + 84, gY, gW, 24);
    btnTriplet1_3.setBounds (gX + 126, gY, gW + 4, 24);
    btnTriplet1_6.setBounds (gX + 172, gY, gW + 4, 24);
    btnTransient.setBounds (gX + 218, gY, 48, 24);

    halfTimeToggle.setBounds (cX - 70, 345, 140, 20);

    // Audio Export Button at bottom right
    dragAudioButton.setBounds (getWidth() - 145, 524, 95, 22);
}
