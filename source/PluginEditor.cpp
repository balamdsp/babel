#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Components/AudioSettingsPanel.h"

#include <cmath>
#include <limits>

#if JucePlugin_Build_Standalone
#include "Standalone/CustomStandaloneFilterWindow.h"
#endif

extern "C" int babelGetFrameExtents (unsigned long windowH,
                                     int* outFrameW, int* outFrameH);

static constexpr int W = 1000;
static constexpr int H = 700;
static constexpr int kLoadFromFileId = 0x1001;

static bool isInStandaloneApp (const juce::Component* c);

static juce::Point<int> getNativeFrameSize (juce::Component* topLevelWindow)
{
    if (topLevelWindow != nullptr)
        if (auto* peer = topLevelWindow->getPeer())
            if (peer->getNativeHandle() != nullptr)
            {
                int frameW = 0, frameH = 0;
                if (babelGetFrameExtents ((unsigned long) peer->getNativeHandle(),
                                          &frameW, &frameH) != 0)
                    return { frameW, frameH };
            }
    return {};
}

struct Metrics
{
    explicit Metrics (float s) : scale (s) {}
    float scale = 1.0f;
    int sc (float v) const { return juce::roundToInt (v * scale); }
    int TOP_BAR_H = sc (77.0f);
    int BODY_TOP  = sc (87.0f);
};

SaveAsDialog::SaveAsDialog (const juce::String& initialName, float scale)
{
    uiScale = scale;
    babelDialogLnf().setScale (uiScale);
    setLookAndFeel (&babelDialogLnf());

    titleLabel.setText (">> SAVE PRESET", juce::dontSendNotification);
    titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f * uiScale));
    titleLabel.setColour (juce::Label::textColourId,
                          BabelColors::textPrimary.withAlpha (0.50f));
    addAndMakeVisible (titleLabel);

    messageLabel.setText ("Enter a name for your preset:", juce::dontSendNotification);
    messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
    messageLabel.setColour (juce::Label::textColourId, BabelColors::textPrimary);
    addAndMakeVisible (messageLabel);

    nameEditor.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
    nameEditor.setText (initialName);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, BabelColors::buttonOff);
    nameEditor.setColour (juce::TextEditor::textColourId, BabelColors::textPrimary);
    nameEditor.setColour (juce::TextEditor::outlineColourId, BabelColors::buttonBorder);
    addAndMakeVisible (nameEditor);

    cancelButton.setButtonText ("CANCEL");
    cancelButton.onClick = [this] { closeWindow(); };
    addAndMakeVisible (cancelButton);

    confirmButton.setButtonText ("CONFIRM");
    confirmButton.onClick = [this]
    {
        if (onConfirm)
            onConfirm (nameEditor.getText());
        closeWindow();
    };
    addAndMakeVisible (confirmButton);
}

SaveAsDialog::~SaveAsDialog() { setLookAndFeel (nullptr); }

void SaveAsDialog::paint (juce::Graphics& g)
{
    paintCardLayers (g, getLocalBounds().toFloat());
}

void SaveAsDialog::resized()
{
    auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * uiScale));
    titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * uiScale)));
    area.removeFromTop (juce::roundToInt (5.0f * uiScale));
    messageLabel.setBounds (area.removeFromTop (juce::roundToInt (26.0f * uiScale)));
    area.removeFromTop (juce::roundToInt (8.0f * uiScale));
    nameEditor.setBounds (area.removeFromTop (juce::roundToInt (36.0f * uiScale)));
    area.removeFromTop (juce::roundToInt (12.0f * uiScale));
    auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * uiScale));
    cancelButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (150.0f * uiScale)));
    buttonsRow.removeFromRight (juce::roundToInt (10.0f * uiScale));
    confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * uiScale)));
}

void SaveAsDialog::visibilityChanged()
{
    if (isVisible())
        nameEditor.grabKeyboardFocus();
}

void SaveAsDialog::closeWindow()
{
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->closeButtonPressed();
}

ConfirmDialog::ConfirmDialog (const juce::String& titleText,
                              const juce::String& messageText, float scale)
{
    uiScale = scale;
    babelDialogLnf().setScale (uiScale);
    setLookAndFeel (&babelDialogLnf());

    titleLabel.setText (titleText, juce::dontSendNotification);
    titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f * uiScale));
    titleLabel.setColour (juce::Label::textColourId,
                          BabelColors::textPrimary.withAlpha (0.50f));
    addAndMakeVisible (titleLabel);

    messageLabel.setText (messageText, juce::dontSendNotification);
    messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
    messageLabel.setColour (juce::Label::textColourId, BabelColors::textPrimary);
    addAndMakeVisible (messageLabel);

    cancelButton.setButtonText ("CANCEL");
    cancelButton.onClick = [this] { closeWindow(); };
    addAndMakeVisible (cancelButton);

    confirmButton.setButtonText ("CONFIRM");
    confirmButton.onClick = [this]
    {
        if (onConfirm)
            onConfirm();
        closeWindow();
    };
    addAndMakeVisible (confirmButton);
}

ConfirmDialog::~ConfirmDialog() { setLookAndFeel (nullptr); }

void ConfirmDialog::paint (juce::Graphics& g)
{
    paintCardLayers (g, getLocalBounds().toFloat());
}

