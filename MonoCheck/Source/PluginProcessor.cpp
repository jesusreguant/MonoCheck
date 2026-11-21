#include "PluginProcessor.h"
#include "PluginEditor.h"

MonoCheckProcessor::MonoCheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MonoCheckProcessor::prepareToPlay (double sampleRate, int)
{
    minMonoSamples = (juce::int64) (sampleRate * minMonoSeconds);
    monoRunSamples = 0;
}

bool MonoCheckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MonoCheckProcessor::resetLatch()
{
    monoLatched = false;
    monoRunSamples = 0;
    currentState = NoSignal;
}

// Un bloque mono suma al contador; al llegar al tiempo mínimo se retiene en rojo.
// Un bloque estéreo reinicia el contador.
void MonoCheckProcessor::registerBlock (bool isMono, int numSamples)
{
    if (isMono)
    {
        const auto run = monoRunSamples.load() + numSamples;
        monoRunSamples = run;

        if (run >= minMonoSamples)
        {
            currentState = Mono;
            monoLatched = true;
        }
    }
    else
    {
        monoRunSamples = 0;
        currentState = Stereo;
    }
}

void MonoCheckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // El audio pasa sin modificarse (plugin solo de medición)

    // Pista mono: no hay canal R, se considera mono si hay señal
    if (getTotalNumInputChannels() < 2 || buffer.getNumChannels() < 2)
    {
        if (buffer.getMagnitude (0, numSamples) > signalThreshold)
            registerBlock (true, numSamples);
        return;
    }

    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (1);

    float peak = 0.0f, side = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        peak = juce::jmax (peak, std::abs (l[i]), std::abs (r[i]));
        side = juce::jmax (side, std::abs (l[i] - r[i]));
    }

    if (peak < signalThreshold)
    {
        // Silencio: no cambia el contador ni el latch
        if (! monoLatched.load() && monoRunSamples.load() == 0)
            currentState = NoSignal;
        return;
    }

    registerBlock (side < sideThreshold, numSamples);
}

juce::AudioProcessorEditor* MonoCheckProcessor::createEditor()
{
    return new MonoCheckEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoCheckProcessor();
}
