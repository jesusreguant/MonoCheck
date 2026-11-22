#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include "MonoCheckEngine.h"

// Interfaz de MonoCheck (cuadro de detección + medidor). La usan el plugin y la app.
// Ahorra recursos: solo se repinta cuando algo visible cambia y no hace nada si la ventana está oculta.
class MonoCheckView : public juce::Component,
                      private juce::Timer
{
public:
    explicit MonoCheckView (MonoCheckEngine&);
    ~MonoCheckView() override;

    // Opcional: si se define, se dibuja un engranaje y se llama al hacer clic en él (lo usa la app)
    std::function<void()> onGearClicked;

    // Se llama cuando el usuario elige otra tecla de reset
    std::function<void (juce::juce_wchar)> onResetKeyChanged;

    void setResetKey (juce::juce_wchar key);
    void setRefreshRate (int hz);
    void setVersionText (const juce::String& text);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // Tamaño de referencia: solo sirve para calcular el tamaño de textos y detalles
    static constexpr float refW = 240.0f;
    static constexpr float refH = 384.0f;

    struct Layout
    {
        float u = 1.0f;                       // factor de escala para textos y detalles
        juce::Rectangle<float> panel, meterL, meterR, gearRect;
    };

    Layout getLayout() const;

    void timerCallback() override;
    void showKeyMenu();

    static void drawMeter (juce::Graphics&, juce::Rectangle<float>, float peakDb, float holdDb, float u);
    static void updateHold (float& hold, int& timer, float db);

    MonoCheckEngine& engine;

    juce::String versionText { "TxuSo" };

    float holdDbL = -100.0f, holdDbR = -100.0f;
    int   holdTimerL = 0,    holdTimerR = 0;

    bool             waitingForKey = false;   // esperando que el usuario elija la tecla de reset
    bool             gearHover     = false;
    juce::juce_wchar resetKey      = 'R';     // tecla de reset (por defecto R)

    std::array<int, 8> lastSnapshot {};        // lo último que se pintó (para no repintar sin cambios)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckView)
};
