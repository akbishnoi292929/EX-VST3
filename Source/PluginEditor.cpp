#include "PluginProcessor.h"
#include "PluginEditor.h"

LuxuryHardwareKnobLookAndFeel::LuxuryHardwareKnobLookAndFeel() {}

void LuxuryHardwareKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                      float sliderPos, float rotaryStartAngle,
                                                      float rotaryEndAngle, juce::Slider&)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 3.0f;
    auto centreX = (float) x + (float) width * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour (0xff141518));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    float trackR = radius - 2.5f;
    juce::Path trackPath;
    trackPath.addCentredArc (centreX, centreY, trackR, trackR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2b2d35));
    g.strokePath (trackPath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path activePath;
    activePath.addCentredArc (centreX, centreY, trackR, trackR, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xffe63946));
    g.strokePath (activePath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float capR = radius * 0.72f;
    juce::ColourGradient grad (juce::Colour (0xff2d3038), centreX, centreY - capR,
                               juce::Colour (0xff1a1b20), centreX, centreY + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (juce::Colour (0x33ffffff));
    g.drawEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.2f);

    juce::Path p;
    auto pointerLen = capR * 0.75f;
    p.addRoundedRectangle (-1.2f, -pointerLen, 2.4f, pointerLen * 0.75f, 1.0f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colours::white);
    g.fillPath (p);
}

void EXAudioProcessorEditor::setupHardwareKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setLookAndFeel (&luxuryKnobLAF);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 14);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff22252a));
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (10.0f, juce::Font::bold));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff555862));
    addAndMakeVisible (l);
}

void EXAudioProcessorEditor::setupKeySelector (juce::ComboBox& box)
{
    const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        box.addItem (keys[i], i + 1);

    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffdedad0));
    box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1a1b1f));
    box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffb3ada0));
    box.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff33353c));
    addAndMakeVisible (box);
}

void EXAudioProcessorEditor::setupModeSelector (juce::ComboBox& box)
{
    box.addItem ("MINOR", 1);
    box.addItem ("MAJOR", 2);
    box.addItem ("DORIAN", 3);
    box.addItem ("PHRYG", 4);

    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffdedad0));
    box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1a1b1f));
    box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffb3ada0));
    box.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff33353c));
    addAndMakeVisible (box);
}

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

    g.setColour (juce::Colour (0x55000000));
    g.fillEllipse (cx - size * 0.5f, cy + size * 0.35f, size, size * 0.35f);

    auto transform = juce::AffineTransform::rotation (rollAngle, cx, cy).translated (0.0f, wobble);
    g.addTransform (transform);

    float half = size * 0.45f;

    juce::Path topFace;
    topFace.startNewSubPath (cx, cy - half * 1.1f);
    topFace.lineTo (cx + half * 0.9f, cy - half * 0.45f);
    topFace.lineTo (cx, cy + half * 0.15f);
    topFace.lineTo (cx - half * 0.9f, cy - half * 0.45f);
    topFace.closeSubPath();
    g.setColour (juce::Colour (0xffe63946));
    g.fillPath (topFace);

    juce::Path leftFace;
    leftFace.startNewSubPath (cx - half * 0.9f, cy - half * 0.45f);
    leftFace.lineTo (cx, cy + half * 0.15f);
    leftFace.lineTo (cx, cy + half * 1.05f);
    leftFace.lineTo (cx - half * 0.9f, cy + half * 0.45f);
    leftFace.closeSubPath();
    g.setColour (juce::Colour (0xffba1825));
    g.fillPath (leftFace);

    juce::Path rightFace;
    rightFace.startNewSubPath (cx + half * 0.9f, cy - half * 0.45f);
    rightFace.lineTo (cx, cy + half * 0.15f);
    rightFace.lineTo (cx, cy + half * 1.05f);
    rightFace.lineTo (cx + half * 0.9f, cy + half * 0.45f);
    rightFace.closeSubPath();
    g.setColour (juce::Colour (0xff8f0e18));
    g.fillPath (rightFace);

    g.setColour (juce::Colour (0x44ffffff));
    g.strokePath (topFace, juce::PathStrokeType (1.5f));
    g.strokePath (leftFace, juce::PathStrokeType (1.0f));
    g.strokePath (rightFace, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colour (0xfff1faee));
    float dotR = size * 0.08f;
    int num = audioProcessor.activeDiceNumber;

    if (num % 2 == 1)
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

