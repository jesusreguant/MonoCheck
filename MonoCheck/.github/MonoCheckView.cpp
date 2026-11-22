#include "MonoCheckView.h"

namespace
{
    // Escala visual del medidor: empieza en -∞ (abajo) y termina en 0 dBFS (arriba).
    // Se define por puntos (dB -> altura de 0 a 1). Cuanto más arriba, más resolución:
    // -18 dB queda un poco por encima de la mitad (55 %) y de -18 a 0 dB se ocupa el 45 % superior.
    struct Anchor { float db; float frac; };

    constexpr Anchor anchors[] = {
        { -100.0f, 0.00f },   // -∞
        {  -70.0f, 0.05f },
        {  -60.0f, 0.10f },
        {  -40.0f, 0.25f },
        {  -30.0f, 0.35f },
        {  -18.0f, 0.55f },
        {    0.0f, 1.00f }
    };
    constexpr int numAnchors = (int) (sizeof (anchors) / sizeof (anchors[0]));

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

MonoCheckView::MonoCheckView (MonoCheckEngine& e)
    : engine (e)
{
    lastSnapshot.fill (-1);
    setWantsKeyboardFocus (true);
    setOpaque (true);            // se pinta todo el fondo: evita trabajo extra
    startTimerHz (30);
}

MonoCheckView::~MonoCheckView()
{
    stopTimer();
}

void MonoCheckView::setResetKey (juce::juce_wchar key)
{
    resetKey = key;
    repaint();
}

void MonoCheckView::setRefreshRate (int hz)
{
    startTimerHz (juce::jlimit (5, 120, hz));
}

void MonoCheckView::setVersionText (const juce::String& text)
{
    versionText = text;
    repaint();
}

MonoCheckView::Layout MonoCheckView::getLayout() const
{
    Layout l;

    const float W = (float) getWidth();
    const float H = (float) getHeight();
    const float u = juce::jmax (0.3f, juce::jmin (W / refW, H / refH));
    l.u = u;

    const float mx     = 14.0f * u;               // margen lateral
    const float top    = 40.0f * u;               // bajo la cabecera compacta y las etiquetas L / R
    const float bottom = H - 14.0f * u;
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

    l.gearRect = juce::Rectangle<float> (W - mx - 14.0f * u, 4.0f * u, 14.0f * u, 14.0f * u);

    return l;
}

void MonoCheckView::updateHold (float& hold, int& timer, float db)
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

void MonoCheckView::timerCallback()
{
    if (! isShowing())                                // ventana oculta o minimizada: no hace nada
        return;

    const bool signal = engine.hasSignal.load();

    const float dbL = signal ? juce::Decibels::gainToDecibels (engine.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (engine.peakR.load(), -100.0f) : -100.0f;

    updateHold (holdDbL, holdTimerL, dbL);
    updateHold (holdDbR, holdTimerR, dbR);

    // Solo se repinta si algo visible ha cambiado (en reposo no consume nada)
    const std::array<int, 8> snapshot {
        engine.monoLatched.load() ? 1 : 0,
        signal ? 1 : 0,
        engine.state.load(),
        (int) (dbToFraction (dbL) * 1000.0f),
        (int) (dbToFraction (dbR) * 1000.0f),
        holdDbL > anchors[0].db ? (int) (dbToFraction (holdDbL) * 1000.0f) : -1,
        holdDbR > anchors[0].db ? (int) (dbToFraction (holdDbR) * 1000.0f) : -1,
        (waitingForKey ? 1 : 0) + (gearHover ? 2 : 0)
    };

    if (snapshot != lastSnapshot)
    {
        lastSnapshot = snapshot;
        repaint();
    }
}

void MonoCheckView::showKeyMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Cambiar tecla de reset (actual: " + juce::String::charToString (resetKey) + ")");

    juce::Component::SafePointer<MonoCheckView> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options(), [safe] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        safe->waitingForKey = true;
        safe->grabKeyboardFocus();
        safe->repaint();
    });
}

