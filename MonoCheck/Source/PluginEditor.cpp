#include "PluginEditor.h"

MonoCheckEditor::MonoCheckEditor (MonoCheckProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Ventana escalable manteniendo la proporción (de 75 % a 300 %)
    setResizeLimits (195, 255, 780, 1020);
    getConstrainer()->setFixedAspectRatio (designW / designH);
    setResizable (true, true);
    setSize (260, 340);

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this]
    {
        processor.resetLatch();
        repaint();
    };

    startTimerHz (30);
}

MonoCheckEditor::~MonoCheckEditor()
{
    stopTimer();
}

juce::Rectangle<float> MonoCheckEditor::getSquareBounds()
{
    return juce::Rectangle<float> (140.0f, 140.0f).withCentre ({ designW * 0.5f, 85.0f });
}

void MonoCheckEditor::mouseDown (const juce::MouseEvent& e)
{
    // Clic sobre el cuadrado = Reset
    if (getSquareBounds().contains (e.position / getScale()))
    {
        processor.resetLatch();
        repaint();
    }
}

void MonoCheckEditor::drawBar (juce::Graphics& g, juce::Rectangle<float> r, float linearPeak)
{
    constexpr float floorDb = -60.0f;

    g.setColour (juce::Colour (0xff111111));
    g.fillRect (r);

    const float db = juce::Decibels::gainToDecibels (linearPeak, floorDb);
    const float w  = juce::jmap (juce::jlimit (floorDb, 0.0f, db), floorDb, 0.0f, 0.0f, r.getWidth());

    auto zone = [&] (float fromDb, float toDb, juce::Colour c)
    {
        const float x0 = juce::jmap (fromDb, floorDb, 0.0f, 0.0f, r.getWidth());
        const float x1 = juce::jmin (juce::jmap (toDb, floorDb, 0.0f, 0.0f, r.getWidth()), w);

        if (x1 > x0)
        {
            g.setColour (c);
            g.fillRect (r.getX() + x0, r.getY(), x1 - x0, r.getHeight());
        }
    };

    zone (-60.0f, -12.0f, juce::Colours::limegreen);
    zone (-12.0f,  -3.0f, juce::Colours::gold);
    zone ( -3.0f,   0.0f, juce::Colours::red);

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRect (r, 1.0f);
}

void MonoCheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
    g.addTransform (juce::AffineTransform::scale (getScale()));

    const bool signal  = processor.hasSignal.load();
    const bool latched = processor.monoLatched.load();

    // Rojo = mono retenido; verde = estéreo; negro = sin audio
    juce::Colour colour = juce::Colours::black;
    juce::Colour labelColour = juce::Colours::white.withAlpha (0.6f);
    juce::String label = "No audio";

    if (latched)
    {
        colour = juce::Colours::red;
        labelColour = juce::Colours::white;
        label = "MONO (clic para reset)";
    }
    else if (signal && processor.currentState.load() == MonoCheckProcessor::Stereo)
    {
        colour = juce::Colours::limegreen;
        labelColour = juce::Colours::white;
        label = "STEREO";
    }

    // Cuadrado de detección
    const auto square = getSquareBounds();
    g.setColour (colour);
    g.fillRect (square);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRect (square, 2.0f);

    g.setColour (labelColour);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText (label, juce::Rectangle<float> (0.0f, 165.0f, designW, 28.0f), juce::Justification::centred, false);

    // Picómetro L / R (solo se mueve cuando entra señal)
    const float barX = 36.0f, barW = 188.0f;
    const auto barL = juce::Rectangle<float> (barX, 208.0f, barW, 14.0f);
    const auto barR = juce::Rectangle<float> (barX, 228.0f, barW, 14.0f);

    drawBar (g, barL, signal ? processor.peakL.load() : 0.0f);
    drawBar (g, barR, signal ? processor.peakR.load() : 0.0f);

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (10.0f, 208.0f, 22.0f, 14.0f), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (10.0f, 228.0f, 22.0f, 14.0f), juce::Justification::centred, false);

    // Escala en dBFS
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::FontOptions (10.0f));
    for (float db : { -60.0f, -40.0f, -20.0f, -10.0f, 0.0f })
    {
        const float x = barX + juce::jmap (db, -60.0f, 0.0f, 0.0f, barW);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (x - 15.0f, 245.0f, 30.0f, 12.0f),
                    juce::Justification::centred, false);
    }
}

void MonoCheckEditor::resized()
{
    resetButton.setBounds ((juce::Rectangle<float> (50.0f, 285.0f, 160.0f, 35.0f) * getScale()).toNearestInt());
}