bool EXAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::spaceKey))
    {
        audioProcessor.togglePlayback();
        bool isPlay = audioProcessor.isPlayingActive();
        playPauseButton.setButtonText (isPlay ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE") : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
        playPauseButton.setColour (juce::TextButton::buttonColourId, isPlay ? juce::Colour (0xff9e1e1e) : juce::Colour (0xff1b633b));
        return true;
    }
    return false;
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setWantsKeyboardFocus (true);

    brandTitle.setText ("EX", juce::dontSendNotification);
    brandTitle.setFont (juce::Font (30.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER // PRO 4-LAYER WORKSTATION", juce::dontSendNotification);
    subTitle.setFont (juce::Font (11.5f, juce::Font::bold));
    subTitle.setColour (juce::Label::textColourId, juce::Colour (0xff63666f));
    addAndMakeVisible (subTitle);

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

        ui.keySelector.onChange = [this, i]() {
            audioProcessor.getTrack (i).sourceKey = layerUIs[i].keySelector.getSelectedId() - 1;
            audioProcessor.renderMasterPreviewBuffer();
        };
        ui.modeSelector.onChange = [this, i]() {
            audioProcessor.getTrack (i).sourceScaleMode = layerUIs[i].modeSelector.getSelectedId() - 1;
            audioProcessor.renderMasterPreviewBuffer();
        };

        ui.bpmSlider.setRange (60, 200, 1);
        ui.bpmSlider.setValue (audioProcessor.getTrack (i).sourceBpm, juce::dontSendNotification);
        ui.bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
        ui.bpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
        ui.bpmSlider.onValueChange = [this, i]() {
            audioProcessor.getTrack (i).sourceBpm = (int) layerUIs[i].bpmSlider.getValue();
            audioProcessor.renderMasterPreviewBuffer();
        };
        addAndMakeVisible (ui.bpmSlider);

        ui.volSlider.setLookAndFeel (&luxuryKnobLAF);
        ui.volSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        ui.volSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        ui.volSlider.onValueChange = [this]() { audioProcessor.renderMasterPreviewBuffer(); };
        addAndMakeVisible (ui.volSlider);

        ui.volLabel.setText ("VOL", juce::dontSendNotification);
        ui.volLabel.setFont (juce::Font (9.0f, juce::Font::bold));
        ui.volLabel.setJustificationType (juce::Justification::centred);
        ui.volLabel.setColour (juce::Label::textColourId, juce::Colour (0xff656872));
        addAndMakeVisible (ui.volLabel);

        ui.muteBtn.setButtonText ("M");
        ui.muteBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff32353e));
        ui.muteBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        ui.muteBtn.onClick = [this, i]() {
            audioProcessor.getTrack (i).isMute = !audioProcessor.getTrack (i).isMute;
            layerUIs[i].muteBtn.setColour (juce::TextButton::buttonColourId, audioProcessor.getTrack (i).isMute ? juce::Colour (0xffb82222) : juce::Colour (0xff32353e));
            audioProcessor.renderMasterPreviewBuffer();
        };
        addAndMakeVisible (ui.muteBtn);

        ui.clearBtn.setButtonText ("X");
        ui.clearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2d34));
        ui.clearBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
        ui.clearBtn.onClick = [this, i]() { audioProcessor.clearLayer (i); repaint(); };
        addAndMakeVisible (ui.clearBtn);
    }

    vol1Att = std::make_unique<Attachment> (audioProcessor.apvts, "VOL_LAYER_1", layerUIs[0].volSlider);
    vol2Att = std::make_unique<Attachment> (audioProcessor.apvts, "VOL_LAYER_2", layerUIs[1].volSlider);
    vol3Att = std::make_unique<Attachment> (audioProcessor.apvts, "VOL_LAYER_3", layerUIs[2].volSlider);

    previewTrackTitle.setText ("MASTER FLIP", juce::dontSendNotification);
    previewTrackTitle.setFont (juce::Font (11.5f, juce::Font::bold));
    previewTrackTitle.setColour (juce::Label::textColourId, juce::Colour (0xff7b2cbf));
    addAndMakeVisible (previewTrackTitle);

    directExportWavBtn.setButtonText ("DRAG AUDIO");
    directExportWavBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7b2cbf));
    directExportWavBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    directExportWavBtn.onClick = [this]() {
        auto wf = audioProcessor.exportCurrentAsAudioWav();
        if (wf.existsAsFile())
        {
            juce::StringArray files;
            files.add (wf.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false);
        }
    };
    addAndMakeVisible (directExportWavBtn);

    exportStemsBtn.setButtonText ("DRAG STEMS");
    exportStemsBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a6f97));
    exportStemsBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    exportStemsBtn.onClick = [this]() {
        auto stemFiles = audioProcessor.exportCurrentAsStems();
        if (stemFiles.size() > 0)
        {
            juce::DragAndDropContainer::performExternalDragDropOfFiles (stemFiles, false);
        }
    };
    addAndMakeVisible (exportStemsBtn);

    masterTargetTitle.setText ("MASTER TARGET", juce::dontSendNotification);
    masterTargetTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    masterTargetTitle.setColour (juce::Label::textColourId, juce::Colour (0xff1b633b));
    addAndMakeVisible (masterTargetTitle);

    setupKeySelector (masterKeySelector);
    setupModeSelector (masterModeSelector);
    masterKeySelector.setSelectedId (audioProcessor.masterTargetKey + 1, juce::dontSendNotification);
    masterModeSelector.setSelectedId (audioProcessor.masterTargetScaleMode + 1, juce::dontSendNotification);
    masterKeySelector.onChange = [this]() {
        audioProcessor.masterTargetKey = masterKeySelector.getSelectedId() - 1;
        audioProcessor.renderMasterPreviewBuffer();
    };
    masterModeSelector.onChange = [this]() {
        audioProcessor.masterTargetScaleMode = masterModeSelector.getSelectedId() - 1;
        audioProcessor.renderMasterPreviewBuffer();
    };

    masterBpmSlider.setRange (60, 200, 1);
    masterBpmSlider.setValue (audioProcessor.masterTargetBpm, juce::dontSendNotification);
    masterBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
    masterBpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
    masterBpmSlider.onValueChange = [this]() {
        audioProcessor.masterTargetBpm = (int) masterBpmSlider.getValue();
        audioProcessor.renderMasterPreviewBuffer();
    };
    addAndMakeVisible (masterBpmSlider);

    auto setupGridBtn = [this](juce::TextButton& b, const juce::String& text, GridDivision div) {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2e35));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.onClick = [this, div]() {
            for (int i = 0; i < 3; ++i) audioProcessor.applyGridSlicingToLayer (i, div);
            audioProcessor.renderMasterPreviewBuffer();
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

    playPauseButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    playPauseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b633b));
    playPauseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    playPauseButton.onClick = [this]() {
        audioProcessor.togglePlayback();
        bool isPlay = audioProcessor.isPlayingActive();
        playPauseButton.setButtonText (isPlay ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE") : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
        playPauseButton.setColour (juce::TextButton::buttonColourId, isPlay ? juce::Colour (0xff9e1e1e) : juce::Colour (0xff1b633b));
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

    halfTimeToggle.setButtonText ("HALFTIME (1/2X)");
    halfTimeToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff333333));
    halfTimeToggle.onClick = [this]() {
        audioProcessor.halfTimeMode = halfTimeToggle.getToggleState();
        audioProcessor.renderMasterPreviewBuffer();
    };
    addAndMakeVisible (halfTimeToggle);

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

    // WIDE PRO RESOLUTION: 1100 x 740 for large, comfortable precision editing
    setSize (1100, 740);
    startTimerHz (40);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EXAudioProcessorEditor::timerCallback()
{
    bool isPlay = audioProcessor.isPlayingActive();
    playPauseButton.setButtonText (isPlay ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE") : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    playPauseButton.setColour (juce::TextButton::buttonColourId, isPlay ? juce::Colour (0xff9e1e1e) : juce::Colour (0xff1b633b));

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

    int targetLayer = -1;
    if (layerUIs[0].waveArea.getY() - 10 <= y && y <= layerUIs[0].waveArea.getBottom() + 10) targetLayer = 0;
    else if (layerUIs[1].waveArea.getY() - 10 <= y && y <= layerUIs[1].waveArea.getBottom() + 10) targetLayer = 1;
    else if (layerUIs[2].waveArea.getY() - 10 <= y && y <= layerUIs[2].waveArea.getBottom() + 10) targetLayer = 2;

    if (targetLayer >= 0 && targetLayer < 3)
    {
        audioProcessor.loadAudioFileToLayer (targetLayer, juce::File (files[0]));
        layerUIs[targetLayer].keySelector.setSelectedId (audioProcessor.getTrack (targetLayer).sourceKey + 1, juce::dontSendNotification);
        layerUIs[targetLayer].modeSelector.setSelectedId (audioProcessor.getTrack (targetLayer).sourceScaleMode + 1, juce::dontSendNotification);
        layerUIs[targetLayer].bpmSlider.setValue (audioProcessor.getTrack (targetLayer).sourceBpm, juce::dontSendNotification);
        repaint();
    }
}

void EXAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (radarCircleArea.contains (e.position))
    {
        audioProcessor.rollDiceAndGenerateMasterPattern();
        return;
    }

    if (previewWaveArea.contains (e.position) && audioProcessor.getMasterPreviewBuffer().getNumSamples() > 0)
    {
        auto wf = audioProcessor.exportCurrentAsAudioWav();
        if (wf.existsAsFile())
        {
            juce::StringArray files;
            files.add (wf.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false);
            return;
        }
    }

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

            // Wide grab tolerance (10 pixels) for comfortable mouse grabbing
            for (size_t m = 0; m < trk.sliceMarkers.size(); ++m)
            {
                float markerPx = trk.sliceMarkers[m] / samplePerPx;
                if (std::abs (clickX - markerPx) < 10.0f)
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
            audioProcessor.renderMasterPreviewBuffer();
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

    float cX = getWidth() * 0.5f;
    float cY = 185.0f;
    float r = 78.0f;
    radarCircleArea = juce::Rectangle<float> (cX - r, cY - r, r * 2.0f, r * 2.0f);

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

    drawRealistic3DCubeDice (g, cX, cY, 52.0f);

    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (40.0f, 65.0f, 275.0f, 240.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 315), 65.0f, 275.0f, 240.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    auto lowerBar = juce::Rectangle<float> (40.0f, 320.0f, static_cast<float> (getWidth() - 80), 380.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    // Large, Readable Waveform Panels
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        auto& trk = audioProcessor.getTrack (i);
        g.setColour (juce::Colour (0xff1a1c22));
        g.fillRoundedRectangle (ui.waveArea, 4.0f);

        if (trk.hasAudio && trk.buffer.getNumSamples() > 0)
        {
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
                if (!std::isnan(v) && !std::isinf(v))
                    g.drawLine (ui.waveArea.getX() + x, mid - (v * 22.0f), ui.waveArea.getX() + x, mid + (v * 22.0f), 1.0f);
            }

            for (int slice : trk.sliceMarkers)
            {
                float cutX = ui.waveArea.getX() + ((slice / (float) total) * w);
                g.setColour (juce::Colours::white);
                g.drawVerticalLine ((int) cutX, ui.waveArea.getY(), ui.waveArea.getBottom());
                // Easy-to-grab Golden Marker Head
                g.setColour (juce::Colour (0xffffcc00));
                g.fillEllipse (cutX - 3.5f, ui.waveArea.getY(), 7.0f, 7.0f);
            }
        }
        else
        {
            g.setColour (juce::Colour (0xff656978));
            g.setFont (12.0f);
            g.drawText ("DROP AUDIO HERE", ui.waveArea.toNearestInt(), juce::Justification::centred);
        }
    }

    g.setColour (juce::Colour (0xff161324));
    g.fillRoundedRectangle (previewWaveArea, 4.0f);
    g.setColour (juce::Colour (0x667b2cbf));
    g.drawRoundedRectangle (previewWaveArea, 4.0f, 1.2f);

    auto& pBuf = audioProcessor.getMasterPreviewBuffer();
    if (pBuf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xff9d4edd));
        int total = pBuf.getNumSamples();
        float w = previewWaveArea.getWidth();
        float mid = previewWaveArea.getCentreY();

        for (int x = 0; x < (int) w; ++x)
        {
            int sIdx = static_cast<int> ((x / w) * total);
            sIdx = juce::jlimit (0, total - 1, sIdx);
            float v = pBuf.getSample (0, sIdx);
            if (!std::isnan(v) && !std::isinf(v))
                g.drawLine (previewWaveArea.getX() + x, mid - (v * 24.0f), previewWaveArea.getX() + x, mid + (v * 24.0f), 1.0f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff7b2cbf));
        g.setFont (12.0f);
        g.drawText ("MASTER FLIP PREVIEW // DRAG FULL AUDIO OR STEMS DIRECTLY TO PLAYLIST", previewWaveArea.toNearestInt(), juce::Justification::centred);
    }
}