void ConfirmDialog::resized()
{
    auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * uiScale));
    titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * uiScale)));
    area.removeFromTop (juce::roundToInt (5.0f * uiScale));
    messageLabel.setBounds (area.removeFromTop (juce::roundToInt (52.0f * uiScale)));
    area.removeFromTop (juce::roundToInt (12.0f * uiScale));
    auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * uiScale));
    cancelButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (150.0f * uiScale)));
    buttonsRow.removeFromRight (juce::roundToInt (10.0f * uiScale));
    confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * uiScale)));
}

void ConfirmDialog::closeWindow()
{
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->closeButtonPressed();
}

BabelAudioProcessorEditor::ScreenContent::ScreenContent()
{
    startTimerHz (2);
}

void BabelAudioProcessorEditor::ScreenContent::timerCallback()
{
    cursorVisible = ! cursorVisible;
    repaint();
}

void BabelAudioProcessorEditor::ScreenContent::paint (juce::Graphics& g)
{
    using namespace BabelColors;
    const int w = getWidth();
    const int h = getHeight();
    const float s = (float) w / (float) W;
    const auto mf = [] (float size, float sc) { return CustomLookAndFeel::makeFont (size * sc); };

    g.fillAll (background);
    drawScanlines (g, { 0, 0, w, h }, juce::Colours::black.withAlpha (0.22f));
    g.setColour (body);
    g.fillRect (0, 0, w, juce::roundToInt (77.0f * s));

    const int titleX = juce::roundToInt (18.0f * s);
    g.setColour (textBrand);
    g.setFont (mf (38.0f, s));
    g.drawText ("BABEL...", titleX, juce::roundToInt (12.0f * s),
                juce::roundToInt (260.0f * s), juce::roundToInt (32.0f * s),
                juce::Justification::centredLeft, false);

    if (cursorVisible)
    {
        const auto titleFont = mf (38.0f, s);
        const float bannerWidth = (float) juce::GlyphArrangement::getStringWidthInt (titleFont, "BABEL...");
        const float asc = titleFont.getAscent();
        const float desc = titleFont.getDescent();
        const float boxY = (float) juce::roundToInt (12.0f * s);
        const float boxH = (float) juce::roundToInt (32.0f * s);
        const float baseline = boxY + (boxH - (asc + desc)) * 0.5f + asc;
        g.setColour (textBrand.withAlpha (0.85f));
        g.fillRect ((float) titleX + bannerWidth + 8.0f * s,
                    baseline - asc * 0.7f - 2.0f * s,
                    13.0f * s, asc * 0.7f + 3.0f * s);
    }

    g.setColour (textMid);
    g.setFont (mf (18.0f, s));
    g.drawText ("MIDI FX GRID PLAYER", titleX, juce::roundToInt (46.0f * s),
                juce::roundToInt (300.0f * s), juce::roundToInt (18.0f * s),
                juce::Justification::centredLeft, false);

    const int brandX = w - juce::roundToInt (120.0f * s);
    g.setColour (textBrand);
    g.setFont (mf (24.0f, s));
    g.drawText ("BalamDSP", brandX, juce::roundToInt (16.0f * s),
                juce::roundToInt (100.0f * s), juce::roundToInt (26.0f * s),
                juce::Justification::centredRight, false);
    g.setColour (textMid.withAlpha (0.55f));
    g.setFont (mf (22.0f, s));
    g.drawText ("v" + juce::String (ProjectInfo::versionString), brandX,
                juce::roundToInt (40.0f * s), juce::roundToInt (100.0f * s),
                juce::roundToInt (22.0f * s), juce::Justification::centredRight, false);
}

void BabelAudioProcessorEditor::HamburgerButton::paint (juce::Graphics& g)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                           isMouseOver(), isDown());
    const auto bounds = getLocalBounds().toFloat();
    const float sc = static_cast<CustomLookAndFeel&> (getLookAndFeel()).getScale();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float barW = 18.0f * sc;
    const float barH = 2.0f * sc;
    const float gap = 5.0f * sc;
    g.setColour (isMouseOver() ? BabelColors::textPrimary : BabelColors::textMid);
    for (int i = -1; i <= 1; ++i)
        g.fillRect (cx - barW * 0.5f, cy + (float) i * gap - barH * 0.5f, barW, barH);
}

void BabelAudioProcessorEditor::RecButton::setRecState (int s)
{
    s = juce::jlimit (0, 2, s);
    if (s != recState)
    {
        recState = s;
        repaint();
    }
}

void BabelAudioProcessorEditor::RecButton::paint (juce::Graphics& g)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                           isMouseOver(), isDown());
    const auto bounds = getLocalBounds().toFloat();
    const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.45f;
    const auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());

    if (recState == 2)
    {
        g.setColour (BabelColors::highlight);
        g.fillEllipse (circle);
    }
    else
    {
        g.setColour (recState == 1 ? BabelColors::highlight
                                   : BabelColors::textMid.withAlpha (0.45f));
        g.drawEllipse (circle, juce::jmax (1.0f, d * 0.12f));
    }
}

