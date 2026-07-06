#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent,
                      public juce::Timer,
                      public juce::MidiInputCallback
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
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void initializeISO226Filter(double sampleRate);
    void updateFreqResponse(double sampleRate);
    void rebuildFilter();
    std::vector<float> buildIR(double sampleRate);

    float inputLevels[6] = {};
    float outputLevels[6] = {};
    juce::ToggleButton bypassToggle;
    bool bypass = false;
    std::unique_ptr<juce::MidiInput> midiInput;
    juce::ComboBox midiDeviceCombo;
    juce::Label midiDeviceLabel;
    juce::TextButton sourceLearnBtn{ "L" }, targetLearnBtn{ "L" };
    int midiSourceCC = -1, midiTargetCC = -1;
    bool learningForSource = false, learningForTarget = false;
    juce::Slider sourcePhonSlider;
    juce::Slider targetPhonSlider;
    juce::Label sourceTitle;
    juce::Label targetTitle;
    juce::Label sourcePhonLabel;
    juce::Label targetPhonLabel;
    juce::Label phonUnitLabel;
    double sourcePhon = 60.0;
    double targetPhon = 80.0;
    double currentSampleRate = 0.0;
    juce::dsp::ProcessSpec spec;
    juce::dsp::Convolution convolutionL, convolutionR;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;
    juce::ApplicationProperties appProperties;

    std::vector<float> freqRespFrequencies;
    std::vector<float> freqRespDb;
    double nyquist = 0.0;

    juce::TextButton minimiseButton;
    juce::TextButton closeButton;
    juce::ComponentDragger windowDragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
