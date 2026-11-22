#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MonoCheckEngine.h"

class MonoCheckProcessor : public juce::AudioProcessor
{
public:
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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Parámetro "reset": cada cambio de valor resetea el rojo (para asignarlo a una tecla/controlador del DAW).
    // También guarda la tecla de reset elegida por el usuario.
    juce::AudioProcessorValueTreeState apvts;

    // Motor de análisis (lo lee la interfaz)
    MonoCheckEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* resetParam = nullptr;
    bool lastResetOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckProcessor)
};
