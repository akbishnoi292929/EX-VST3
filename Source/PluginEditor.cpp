#include "PluginProcessor.h"
#include "PluginEditor.h"

LuxuryHardwareKnobLookAndFeel::LuxuryHardwareKnobLookAndFeel() {}

void LuxuryHardwareKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                      float sliderPos, float rotaryStartAngle,
                                                      float rotaryEndAngle, juce::Slider&)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 2.5f;
    auto centreX = (float) x + (float) width * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour (0xff141518));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    float trackR = radius - 2.5f;
    juce::Path trackPath;
    trackPath.addCentredArc (centreX, centreY, trackR, trackR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2b2d35));
    g.strokePath (trackPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path activePath;
    activePath.addCentredArc (centreX, centreY, trackR, trackR, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xffe63946));
    g.strokePath (activePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float capR = radius * 0.72f;
    juce::ColourGradient grad (juce::Colour (0xff2d3038), centreX, centreY - capR,
                               juce::Colour (0xff1a1b20), centreX, centreY + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (juce::Colour (0x33ffffff));
    g.drawEllipse (centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.0f);

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
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 13);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff22252a));
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (9.5f, juce::Font::bold));
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
        wobble = std::sin (audioProcessor.diceRollProgress * juce::MathConstants<float>::pi * 6.0f) * 5.0f;
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
    g.strokePath (topFace, juce::PathStrokeType (1.2f));
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
    brandTitle.setFont (juce::Font (26.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER // PRO WORKSTATION", juce::dontSendNotification);
    subTitle.setFont (juce::Font (11.0f, juce::Font::bold));
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
        ui.bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
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
    masterTargetTitle.setFont (juce::Font (11.5f, juce::Font::bold));
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
    masterBpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
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

    // Optimized widescreen workspace
    setSize (1140, 760);
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

            // Broad grab tolerance (12 pixels) for immediate responsive click
            for (size_t m = 0; m < trk.sliceMarkers.size(); ++m)
            {
                float markerPx = trk.sliceMarkers[m] / samplePerPx;
                if (std::abs (clickX - markerPx) < 12.0f)
                {
                    draggingLayerIdx = i;
                    draggingMarkerIdx = (int) m;
                    return;
                }
            }
        }
    }
}

// 60FPS BUTTERY SMOOTH MOUSE DRAG (Only updates visual position without audio freeze)
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
            int newSamplePos = static_cast<int> (curX * samplePerPx);

            // Fast responsive move
            trk.sliceMarkers[(size_t) draggingMarkerIdx] = juce::jlimit (0, trk.buffer.getNumSamples() - 1, newSamplePos);
            std::sort (trk.sliceMarkers.begin(), trk.sliceMarkers.end());

            repaint(); // Instant 60fps graphical redraw without recalculating heavy DSP
        }
    }
}