void EXAudioProcessorEditor::resized()
{
    brandTitle.setBounds (45, 14, 60, 32);
    subTitle.setBounds (115, 22, 600, 20);

    delayTimeKnob.setBounds (60, 80, 76, 82);
    delayTimeLabel.setBounds (60, 162, 76, 16);
    delayMixKnob.setBounds (155, 80, 76, 82);
    delayMixLabel.setBounds (155, 162, 76, 16);

    reverbSizeKnob.setBounds (60, 192, 76, 82);
    rvbSizeLabel.setBounds (60, 274, 76, 16);
    reverbMixKnob.setBounds (155, 192, 76, 82);
    rvbMixLabel.setBounds (155, 274, 76, 16);

    tapeDriveKnob.setBounds (getWidth() - 295, 80, 76, 82);
    tapeLabel.setBounds (getWidth() - 295, 162, 76, 16);
    filterCutoffKnob.setBounds (getWidth() - 200, 80, 76, 82);
    filterLabel.setBounds (getWidth() - 200, 162, 76, 16);

    masterOutKnob.setBounds (getWidth() - 248, 192, 76, 82);
    masterLabel.setBounds (getWidth() - 248, 274, 76, 16);

    int cX = getWidth() / 2;
    playPauseButton.setBounds (cX - 90, 278, 85, 32);
    dragMidiButton.setBounds (cX + 5, 278, 85, 32);

    masterTargetTitle.setBounds (55, 334, 125, 20);
    masterKeySelector.setBounds (185, 332, 68, 24);
    masterModeSelector.setBounds (258, 332, 85, 24);
    masterBpmSlider.setBounds (350, 332, 105, 24);

    halfTimeToggle.setBounds (468, 332, 130, 24);

    int gX = 615;
    int gY = 332;
    int gW = 44;
    btnQuarter.setBounds (gX, gY, gW, 24);
    btnEighth.setBounds (gX + 48, gY, gW, 24);
    btnSixteenth.setBounds (gX + 96, gY, gW, 24);
    btnTriplet1_3.setBounds (gX + 144, gY, gW + 6, 24);
    btnTriplet1_6.setBounds (gX + 196, gY, gW + 6, 24);
    btnTransient.setBounds (gX + 248, gY, 56, 24);

    int startY = 372;
    int slotH = 64; // Taller slot for big waveforms
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        int y = startY + (i * slotH);

        ui.titleLabel.setBounds (55, y + 18, 58, 20);
        ui.keySelector.setBounds (118, y + 18, 62, 24);
        ui.modeSelector.setBounds (185, y + 18, 80, 24);
        ui.bpmSlider.setBounds (270, y + 18, 82, 24);

        ui.volSlider.setBounds (360, y + 10, 36, 36);
        ui.volLabel.setBounds (360, y + 46, 36, 10);

        ui.muteBtn.setBounds (405, y + 18, 24, 24);
        ui.clearBtn.setBounds (433, y + 18, 24, 24);

        // Huge, wide waveform area with lots of height (50px tall!)
        ui.waveArea = juce::Rectangle<float> (468.0f, (float) (y + 6), (float) (getWidth() - 525), 50.0f);
    }

    int masterY = startY + (3 * slotH) + 12;
    previewTrackTitle.setBounds (55, masterY + 16, 95, 20);
    directExportWavBtn.setBounds (158, masterY + 14, 105, 28);
    exportStemsBtn.setBounds (268, masterY + 14, 105, 28);
    previewWaveArea = juce::Rectangle<float> (385.0f, (float) masterY, (float) (getWidth() - 440), 54.0f);
}
