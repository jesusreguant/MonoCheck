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
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> getSquareBounds() const;

    MonoCheckProcessor& processor;
    juce::TextButton resetButton { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoCheckEditor)
};
