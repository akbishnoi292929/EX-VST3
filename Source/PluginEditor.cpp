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
    box.addItem ("PHRYG", 4);

    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffe8e6df));
    box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1f2124));
    box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffb8b4a8));
    addAndMakeVisible (box);
}

// True Isometric 3D Shaded Dice with No Outer Box
void EXAudioProcessorEditor::drawRealistic3DCubeDice (juce::Graphics& g, float cx, float cy, float size)
{
    juce::Graphics::ScopedSaveState state (g);

    float rollAngle = 0.0f;
    float wobble = 0.0f;
    if (audioProcessor.isDiceRolling.load())
    {
        rollAngle = audioProcessor.diceRollProgress * juce::MathConstants<float>::twoPi * 4.0f;
        wobble = std::sin (audioProcessor.diceRollProgress * juce::MathConstants<float>::pi * 6.0f) * 6.0f;
    }

    // Drop Shadow
    g.setColour (juce::Colour (0x55000000));
    g.fillEllipse (cx - size * 0.5f, cy + size * 0.35f, size, size * 0.35f);

    auto transform = juce::AffineTransform::rotation (rollAngle, cx, cy).translated (0.0f, wobble);
    g.addTransform (transform);

    // Isometric Cube Faces
    float half = size * 0.45f;

    // 1. Top Face (Bright specular gloss)
    juce::Path topFace;
    topFace.startNewSubPath (cx, cy - half * 1.1f);
    topFace.lineTo (cx + half * 0.9f, cy - half * 0.45f);
    topFace.lineTo (cx, cy + half * 0.15f);
    topFace.lineTo (cx - half * 0.9f, cy - half * 0.45f);
    topFace.closeSubPath();
    g.setColour (juce::Colour (0xffe63946)); // Vibrant red
    g.fillPath (topFace);

    // 2. Left Face (Midtone)
    juce::Path leftFace;
    leftFace.startNewSubPath (cx - half * 0.9f, cy - half * 0.45f);
    leftFace.lineTo (cx, cy + half * 0.15f);
    leftFace.lineTo (cx, cy + half * 1.05f);
    leftFace.lineTo (cx - half * 0.9f, cy + half * 0.45f);
    leftFace.closeSubPath();
    g.setColour (juce::Colour (0xffba1825)); // Deep red shadow
    g.fillPath (leftFace);

    // 3. Right Face (Ambient Occlusion)
    juce::Path rightFace;
    rightFace.startNewSubPath (cx + half * 0.9f, cy - half * 0.45f);
    rightFace.lineTo (cx, cy + half * 0.15f);
    rightFace.lineTo (cx, cy + half * 1.05f);
    rightFace.lineTo (cx + half * 0.9f, cy + half * 0.45f);
    rightFace.closeSubPath();
    g.setColour (juce::Colour (0xff8f0e18)); // Darkest red
    g.fillPath (rightFace);

    // Subtle 3D Edge Bevel Highlights
    g.setColour (juce::Colour (0x44ffffff));
    g.strokePath (topFace, juce::PathStrokeType (1.5f));
    g.strokePath (leftFace, juce::PathStrokeType (1.0f));
    g.strokePath (rightFace, juce::PathStrokeType (1.0f));

    // Recessed 3D White Dots on Front-Top
    g.setColour (juce::Colour (0xfff1faee));
    float dotR = size * 0.08f;
    int num = audioProcessor.activeDiceNumber;

    if (num % 2 == 1) // Center pip
        g.fillEllipse (cx - dotR * 0.5f, cy - half * 0.45f - dotR * 0.5f, dotR, dotR);

    if (num >= 2)
    {
        g.fillEllipse (cx - half * 0.45f, cy - half * 0.65f, dotR, dotR);
        g.fillEllipse (cx + half * 0.25f, cy - half * 0.25f, dotR, dotR);
    }
    if (num >= 4)
    {
        g.fillEllipse (cx + half * 0.25f, cy - half * 0.65f, dotR, dotR);
        g.fillEllipse (cx - half * 0.45f, cy - half * 0.25f, dotR, dotR);
    }
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    brandTitle.setText ("EX", juce::dontSendNotification);
    brandTitle.setFont (juce::Font (26.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER // MULTI-LAYER WORKSTATION", juce::dontSendNotification);
    subTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    subTitle.setColour (juce::Label::textColourId, juce::Colour (0xff63666f));
    addAndMakeVisible (subTitle);

    // Setup 3 Independent Layer Controllers
    const char* layerNames[] = { "LAYER 1", "LAYER 2", "LAYER 3" };
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        ui.titleLabel.setText (layerNames[i], juce::dontSendNotification);
        ui.titleLabel.setFont (juce::Font (11.0f, juce::Font::bold));
        ui.titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff252830));
        addAndMakeVisible (ui.titleLabel);

        setupKeySelector (ui.keySelector);
        setupModeSelector (ui.modeSelector);
        ui.keySelector.setSelectedId (audioProcessor.getTrack (i).sourceKey + 1, juce::dontSendNotification);
        ui.modeSelector.setSelectedId (audioProcessor.getTrack (i).sourceScaleMode + 1, juce::dontSendNotification);

        ui.keySelector.onChange = [this, i]() { audioProcessor.getTrack (i).sourceKey = layerUIs[i].keySelector.getSelectedId() - 1; };
        ui.modeSelector.onChange = [this, i]() { audioProcessor.getTrack (i).sourceScaleMode = layerUIs[i].modeSelector.getSelectedId() - 1; };

        ui.bpmSlider.setRange (60, 200, 1);
        ui.bpmSlider.setValue (audioProcessor.getTrack (i).sourceBpm, juce::dontSendNotification);
        ui.bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 18);
        ui.bpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
        ui.bpmSlider.onValueChange = [this, i]() { audioProcessor.getTrack (i).sourceBpm = (int) layerUIs[i].bpmSlider.getValue(); };
        addAndMakeVisible (ui.bpmSlider);

        ui.muteBtn.setButtonText ("M");
        ui.muteBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff32353e));
        ui.muteBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        ui.muteBtn.onClick = [this, i]() {
            audioProcessor.getTrack (i).isMute = !audioProcessor.getTrack (i).isMute;
            layerUIs[i].muteBtn.setColour (juce::TextButton::buttonColourId, audioProcessor.getTrack (i).isMute ? juce::Colour (0xffb82222) : juce::Colour (0xff32353e));
        };
        addAndMakeVisible (ui.muteBtn);

        ui.clearBtn.setButtonText ("X");
        ui.clearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2d34));
        ui.clearBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
        ui.clearBtn.onClick = [this, i]() { audioProcessor.clearLayer (i); repaint(); };
        addAndMakeVisible (ui.clearBtn);
    }

    // Master Project Target Controls
    masterTargetTitle.setText ("MASTER TARGET", juce::dontSendNotification);
    masterTargetTitle.setFont (juce::Font (11.0f, juce::Font::bold));
    masterTargetTitle.setColour (juce::Label::textColourId, juce::Colour (0xff1b633b));
    addAndMakeVisible (masterTargetTitle);

    setupKeySelector (masterKeySelector);
    setupModeSelector (masterModeSelector);
    masterKeySelector.setSelectedId (audioProcessor.masterTargetKey + 1, juce::dontSendNotification);
    masterModeSelector.setSelectedId (audioProcessor.masterTargetScaleMode + 1, juce::dontSendNotification);
    masterKeySelector.onChange = [this]() { audioProcessor.masterTargetKey = masterKeySelector.getSelectedId() - 1; };
    masterModeSelector.onChange = [this]() { audioProcessor.masterTargetScaleMode = masterModeSelector.getSelectedId() - 1; };

    masterBpmSlider.setRange (60, 200, 1);
    masterBpmSlider.setValue (audioProcessor.masterTargetBpm, juce::dontSendNotification);
    masterBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
    masterBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    masterBpmSlider.onValueChange = [this]() { audioProcessor.masterTargetBpm = (int) masterBpmSlider.getValue(); };
    addAndMakeVisible (masterBpmSlider);

    // Grid Slicing Buttons
    auto setupGridBtn = [this](juce::TextButton& b, const juce::String& text, GridDivision div) {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2e35));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.onClick = [this, div]() {
            for (int i = 0; i < 3; ++i) audioProcessor.applyGridSlicingToLayer (i, div);
            repaint();
        };
        addAndMakeVisible (b);
    };

    setupGridBtn (btnQuarter, "1/4", GridDivision::Quarter);
    setupGridBtn (btnEighth, "1/8", GridDivision::Eighth);
    setupGridBtn (btnSixteenth, "1/16", GridDivision::Sixteenth);
    setupGridBtn (btnTriplet1_3, "1/3T", GridDivision::Triplet1_3);
    setupGridBtn (btnTriplet1_6, "1/6T", GridDivision::Triplet1_6);
    setupGridBtn (btnTransient, "TRANS", GridDivision::Transient);

    // Center Controls (Transparent Hitbox for 3D Dice)
    diceHitboxButton.setButtonText ("");
    diceHitboxButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    diceHitboxButton.onClick = [this]() { audioProcessor.rollDiceAndGenerateMasterPattern(); };
    addAndMakeVisible (diceHitboxButton);

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

    setSize (860, 600);
    startTimerHz (40);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EXAudioProcessorEditor::timerCallback()
{
    for (int i = 0; i < 3; ++i)
    {
        layerUIs[i].keySelector.setSelectedId (audioProcessor.getTrack (i).sourceKey + 1, juce::dontSendNotification);
        layerUIs[i].bpmSlider.setValue (audioProcessor.getTrack (i).sourceBpm, juce::dontSendNotification);
    }

    radarAngle += 0.05f;
    if (radarAngle > juce::MathConstants<float>::twoPi)
        radarAngle -= juce::MathConstants<float>::twoPi;

    if (audioProcessor.isDiceRolling.load())
    {
        audioProcessor.diceRollProgress += 0.08f;
        if (audioProcessor.diceRollProgress >= 1.0f)
        {
            audioProcessor.diceRollProgress = 1.0f;
            audioProcessor.isDiceRolling.store (false);
        }
    }

    repaint();
}

