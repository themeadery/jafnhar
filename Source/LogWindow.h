#pragma once

#include <JuceHeader.h>
#include "MeaderyLogo.h"

class LogWindow : public juce::DialogWindow,
                  private juce::Timer
{
public:
    LogWindow();
    void closeButtonPressed() override;

private:
    class LogContent : public juce::Component
    {
    public:
        LogContent();
        void paint(juce::Graphics& g) override;
        void resized() override;

        MeaderyLogo watermark;
        juce::TextEditor editor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogContent)
    };

    void timerCallback() override;

    LogContent* content = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogWindow)
};
