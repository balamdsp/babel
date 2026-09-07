#include "PadGrid.h"
#include "../PluginProcessor.h"
#include "CustomLookAndFeel.h"

PadGrid::PadGrid (BabelAudioProcessor* p) : processor (p) {}

juce::Point<int> PadGrid::cellAt (juce::Point<int> pos) const
{
    if (processor == nullptr)
        return { -1, -1 };
    auto cfg = processor->getGridConfig();
    if (cfg.cols <= 0 || cfg.rows <= 0 || getWidth() <= 0 || getHeight() <= 0)
        return { -1, -1 };
    const int col = (pos.x * cfg.cols) / juce::jmax (1, getWidth());
    const int rowTop = (pos.y * cfg.rows) / juce::jmax (1, getHeight());
    if (col < 0 || col >= cfg.cols || rowTop < 0 || rowTop >= cfg.rows)
        return { -1, -1 };
    return { col, cfg.rows - 1 - rowTop }; // flip to bottom origin
}

juce::Rectangle<float> PadGrid::cellRect (int col, int rowBottom) const
{
    if (processor == nullptr)
        return {};
    auto cfg = processor->getGridConfig();
    const float cw = (float) getWidth() / (float) juce::jmax (1, cfg.cols);
    const float ch = (float) getHeight() / (float) juce::jmax (1, cfg.rows);
    const int rowTop = cfg.rows - 1 - rowBottom;
    return { col * cw + 1.0f, rowTop * ch + 1.0f, cw - 2.0f, ch - 2.0f };
}

void PadGrid::triggerCell (int col, int rowBottom, bool isDrag)
{
    if (processor == nullptr)
        return;
    auto cfg = processor->getGridConfig();
    const int midi = babel::gridToMidi (cfg, col, rowBottom);
    if (midi < 0)
        return;
    // Chromatic + hidden non-scale: pad is unlit and unplayable (APC-style).
    if (cfg.layoutMode == babel::LayoutMode::Chromatic && ! cfg.showNonScale)
    {
        const auto& all = babel::scales();
        if (cfg.scaleIndex >= 0 && cfg.scaleIndex < (int) all.size()
            && ! babel::isScaleNote (all[(size_t) cfg.scaleIndex], cfg.root, midi))
            return;
    }
    if (isDrag && col == activeCol && rowBottom == activeRow)
        return;
    activeCol = col;
    activeRow = rowBottom;
    activeNote = midi;
    lastNote = midi;
    lastXNorm = cfg.cols <= 1 ? 0.5f : (float) col / (float) (cfg.cols - 1);
    lastYNorm = cfg.rows <= 1 ? 0.5f : (float) rowBottom / (float) (cfg.rows - 1);

    BabelGesture g;
    g.type = BabelGesture::Type::NoteOn;
    g.note = midi;
    g.velocity = processor->getVelocityForRow (rowBottom, cfg.rows);
    g.xNorm = lastXNorm;
    g.yNorm = lastYNorm;
    processor->pushGesture (g);
    if (onGesture)
        onGesture();
    repaint();
}

void PadGrid::sendCCs (juce::Point<int> pos)
{
    if (processor == nullptr)
        return;
    BabelGesture g;
    g.type = BabelGesture::Type::PitchCCs;
    g.xNorm = juce::jlimit (0.0f, 1.0f, (float) pos.x / (float) juce::jmax (1, getWidth()));
    g.yNorm = juce::jlimit (0.0f, 1.0f, 1.0f - (float) pos.y / (float) juce::jmax (1, getHeight()));
    lastXNorm = g.xNorm;
    lastYNorm = g.yNorm;
    processor->pushGesture (g);
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto cell = cellAt (e.getPosition());
    if (cell.x >= 0)
        triggerCell (cell.x, cell.y, false);
    else
        sendCCs (e.getPosition());
}

