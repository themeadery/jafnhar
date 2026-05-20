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
    void updateFreqResponse(double sampleRate);
    void rebuildFilter();
    std::vector<float> buildIR(double sampleRate);

    float inputLevels[6] = {};
    float outputLevels[6] = {};
    juce::ToggleButton bypassToggle;
    bool bypass = false;
    juce::Slider sourcePhonSlider;
    juce::Slider targetPhonSlider;
    juce::Label sourceTitle;
    juce::Label targetTitle;
    juce::Label sourcePhonLabel;
    juce::Label targetPhonLabel;
    juce::Label phonUnitLabel;
    int sourcePhonIdx = 2;
    int targetPhonIdx = 3;
    double currentSampleRate = 0.0;
    juce::dsp::ProcessSpec spec;
    juce::dsp::Convolution convolutionL, convolutionR;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    std::vector<float> freqRespFrequencies;
    std::vector<float> freqRespDb;
    double nyquist = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
