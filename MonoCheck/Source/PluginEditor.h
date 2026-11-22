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
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    // Todo se dibuja en un lienzo de 240 x 384 que se escala con la ventana
    static constexpr float designW = 240.0f;
    static constexpr float designH = 384.0f;

    void timerCallback() override;
    float getScale() const { return (float) getWidth() / designW; }

    static juce::Rectangle<float> getPanelBounds()           { return { 20.0f,  64.0f, 90.0f, 270.0f }; }
    static juce::Rectangle<float> getMeterBounds (int index) { return { 128.0f + 32.0f * (float) index, 64.0f, 26.0f, 270.0f }; }

    static void drawMeter (juce::Graphics&, juce::Rectangle<float>, float peakDb, float holdDb);
    static void updateHold (float& hold, int& timer, float db);

    MonoCheckProcessor& processor;

    float holdDbL = -100.0f, holdDbR = -100.0f;
    int   holdTimerL = 0,    holdTimerR = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckEditor)
};
