#include "PluginEditor.h"

namespace
{
    // Escala visual del medidor: empieza en -∞ (abajo) y termina en 0 dBFS (arriba).
    // Se define por puntos (dB -> altura de 0 a 1). Cuanto más arriba, más resolución:
    // -18 dB queda un poco por encima de la mitad (55 %) y de -18 a 0 dB se ocupa el 45 % superior.
    struct Anchor { float db; float frac; };

    constexpr Anchor anchors[] = {
        { -90.0f, 0.00f },   // -∞
        { -70.0f, 0.05f },
        { -60.0f, 0.10f },
        { -40.0f, 0.25f },
        { -30.0f, 0.35f },
        { -18.0f, 0.55f },
        {   0.0f, 1.00f }
    };
    constexpr int numAnchors = (int) (sizeof (anchors) / sizeof (anchors[0]));

    constexpr int numSegments = 80;        // de -18 a 0 dB: 36 segmentos de 0,5 dB

    constexpr float yellowFromDb = -18.0f;  // a partir de aquí, amarillo
    constexpr float redFromDb    = -3.0f;   // a partir de aquí, rojo

    const juce::Colour colGreen { 0xff22c55e };
    const juce::Colour colAmber { 0xfffbbf24 };
    const juce::Colour colRed   { 0xffef4444 };

    // Altura (0 = abajo, 1 = arriba) de un nivel en dB
    float dbToFraction (float db)
    {
        db = juce::jlimit (anchors[0].db, 0.0f, db);

        for (int i = 1; i < numAnchors; ++i)
        {
            if (db <= anchors[i].db)
            {
                const float t = (db - anchors[i - 1].db) / (anchors[i].db - anchors[i - 1].db);
                return anchors[i - 1].frac + t * (anchors[i].frac - anchors[i - 1].frac);
            }
        }

        return 1.0f;
    }

    // Nivel en dB que corresponde a una altura del medidor
    float fractionToDb (float f)
    {
        f = juce::jlimit (0.0f, 1.0f, f);

        for (int i = 1; i < numAnchors; ++i)
        {
            if (f <= anchors[i].frac)
            {
                const float t = (f - anchors[i - 1].frac) / (anchors[i].frac - anchors[i - 1].frac);
                return anchors[i - 1].db + t * (anchors[i].db - anchors[i - 1].db);
            }
        }

        return 0.0f;
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

    const float mx     = 14.0f * u;               // margen lateral
    const float top    = 34.0f * u;               // bajo la cabecera compacta
    const float bottom = H - 22.0f * u;           // sobre las etiquetas L / R
    const float cw     = W - 2.0f * mx;

    const float scaleW   = 36.0f * u;             // espacio para los números de dB
    const float gap      = 14.0f * u;
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

    const int holdIndex = holdDb > anchors[0].db
                            ? juce::jlimit (0, numSegments - 1, (int) (dbToFraction (holdDb) * (float) numSegments))
                            : -1;

    for (int i = 0; i < numSegments; ++i)
    {
        const float lowDb = fractionToDb ((float) i / (float) numSegments);

        const juce::Colour base = lowDb >= redFromDb    - 0.01f ? colRed
                                : lowDb >= yellowFromDb - 0.01f ? colAmber
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
    const float mx = 14.0f * u;

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

    // Cabecera compacta: nombre a la izquierda; firma y versión en pequeño a la derecha
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (14.0f * u, juce::Font::bold));
    g.drawText ("MonoCheck", juce::Rectangle<float> (mx, 6.0f * u, 120.0f * u, 18.0f * u),
                juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (9.0f * u));
    g.drawText ("TxuSo  v1.5", juce::Rectangle<float> (W - mx - 80.0f * u, 9.0f * u, 80.0f * u, 12.0f * u),
                juce::Justification::centredRight, false);

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

    if (latched)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (juce::FontOptions (9.0f * u));
        g.drawFittedText ("clic para resetear",
                          juce::Rectangle<float> (panel.getX() + 4.0f * u, panel.getBottom() - 24.0f * u,
                                                  panel.getWidth() - 8.0f * u, 16.0f * u).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
    }

    // Picómetro vertical L / R (de -∞ a 0 dB, más resolución cuanto más arriba)
    const auto mL = layout.meterL;
    const auto mR = layout.meterR;

    const float dbL = signal ? juce::Decibels::gainToDecibels (processor.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (processor.peakR.load(), -100.0f) : -100.0f;

    drawMeter (g, mL, dbL, holdDbL, u);
    drawMeter (g, mR, dbR, holdDbR, u);

    // Escala en dBFS a la derecha (el último valor, -90, se muestra como -∞)
    g.setFont (juce::FontOptions (10.0f * u));
    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -40.0f, -60.0f, -70.0f, -90.0f })
    {
        const float y = mL.getBottom() - dbToFraction (db) * mL.getHeight();

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (mR.getRight() + 4.0f * u, y, mR.getRight() + 8.0f * u, y, 1.0f);

        g.setColour (db == yellowFromDb ? colAmber.withAlpha (0.8f)
                                        : juce::Colours::white.withAlpha (0.55f));

        const juce::String text = db <= anchors[0].db ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e"))
                                                      : juce::String ((int) db);

        g.drawText (text,
                    juce::Rectangle<float> (mR.getRight() + 10.0f * u, y - 6.0f * u, 28.0f * u, 12.0f * u),
                    juce::Justification::centredLeft, false);
    }

    // Etiquetas L / R
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (12.0f * u, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (mL.getX(), mL.getBottom() + 2.0f * u, mL.getWidth(), 14.0f * u),
                juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (mR.getX(), mR.getBottom() + 2.0f * u, mR.getWidth(), 14.0f * u),
                juce::Justification::centred, false);
}