void MonoCheckView::mouseEnter (const juce::MouseEvent&)
{
    // Con el ratón encima, recibe las teclas (para el atajo de reset)
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
}

void MonoCheckView::mouseExit (const juce::MouseEvent&)
{
    if (gearHover)
    {
        gearHover = false;
        repaint();
    }
}

void MonoCheckView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto layout = getLayout();

    if (onGearClicked && layout.gearRect.expanded (4.0f * layout.u).contains (e.position))
    {
        onGearClicked();
        return;
    }

    if (layout.panel.contains (e.position))
    {
        if (e.mods.isPopupMenu())
        {
            showKeyMenu();                      // clic derecho: elegir tecla de reset
            return;
        }

        engine.reset();                         // clic sobre el cuadro = Reset
        repaint();
    }
}

void MonoCheckView::mouseMove (const juce::MouseEvent& e)
{
    const auto layout = getLayout();

    const bool overGear = onGearClicked && layout.gearRect.expanded (4.0f * layout.u).contains (e.position);

    if (overGear != gearHover)
    {
        gearHover = overGear;
        repaint();
    }

    if (overGear || layout.panel.contains (e.position))
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

bool MonoCheckView::keyPressed (const juce::KeyPress& key)
{
    // Modo "elegir tecla": la siguiente tecla pasa a ser el atajo de reset
    if (waitingForKey)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            waitingForKey = false;
            repaint();
            return true;
        }

        const juce::juce_wchar c = juce::CharacterFunctions::toUpperCase (key.getTextCharacter());

        if (c > 32 && c != 127)
        {
            resetKey = c;
            waitingForKey = false;

            if (onResetKeyChanged)
                onResetKeyChanged (c);

            repaint();
        }

        return true;
    }

    const auto mods = key.getModifiers();

    if (! mods.isCommandDown() && ! mods.isCtrlDown() && ! mods.isAltDown()
        && juce::CharacterFunctions::toUpperCase (key.getTextCharacter()) == resetKey)
    {
        engine.reset();
        repaint();
        return true;
    }

    return false;
}

void MonoCheckView::drawMeter (juce::Graphics& g, juce::Rectangle<float> r,
                               float peakDb, float holdDb, float u)
{
    // Marco fino sobre fondo oscuro
    const auto frame = r.expanded (2.0f * u);
    g.setColour (juce::Colour (0xff0d0d10));
    g.fillRect (frame);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRect (frame, 1.0f);

    // Líneas finas: tantas como permita la altura (más resolución al agrandar la ventana)
    const int   numLines = juce::jlimit (60, 260, (int) (r.getHeight() / (2.2f * u)));
    const float pitch    = r.getHeight() / (float) numLines;
    const float lineH    = juce::jmax (0.8f, pitch * 0.55f);

    for (int i = 0; i < numLines; ++i)
    {
        const float lowDb = fractionToDb ((float) i / (float) numLines);

        const juce::Colour base = lowDb >= redFromDb    - 0.01f ? colRed
                                : lowDb >= yellowFromDb - 0.01f ? colAmber
                                                                : colGreen;

        const bool lit = peakDb > lowDb;
        const float y  = r.getBottom() - ((float) i + 0.5f) * pitch - lineH * 0.5f;

        g.setColour (lit ? base : base.withAlpha (0.07f));
        g.fillRect (r.getX(), y, r.getWidth(), lineH);
    }

    // Pico retenido: línea blanca fina
    if (holdDb > anchors[0].db)
    {
        const float y = r.getBottom() - dbToFraction (holdDb) * r.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillRect (r.getX(), y - 0.75f * u, r.getWidth(), 1.5f * u);
    }
}

