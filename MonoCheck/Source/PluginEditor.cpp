#include "PluginEditor.h"

MonoCheckEditor::MonoCheckEditor (MonoCheckProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (260, 280);

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

juce::Rectangle<float> MonoCheckEditor::getSquareBounds() const
{
    return juce::Rectangle<float> (120.0f, 120.0f).withCentre ({ getWidth() * 0.5f, 90.0f });
}

void MonoCheckEditor::mouseDown (const juce::MouseEvent& e)
{
    // Clic sobre el cuadrado = Reset
    if (getSquareBounds().contains (e.position))
    {
        processor.resetLatch();
        repaint();
    }
}

void MonoCheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));

    // Rojo si hay mono retenido; verde si es estéreo; gris sin señal
    juce::Colour colour = juce::Colours::darkgrey;
    juce::String label  = "SIN SEÑAL";

    if (processor.monoLatched.load())
    {
        colour = juce::Colours::red;
        label  = "MONO (clic para reset)";
    }
    else if (processor.currentState.load() == MonoCheckProcessor::Stereo)
    {
        colour = juce::Colours::limegreen;
        label  = "STEREO";
    }

    auto square = getSquareBounds();
    g.setColour (colour);
    g.fillRect (square);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRect (square, 2.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (label, 0, 160, getWidth(), 28, juce::Justification::centred);
}

void MonoCheckEditor::resized()
{
    resetButton.setBounds (getLocalBounds().removeFromBottom (54).reduced (50, 10));
}
