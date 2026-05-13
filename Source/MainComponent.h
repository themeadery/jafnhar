#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent,
                      public juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void initializeISO226Filter(double sampleRate);

    float inputLevels[6] = {};
    float outputLevels[6] = {};
    juce::ToggleButton iso226Toggle;
    bool iso226Enabled = true;
    juce::dsp::ProcessSpec spec;
    juce::dsp::Convolution convolutionL, convolutionR;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
