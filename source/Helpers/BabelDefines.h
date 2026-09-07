#pragma once

#include <JuceHeader.h>

// Zoom-aware layout tokens (fixed logical canvas).

#define BABEL_PANEL_WIDTH 1000
#define BABEL_PANEL_HEIGHT 700

namespace BabelZoom
{
    constexpr float Min = 0.75f;
    constexpr float Max = 3.0f;
    constexpr float BaseW = (float) BABEL_PANEL_WIDTH;
    constexpr float BaseH = (float) BABEL_PANEL_HEIGHT;
    inline float uiScale = 1.0f;
}

struct BabelMetrics
{
    float scale = 1.0f;
    int sc (float v) const { return juce::roundToInt (v * scale); }
    float scf (float v) const { return v * scale; }
};

inline float babelScaleFor (const juce::Component& c)
{
    return juce::jlimit (BabelZoom::Min, BabelZoom::Max,
                         (float) c.getWidth() / BabelZoom::BaseW);
}

namespace BabelGUI
{
    namespace Layout
    {
        inline float MainMargin()   { return 15.0f * BabelZoom::uiScale; }
        inline float MainGap()      { return 14.0f * BabelZoom::uiScale; }
        inline float CardGap()      { return 12.0f * BabelZoom::uiScale; }
        inline float CardInset()    { return 12.0f * BabelZoom::uiScale; }
        inline float ContentInset() { return 13.0f * BabelZoom::uiScale; }
        inline float CardCorner()   { return 4.0f; }
    }

    namespace Paint
    {
        inline void drawCardOutline (juce::Graphics& g, juce::Rectangle<float> bounds,
                                     float corner, float alpha = 0.40f)
        {
            g.setColour (juce::Colour (0xD8, 0xD8, 0xD8).withAlpha (alpha));
            g.drawRoundedRectangle (bounds, corner, 1.0f);
        }
    }
}
