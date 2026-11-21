#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MonoCheckEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit MonoCheckEditor (MonoCheckProcessor&);
    ~MonoCheckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    // Todo se dibuja en un lienzo de 260 x 340 que se escala con la ventana
    static constexpr float designW = 260.0f;
    static constexpr float designH = 340.0f;

    void timerCallback() override { repaint(); }
    float getScale() const { return (float) getWidth() / designW; }
    static juce::Rectangle<float> getSquareBounds();
    static void drawBar (juce::Graphics&, juce::Rectangle<float>, float linearPeak);

    MonoCheckProcessor& processor;
    juce::TextButton resetButton { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckEditor)
};
