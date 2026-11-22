#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MonoCheckView.h"

class MonoCheckEditor : public juce::AudioProcessorEditor
{
public:
    explicit MonoCheckEditor (MonoCheckProcessor&);
    ~MonoCheckEditor() override = default;

    void resized() override;

private:
    MonoCheckProcessor& owner;
    MonoCheckView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckEditor)
};
