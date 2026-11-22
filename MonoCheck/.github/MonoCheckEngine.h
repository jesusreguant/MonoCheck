#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>

// Núcleo de análisis de MonoCheck. Lo usan el plugin y la app.
// Muy ligero: por cada bloque de audio solo recorre las muestras una vez.
class MonoCheckEngine
{
public:
    enum State { NoSignal = 0, Stereo = 1, Mono = 2 };

    // Lo lee la interfaz
    std::atomic<int>   state       { NoSignal };  // último estado confirmado
    std::atomic<bool>  monoLatched { false };     // true = mono retenido (rojo)
    std::atomic<bool>  hasSignal   { false };     // hay audio entrando
    std::atomic<float> peakL       { 0.0f };      // pico canal izquierdo (lineal)
    std::atomic<float> peakR       { 0.0f };      // pico canal derecho (lineal)

    void prepare (double sampleRate)
    {
        sampleRateHz      = sampleRate > 0.0 ? sampleRate : 44100.0;
        minMonoSamples    = (juce::int64) (sampleRateHz * minMonoSeconds);
        signalHoldSamples = (juce::int64) (sampleRateHz * signalHoldSeconds);
        silenceCounter    = 0;
        monoRunSamples    = 0;
        hasSignal         = false;
        peakL = 0.0f;
        peakR = 0.0f;
    }

    // Quita el rojo retenido (se llama desde la interfaz o desde el parámetro del DAW)
    void reset() noexcept
    {
        monoLatched    = false;
        monoRunSamples = 0;
        state          = NoSignal;
    }

    // l y r pueden apuntar al mismo buffer (entrada mono)
    void process (const float* l, const float* r, int n) noexcept
    {
        float pL = 0.0f, pR = 0.0f, side = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (l[i]);
            const float b = std::abs (r[i]);
            pL   = juce::jmax (pL, a);
            pR   = juce::jmax (pR, b);
            side = juce::jmax (side, std::abs (l[i] - r[i]));
        }

        // Picómetro: sube al instante y cae unos 20 dB por segundo
        const float decay = (float) std::pow (10.0, -(double) n / sampleRateHz);
        peakL = juce::jmax (pL, peakL.load() * decay);
        peakR = juce::jmax (pR, peakR.load() * decay);

        const float peak = juce::jmax (pL, pR);

        if (peak >= signalThreshold)
        {
            silenceCounter = 0;
            hasSignal = true;

            // En señales muy bajas la diferencia L-R se compara también con el propio nivel (-40 dB),
            // para no confundir un estéreo muy bajo con mono
            const float sideLimit = juce::jmin (sideThreshold, peak * 0.01f);
            registerBlock (side < sideLimit, n);
        }
        else
        {
            processSilence (n);
        }
    }

    // Bloque sin señal (o sin entrada): solo avanza el contador y deja caer el medidor
    void processSilence (int n) noexcept
    {
        const float decay = (float) std::pow (10.0, -(double) n / sampleRateHz);
        peakL = peakL.load() * decay;
        peakR = peakR.load() * decay;

        silenceCounter += n;

        if (silenceCounter >= signalHoldSamples)
        {
            hasSignal = false;

            // Sin señal: el rojo retenido se conserva; si no, vuelve a "No audio"
            if (! monoLatched.load() && monoRunSamples.load() == 0)
                state = NoSignal;
        }
    }

private:
    // Por debajo de esto se considera silencio (-100 dBFS)
    static constexpr float signalThreshold = 0.00001f;
    // Diferencia L-R por debajo de esto = mono (-80 dBFS)
    static constexpr float sideThreshold = 0.0001f;
    // El mono debe mantenerse este tiempo seguido antes de marcarse en rojo (casi instantáneo)
    static constexpr double minMonoSeconds = 0.005;
    // Tiempo sin señal antes de mostrar "No audio"
    static constexpr double signalHoldSeconds = 0.3;

    // Un bloque mono suma al contador; al llegar al tiempo mínimo se retiene en rojo.
    // Un bloque estéreo reinicia el contador.
    void registerBlock (bool isMono, int numSamples) noexcept
    {
        if (isMono)
        {
            const auto run = monoRunSamples.load() + numSamples;
            monoRunSamples = run;

            if (run >= minMonoSamples)
            {
                state = Mono;
                monoLatched = true;
            }
        }
        else
        {
            monoRunSamples = 0;
            state = Stereo;
        }
    }

    double sampleRateHz = 44100.0;
    juce::int64 minMonoSamples    = 220;
    juce::int64 signalHoldSamples = 13230;
    juce::int64 silenceCounter    = 0;              // solo hilo de audio
    std::atomic<juce::int64> monoRunSamples { 0 };  // muestras mono consecutivas
};
