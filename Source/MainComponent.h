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
    void initializeISO226Filter();

    float inputLevels[6] = {};
    float outputLevels[6] = {};
    juce::Random random;
    juce::ToggleButton noiseToggle;
    bool noiseEnabled = false;
    juce::ToggleButton iso226Toggle;
    bool iso226Enabled = true;
    std::array<float, 29> firCoefficients;
    juce::dsp::FIR::Filter<float> firFilterL, firFilterR; // One per channel
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