void BabelAudioProcessorEditor::ExportButton::paint (juce::Graphics& g)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                            isMouseOver(), isDown());
    const auto bounds = getLocalBounds().toFloat();
    const float sc = static_cast<CustomLookAndFeel&> (getLookAndFeel()).getScale();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float s = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float thick = juce::jmax (1.5f * sc, s * 0.08f);
    g.setColour (! isEnabled() ? BabelColors::textMid.withAlpha (0.30f)
                : isMouseOver() ? BabelColors::textPrimary : BabelColors::textMid);
    const float iw = s * 0.52f;
    const float top = cy - s * 0.28f;
    const float mid = cy + s * 0.12f;
    const float bot = cy + s * 0.30f;
    g.drawLine (cx, top, cx, mid, thick);
    g.drawLine (cx, mid, cx - iw * 0.28f, mid - iw * 0.28f, thick);
    g.drawLine (cx, mid, cx + iw * 0.28f, mid - iw * 0.28f, thick);
    g.drawLine (cx - iw * 0.5f, mid, cx - iw * 0.5f, bot, thick);
    g.drawLine (cx + iw * 0.5f, mid, cx + iw * 0.5f, bot, thick);
    g.drawLine (cx - iw * 0.5f, bot, cx + iw * 0.5f, bot, thick);
}

BabelAudioProcessorEditor::BabelAudioProcessorEditor (BabelAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetManager (&p),
      padGrid (&p)
{
    setLookAndFeel (&customLookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&customLookAndFeel);

    if (auto* scaleParam = audioProcessor.getAPVTS().getParameter (BabelIds::UI_SCALE_ID))
    {
        const int idx = juce::roundToInt (scaleParam->getValue() * (BabelIds::ZOOM_PERCENTS.size() - 1));
        const int clampedIdx = juce::jlimit (0, (int) BabelIds::ZOOM_PERCENTS.size() - 1, idx);
        uiScale = BabelIds::ZOOM_PERCENTS[(size_t) clampedIdx] / 100.0f;
    }
    customLookAndFeel.setScale (uiScale);

    addAndMakeVisible (screenContent);
    addAndMakeVisible (crtOverlay);
    crtOverlay.toFront (false);

    presetDisplay.setButtonText (presetManager.getCurrentPresetName());
    presetDisplay.setClickingTogglesState (false);
    presetDisplay.addListener (this);
    screenContent.addAndMakeVisible (presetDisplay);

    menuButton.setClickingTogglesState (false);
    menuButton.setRepaintsOnMouseActivity (false);
    menuButton.addListener (this);
    screenContent.addAndMakeVisible (menuButton);

    recButton.setClickingTogglesState (false);
    recButton.setTooltip ("Record take");
    recButton.addListener (this);
    screenContent.addAndMakeVisible (recButton);

    exportButton.setButtonText (juce::String());
    exportButton.setTooltip ("Export MIDI take");
    exportButton.setClickingTogglesState (false);
    exportButton.addListener (this);
    exportButton.addMouseListener (this, false);
    screenContent.addAndMakeVisible (exportButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, BabelColors::textMid);
    screenContent.addAndMakeVisible (statusLabel);

    screenContent.addAndMakeVisible (padGrid);
    padGrid.onGesture = [this]
    {
        const int n = padGrid.getLastNote();
        if (n >= 0)
        {
            static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B" };
            statusLabel.setText (">> NOTE " + juce::String (names[n % 12])
                                  + juce::String (n / 12 - 1) + "  (" + juce::String (n) + ")",
                                  juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText (">> READY", juce::dontSendNotification);
        }
    };

    auto setupCombo = [this] (HoverableComboBox& box, juce::Label& label,
                              const juce::String& text, const juce::StringArray& items)
    {
        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
        box.setRepaintsOnMouseActivity (true);
        screenContent.addAndMakeVisible (box);
        label.setText (text.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, BabelColors::textPrimary);
        screenContent.addAndMakeVisible (label);
    };

    setupCombo (rootCombo, rootLabel, "Root", BabelIds::rootChoices());
    setupCombo (scaleCombo, scaleLabel, "Scale", BabelIds::scaleChoices());
    setupCombo (rowOffsetCombo, rowOffsetLabel, "Row Offset", BabelIds::rowOffsetChoices());
    setupCombo (layoutCombo, layoutLabel, "Layout", BabelIds::layoutChoices());

    rootAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), BabelIds::ROOT_ID, rootCombo);
    scaleAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), BabelIds::SCALE_ID, scaleCombo);
    rowOffsetAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), BabelIds::ROW_OFFSET_ID, rowOffsetCombo);
    layoutAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getAPVTS(), BabelIds::LAYOUT_ID, layoutCombo);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false,
                           juce::roundToInt (60.0f * uiScale),
                           juce::roundToInt (24.0f * uiScale));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        screenContent.addAndMakeVisible (s);
        l.setText (text.toUpperCase(), juce::dontSendNotification);
        l.setJustificationType (juce::Justification::bottomLeft);
        l.setColour (juce::Label::textColourId, BabelColors::textPrimary);
        l.setInterceptsMouseClicks (false, false);
        screenContent.addAndMakeVisible (l);
    };

    setupSlider (octaveSlider, octaveLabel, "Octave");
    setupSlider (channelSlider, channelLabel, "Channel");
    setupSlider (colsSlider, colsLabel, "Grid Cols");
    setupSlider (rowsSlider, rowsLabel, "Grid Rows");
    setupSlider (velocitySlider, velocityLabel, "Velocity");
    setupSlider (xCcNumSlider, xCcLabel, "X CC");
    setupSlider (yCcNumSlider, yCcLabel, "Y CC");

    octaveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::OCTAVE_ID, octaveSlider);
    channelAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::CHANNEL_ID, channelSlider);
    colsAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::GRID_COLS_ID, colsSlider);
    rowsAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::GRID_ROWS_ID, rowsSlider);
    velocityAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::VELOCITY_ID, velocitySlider);
    xCcNumAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::X_CC_NUM_ID, xCcNumSlider);
    yCcNumAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getAPVTS(), BabelIds::Y_CC_NUM_ID, yCcNumSlider);

    auto setupToggle = [this] (juce::ToggleButton& b, const juce::String& text)
    {
        b.setButtonText (text.toUpperCase());
        b.setRepaintsOnMouseActivity (true);
        screenContent.addAndMakeVisible (b);
    };
    setupToggle (showNonScaleButton, "Show Non-Scale");
    setupToggle (velFromYButton, "Vel From Y");
    setupToggle (xCcEnabledButton, "X CC On");
    setupToggle (yCcEnabledButton, "Y CC On");
    setupToggle (legatoButton, "Legato");

    showNonScaleAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), BabelIds::SHOW_NON_SCALE_ID, showNonScaleButton);
    velFromYAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), BabelIds::VEL_FROM_Y_ID, velFromYButton);
    xCcEnabledAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), BabelIds::X_CC_ENABLED_ID, xCcEnabledButton);
    yCcEnabledAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), BabelIds::Y_CC_ENABLED_ID, yCcEnabledButton);
    legatoAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getAPVTS(), BabelIds::LEGATO_ID, legatoButton);

    audioProcessor.getAPVTS().addParameterListener (BabelIds::UI_SCALE_ID, this);
    for (auto* id : { BabelIds::ROOT_ID, BabelIds::SCALE_ID, BabelIds::OCTAVE_ID,
                      BabelIds::ROW_OFFSET_ID, BabelIds::SHOW_NON_SCALE_ID,
                      BabelIds::LAYOUT_ID,
                      BabelIds::GRID_COLS_ID, BabelIds::GRID_ROWS_ID })
        audioProcessor.getAPVTS().addParameterListener (id, this);

    crtEnabled.store (presetManager.getCrtEnabled());
    crtOverlay.setCrtStrength (presetManager.getCrtStrength());

    statusLabel.setText (">> READY", juce::dontSendNotification);
    updatePresetDisplay();
    applyZoom (uiScale);
    startTimerHz (10);

    if (isInStandaloneApp (this))
        juce::Timer::callAfterDelay (250, [safeThis = juce::Component::SafePointer<BabelAudioProcessorEditor> (this)]
        {
            if (safeThis != nullptr)
                safeThis->applyZoom (safeThis->uiScale);
        });
}