// TRIGGER RE-RENDER ONLY WHEN MOUSE IS RELEASED (Kills all drag stuttering!)
void EXAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (draggingLayerIdx >= 0 && draggingMarkerIdx >= 0)
    {
        audioProcessor.renderMasterPreviewBuffer();
    }
    draggingLayerIdx = -1;
    draggingMarkerIdx = -1;
}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff3d2212)); // Deep Walnut Frame

    auto body = getLocalBounds().reduced (12, 10).toFloat();
    g.setColour (juce::Colour (0xffece9e1));
    g.fillRoundedRectangle (body, 6.0f);

    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (body, 6.0f, 1.8f);

    // Subtle corner studs
    g.setColour (juce::Colour (0xffa8a49c));
    g.fillEllipse (18.0f, 14.0f, 6.0f, 6.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 24), 14.0f, 6.0f, 6.0f);
    g.fillEllipse (18.0f, static_cast<float> (getHeight() - 20), 6.0f, 6.0f);
    g.fillEllipse (static_cast<float> (getWidth() - 24), static_cast<float> (getHeight() - 20), 6.0f, 6.0f);

    // COMPACT RADAR CENTER
    float cX = getWidth() * 0.5f;
    float cY = 115.0f;
    float r = 58.0f;
    radarCircleArea = juce::Rectangle<float> (cX - r, cY - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colour (0xffb8b4a8));
    g.drawEllipse (cX - r - 3.0f, cY - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, 2.5f);

    g.setColour (juce::Colour (0xff3b0a0a));
    g.fillEllipse (cX - r, cY - r, r * 2.0f, r * 2.0f);

    g.setColour (juce::Colour (0x33cf3838));
    for (float gR = 15.0f; gR < 60.0f; gR += 15.0f)
        g.drawEllipse (cX - gR, cY - gR, gR * 2.0f, gR * 2.0f, 1.0f);
    g.drawLine (cX - r, cY, cX + r, cY, 1.0f);
    g.drawLine (cX, cY - r, cX, cY + r, 1.0f);

    float scanX = cX + std::cos (radarAngle) * r;
    float scanY = cY + std::sin (radarAngle) * r;
    g.setColour (juce::Colour (0x66ff3b30));
    g.drawLine (cX, cY, scanX, scanY, 1.5f);

    drawRealistic3DCubeDice (g, cX, cY, 44.0f);

    // SLENDER TOP KNOB PANELS
    g.setColour (juce::Colour (0xffe2dfd4));
    auto leftBox = juce::Rectangle<float> (35.0f, 48.0f, 320.0f, 140.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 355), 48.0f, 320.0f, 140.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    // EXPANDED MASSIVE BOTTOM WORKSPACE
    auto lowerBar = juce::Rectangle<float> (35.0f, 202.0f, static_cast<float> (getWidth() - 70), 530.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    // HUGE TALL WAVEFORM PANELS
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        auto& trk = audioProcessor.getTrack (i);
        g.setColour (juce::Colour (0xff14151b));
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
                    g.drawLine (ui.waveArea.getX() + x, mid - (v * 32.0f), ui.waveArea.getX() + x, mid + (v * 32.0f), 1.0f);
            }

            for (int slice : trk.sliceMarkers)
            {
                float cutX = ui.waveArea.getX() + ((slice / (float) total) * w);
                g.setColour (juce::Colours::white);
                g.drawVerticalLine ((int) cutX, ui.waveArea.getY(), ui.waveArea.getBottom());

                // Glowing Solid Gold Grab Handle on Top
                g.setColour (juce::Colour (0xffffcc00));
                g.fillEllipse (cutX - 4.5f, ui.waveArea.getY() - 1.0f, 9.0f, 9.0f);
                g.setColour (juce::Colour (0xff000000));
                g.drawEllipse (cutX - 4.5f, ui.waveArea.getY() - 1.0f, 9.0f, 9.0f, 1.0f);
            }
        }
        else
        {
            g.setColour (juce::Colour (0xff585b68));
            g.setFont (13.0f);
            g.drawText ("DRAG & DROP SAMPLES HERE", ui.waveArea.toNearestInt(), juce::Justification::centred);
        }
    }

    // Huge Master Preview Strip
    g.setColour (juce::Colour (0xff120f1e));
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
                g.drawLine (previewWaveArea.getX() + x, mid - (v * 28.0f), previewWaveArea.getX() + x, mid + (v * 28.0f), 1.0f);
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
    brandTitle.setBounds (40, 10, 50, 30);
    subTitle.setBounds (95, 16, 500, 20);

    // COMPACT TOP KNOBS SECTION (Saved 100+ Vertical Pixels!)
    int leftK_X = 50;
    int kSpacing = 72;
    delayTimeKnob.setBounds (leftK_X, 56, 62, 66);
    delayTimeLabel.setBounds (leftK_X, 122, 62, 14);
    delayMixKnob.setBounds (leftK_X + kSpacing, 56, 62, 66);
    delayMixLabel.setBounds (leftK_X + kSpacing, 122, 62, 14);
    reverbSizeKnob.setBounds (leftK_X + kSpacing * 2, 56, 62, 66);
    rvbSizeLabel.setBounds (leftK_X + kSpacing * 2, 122, 62, 14);
    reverbMixKnob.setBounds (leftK_X + kSpacing * 3, 56, 62, 66);
    rvbMixLabel.setBounds (leftK_X + kSpacing * 3, 122, 62, 14);

    int rightK_X = getWidth() - 340;
    tapeDriveKnob.setBounds (rightK_X, 56, 62, 66);
    tapeLabel.setBounds (rightK_X, 122, 62, 14);
    filterCutoffKnob.setBounds (rightK_X + kSpacing, 56, 62, 66);
    filterLabel.setBounds (rightK_X + kSpacing, 122, 62, 14);
    masterOutKnob.setBounds (rightK_X + kSpacing * 2, 56, 62, 66);
    masterLabel.setBounds (rightK_X + kSpacing * 2, 122, 62, 14);

    int cX = getWidth() / 2;
    playPauseButton.setBounds (cX - 88, 178, 84, 28);
    dragMidiButton.setBounds (cX + 4, 178, 84, 28);

    // MASTER TARGET CONTROL BAR
    int targetY = 212;
    masterTargetTitle.setBounds (48, targetY, 115, 24);
    masterKeySelector.setBounds (168, targetY, 64, 24);
    masterModeSelector.setBounds (238, targetY, 80, 24);
    masterBpmSlider.setBounds (324, targetY, 95, 24);
    halfTimeToggle.setBounds (430, targetY, 120, 24);

    int gX = 570;
    int gW = 42;
    btnQuarter.setBounds (gX, targetY, gW, 24);
    btnEighth.setBounds (gX + 46, targetY, gW, 24);
    btnSixteenth.setBounds (gX + 92, targetY, gW, 24);
    btnTriplet1_3.setBounds (gX + 138, targetY, gW + 6, 24);
    btnTriplet1_6.setBounds (gX + 188, targetY, gW + 6, 24);
    btnTransient.setBounds (gX + 238, targetY, 54, 24);

    // GIGANTIC EXPANDED LAYER WORKSPACE
    int startY = 246;
    int slotH = 105; // MASSIVE 105px per layer slot!
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        int y = startY + (i * slotH);

        ui.titleLabel.setBounds (48, y + 10, 56, 20);
        ui.keySelector.setBounds (108, y + 10, 60, 22);
        ui.modeSelector.setBounds (174, y + 10, 74, 22);
        ui.bpmSlider.setBounds (254, y + 10, 78, 22);

        ui.volSlider.setBounds (338, y + 6, 34, 34);
        ui.volLabel.setBounds (338, y + 42, 34, 10);

        ui.muteBtn.setBounds (378, y + 10, 22, 22);
        ui.clearBtn.setBounds (404, y + 10, 22, 22);

        // Huge 72px Tall Waveform Box with massive horizontal length
        ui.waveArea = juce::Rectangle<float> (436.0f, (float) (y + 4), (float) (getWidth() - 485), 78.0f);
    }

    int masterY = startY + (3 * slotH) + 12;
    previewTrackTitle.setBounds (48, masterY + 18, 90, 20);
    directExportWavBtn.setBounds (144, masterY + 16, 100, 28);
    exportStemsBtn.setBounds (250, masterY + 16, 100, 28);
    previewWaveArea = juce::Rectangle<float> (360.0f, (float) masterY, (float) (getWidth() - 410), 62.0f);
}
