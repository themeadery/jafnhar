#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class LogWindow;

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
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class GearButton : public juce::Component
    {
    public:
        GearButton()
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().reduced(juce::roundToInt(getWidth() * 0.12f)).toFloat();
            auto cx = b.getCentreX(), cy = b.getCentreY();
            float size = juce::jmin(b.getWidth(), b.getHeight());
            float s = size / 1024.0f;

            juce::Path gear;
            gear.setUsingNonZeroWinding(false);

            gear.addEllipse(511.92f - 162.0f, 512.032f - 162.0f, 324.0f, 324.0f);

            gear.startNewSubPath(836.624f, 605.056f);
            gear.lineTo(807.44f, 675.36f);
            gear.lineTo(866.384f, 791.104f);
            gear.lineTo(794.192f, 863.296f);
            gear.lineTo(675.76f, 807.36f);
            gear.lineTo(605.456f, 836.224f);
            gear.lineTo(569.776f, 945.472f);
            gear.lineTo(565.2f, 959.968f);
            gear.lineTo(463.184f, 959.968f);
            gear.lineTo(419.024f, 836.64f);
            gear.lineTo(348.72f, 807.648f);
            gear.lineTo(232.816f, 866.336f);
            gear.lineTo(160.656f, 794.208f);
            gear.lineTo(216.528f, 675.712f);
            gear.lineTo(187.568f, 605.472f);
            gear.lineTo(64.048f, 565.152f);
            gear.lineTo(64.048f, 463.168f);
            gear.lineTo(187.44f, 418.944f);
            gear.lineTo(216.4f, 348.768f);
            gear.lineTo(164.496f, 246.304f);
            gear.lineTo(157.648f, 232.864f);
            gear.lineTo(229.712f, 160.8f);
            gear.lineTo(348.304f, 216.64f);
            gear.lineTo(418.512f, 187.616f);
            gear.lineTo(454.16f, 78.432f);
            gear.lineTo(458.768f, 63.968f);
            gear.lineTo(560.784f, 63.968f);
            gear.lineTo(604.976f, 187.424f);
            gear.lineTo(675.088f, 216.448f);
            gear.lineTo(791.152f, 157.632f);
            gear.lineTo(863.28f, 229.696f);
            gear.lineTo(807.408f, 348.096f);
            gear.lineTo(836.272f, 418.432f);
            gear.lineTo(960.016f, 458.688f);
            gear.lineTo(960.016f, 560.64f);
            gear.lineTo(836.656f, 605.024f);
            gear.closeSubPath();

            gear.applyTransform(juce::AffineTransform::scale(s, s)
                .translated(cx - 512.0f * s, cy - 512.0f * s));

            g.setColour(juce::Colours::lightgrey);
            g.fillPath(gear);
        }
        void mouseDown(const juce::MouseEvent&) override
        {
            if (onClick)
                onClick();
        }
        std::function<void()> onClick;
    };

    class MeaderyLogo : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().reduced(juce::roundToInt(getWidth() * 0.05f)).toFloat();
            auto cx = b.getCentreX(), cy = b.getCentreY();
            float size = juce::jmin(b.getWidth(), b.getHeight());
            float s = size / 200.0f;

            juce::Path hex;
            hex.startNewSubPath(100.0f, 20.0f);
            hex.lineTo(169.28f, 60.0f);
            hex.lineTo(169.28f, 140.0f);
            hex.lineTo(100.0f, 180.0f);
            hex.lineTo(30.72f, 140.0f);
            hex.lineTo(30.72f, 60.0f);
            hex.closeSubPath();

            hex.applyTransform(juce::AffineTransform::scale(s, s)
                .translated(cx - 100.0f * s, cy - 100.0f * s));

            g.setColour(juce::Colours::grey.darker(0.4f));
            g.strokePath(hex, juce::PathStrokeType(8.0f));
        }
    };

    void initializeISO226Filter(double sampleRate);
    void updateFreqResponse(double sampleRate);
    void rebuildFilter();
    void setUIScale(float scale);
    std::vector<float> buildIR(double sampleRate);

    float inputLevels[6] = {};
    float outputLevels[6] = {};
    juce::ToggleButton bypassToggle;
    bool bypass = false;
    std::unique_ptr<juce::MidiInput> midiInput;
    juce::ComboBox midiDeviceCombo;
    juce::Label midiDeviceLabel;
    juce::TextButton sourceLearnBtn{ "L" }, targetLearnBtn{ "L" };
    juce::Slider actualPhonSlider;
    juce::Slider targetPhonSlider;
    juce::Label actualTitle;
    juce::Label targetTitle;
    juce::Label appTitle;
    juce::Font vikingFont;
    juce::Label actualPhonLabel;
    juce::Label targetPhonLabel;
    juce::Label phonUnitLabel;
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    juce::Label volumeTitle;
    juce::TextButton volumeLearnBtn{ "L" };
    MeaderyLogo meaderyLogo;
    juce::Label meaderyLabel;
    double actualPhon = 60.0;
    double targetPhon = 80.0;
    double masterVolume = 1.0;
    int midiSourceCC = -1, midiTargetCC = -1, midiVolumeCC = -1;
    bool learningForSource = false, learningForTarget = false, learningForVolume = false;
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
    GearButton settingsButton;
    juce::TextButton logButton{ "Log" };
    std::unique_ptr<LogWindow> logWindow;
    float uiScale = 1.0f;
    bool noDeviceLogged = false;
    juce::String lastDeviceName;
    juce::String lastMidiDeviceName;
    juce::ComponentDragger windowDragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