BabelAudioProcessorEditor::~BabelAudioProcessorEditor()
{
    stopTimer();
    for (auto* id : { BabelIds::UI_SCALE_ID, BabelIds::ROOT_ID, BabelIds::SCALE_ID,
                      BabelIds::OCTAVE_ID, BabelIds::ROW_OFFSET_ID,
                      BabelIds::SHOW_NON_SCALE_ID, BabelIds::LAYOUT_ID,
                      BabelIds::GRID_COLS_ID,
                      BabelIds::GRID_ROWS_ID })
        audioProcessor.getAPVTS().removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

void BabelAudioProcessorEditor::timerCallback()
{
    presetDisplay.setButtonText (presetManager.getCurrentPresetName());

    if (audioProcessor.isRecording())
        recButton.setRecState (2);
    else if (audioProcessor.isArmed())
        recButton.setRecState (1);
    else
        recButton.setRecState (0);
    exportButton.setEnabled (audioProcessor.hasMidiTake());
}

void BabelAudioProcessorEditor::onRecClicked()
{
    if (audioProcessor.isRecording() || audioProcessor.isArmed())
    {
        audioProcessor.stopMidiTake();
        return;
    }
    if (audioProcessor.getPlayHead() == nullptr)
    {
        showTakeDialog (">> RECORD", "No host transport available.\nTakes need a playing host.", {});
        return;
    }
    audioProcessor.armMidiTake();
}

juce::File BabelAudioProcessorEditor::exportTakeToFolder()
{
    if (! audioProcessor.hasMidiTake())
    {
        showTakeDialog (">> EXPORT", "No take recorded.\nPress REC while the host plays.", {});
        return {};
    }
    juce::File dir (presetManager.getExportDirectory());
    if (! dir.isDirectory())
        dir.createDirectory();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    juce::File outFile = dir.getChildFile ("Babel-take-" + stamp + ".mid");
    if (! audioProcessor.writeMidiTake (outFile))
    {
        showTakeDialog (">> EXPORT", "Failed to write the MIDI file.\nCheck disk space / permissions.", {});
        return {};
    }
    return outFile;
}

void BabelAudioProcessorEditor::onExportClicked()
{
    const juce::File outFile = exportTakeToFolder();
    if (outFile != juce::File{})
        showTakeDialog (">> EXPORT", "Take saved.", outFile);
}

void BabelAudioProcessorEditor::onExportDragged()
{
    const juce::File outFile = exportTakeToFolder();
    if (outFile != juce::File{})
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { outFile.getFullPathName() }, false, &exportButton);
}

void BabelAudioProcessorEditor::showTakeDialog (const juce::String& title,
                                                const juce::String& message,
                                                const juce::File& path)
{
    auto* dialog = new BabelCardDialog (title, message, "CLOSE",
                                        juce::String(), path, uiScale);
    BabelDialogs::openWindow (dialog, title, this,
                              juce::roundToInt (460.0f * uiScale),
                              juce::roundToInt (240.0f * uiScale));
}

