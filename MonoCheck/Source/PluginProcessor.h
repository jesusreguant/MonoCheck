#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class MonoCheckProcessor : public juce::AudioProcessor
{
public:
    enum State { NoSignal = 0, Stereo = 1, Mono = 2 };

    MonoCheckProcessor();
    ~MonoCheckProcessor() override = default;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MonoCheck"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // Lo lee el editor (hilo de interfaz)
    std::atomic<int>  currentState { NoSignal };  // último estado confirmado
    std::atomic<bool> monoLatched  { false };     // true = mono retenido (rojo)

    void resetLatch();

private:
    void registerBlock (bool isMono, int numSamples);

    // Por debajo de esto se considera silencio (-60 dBFS)
    static constexpr float signalThreshold = 0.001f;
    // Diferencia L-R por debajo de esto = mono (-80 dBFS)
    static constexpr float sideThreshold   = 0.0001f;
    // El mono debe mantenerse este tiempo seguido antes de marcarse en rojo
    static constexpr double minMonoSeconds = 0.5;

    juce::int64 minMonoSamples = 24000;            // se recalcula en prepareToPlay
    std::atomic<juce::int64> monoRunSamples { 0 }; // muestras mono consecutivas

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckProcessor)
};
