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
    // Tamaño de referencia: solo sirve para calcular el tamaño de textos y detalles
    static constexpr float refW = 240.0f;
    static constexpr float refH = 384.0f;

    // Disposición calculada a partir del ancho y el alto reales de la ventana
    struct Layout
    {
        float u = 1.0f;                       // factor de escala para textos y detalles
        juce::Rectangle<float> panel, meterL, meterR;
    };

    Layout getLayout() const;

    void timerCallback() override;

    static void drawMeter (juce::Graphics&, juce::Rectangle<float>, float peakDb, float holdDb, float u);
    static void updateHold (float& hold, int& timer, float db);

    MonoCheckProcessor& processor;

    float holdDbL = -100.0f, holdDbR = -100.0f;
    int   holdTimerL = 0,    holdTimerR = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckEditor)
};