void BabelAudioProcessorEditor::parentHierarchyChanged()
{
    if (topLevelIsWindow)
        return;
    if (auto* sfw = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
    {
        topLevelIsWindow = true;
        sfw->setColour (juce::ResizableWindow::backgroundColourId, BabelColors::background);
        juce::Component::SafePointer<juce::ResizableWindow> safeSfw { sfw };
        juce::Component::SafePointer<BabelAudioProcessorEditor> safeThis { this };
        juce::MessageManager::callAsync ([safeSfw, safeThis]()
        {
            if (safeSfw == nullptr || safeThis == nullptr)
                return;
            const int w = juce::roundToInt (W * safeThis->uiScale);
            const int h = juce::roundToInt (H * safeThis->uiScale);
            if (isInStandaloneApp (safeThis.getComponent()))
            {
                if (! safeSfw->isResizable())
                    safeSfw->setResizable (true, false);
                safeSfw->setUsingNativeTitleBar (true);
                const auto frame = getNativeFrameSize (safeSfw.getComponent());
                const int outerW = juce::jmax (1, w + frame.x);
                const int outerH = juce::jmax (1, h + frame.y);
                if (auto* c = safeSfw->getConstrainer())
                    c->setSizeLimits (outerW, outerH, outerW, outerH);
                safeThis->setSize (w, h);
                safeSfw->setSize (outerW, outerH);
            }
            else
            {
                safeSfw->setUsingNativeTitleBar (true);
                safeSfw->setResizable (false, false);
                safeSfw->setSize (w, h);
            }
        });
       #if JUCE_WINDOWS
        if (auto* peer = getPeer())
            peer->setCustomPlatformScaleFactor (1.0f);
       #endif
    }
}

void BabelAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (BabelColors::background);
}

void BabelAudioProcessorEditor::resized()
{
    layoutControls (getWidth(), getHeight());
}

void BabelAudioProcessorEditor::applyZoom (float scale)
{
    scale = juce::jlimit (BabelIds::ZOOM_PERCENTS.front() / 100.0f,
                          BabelIds::ZOOM_PERCENTS.back() / 100.0f, scale);
    uiScale = scale;
    customLookAndFeel.setScale (uiScale);

    const int pixW = juce::roundToInt (W * uiScale);
    const int pixH = juce::roundToInt (H * uiScale);

    const auto frame = getNativeFrameSize (isInStandaloneApp (this)
                                           ? getTopLevelComponent() : nullptr);
    const int outerW = juce::jmax (1, pixW + frame.x);
    const int outerH = juce::jmax (1, pixH + frame.y);

    if (isInStandaloneApp (this))
        if (auto* tl = getTopLevelComponent())
            if (tl != this)
                if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tl))
                    if (auto* c = rw->getConstrainer())
                        c->setSizeLimits (outerW, outerH, outerW, outerH);

    if (isInStandaloneApp (this))
        setResizeLimits (pixW, pixH, pixW, pixH);
    else
        setResizeLimits (juce::roundToInt (W * 0.75f), juce::roundToInt (H * 0.75f),
                         juce::roundToInt (W * 3.0f), juce::roundToInt (H * 3.0f));
    setResizable (false, false);

    setSize (pixW, pixH);

    if (isInStandaloneApp (this))
        if (auto* tl = getTopLevelComponent())
            if (tl != this)
                tl->setSize (outerW, outerH);

    resized();
    repaint();
}

void BabelAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float)
{
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<BabelAudioProcessorEditor> (this),
                                      parameterID]()
    {
        if (safeThis == nullptr)
            return;
        if (parameterID == BabelIds::UI_SCALE_ID)
        {
            auto* scaleParam = safeThis->audioProcessor.getAPVTS().getParameter (BabelIds::UI_SCALE_ID);
            if (scaleParam == nullptr)
                return;
            const int idx = juce::roundToInt (scaleParam->getValue() * (BabelIds::ZOOM_PERCENTS.size() - 1));
            const int clamped = juce::jlimit (0, (int) BabelIds::ZOOM_PERCENTS.size() - 1, idx);
            safeThis->applyZoom (BabelIds::ZOOM_PERCENTS[(size_t) clamped] / 100.0f);
        }
        else
        {
            safeThis->padGrid.repaint();
        }
    });
}

int BabelAudioProcessorEditor::uiScaleIndex() const
{
    int best = BabelIds::UI_SCALE_DEFAULT;
    float bestDiff = std::numeric_limits<float>::max();
    for (size_t i = 0; i < BabelIds::ZOOM_PERCENTS.size(); ++i)
    {
        const float diff = std::fabs (BabelIds::ZOOM_PERCENTS[i] / 100.0f - uiScale);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = (int) i;
        }
    }
    return best;
}

