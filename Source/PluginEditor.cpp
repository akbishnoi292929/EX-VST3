#include "PluginProcessor.h"
#include "PluginEditor.h"

void EXAudioProcessorEditor::styleKnob (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 16);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff2a42));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252830));
    s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffcccccc));
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (11.0f, juce::Font::bold));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff888f9e));
    addAndMakeVisible (l);
}

void EXAudioProcessorEditor::styleButton (juce::TextButton& b, juce::Colour col)
{
    b.setColour (juce::TextButton::buttonColourId, col);
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (b);
}

EXAudioProcessorEditor::EXAudioProcessorEditor (EXAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    headerLabel.setText ("EX // SMART WORKSTATION", juce::dontSendNotification);
    headerLabel.setFont (juce::Font (17.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (headerLabel);

    infoBadgeLabel.setText ("KEY: -- | BPM: --", juce::dontSendNotification);
    infoBadgeLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    infoBadgeLabel.setJustificationType (juce::Justification::centredRight);
    infoBadgeLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00f5d4)); // Neon Cyan
    addAndMakeVisible (infoBadgeLabel);

    // Transport & Smart Features
    styleButton (playPauseBtn, juce::Colour (0xff242732));
    styleButton (diceBtn, juce::Colour (0xff242732));
    styleButton (dragMidiBtn, juce::Colour (0xff2d3142));

    playPauseBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    diceBtn.setButtonText (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb2 DICE"));
    dragMidiBtn.setButtonText ("DRAG MIDI");

    playPauseBtn.onClick = [this]() {
        audioProcessor.togglePlayPause();
        playPauseBtn.setButtonText (audioProcessor.getIsAuditioning() 
            ? juce::CharPointer_UTF8 ("\xe2\x8f\xb8 PAUSE") 
            : juce::CharPointer_UTF8 ("\xe2\x96\xb6 PLAY"));
    };

    diceBtn.onClick = [this]() {
        audioProcessor.generateRandomMelodySequence();
        repaint();
    };

    dragMidiBtn.onClick = [this]() {
        auto midiFile = audioProcessor.exportCurrentSequenceAsMidi();
        if (midiFile.existsAsFile())
        {
            juce::StringArray files;
            files.add (midiFile.getFullPathName());
            startDragging ("MIDI Sequence", dragMidiBtn.getBounds());
            performExternalDragDropOfFiles (files, false);
        }
    };

    // Chops Buttons
    styleButton (chop1_4Btn, juce::Colour (0xff1f2128));
    styleButton (chop1_8Btn, juce::Colour (0xff1f2128));
    styleButton (chop1_16Btn, juce::Colour (0xff1f2128));
    styleButton (chopTransientBtn, juce::Colour (0xff1f2128));
    styleButton (stemSplitBtn, juce::Colour (0xffff2a42));

    chop1_4Btn.setButtonText ("1/4");
    chop1_8Btn.setButtonText ("1/8");
    chop1_16Btn.setButtonText ("1/16");
    chopTransientBtn.setButtonText ("TRANSIENT");
    stemSplitBtn.setButtonText ("AI 4-WAY SPLIT");

    chop1_4Btn.onClick = [this]() { audioProcessor.sliceAudio (4); repaint(); };
    chop1_8Btn.onClick = [this]() { audioProcessor.sliceAudio (8); repaint(); };
    chop1_16Btn.onClick = [this]() { audioProcessor.sliceAudio (16); repaint(); };
    chopTransientBtn.onClick = [this]() { audioProcessor.detectTransientsAndSlice(); repaint(); };
    stemSplitBtn.onClick = [this]() { audioProcessor.triggerStemSeparation(); repaint(); };

    // Stem tabs
    styleButton (masterTab, juce::Colour (0xff2b2e38));
    styleButton (vocalTab, juce::Colour (0xff1c1e24));
    styleButton (drumTab, juce::Colour (0xff1c1e24));
    styleButton (bassTab, juce::Colour (0xff1c1e24));
    styleButton (otherTab, juce::Colour (0xff1c1e24));

    masterTab.setButtonText ("MASTER");
    vocalTab.setButtonText ("VOCALS");
    drumTab.setButtonText ("DRUMS");
    bassTab.setButtonText ("BASS");
    otherTab.setButtonText ("OTHER");

    masterTab.onClick = [this]() { audioProcessor.selectStem (StemType::Master); repaint(); };
    vocalTab.onClick  = [this]() { audioProcessor.selectStem (StemType::Vocals); repaint(); };
    drumTab.onClick   = [this]() { audioProcessor.selectStem (StemType::Drums); repaint(); };
    bassTab.onClick   = [this]() { audioProcessor.selectStem (StemType::Bass); repaint(); };
    otherTab.onClick  = [this]() { audioProcessor.selectStem (StemType::Other); repaint(); };

    // Knobs
    styleKnob (driveKnob, driveLabel, "DRIVE");
    styleKnob (crushKnob, crushLabel, "BIT CRUSH");
    styleKnob (spaceKnob, spaceLabel, "SPACE");
    styleKnob (filterKnob, filterLabel, "LP FILTER", "Hz");
    styleKnob (pitchKnob, pitchLabel, "PITCH", "st");
    styleKnob (gainKnob, gainLabel, "OUT GAIN");

    driveAtt  = std::make_unique<Attachment> (audioProcessor.apvts, "DRIVE",  driveKnob);
    crushAtt  = std::make_unique<Attachment> (audioProcessor.apvts, "CRUSH",  crushKnob);
    spaceAtt  = std::make_unique<Attachment> (audioProcessor.apvts, "SPACE",  spaceKnob);
    filterAtt = std::make_unique<Attachment> (audioProcessor.apvts, "FILTER", filterKnob);
    pitchAtt  = std::make_unique<Attachment> (audioProcessor.apvts, "PITCH",  pitchKnob);
    gainAtt   = std::make_unique<Attachment> (audioProcessor.apvts, "GAIN",   gainKnob);

    setSize (780, 520);
    startTimerHz (30);
}

EXAudioProcessorEditor::~EXAudioProcessorEditor()
{
    stopTimer();
}

void EXAudioProcessorEditor::timerCallback()
{
    infoBadgeLabel.setText ("KEY: " + audioProcessor.detectedKey + " | BPM: " + juce::String (audioProcessor.detectedBpm), juce::dontSendNotification);

    if (audioProcessor.isSeparating.load() || audioProcessor.getIsAuditioning())
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
    auto& buffer = audioProcessor.getAudioBuffer();
    if (buffer.getNumSamples() == 0 || !waveArea.contains (e.getPosition()))
        return;

    int clickX = e.x - waveArea.getX();
    float samplePerPixel = (float) buffer.getNumSamples() / (float) waveArea.getWidth();

    draggedSliceIndex = -1;
    for (size_t i = 0; i < audioProcessor.getSlicePoints().size(); ++i)
    {
        int sliceSample = audioProcessor.getSlicePoints()[i];
        int slicePx = (int) (sliceSample / samplePerPixel);
        if (std::abs (clickX - slicePx) < 8)
        {
            draggedSliceIndex = (int) i;
            break;
        }
    }
}

void EXAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto& buffer = audioProcessor.getAudioBuffer();
    if (draggedSliceIndex >= 0 && buffer.getNumSamples() > 0)
    {
        int curX = juce::jlimit (waveArea.getX(), waveArea.getRight(), e.x) - waveArea.getX();
        float samplePerPixel = (float) buffer.getNumSamples() / (float) waveArea.getWidth();
        int newPos = (int) (curX * samplePerPixel);
        audioProcessor.setSlicePosition (draggedSliceIndex, newPos);
        repaint();
    }
}

void EXAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    draggedSliceIndex = -1;
}

void EXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0f12));

    // Header Bar
    g.setColour (juce::Colour (0xff14161c));
    g.fillRect (0, 0, getWidth(), 48);
    g.setColour (juce::Colour (0xff22252f));
    g.drawHorizontalLine (48, 0.0f, (float) getWidth());

    // Waveform Container
    waveArea = juce::Rectangle<int> (20, 92, getWidth() - 40, 180);
    g.setColour (juce::Colour (0xff13141a));
    g.fillRoundedRectangle (waveArea.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff232630));
    g.drawRoundedRectangle (waveArea.toFloat(), 6.0f, 1.2f);

    auto& buf = audioProcessor.getAudioBuffer();
    if (buf.getNumSamples() > 0)
    {
        g.setColour (juce::Colour (0xff00f5d4));
        auto numSamples = buf.getNumSamples();
        auto w = waveArea.getWidth();
        auto midY = waveArea.getCentreY();
        auto maxH = waveArea.getHeight() / 2 - 12;

        for (int x = 0; x < w; ++x)
        {
            int idx = (int) ((x / (float) w) * numSamples);
            float val = buf.getSample (0, idx);
            g.drawLine ((float) waveArea.getX() + x, midY - (val * maxH),
                        (float) waveArea.getX() + x, midY + (val * maxH), 1.0f);
        }

        // Draggable Slice Markers
        for (size_t i = 0; i < audioProcessor.getSlicePoints().size(); ++i)
        {
            int slicePos = audioProcessor.getSlicePoints()[i];
            float lineX = waveArea.getX() + ((slicePos / (float) numSamples) * w);
            g.setColour (juce::Colour (0xffff2a42));
            g.drawLine (lineX, (float) waveArea.getY(), lineX, (float) waveArea.getBottom(), 2.0f);

            g.setColour (juce::Colours::white);
            g.fillRect (lineX - 3.0f, (float) waveArea.getY(), 6.0f, 12.0f);
        }
    }
    else
    {
        g.setColour (juce::Colour (0xff5a5f70));
        g.setFont (14.0f);
        g.drawText ("DROP SAMPLE HERE // MP3 / WAV", waveArea, juce::Justification::centred);
    }

    // Lower Rack
    auto rackBounds = juce::Rectangle<int> (20, 330, getWidth() - 40, 170);
    g.setColour (juce::Colour (0xff14151b));
    g.fillRoundedRectangle (rackBounds.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff20232c));
    g.drawRoundedRectangle (rackBounds.toFloat(), 6.0f, 1.0f);
}