bool EXAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".mp3")) return true;
    return false;
}

void EXAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int y)
{
    if (files.size() == 0) return;

    // Detect which layer track bed it was dropped on
    int targetLayer = 0;
    if (y > 410 && y <= 470) targetLayer = 1;
    else if (y > 470) targetLayer = 2;

    audioProcessor.loadAudioFileToLayer (targetLayer, juce::File (files[0]));
    repaint();
}

void EXAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    draggingLayerIdx = -1;
    draggingMarkerIdx = -1;

    for (int i = 0; i < 3; ++i)
    {
        auto& trk = audioProcessor.getTrack (i);
        auto& area = layerUIs[i].waveArea;
        if (trk.hasAudio && area.contains (e.position))
        {
            float clickX = e.position.x - area.getX();
            float samplePerPx = (float) trk.buffer.getNumSamples() / area.getWidth();

            for (size_t m = 0; m < trk.sliceMarkers.size(); ++m)
            {
                float markerPx = trk.sliceMarkers[m] / samplePerPx;
                if (std::abs (clickX - markerPx) < 7.0f)
                {
                    draggingLayerIdx = i;
                    draggingMarkerIdx = (int) m;
                    return;
                }
            }
        }
    }
}

void EXAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingLayerIdx >= 0 && draggingMarkerIdx >= 0)
    {
        auto& trk = audioProcessor.getTrack (draggingLayerIdx);
        auto& area = layerUIs[draggingLayerIdx].waveArea;
        if (trk.buffer.getNumSamples() > 0)
        {
            float curX = juce::jlimit (area.getX(), area.getRight(), e.position.x) - area.getX();
            float samplePerPx = (float) trk.buffer.getNumSamples() / area.getWidth();
            audioProcessor.setSlicePosition (draggingLayerIdx, draggingMarkerIdx, (int) (curX * samplePerPx));
            repaint();
        }
    }
}

void EXAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    draggingLayerIdx = -1;
    draggingMarkerIdx = -1;
}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Wood Cheeks
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

    // Center Circular Oscilloscope Radar
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

    // Render Beautiful Clean 3D Cube Dice
    drawRealistic3DCubeDice (g, cX, cY, 52.0f);

    // Inset Boxes
    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (30.0f, 70.0f, 230.0f, 245.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 260), 70.0f, 230.0f, 245.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    // Lower Plate: 3-Layer Multitrack Bed
    auto lowerBar = juce::Rectangle<float> (30.0f, 330.0f, static_cast<float> (getWidth() - 60), 225.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    // Paint 3 Layer Waveform Slots
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        auto& trk = audioProcessor.getTrack (i);
        g.setColour (juce::Colour (0xff1f2128));
        g.fillRoundedRectangle (ui.waveArea, 3.0f);

        if (trk.hasAudio && trk.buffer.getNumSamples() > 0)
        {
            // Distinct layer colors: Layer 1 = Red, Layer 2 = Amber, Layer 3 = Cyan
            juce::Colour waveCols[] = { juce::Colour (0xffe63946), juce::Colour (0xffffb703), juce::Colour (0xff06d6a0) };
            g.setColour (waveCols[i]);

            int total = trk.buffer.getNumSamples();
            float w = ui.waveArea.getWidth();
            float mid = ui.waveArea.getCentreY();

            for (int x = 0; x < (int) w; ++x)
            {
                int sIdx = static_cast<int> ((x / w) * total);
                sIdx = juce::jlimit (0, total - 1, sIdx);
                float v = trk.buffer.getSample (0, sIdx);
                g.drawLine (ui.waveArea.getX() + x, mid - (v * 20.0f), ui.waveArea.getX() + x, mid + (v * 20.0f), 1.0f);
            }

            // Draggable Slice Markers
            for (int slice : trk.sliceMarkers)
            {
                float cutX = ui.waveArea.getX() + ((slice / (float) total) * w);
                g.setColour (juce::Colours::white);
                g.drawVerticalLine ((int) cutX, ui.waveArea.getY(), ui.waveArea.getBottom());
                g.setColour (juce::Colour (0xffffcc00));
                g.fillEllipse (cutX - 2.5f, ui.waveArea.getY(), 5.0f, 5.0f);
            }
        }
        else
        {
            g.setColour (juce::Colour (0xff656978));
            g.setFont (11.0f);
            g.drawText ("DROP AUDIO HERE", ui.waveArea.toNearestInt(), juce::Justification::centred);
        }
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
    diceHitboxButton.setBounds (cX - 38, 164, 76, 76); // Over 3D Dice
    playPauseButton.setBounds (cX - 80, 285, 75, 28);
    dragMidiButton.setBounds (cX + 5, 285, 75, 28);

    // Master Project Target Controls (Top of Lower Plate)
    masterTargetTitle.setBounds (40, 338, 120, 18);
    masterKeySelector.setBounds (150, 336, 48, 22);
    masterModeSelector.setBounds (202, 336, 68, 22);
    masterBpmSlider.setBounds (274, 336, 85, 22);

    halfTimeToggle.setBounds (370, 336, 120, 22);

    // Grid buttons right aligned on header
    int gX = 505;
    int gY = 336;
    int gW = 36;
    btnQuarter.setBounds (gX, gY, gW, 22);
    btnEighth.setBounds (gX + 39, gY, gW, 22);
    btnSixteenth.setBounds (gX + 78, gY, gW, 22);
    btnTriplet1_3.setBounds (gX + 117, gY, gW + 4, 22);
    btnTriplet1_6.setBounds (gX + 160, gY, gW + 4, 22);
    btnTransient.setBounds (gX + 203, gY, 46, 22);

    // Layout 3 Layer Slots Vertically
    int startY = 368;
    int slotH = 55;
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        int y = startY + (i * slotH);

        ui.titleLabel.setBounds (40, y + 16, 55, 18);
        ui.keySelector.setBounds (100, y + 16, 45, 20);
        ui.modeSelector.setBounds (148, y + 16, 60, 20);
        ui.bpmSlider.setBounds (212, y + 16, 72, 20);
        ui.muteBtn.setBounds (288, y + 16, 22, 20);
        ui.clearBtn.setBounds (313, y + 16, 22, 20);

        ui.waveArea = juce::Rectangle<float> (345.0f, (float) (y + 6), (float) (getWidth() - 390), 40.0f);
    }

    // Drag Audio export button at bottom
    dragAudioButton.setBounds (getWidth() - 140, 562, 95, 22);
}