void BabelAudioProcessorEditor::layoutControls (int w, int h)
{
    screenContent.setBounds (0, 0, w, h);
    crtOverlay.setBounds (0, 0, w, h);

    if (crtEnabled)
    {
        const float kFrameSize = CRTScreen::getFrameSize();
        const float scale = 1.0f / (1.0f + 2.0f * kFrameSize);
        const float tx = (float) w * 0.5f * (1.0f - scale);
        const float ty = (float) h * 0.5f * (1.0f - scale);
        screenContent.setTransform (
            juce::AffineTransform::scale (scale, scale).translated (tx, ty));
    }
    else
    {
        screenContent.setTransform (juce::AffineTransform());
    }

    const float s = (float) w / (float) W;
    const Metrics m (s);
    auto sc = [&m] (float v) { return m.sc (v); };

    {
        const int controlH = sc (28.0f);
        const int controlGap = sc (9.0f);
        const int menuW = sc (80.0f);
        const int recW = sc (40.0f);
        const int exportW = sc (40.0f);
        const int maxPresetW = sc (220.0f);
        const int totalW = recW + controlGap + exportW + controlGap
                         + maxPresetW + controlGap + menuW;
        int x = (w - totalW) / 2;
        const int y = (m.TOP_BAR_H - controlH) / 2;
        recButton.setBounds (x, y, recW, controlH);
        x += recW + controlGap;
        exportButton.setBounds (x, y, exportW, controlH);
        x += exportW + controlGap;
        presetDisplay.setBounds (x, y, maxPresetW, controlH);
        x += maxPresetW + controlGap;
        menuButton.setBounds (x, y, menuW, controlH);
    }

    const int margin = sc (15.0f);
    const int leftW = sc (250.0f);
    const int gap = sc (14.0f);
    const int bodyY = m.BODY_TOP;
    const int bodyH = h - bodyY - sc (38.0f); // status bar
    const int padX = margin + leftW + gap;
    const int padW = w - padX - margin;

    const int labelH = sc (18.0f);
    const int labelLift = sc (2.0f);
    const int sliderLabelDrop = sc (9.0f);
    const int comboH = sc (32.0f);
    const int sliderH = sc (34.0f);
    const int toggleH = sc (28.0f);

    const int comboRowH = labelH + comboH;
    const int sliderRowH = labelH + sliderH;
    const int sliderGap = sc (4.0f);
    const int numSpreadGaps = 5;
    const int toggleTopPad = sc (14.0f);
    const int toggleGap = sc (4.0f);
    const int leftContentH = 4 * comboRowH + 4 * sliderRowH + 2 * toggleH
                           + 3 * sliderGap + toggleTopPad + toggleGap;
    const int leftGap = juce::jmax (sc (2.0f),
        (bodyH - leftContentH) / juce::jmax (1, numSpreadGaps));

    int y = bodyY;
    auto placeCombo = [&] (juce::Label& l, HoverableComboBox& b)
    {
        l.setBounds (margin, y - labelLift, leftW, labelH);
        b.setBounds (margin, y + labelH, leftW, comboH);
        y += comboRowH + leftGap;
    };
    placeCombo (rootLabel, rootCombo);
    placeCombo (scaleLabel, scaleCombo);
    placeCombo (rowOffsetLabel, rowOffsetCombo);
    placeCombo (layoutLabel, layoutCombo);

    auto placeSlider = [&] (juce::Label& l, juce::Slider& sl, bool tight)
    {
        l.setBounds (margin, y + sliderLabelDrop - labelLift, leftW, labelH);
        sl.setBounds (margin, y + labelH, leftW, sliderH);
        y += sliderRowH + (tight ? sliderGap : leftGap);
    };
    placeSlider (octaveLabel, octaveSlider, true);
    placeSlider (channelLabel, channelSlider, true);
    placeSlider (colsLabel, colsSlider, true);
    placeSlider (rowsLabel, rowsSlider, true);

    y += toggleTopPad;
    showNonScaleButton.setBounds (margin, y, leftW, toggleH);
    y += toggleH + toggleGap;
    velFromYButton.setBounds (margin, y, leftW, toggleH);
    y += toggleH + leftGap;

    const int ccStripH = sc (96.0f);
    padGrid.setBounds (padX, bodyY, padW, bodyH - ccStripH - sc (8.0f));
    int cy = bodyY + bodyH - ccStripH;
    const int ccW = (padW - 2 * gap) / 3;
    const int ccX0 = padX, ccX1 = padX + ccW + gap, ccX2 = padX + 2 * (ccW + gap);
    velocityLabel.setBounds (ccX0, cy + sliderLabelDrop - labelLift, ccW, labelH);
    velocitySlider.setBounds (ccX0, cy + labelH, ccW, sliderH);
    legatoButton.setBounds (ccX0, cy + labelH + sliderH + sc (2.0f), ccW, toggleH);
    xCcLabel.setBounds (ccX1, cy + sliderLabelDrop - labelLift, ccW, labelH);
    xCcNumSlider.setBounds (ccX1, cy + labelH, ccW, sliderH);
    xCcEnabledButton.setBounds (ccX1, cy + labelH + sliderH + sc (2.0f), ccW, toggleH);
    yCcLabel.setBounds (ccX2, cy + sliderLabelDrop - labelLift, ccW, labelH);
    yCcNumSlider.setBounds (ccX2, cy + labelH, ccW, sliderH);
    yCcEnabledButton.setBounds (ccX2, cy + labelH + sliderH + sc (2.0f), ccW, toggleH);

    statusLabel.setBounds (margin, h - sc (30.0f), w - 2 * margin, sc (24.0f));
    statusLabel.setFont (CustomLookAndFeel::makeFont (18.0f * uiScale));
}

void BabelAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &menuButton)
        showHamburgerMenu();
    else if (button == &presetDisplay)
        showPresetMenu();
    else if (button == &recButton)
        onRecClicked();
    else if (button == &exportButton)
        onExportClicked();
}

void BabelAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &exportButton)
        exportDragActive = true;
}

void BabelAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent == &exportButton && exportDragActive
        && e.mouseWasDraggedSinceMouseDown())
    {
        exportDragActive = false;
        onExportDragged();
    }
}

void BabelAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == &exportButton)
        exportDragActive = false;
}

void BabelAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    if (presetManager.getNumberOfPresets() == 0)
        menu.addItem (1, "(no presets found)");
    else
        addPresetLevel (menu, juce::File (presetManager.getPresetDirectory()));

    menu.addSeparator();
    menu.addItem (kLoadFromFileId, "Load From File...");

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&presetDisplay),
        [this] (int result)
        {
            if (result == kLoadFromFileId)
                loadPresetFileDialog();
            else if (result > 0)
                if (presetManager.loadPreset (result - 1))
                    updatePresetDisplay();
        });
}

void BabelAudioProcessorEditor::loadPresetFileDialog()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Preset File",
        juce::File (presetManager.getPresetDirectory()),
        "*" + juce::String (BABEL_PRESET_EXTENSION));

    juce::Component::SafePointer<BabelAudioProcessorEditor> safeThis { this };
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            const auto result = fc.getResult();
            if (result.existsAsFile()
                && safeThis->presetManager.loadPresetFile (result))
                safeThis->updatePresetDisplay();
        });
}

void BabelAudioProcessorEditor::addPresetLevel (juce::PopupMenu& parent, const juce::File& dir)
{
    const int n = presetManager.getNumberOfPresets();
    juce::Array<juce::File> subdirs;
    for (int i = 0; i < n; ++i)
    {
        const auto f = presetManager.getPresetFile (i);
        if (! f.isAChildOf (dir))
            continue;
        if (f.getParentDirectory() == dir)
            parent.addItem (i + 1, presetManager.getPresetName (i));
        else
        {
            auto child = f.getParentDirectory();
            while (child.getParentDirectory() != dir)
                child = child.getParentDirectory();
            if (! subdirs.contains (child))
                subdirs.add (child);
        }
    }
    for (auto& sub : subdirs)
    {
        juce::PopupMenu sm;
        addPresetLevel (sm, sub);
        parent.addSubMenu (">> " + sub.getFileName(), sm);
    }
}

enum HamburgerMenuOption
{
    None = 0,
    Init,
    Save,
    SaveAs,
    LoadFromFile,
    SetPresetFolder,
    ResetPresetFolder,
    SetExportFolder,
    ResetExportFolder,
    CrtEnabled,
    CrtStrengthLow,
    CrtStrengthMedium,
    CrtStrengthHigh,
    StandaloneAudioSettings,
    StandaloneSaveState,
    StandaloneLoadState,
    StandaloneReset,
    About,
    ZoomBase = 0x1000
};

static bool isInStandaloneApp (const juce::Component* c)
{
#if JucePlugin_Build_Standalone
    return c != nullptr
        && dynamic_cast<const juce::BabelFilterWindow*> (c->getTopLevelComponent()) != nullptr;
#else
    return false;
#endif
}

void BabelAudioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;
    menu.addItem (HamburgerMenuOption::Init, "Init");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::Save, "Save");
    menu.addItem (HamburgerMenuOption::SaveAs, "Save As...");
    menu.addItem (HamburgerMenuOption::LoadFromFile, "Load From File...");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::SetPresetFolder, "Set Preset Folder");
    menu.addItem (HamburgerMenuOption::ResetPresetFolder, "Reset Preset Folder");
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::SetExportFolder, "Set Export Folder");
    menu.addItem (HamburgerMenuOption::ResetExportFolder, "Reset Export Folder");
    menu.addSeparator();
    {
        juce::PopupMenu crtSub;
        crtSub.addItem (HamburgerMenuOption::CrtEnabled,
                        juce::String ("CRT Enabled - ") + (crtEnabled ? "[X]" : "[ ]"));
        juce::PopupMenu strengthSub;
        const int strength = presetManager.getCrtStrength();
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthLow, "Low", true, strength == 0);
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthMedium, "Medium", true, strength == 1);
        strengthSub.addItem (HamburgerMenuOption::CrtStrengthHigh, "High", true, strength == 2);
        crtSub.addSubMenu (">> Strength", strengthSub);
        menu.addSubMenu (">> CRT Layout", crtSub);
    }
    {
        juce::PopupMenu zoomMenu;
        const int currentIndex = uiScaleIndex();
        for (size_t i = 0; i < BabelIds::ZOOM_PERCENTS.size(); ++i)
            zoomMenu.addItem (HamburgerMenuOption::ZoomBase + (int) i,
                              juce::String ((int) BabelIds::ZOOM_PERCENTS[i]) + "%",
                              true, i == (size_t) currentIndex);
        menu.addSubMenu (">> Zoom", zoomMenu);
    }
    menu.addSeparator();
    menu.addItem (HamburgerMenuOption::About, "About");

    if (isInStandaloneApp (this))
    {
        menu.addSeparator();
        juce::PopupMenu standaloneSub;
        standaloneSub.addItem (HamburgerMenuOption::StandaloneAudioSettings, "Audio/MIDI Settings...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneSaveState, "Save State...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneLoadState, "Load State...");
        standaloneSub.addItem (HamburgerMenuOption::StandaloneReset, "Reset to Default");
        menu.addSubMenu (">> Standalone", standaloneSub);
    }

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&menuButton),
        [this] (int result) { handleMenuResult (result); });
}

