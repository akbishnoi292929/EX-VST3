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
    brandTitle.setFont (juce::Font (28.0f, juce::Font::bold));
    brandTitle.setColour (juce::Label::textColourId, juce::Colour (0xff9e1e1e));
    addAndMakeVisible (brandTitle);

    subTitle.setText ("HEARTLESS SAMPLE FLIPPER // PRO 4-LAYER WORKSTATION", juce::dontSendNotification);
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

        ui.bpmSlider.setRange (60, 200, 1);
        ui.bpmSlider.setValue (audioProcessor.getTrack (i).sourceBpm, juce::dontSendNotification);
        ui.bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
        ui.bpmSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
        addAndMakeVisible (ui.bpmSlider);

        // Dedicated Layer Volume Slider
        ui.volSlider.setLookAndFeel (&vintageKnobLAF);
        ui.volSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        ui.volSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
        ui.volSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
        addAndMakeVisible (ui.volSlider);

        ui.volLabel.setText ("VOL", juce::dontSendNotification);
        ui.volLabel.setFont (juce::Font (9.0f, juce::Font::bold));
        ui.volLabel.setJustificationType (juce::Justification::centred);
        ui.volLabel.setColour (juce::Label::textColourId, juce::Colour (0xff4a4c52));
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
    previewTrackTitle.setFont (juce::Font (11.0f, juce::Font::bold));
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

    // Multi-Track STEMS Drag Button
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

    setSize (980, 680);
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
        layerUIs[i].modeSelector.setSelectedId (audioProcessor.getTrack (i).sourceScaleMode + 1, juce::dontSendNotification);
        layerUIs[i].bpmSlider.setValue (audioProcessor.getTrack (i).sourceBpm, juce::dontSendNotification);
    }

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
    auto leftBox = juce::Rectangle<float> (35.0f, 65.0f, 255.0f, 240.0f);
    auto rightBox = juce::Rectangle<float> (static_cast<float> (getWidth() - 290), 65.0f, 255.0f, 240.0f);
    g.fillRoundedRectangle (leftBox, 4.0f);
    g.fillRoundedRectangle (rightBox, 4.0f);
    g.setColour (juce::Colour (0xffcfcac0));
    g.drawRoundedRectangle (leftBox, 4.0f, 1.0f);
    g.drawRoundedRectangle (rightBox, 4.0f, 1.0f);

    auto lowerBar = juce::Rectangle<float> (35.0f, 320.0f, static_cast<float> (getWidth() - 70), 325.0f);
    g.setColour (juce::Colour (0xffdfdbcf));
    g.fillRoundedRectangle (lowerBar, 4.0f);
    g.setColour (juce::Colour (0xffc4beb2));
    g.drawRoundedRectangle (lowerBar, 4.0f, 1.0f);

    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        auto& trk = audioProcessor.getTrack (i);
        g.setColour (juce::Colour (0xff1f2128));
        g.fillRoundedRectangle (ui.waveArea, 3.0f);

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
                    g.drawLine (ui.waveArea.getX() + x, mid - (v * 18.0f), ui.waveArea.getX() + x, mid + (v * 18.0f), 1.0f);
            }

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

    g.setColour (juce::Colour (0xff191724));
    g.fillRoundedRectangle (previewWaveArea, 3.0f);
    g.setColour (juce::Colour (0x667b2cbf));
    g.drawRoundedRectangle (previewWaveArea, 3.0f, 1.2f);

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
                g.drawLine (previewWaveArea.getX() + x, mid - (v * 20.0f), previewWaveArea.getX() + x, mid + (v * 20.0f), 1.0f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff7b2cbf));
        g.setFont (11.5f);
        g.drawText ("MASTER FLIP PREVIEW // DRAG FULL AUDIO OR STEMS DIRECTLY TO PLAYLIST", previewWaveArea.toNearestInt(), juce::Justification::centred);
    }
}

void EXAudioProcessorEditor::resized()
{
    brandTitle.setBounds (35, 12, 60, 32);
    subTitle.setBounds (100, 20, 600, 20);

    delayTimeKnob.setBounds (50, 80, 72, 80);
    delayTimeLabel.setBounds (50, 160, 72, 16);
    delayMixKnob.setBounds (140, 80, 72, 80);
    delayMixLabel.setBounds (140, 160, 72, 16);

    reverbSizeKnob.setBounds (50, 190, 72, 80);
    rvbSizeLabel.setBounds (50, 270, 72, 16);
    reverbMixKnob.setBounds (140, 190, 72, 80);
    rvbMixLabel.setBounds (140, 270, 72, 16);

    tapeDriveKnob.setBounds (getWidth() - 270, 80, 72, 80);
    tapeLabel.setBounds (getWidth() - 270, 160, 72, 16);
    filterCutoffKnob.setBounds (getWidth() - 180, 80, 72, 80);
    filterLabel.setBounds (getWidth() - 180, 160, 72, 16);

    masterOutKnob.setBounds (getWidth() - 225, 190, 72, 80);
    masterLabel.setBounds (getWidth() - 225, 270, 72, 16);

    int cX = getWidth() / 2;
    playPauseButton.setBounds (cX - 86, 278, 82, 30);
    dragMidiButton.setBounds (cX + 4, 278, 82, 30);

    masterTargetTitle.setBounds (45, 332, 120, 20);
    masterKeySelector.setBounds (165, 330, 62, 24);
    masterModeSelector.setBounds (232, 330, 84, 24);
    masterBpmSlider.setBounds (322, 330, 95, 24);

    halfTimeToggle.setBounds (428, 330, 120, 24);

    int gX = 560;
    int gY = 330;
    int gW = 38;
    btnQuarter.setBounds (gX, gY, gW, 24);
    btnEighth.setBounds (gX + 42, gY, gW, 24);
    btnSixteenth.setBounds (gX + 84, gY, gW, 24);
    btnTriplet1_3.setBounds (gX + 126, gY, gW + 6, 24);
    btnTriplet1_6.setBounds (gX + 172, gY, gW + 6, 24);
    btnTransient.setBounds (gX + 218, gY, 50, 24);

    int startY = 368;
    int slotH = 55;
    for (int i = 0; i < 3; ++i)
    {
        auto& ui = layerUIs[i];
        int y = startY + (i * slotH);

        ui.titleLabel.setBounds (45, y + 14, 55, 20);
        ui.keySelector.setBounds (102, y + 14, 58, 22);
        ui.modeSelector.setBounds (163, y + 14, 75, 22);
        ui.bpmSlider.setBounds (242, y + 14, 75, 22);

        // Layer Volume Rotary Knob & Label
        ui.volSlider.setBounds (322, y + 4, 38, 42);
        ui.volLabel.setBounds (322, y + 44, 38, 10);

        ui.muteBtn.setBounds (366, y + 14, 22, 22);
        ui.clearBtn.setBounds (392, y + 14, 22, 22);

        ui.waveArea = juce::Rectangle<float> (422.0f, (float) (y + 4), (float) (getWidth() - 470), 42.0f);
    }

    int masterY = startY + (3 * slotH) + 12;
    previewTrackTitle.setBounds (45, masterY + 12, 85, 20);
    directExportWavBtn.setBounds (135, masterY + 10, 95, 26);
    exportStemsBtn.setBounds (235, masterY + 10, 95, 26);
    previewWaveArea = juce::Rectangle<float> (340.0f, (float) masterY, (float) (getWidth() - 390), 46.0f);
}
