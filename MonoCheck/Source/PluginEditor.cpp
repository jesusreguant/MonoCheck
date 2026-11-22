#include "PluginEditor.h"

namespace
{
    // Escala del medidor: de -60 a 0 dBFS.
    // Tramo bajo (-60 a -20 dB): 20 segmentos de 2 dB.
    // Tramo alto (-20 a 0 dB):   40 segmentos de 0,5 dB (más resolución).
    constexpr int numLowerSegments = 20;
    constexpr int numUpperSegments = 40;
    constexpr int numSegments      = numLowerSegments + numUpperSegments;

    constexpr float yellowFromDb = -18.0f;  // a partir de aquí, amarillo
    constexpr float redFromDb    = -3.0f;   // a partir de aquí, rojo

    const juce::Colour colGreen { 0xff22c55e };
    const juce::Colour colAmber { 0xfffbbf24 };
    const juce::Colour colRed   { 0xffef4444 };

    float segmentLowDb (int i)
    {
        return i < numLowerSegments ? -60.0f + 2.0f * (float) i
                                    : -20.0f + 0.5f * (float) (i - numLowerSegments);
    }

    int dbToSegment (float db)
    {
        if (db < -20.0f)
            return juce::jlimit (0, numLowerSegments - 1, (int) ((db + 60.0f) / 2.0f));

        return juce::jlimit (numLowerSegments, numSegments - 1,
                             numLowerSegments + (int) ((db + 20.0f) / 0.5f));
    }

    // Posición vertical (0 = abajo, 1 = arriba) de un nivel en dB
    float dbToFraction (float db)
    {
        db = juce::jlimit (-60.0f, 0.0f, db);
        const float lowerShare = (float) numLowerSegments / (float) numSegments;

        if (db <= -20.0f)
            return lowerShare * (db + 60.0f) / 40.0f;

        return lowerShare + (1.0f - lowerShare) * (db + 20.0f) / 20.0f;
    }
}

MonoCheckEditor::MonoCheckEditor (MonoCheckProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Ventana escalable libremente en ancho y en alto
    setResizeLimits (160, 240, 700, 1100);
    setResizable (true, true);
    setSize (240, 384);

    startTimerHz (30);
}

MonoCheckEditor::~MonoCheckEditor()
{
    stopTimer();
}

MonoCheckEditor::Layout MonoCheckEditor::getLayout() const
{
    Layout l;

    const float W = (float) getWidth();
    const float H = (float) getHeight();
    const float u = juce::jmax (0.3f, juce::jmin (W / refW, H / refH));
    l.u = u;

    const float mx     = 20.0f * u;               // margen lateral
    const float top    = 64.0f * u;               // bajo la cabecera
    const float bottom = H - 44.0f * u;           // sobre las etiquetas L / R
    const float cw     = W - 2.0f * mx;

    const float scaleW   = 36.0f * u;             // espacio para los números de dB
    const float gap      = 18.0f * u;
    const float meterGap = 6.0f * u;

    const float remaining = juce::jmax (10.0f, cw - scaleW - gap);
    const float panelW    = remaining * 0.55f;
    const float metersW   = remaining - panelW;
    const float meterW    = juce::jmax (4.0f, (metersW - meterGap) * 0.5f);
    const float h         = juce::jmax (20.0f, bottom - top);

    l.panel  = juce::Rectangle<float> (mx, top, panelW, h);
    l.meterL = juce::Rectangle<float> (mx + panelW + gap, top, meterW, h);
    l.meterR = juce::Rectangle<float> (l.meterL.getRight() + meterGap, top, meterW, h);

    return l;
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
        hold = juce::jmax (db, hold - 1.0f);          // luego cae ~30 dB/s
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
    if (getLayout().panel.contains (e.position))
    {
        processor.resetLatch();
        repaint();
    }
}

void MonoCheckEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool overPanel = getLayout().panel.contains (e.position);
    setMouseCursor (overPanel ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor);
}

void MonoCheckEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> r,
                                 float peakDb, float holdDb, float u)
{
    // Pista de fondo
    g.setColour (juce::Colour (0xff1a1a1f));
    g.fillRoundedRectangle (r.expanded (3.0f * u), 6.0f * u);

    const float segH = r.getHeight() / (float) numSegments;
    const float gap  = juce::jmax (0.6f, segH * 0.22f);

    const int holdIndex = holdDb > -60.0f ? dbToSegment (holdDb) : -1;

    for (int i = 0; i < numSegments; ++i)
    {
        const float lowDb = segmentLowDb (i);

        const juce::Colour base = lowDb >= redFromDb    ? colRed
                                : lowDb >= yellowFromDb ? colAmber
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

        g.fillRoundedRectangle (seg, juce::jmin (1.5f * u, seg.getHeight() * 0.4f));
    }
}

void MonoCheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff131316));

    const auto layout = getLayout();
    const float u  = layout.u;
    const float W  = (float) getWidth();
    const float H  = (float) getHeight();
    const float mx = 20.0f * u;

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

    // Cabecera: nombre del plugin, firma y versión
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f * u, juce::Font::bold));
    g.drawText ("MonoCheck", juce::Rectangle<float> (mx, 10.0f * u, 160.0f * u, 28.0f * u),
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::FontOptions (10.0f * u));
    g.drawText ("TxuSo", juce::Rectangle<float> (mx + 1.0f * u, 37.0f * u, 100.0f * u, 12.0f * u),
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawText ("v1.2", juce::Rectangle<float> (W - mx - 60.0f * u, 16.0f * u, 60.0f * u, 12.0f * u),
                juce::Justification::centredRight, false);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (mx, 54.0f * u, W - mx, 54.0f * u, 1.0f);

    // Cuadro de detección (rectangular, vertical, paralelo a los medidores)
    const auto panel  = layout.panel;
    const float radius = 14.0f * u;

    if (colored)
    {
        for (int k = 4; k >= 1; --k)                       // resplandor suave
        {
            g.setColour (colour.withAlpha (0.10f));
            g.drawRoundedRectangle (panel.expanded ((float) k * 2.0f * u), radius + (float) k * 2.0f * u, 3.0f * u);
        }
    }

    g.setGradientFill (juce::ColourGradient (colored ? colour.brighter (0.25f) : colour, panel.getX(), panel.getY(),
                                             colored ? colour.darker (0.45f)  : colour, panel.getX(), panel.getBottom(),
                                             false));
    g.fillRoundedRectangle (panel, radius);

    g.setColour (juce::Colours::white.withAlpha (colored ? 0.35f : 0.18f));
    g.drawRoundedRectangle (panel.reduced (0.5f), radius, 1.0f);

    g.setColour (labelColour);
    g.setFont (juce::FontOptions (15.0f * u, juce::Font::bold));
    g.drawFittedText (label, panel.reduced (4.0f * u).toNearestInt(), juce::Justification::centred, 1, 0.6f);

    // Picómetro vertical L / R (de -60 a 0 dB, más resolución de -20 a 0)
    const auto mL = layout.meterL;
    const auto mR = layout.meterR;

    const float dbL = signal ? juce::Decibels::gainToDecibels (processor.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (processor.peakR.load(), -100.0f) : -100.0f;

    drawMeter (g, mL, dbL, holdDbL, u);
    drawMeter (g, mR, dbR, holdDbR, u);

    // Escala en dBFS a la derecha
    g.setFont (juce::FontOptions (10.0f * u));
    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -20.0f, -30.0f, -40.0f, -60.0f })
    {
        const float y = mL.getBottom() - dbToFraction (db) * mL.getHeight();

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (mR.getRight() + 4.0f * u, y, mR.getRight() + 8.0f * u, y, 1.0f);

        g.setColour (db == yellowFromDb ? colAmber.withAlpha (0.8f)
                                        : juce::Colours::white.withAlpha (0.55f));
        g.drawText (juce::String ((int) db),
                    juce::Rectangle<float> (mR.getRight() + 10.0f * u, y - 6.0f * u, 28.0f * u, 12.0f * u),
                    juce::Justification::centredLeft, false);
    }

    // Etiquetas L / R
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (12.0f * u, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (mL.getX(), mL.getBottom() + 4.0f * u, mL.getWidth(), 14.0f * u),
                juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (mR.getX(), mR.getBottom() + 4.0f * u, mR.getWidth(), 14.0f * u),
                juce::Justification::centred, false);

    // Ayuda cuando el mono está retenido
    if (latched)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (11.0f * u));
        g.drawText ("Clic en el cuadro para resetear",
                    juce::Rectangle<float> (0.0f, H - 22.0f * u, W, 16.0f * u),
                    juce::Justification::centred, false);
    }
}