void MonoCheckView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff131316));

    const auto layout = getLayout();
    const float u  = layout.u;
    const float W  = (float) getWidth();
    const float mx = 14.0f * u;
    const bool showGear = (bool) onGearClicked;

    const bool signal  = engine.hasSignal.load();
    const bool latched = engine.monoLatched.load();

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
    else if (signal && engine.state.load() == MonoCheckEngine::Stereo)
    {
        colour = colGreen;
        label = "STEREO";
        colored = true;
    }

    if (colored)
        labelColour = juce::Colours::white;

    // Cabecera compacta: nombre a la izquierda; firma y versión en pequeño a la derecha
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f * u, juce::Font::bold));
    g.drawText ("MonoCheck", juce::Rectangle<float> (mx, 4.0f * u, 120.0f * u, 16.0f * u),
                juce::Justification::centredLeft, false);

    const float rightEdge = W - mx - (showGear ? 20.0f * u : 0.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (9.0f * u));
    g.drawText (versionText, juce::Rectangle<float> (rightEdge - 90.0f * u, 6.0f * u, 90.0f * u, 12.0f * u),
                juce::Justification::centredRight, false);

    // Engranaje de ajustes (solo en la app)
    if (showGear)
    {
        const auto c = layout.gearRect.getCentre();
        const float r = layout.gearRect.getWidth() * 0.5f;

        juce::Path teeth;
        teeth.addStar (c, 8, r * 0.78f, r, 0.0f);

        juce::Path gear = teeth.createPathWithRoundedCorners (r * 0.18f);
        gear.addEllipse (c.x - r * 0.30f, c.y - r * 0.30f, r * 0.60f, r * 0.60f);
        gear.setUsingNonZeroWinding (false);

        g.setColour (juce::Colours::white.withAlpha (gearHover ? 0.95f : 0.55f));
        g.fillPath (gear);
    }

    // Etiquetas L / R sobre los medidores
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::FontOptions (10.0f * u, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (layout.meterL.getX(), layout.meterL.getY() - 15.0f * u,
                                              layout.meterL.getWidth(), 12.0f * u), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (layout.meterR.getX(), layout.meterR.getY() - 15.0f * u,
                                              layout.meterR.getWidth(), 12.0f * u), juce::Justification::centred, false);

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
        g.drawFittedText ("clic o " + juce::String::charToString (resetKey) + " para resetear",
                          juce::Rectangle<float> (panel.getX() + 4.0f * u, panel.getBottom() - 26.0f * u,
                                                  panel.getWidth() - 8.0f * u, 20.0f * u).toNearestInt(),
                          juce::Justification::centred, 2, 0.6f);
    }

    if (waitingForKey)
    {
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (panel, radius);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (11.0f * u, juce::Font::bold));
        g.drawFittedText ("Pulsa la tecla de reset (Esc cancela)",
                          panel.reduced (8.0f * u).toNearestInt(), juce::Justification::centred, 4, 0.7f);
    }

    // Picómetro vertical L / R (de -∞ a 0 dB, más resolución cuanto más arriba)
    const auto mL = layout.meterL;
    const auto mR = layout.meterR;

    const float dbL = signal ? juce::Decibels::gainToDecibels (engine.peakL.load(), -100.0f) : -100.0f;
    const float dbR = signal ? juce::Decibels::gainToDecibels (engine.peakR.load(), -100.0f) : -100.0f;

    drawMeter (g, mL, dbL, holdDbL, u);
    drawMeter (g, mR, dbR, holdDbR, u);

    // Escala en dBFS a la derecha (el último valor, -100, se muestra como -∞)
    g.setFont (juce::FontOptions (9.0f * u));
    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -40.0f, -60.0f, -70.0f, -100.0f })
    {
        const float y = mL.getBottom() - dbToFraction (db) * mL.getHeight();

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawLine (mR.getRight() + 4.0f * u, y, mR.getRight() + 8.0f * u, y, 0.8f);

        g.setColour (db == yellowFromDb ? colAmber.withAlpha (0.8f)
                                        : juce::Colours::white.withAlpha (0.50f));

        const juce::String text = db <= anchors[0].db ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e"))
                                                      : juce::String ((int) db);

        g.drawText (text,
                    juce::Rectangle<float> (mR.getRight() + 10.0f * u, y - 6.0f * u, 28.0f * u, 12.0f * u),
                    juce::Justification::centredLeft, false);
    }
}
