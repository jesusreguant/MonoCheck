#include "PluginEditor.h"

MonoCheckEditor::MonoCheckEditor (MonoCheckProcessor& p)
    : AudioProcessorEditor (&p), owner (p), view (p.engine)
{
    // Tecla de reset guardada en el proyecto (por defecto R)
    const juce::String savedKey = owner.apvts.state.getProperty ("resetKey", "R").toString();
    if (savedKey.isNotEmpty())
        view.setResetKey (savedKey[0]);

    view.onResetKeyChanged = [this] (juce::juce_wchar c)
    {
        owner.apvts.state.setProperty ("resetKey", juce::String::charToString (c), nullptr);
    };

    view.setVersionText ("TxuSo  v1.8");
    view.setRefreshRate (60);
    addAndMakeVisible (view);

    // Ventana escalable libremente en ancho y en alto
    setResizeLimits (160, 240, 700, 1100);
    setResizable (true, true);
    setSize (240, 384);
}

void MonoCheckEditor::resized()
{
    view.setBounds (getLocalBounds());
}