void PadGrid::mouseDrag (const juce::MouseEvent& e)
{
    const auto cell = cellAt (e.getPosition());
    if (cell.x < 0)
    {
        sendCCs (e.getPosition());
        return;
    }
    // Same pad: note already sounds, keep CCs tracking the finger.
    if (cell.x == activeCol && cell.y == activeRow)
    {
        sendCCs (e.getPosition());
        return;
    }
    triggerCell (cell.x, cell.y, true);
}

void PadGrid::mouseUp (const juce::MouseEvent&)
{
    if (processor == nullptr)
        return;
    activeCol = activeRow = -1;
    activeNote = -1;
    BabelGesture g;
    g.type = BabelGesture::Type::NoteOff;
    processor->pushGesture (g);
    if (onGesture)
        onGesture();
    repaint();
}

void PadGrid::resized() {}

void PadGrid::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (BabelColors::background);

    if (processor == nullptr)
        return;
    auto cfg = processor->getGridConfig();
    const auto& all = babel::scales();
    if (cfg.scaleIndex < 0 || cfg.scaleIndex >= (int) all.size())
        return;
    const auto& scale = all[(size_t) cfg.scaleIndex];
    const int rowSteps = babel::rowOffsetSteps (cfg.rowOffsetIndex,
                                                (int) scale.intervals.size());

    const Colour rootCol = BabelColors::highlight; // pale ice-blue for root
    const Colour scaleCol = BabelColors::accent; // bright blue for scale notes
    const Colour activeColG = BabelColors::textBrand; // pale blue for sounding pad
    const Colour activeTextCol = BabelColors::background; // dark navy on pale fill
    const Colour bgCell = BabelColors::card;

    const bool chromatic = (cfg.layoutMode == babel::LayoutMode::Chromatic);

    for (int r = 0; r < cfg.rows; ++r)
    {
        for (int c = 0; c < cfg.cols; ++c)
        {
            const int midi = babel::gridToMidi (cfg, c, r);
            if (midi < 0)
                continue;
            bool isRoot, inScale;
            if (chromatic)
            {
                isRoot = babel::isRootNote (cfg.root, midi);
                inScale = babel::isScaleNote (scale, cfg.root, midi);
            }
            else
            {
                const int degree = c + r * rowSteps;
                isRoot = babel::isRootDegree (scale, degree);
                inScale = true;
            }
            const bool isHidden = chromatic && ! inScale && ! cfg.showNonScale;
            const bool isActive = (c == activeCol && r == activeRow) && ! isHidden;
            auto rc = cellRect (c, r);

            g.setColour (bgCell);
            g.fillRoundedRectangle (rc, 3.0f);

            if (isHidden)
            {
                g.setColour (BabelColors::buttonBorder.withAlpha (0.35f));
                g.drawRoundedRectangle (rc, 3.0f, 1.0f);
                continue;
            }

            if (isActive)
            {
                g.setColour (activeColG);
                g.fillRoundedRectangle (rc, 3.0f);
                g.setColour (activeTextCol);
            }
            else if (isRoot)
                g.setColour (rootCol);
            else if (! inScale)
                g.setColour (scaleCol.withAlpha (0.30f));
            else
                g.setColour (cfg.showNonScale ? scaleCol.withAlpha (0.85f)
                                              : scaleCol.withAlpha (0.30f));

            g.drawRoundedRectangle (rc, 3.0f, isRoot || isActive ? 2.0f : 1.0f);

            if (isRoot || cfg.showNonScale || ! chromatic)
            {
                static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                                 "F#", "G", "G#", "A", "A#", "B" };
                g.setFont (CustomLookAndFeel::makeFont (
                    juce::jlimit (11.0f, 20.0f, rc.getHeight() * 0.34f)));
                g.setColour (isActive ? activeTextCol
                             : isRoot ? rootCol : scaleCol.withAlpha (0.7f));
                g.drawText (String (names[midi % 12]) + String (midi / 12 - 1),
                            rc, Justification::centred, false);
            }
        }
    }

    g.setColour (BabelColors::buttonBorder);
    g.drawRect (getLocalBounds(), 1);
}
