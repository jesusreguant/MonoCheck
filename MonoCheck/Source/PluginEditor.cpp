#include "PluginEditor.h"

namespace
{
    constexpr float meterRangeDb = 20.0f;   // el medidor muestra de -20 a 0 dBFS
    constexpr int   numSegments  = 40;      // 0,5 dB por segmento

    const juce::Colour colGreen { 0xff22c55e };
    const juce::Colour colAmber { 0xfffbbf24 };
    const juce::Colour colRed   { 0xffef4444 };
}

MonoCheckEditor::MonoCheckEditor (MonoCheckProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Ventana escalable manteniendo la proporción (de 67 % a 267 %)
    setResizeLimits (160, 240, 640, 960);
    getConstrainer()->setFixedAspectRatio (designW / designH);
    setResizable (true, true);
    setSize (240, 360);

    startTimerHz (30);
}

MonoCheckEditor::~MonoCheckEditor()
{
    stopTimer();
}

void MonoCheckEditor::updateHold (float& hold, int& timer, float db)
{
    if (db >= hold)
    {
        hold = db;
        timer = 30;                                   // retiene el pico ~1 s
    }
    else if (timer > 0)
    {
        --timer;
    }
    else
    {
        hold = juce::jmax (db, hold - 0.7f);          // luego cae ~20 dB/s
    }
}

void MonoCheckEditor::timerCallback()
{
    const bool signal = processor.hasSignal.load();

    const float dbL = signal ? juce::Decibels::gainToDecibels (processor.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (processor.peakR.load(), -100.0f) : -100.0f;

    updateHold (holdDbL, holdTimerL, dbL);
    updateHold (holdDbR, holdTimerR, dbR);

    repaint();
}

void MonoCheckEditor::mouseDown (const juce::MouseEvent& e)
{
    // Clic sobre el cuadro de detección = Reset
    if (getPanelBounds().contains (e.position / getScale()))
    {
        processor.resetLatch();
        repaint();
    }
}

void MonoCheckEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool overPanel = getPanelBounds().contains (e.position / getScale());
    setMouseCursor (overPanel ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor);
}

void MonoCheckEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float peakDb, float holdDb)
{
    // Pista de fondo
    g.setColour (juce::Colour (0xff1a1a1f));
    g.fillRoundedRectangle (r.expanded (3.0f), 6.0f);

    const float segH = r.getHeight() / (float) numSegments;
    const float gap  = juce::jmax (1.0f, segH * 0.22f);
    const float stepDb = meterRangeDb / (float) numSegments;

    const int holdIndex = holdDb > -meterRangeDb
                            ? juce::jlimit (0, numSegments - 1, (int) ((holdDb + meterRangeDb) / stepDb))
                            : -1;

    for (int i = 0; i < numSegments; ++i)
    {
        const float lowDb = -meterRangeDb + (float) i * stepDb;

        const juce::Colour base = lowDb >= -3.0f ? colRed
                                : lowDb >= -9.0f ? colAmber
                                                 : colGreen;

        const bool lit = peakDb > lowDb;

        const juce::Rectangle<float> seg (r.getX(),
                                          r.getBottom() - (float) (i + 1) * segH + gap * 0.5f,
                                          r.getWidth(),
                                          segH - gap);

        if (i == holdIndex)
            g.setColour (juce::Colours::white.withAlpha (0.9f));   // marca de pico retenido
        else
            g.setColour (lit ? base : base.withAlpha (0.10f));

        g.fillRoundedRectangle (seg, 1.5f);
    }
}

void MonoCheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff131316));
    g.addTransform (juce::AffineTransform::scale (getScale()));

    const bool signal  = processor.hasSignal.load();
    const bool latched = processor.monoLatched.load();

    // Rojo = mono retenido; verde = estéreo; negro = sin audio
    juce::Colour colour      { 0xff08080a };
    juce::Colour labelColour = juce::Colours::white.withAlpha (0.55f);
    juce::String label       = "No audio";
    bool colored = false;

    if (latched)
    {
        colour = colRed;
        label = "MONO";
        colored = true;
    }
    else if (signal && processor.currentState.load() == MonoCheckProcessor::Stereo)
    {
        colour = colGreen;
        label = "STEREO";
        colored = true;
    }

    if (colored)
        labelColour = juce::Colours::white;

    // Cuadro de detección (alargado, paralelo a los medidores)
    const auto panel = getPanelBounds();

    if (colored)
    {
        for (int k = 4; k >= 1; --k)                       // resplandor suave
        {
            g.setColour (colour.withAlpha (0.10f));
            g.drawRoundedRectangle (panel.expanded ((float) k * 2.0f), 14.0f + (float) k * 2.0f, 3.0f);
        }
    }

    g.setGradientFill (juce::ColourGradient (colored ? colour.brighter (0.25f) : colour, panel.getX(), panel.getY(),
                                             colored ? colour.darker (0.45f)  : colour, panel.getX(), panel.getBottom(),
                                             false));
    g.fillRoundedRectangle (panel, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (colored ? 0.35f : 0.18f));
    g.drawRoundedRectangle (panel.reduced (0.5f), 14.0f, 1.0f);

    g.setColour (labelColour);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (label, panel, juce::Justification::centred, false);

    // Picómetro vertical L / R (últimos 20 dB)
    const auto mL = getMeterBounds (0);
    const auto mR = getMeterBounds (1);

    const float dbL = signal ? juce::Decibels::gainToDecibels (processor.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (processor.peakR.load(), -100.0f) : -100.0f;

    drawMeter (g, mL, dbL, holdDbL);
    drawMeter (g, mR, dbR, holdDbR);

    // Escala en dBFS a la derecha
    g.setFont (juce::FontOptions (10.0f));
    for (float db : { 0.0f, -3.0f, -6.0f, -10.0f, -15.0f, -20.0f })
    {
        const float y = mL.getBottom() - (db + meterRangeDb) / meterRangeDb * mL.getHeight();

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (mR.getRight() + 5.0f, y, mR.getRight() + 9.0f, y, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (juce::String ((int) db),
                    juce::Rectangle<float> (mR.getRight() + 12.0f, y - 6.0f, 36.0f, 12.0f),
                    juce::Justification::centredLeft, false);
    }

    // Etiquetas L / R
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (mL.getX(), 316.0f, mL.getWidth(), 14.0f), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (mR.getX(), 316.0f, mR.getWidth(), 14.0f), juce::Justification::centred, false);

    // Ayuda cuando el mono está retenido
    if (latched)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Clic en el cuadro para resetear",
                    juce::Rectangle<float> (0.0f, 336.0f, designW, 16.0f), juce::Justification::centred, false);
    }
}
