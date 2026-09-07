#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

// Function-local static: one L&F instance shared by every dialog lifetime.
// Component::setLookAndFeel does NOT take ownership -- never `new` one here.
inline CustomLookAndFeel& babelDialogLnf()
{
    static CustomLookAndFeel lnf;
    return lnf;
}

inline void paintCardLayers (juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    g.setColour (BabelColors::cardDark);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (BabelColors::accent.withAlpha (0.40f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto inner = bounds.reduced (12.0f);
    g.setColour (BabelColors::card);
    g.fillRoundedRectangle (inner, 4.0f);
    g.setColour (BabelColors::accent.withAlpha (0.10f));
    g.drawRoundedRectangle (inner, 4.0f, 1.0f);
}

class BabelCardDialog : public juce::Component
{
public:
    BabelCardDialog (const juce::String& title,
                        const juce::String& message,
                        const juce::String& confirmText,
                        const juce::String& cancelText = {},
                        const juce::File& pathToShow = {},
                        float scale = 1.0f)
    {
        uiScale = scale;
        babelDialogLnf().setScale (uiScale);
        setLookAndFeel (&babelDialogLnf());

        titleLabel.setText (title, juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f * uiScale));
        titleLabel.setColour (juce::Label::textColourId,
                              BabelColors::textPrimary.withAlpha (0.50f));
        addAndMakeVisible (titleLabel);

        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setFont (CustomLookAndFeel::makeFont (22.0f * uiScale));
        messageLabel.setColour (juce::Label::textColourId,
                                BabelColors::textPrimary);
        messageLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (messageLabel);

        if (! pathToShow.getFullPathName().isEmpty())
        {
            pathEditor.setReadOnly (true);
            pathEditor.setMultiLine (false, false);
            pathEditor.setReturnKeyStartsNewLine (false);
            pathEditor.setFont (CustomLookAndFeel::makeFont (20.0f * uiScale));
            pathEditor.setText (pathToShow.getFullPathName());
            addAndMakeVisible (pathEditor);
        }

        if (cancelText.isNotEmpty())
        {
            cancelButton.setButtonText (cancelText);
            cancelButton.onClick = [this] { closeWindow(); };
            addAndMakeVisible (cancelButton);
        }

        confirmButton.setButtonText (confirmText);
        confirmButton.onClick = [this]
        {
            if (onConfirm)
                onConfirm();
            closeWindow();
        };
        addAndMakeVisible (confirmButton);
    }

    ~BabelCardDialog() override
    {
        setLookAndFeel (nullptr);
    }

    std::function<void()> onConfirm;

    void paint (juce::Graphics& g) override
    {
        paintCardLayers (g, getLocalBounds().toFloat());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * uiScale));

        titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * uiScale)));
        area.removeFromTop (juce::roundToInt (5.0f * uiScale));

        const int rowGap = juce::roundToInt (10.0f * uiScale);
        auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * uiScale));
        area.removeFromBottom (rowGap);

        const int cancelW = cancelButton.isVisible() ? juce::roundToInt (150.0f * uiScale) : 0;
        if (cancelButton.isVisible())
        {
            cancelButton.setBounds (buttonsRow.removeFromRight (cancelW));
            buttonsRow.removeFromRight (rowGap);
        }
        confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * uiScale)));

        if (pathEditor.isVisible())
        {
            pathEditor.setBounds (area.removeFromBottom (juce::roundToInt (32.0f * uiScale)));
            area.removeFromBottom (rowGap);
        }

        messageLabel.setBounds (area);
    }

private:
    void closeWindow()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->closeButtonPressed();
    }

    static constexpr int kTitleInsetPx = 5;
    static constexpr int kTitleHeightPx = 16;

    float uiScale = 1.0f;

    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextEditor pathEditor;
    juce::TextButton confirmButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabelCardDialog)
};

class BabelDialogWindow : public juce::DocumentWindow
{
public:
    BabelDialogWindow (const juce::String& title, juce::Component* content)
        : juce::DocumentWindow (title, juce::Colour (0xff0c0c0c), allButtons)
    {
        setContentOwned (content, true);
        setResizable (false, false);
    }

    // Empty body in JUCE 9 -- without this override nothing closes.
    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabelDialogWindow)
};

namespace BabelDialogs
{
    inline void openWindow (juce::Component* ownedContent,
                            const juce::String& title,
                            juce::Component* centreOn,
                            int w, int h)
    {
        auto* win = new BabelDialogWindow (title, ownedContent);

        if (centreOn != nullptr && centreOn->getPeer() != nullptr)
        {
            const auto centre = centreOn->getScreenBounds().getCentre();
            win->setTopLeftPosition (centre.getX() - w / 2, centre.getY() - h / 2);
            win->setSize (w, h);
        }
        else
        {
            win->centreWithSize (w, h);
        }

        win->setVisible (true);
    }
}

class AboutWindow : public juce::DocumentWindow
{
public:
    AboutWindow (float scale = 1.0f)
        : juce::DocumentWindow ("About Babel",
                                juce::Colour (0xff0c0c0c),
                                allButtons)
    {
        babelDialogLnf().setScale (scale);

        const juce::String message =
            "Version " + juce::String (ProjectInfo::versionString)
            + "\n\nA MIDI grid controller"
            + "\nBy BalamDSP"
            + "\n\nBased on:"
            + "\n  JUCE framework    -- JUCE Ltd (AGPL)"
            + "\n  cool-retro-term   -- Swordfish90 (GPL)"
            + "\n  VT323 typeface    -- Peter Hull (OFL)";

        setContentOwned (
            new BabelCardDialog (">> ABOUT BABEL", message, "CLOSE", {}, {}, scale),
            true);
        setResizable (false, false);

        centreWithSize (juce::roundToInt (460.0f * scale),
                        juce::roundToInt (380.0f * scale));
        setVisible (true);
    }

    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWindow)
};