void BabelAudioProcessorEditor::handleMenuResult (int selectedId)
{
    if (selectedId >= HamburgerMenuOption::ZoomBase
        && selectedId < HamburgerMenuOption::ZoomBase + (int) BabelIds::ZOOM_PERCENTS.size())
    {
        const int idx = selectedId - HamburgerMenuOption::ZoomBase;
        if (auto* scaleParam = audioProcessor.getAPVTS().getParameter (BabelIds::UI_SCALE_ID))
        {
            const float norm = (float) idx / (float) (BabelIds::ZOOM_PERCENTS.size() - 1);
            scaleParam->setValueNotifyingHost (norm);
        }
        applyZoom (BabelIds::ZOOM_PERCENTS[(size_t) juce::jlimit (0, (int) BabelIds::ZOOM_PERCENTS.size() - 1, idx)] / 100.0f);
        return;
    }

    switch (selectedId)
    {
        case HamburgerMenuOption::None: break;
        case HamburgerMenuOption::Init: displayInitPopup(); break;
        case HamburgerMenuOption::Save: presetManager.savePreset(); updatePresetDisplay(); break;
        case HamburgerMenuOption::SaveAs: displaySaveAsPopup(); break;
        case HamburgerMenuOption::LoadFromFile: loadPresetFileDialog(); break;
        case HamburgerMenuOption::SetPresetFolder:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Select Preset Folder",
                juce::File (presetManager.getPresetDirectory()), "*", true, false, this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser] (const juce::FileChooser& c)
                {
                    const auto result = c.getResult();
                    if (result != juce::File{})
                        presetManager.setPresetDirectory (result.getFullPathName());
                });
            break;
        }
        case HamburgerMenuOption::ResetPresetFolder:
            presetManager.resetPresetDirectoryToDefault();
            updatePresetDisplay();
            break;
        case HamburgerMenuOption::SetExportFolder:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Select Export Folder",
                juce::File (presetManager.getExportDirectory()), "*", true, false, this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser] (const juce::FileChooser& c)
                {
                    const auto result = c.getResult();
                    if (result != juce::File{})
                        presetManager.setExportDirectory (result.getFullPathName());
                });
            break;
        }
        case HamburgerMenuOption::ResetExportFolder:
            presetManager.resetExportDirectoryToDefault();
            break;
        case HamburgerMenuOption::CrtEnabled:
        {
            const bool newState = ! crtEnabled.load();
            crtEnabled.store (newState);
            presetManager.setCrtEnabled (newState);
            resized();
            break;
        }
        case HamburgerMenuOption::CrtStrengthLow:
        case HamburgerMenuOption::CrtStrengthMedium:
        case HamburgerMenuOption::CrtStrengthHigh:
        {
            const int strength = selectedId - HamburgerMenuOption::CrtStrengthLow;
            presetManager.setCrtStrength (strength);
            crtOverlay.setCrtStrength (strength);
            break;
        }
        case HamburgerMenuOption::StandaloneAudioSettings:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::BabelFilterWindow*> (tl))
                    new SettingsWindow (sfw->getPluginHolder()->deviceManager, uiScale);
#endif
            break;
        case HamburgerMenuOption::StandaloneSaveState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::BabelFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToSaveState();
#endif
            break;
        case HamburgerMenuOption::StandaloneLoadState:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::BabelFilterWindow*> (tl))
                    sfw->getPluginHolder()->askUserToLoadState();
#endif
            break;
        case HamburgerMenuOption::StandaloneReset:
#if JucePlugin_Build_Standalone
            if (auto* tl = getTopLevelComponent())
                if (auto* sfw = dynamic_cast<juce::BabelFilterWindow*> (tl))
                {
                    juce::Component::SafePointer<juce::BabelFilterWindow> safeSfw { sfw };
                    juce::MessageManager::callAsync ([safeSfw]
                    {
                        if (safeSfw != nullptr)
                            safeSfw->resetToDefaultState();
                    });
                }
#endif
            break;
        case HamburgerMenuOption::About:
            displayAboutPopup();
            break;
        default:
            break;
    }
}

void BabelAudioProcessorEditor::displayInitPopup()
{
    auto* dialog = new ConfirmDialog (">> INIT",
                                      "Are you sure you want to initialize this preset?",
                                      uiScale);
    dialog->onConfirm = [this]
    {
        presetManager.createNewPreset();
        updatePresetDisplay();
    };
    BabelDialogs::openWindow (dialog, "Init", this,
                              juce::roundToInt (460.0f * uiScale),
                              juce::roundToInt (220.0f * uiScale));
}

void BabelAudioProcessorEditor::displaySaveAsPopup()
{
    auto* dialog = new SaveAsDialog (presetManager.getCurrentPresetName(), uiScale);
    dialog->onConfirm = [this] (const juce::String& name)
    {
        if (name.isNotEmpty())
        {
            presetManager.saveAsPreset (name);
            updatePresetDisplay();
        }
    };
    BabelDialogs::openWindow (dialog, "Save Preset", this,
                              juce::roundToInt (460.0f * uiScale),
                              juce::roundToInt (220.0f * uiScale));
}

void BabelAudioProcessorEditor::displayAboutPopup()
{
    new AboutWindow (uiScale);
}

void BabelAudioProcessorEditor::updatePresetDisplay()
{
    presetDisplay.setButtonText (presetManager.getCurrentPresetName());
}
