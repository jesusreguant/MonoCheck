#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MonoCheckProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "reset", 1 }, "Reset", false));

    return layout;
}

MonoCheckProcessor::MonoCheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MonoCheckState", createLayout())
{
    resetParam = apvts.getRawParameterValue ("reset");
    lastResetOn = resetParam->load() > 0.5f;
}

void MonoCheckProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    lastResetOn = resetParam->load() > 0.5f;
}

bool MonoCheckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MonoCheckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();

    if (n <= 0 || buffer.getNumChannels() < 1)
        return;

    // El audio pasa sin modificarse (plugin solo de medición)

    // Reset pedido desde el DAW (parámetro automatizable): cada cambio de valor resetea
    const bool resetOn = resetParam->load() > 0.5f;
    if (resetOn != lastResetOn)
    {
        lastResetOn = resetOn;
        engine.reset();
    }

    // Pista mono: no hay canal R, se usa L para ambos
    const bool monoBus = getTotalNumInputChannels() < 2 || buffer.getNumChannels() < 2;

    const float* l = buffer.getReadPointer (0);
    const float* r = monoBus ? l : buffer.getReadPointer (1);

    engine.process (l, r, n);
}

void MonoCheckProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MonoCheckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MonoCheckProcessor::createEditor()
{
    return new MonoCheckEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonoCheckProcessor();
}