void EXAudioProcessorEditor::resized()
{
    headerLabel.setBounds (20, 10, 280, 28);
    infoBadgeLabel.setBounds (getWidth() - 250, 14, 230, 20);

    // Stem View Tabs (Left)
    int tabW = 68;
    int tabH = 26;
    int tabY = 58;
    masterTab.setBounds (20, tabY, tabW, tabH);
    vocalTab.setBounds (92, tabY, tabW, tabH);
    drumTab.setBounds (164, tabY, tabW, tabH);
    bassTab.setBounds (236, tabY, tabW, tabH);
    otherTab.setBounds (308, tabY, tabW, tabH);

    // Transport & Generative Controls (Middle)
    playPauseBtn.setBounds (390, tabY, 65, tabH);
    diceBtn.setBounds (460, tabY, 58, tabH);
    dragMidiBtn.setBounds (523, tabY, 78, tabH);

    // Chops Buttons right aligned
    chop1_4Btn.setBounds (getWidth() - 175, tabY, 32, tabH);
    chop1_8Btn.setBounds (getWidth() - 140, tabY, 32, tabH);
    chop1_16Btn.setBounds (getWidth() - 105, tabY, 38, tabH);
    stemSplitBtn.setBounds (getWidth() - 65, tabY, 45, tabH);

    // 6-Knob Rack Layout
    int startY = 345;
    int spacing = 122;
    int knobW = 82;
    int knobH = 82;
    int startX = (getWidth() - (spacing * 6)) / 2 + 20;

    juce::Slider* knobs[] = { &driveKnob, &crushKnob, &spaceKnob, &filterKnob, &pitchKnob, &gainKnob };
    juce::Label* labels[] = { &driveLabel, &crushLabel, &spaceLabel, &filterLabel, &pitchLabel, &gainLabel };

    for (int i = 0; i < 6; ++i)
    {
        knobs[i]->setBounds (startX + (i * spacing), startY, knobW, knobH);
        labels[i]->setBounds (startX + (i * spacing), startY + knobH + 4, knobW, 16);
    }
}
