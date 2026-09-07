#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components/CustomLookAndFeel.h"
#include "Components/CRTScreen.h"
#include "Components/PadGrid.h"
#include "Helpers/PresetManager.h"
#include "Components/AboutWindow.h"

class SaveAsDialog : public juce::Component
{
public:
    SaveAsDialog (const juce::String& initialName, float scale = 1.0f);
    ~SaveAsDialog() override;
    std::function<void (const juce::String&)> onConfirm;
    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
private:
    void closeWindow();
    float uiScale = 1.0f;
    juce::Label titleLabel, messageLabel;
    juce::TextEditor nameEditor;
    juce::TextButton confirmButton, cancelButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveAsDialog)
};

class ConfirmDialog : public juce::Component
{
public:
    ConfirmDialog (const juce::String& titleText, const juce::String& messageText,
                   float scale = 1.0f);
    ~ConfirmDialog() override;
    std::function<void()> onConfirm;
    void paint (juce::Graphics& g) override;
    void resized() override;
private:
    void closeWindow();
    float uiScale = 1.0f;
    juce::Label titleLabel, messageLabel;
    juce::TextButton confirmButton, cancelButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfirmDialog)
};

class BabelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Button::Listener,
                                   public juce::AudioProcessorValueTreeState::Listener,
                                   private juce::Timer
{
public:
    BabelAudioProcessorEditor (BabelAudioProcessor&);
    ~BabelAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void parentHierarchyChanged() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    void applyZoom (float scale);
    void layoutControls (int w, int h);
    int uiScaleIndex() const;

    CustomLookAndFeel customLookAndFeel;
    BabelAudioProcessor& audioProcessor;
    PresetManager presetManager;

    float uiScale = 1.0f;

    struct ScreenContent : public juce::Component, private juce::Timer
    {
        ScreenContent();
        void paint (juce::Graphics& g) override;
        void timerCallback() override;
        bool cursorVisible = true;
    };

    ScreenContent screenContent;
    CRTScreen crtOverlay { &screenContent, &crtEnabled };
    std::atomic<bool> crtEnabled { true };

    class HamburgerButton : public juce::TextButton
    {
    public:
        using TextButton::TextButton;
        void paint (juce::Graphics& g) override;
    };

    // 0 idle, 1 armed, 2 rec.
    class RecButton : public juce::TextButton
    {
    public:
        using TextButton::TextButton;
        void paint (juce::Graphics& g) override;
        void setRecState (int s);
    private:
        int recState = 0;
    };

    class ExportButton : public juce::TextButton
    {
    public:
        using TextButton::TextButton;
        void paint (juce::Graphics& g) override;
    };

    juce::TextButton presetDisplay;
    HamburgerButton menuButton;
    RecButton recButton;
    ExportButton exportButton;
    bool exportDragActive = false;
    juce::Label statusLabel;

    void showPresetMenu();
    void addPresetLevel (juce::PopupMenu& parent, const juce::File& dir);
    void loadPresetFileDialog();
    void showHamburgerMenu();
    void handleMenuResult (int selectedId);
    void updatePresetDisplay();
    void displayInitPopup();
    void displaySaveAsPopup();
    void displayAboutPopup();
    void onRecClicked();
    void onExportClicked();
    void onExportDragged();
    juce::File exportTakeToFolder();
    void showTakeDialog (const juce::String& title, const juce::String& message,
                         const juce::File& path = {});
    void timerCallback() override;

    PadGrid padGrid;

    HoverableComboBox rootCombo, scaleCombo, rowOffsetCombo, layoutCombo;
    juce::Slider octaveSlider, channelSlider, colsSlider, rowsSlider;
    juce::Slider velocitySlider, xCcNumSlider, yCcNumSlider;
    juce::Label rootLabel, scaleLabel, rowOffsetLabel, layoutLabel, octaveLabel, channelLabel;
    juce::Label colsLabel, rowsLabel, velocityLabel, xCcLabel, yCcLabel;
    juce::ToggleButton showNonScaleButton, velFromYButton, xCcEnabledButton, yCcEnabledButton,
        legatoButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> rootAttachment, scaleAttachment, rowOffsetAttachment,
        layoutAttachment;
    std::unique_ptr<SliderAttachment> octaveAttachment, channelAttachment, colsAttachment,
        rowsAttachment, velocityAttachment, xCcNumAttachment, yCcNumAttachment;
    std::unique_ptr<ButtonAttachment> showNonScaleAttachment, velFromYAttachment,
        xCcEnabledAttachment, yCcEnabledAttachment, legatoAttachment;

    bool topLevelIsWindow = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabelAudioProcessorEditor)
};
