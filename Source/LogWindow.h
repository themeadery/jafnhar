#pragma once

#include <JuceHeader.h>

class LogWindow : public juce::DialogWindow,
                  private juce::Timer
{
public:
    LogWindow();
    void closeButtonPressed() override;

private:
    void timerCallback() override;

    juce::TextEditor editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogWindow)
};
