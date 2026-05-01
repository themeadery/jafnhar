#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (854, 358);

    noiseToggle.setButtonText ("Test Output");
    noiseToggle.setTooltip("-30 dBFS white noise");
    noiseToggle.onClick = [this] { noiseEnabled = noiseToggle.getToggleState(); };
    addAndMakeVisible (noiseToggle);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 6 : 0, 6); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (6, 6);
    }

    startTimer (60); // repaint screen every 60ms
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{

    // process input channels
    for (int ch = 0; ch < 6 && ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        auto* data = bufferToFill.buffer->getReadPointer(ch, bufferToFill.startSample);
        float peak = 0.0f;
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            peak = std::max(peak, std::abs(data[i]));
        inputLevels[ch] = std::max(inputLevels[ch] * 0.95f, peak);
    }

    // process output channels
    float amplitude = std::pow(10.0f, -30.0f / 20.0f); // -30 dBFS amplitude for white noise

    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        auto* outData = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
        float peak = 0.0f;

        if (noiseEnabled && (ch == 0 || ch == 1))
        {
            for (int i = 0; i < bufferToFill.numSamples; ++i)
            {
                float noise = (random.nextFloat() * 2.0f - 1.0f) * amplitude;
                outData[i] = noise;
                peak = std::max(peak, std::abs(noise));
            }
        }
        else
        {
            for (int i = 0; i < bufferToFill.numSamples; ++i)
            {
                outData[i] = 0.0f;
            }
        }

        outputLevels[ch] = std::max(outputLevels[ch] * 0.95f, peak);
    }

}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto* device = deviceManager.getCurrentAudioDevice();
    juce::String info = device ? device->getName() : "No device";
    g.setColour (juce::Colours::white);
    g.drawText (info, 10, 10, 300, 20, juce::Justification::left);

    int meterHeight = getHeight() - 60;
    int barWidth = 4;
    int spacing = 10;
    int meterTop = 40;
    int labelAreaWidth = 50;
    int inputMeterStartX = labelAreaWidth + 10;

    int windowWidth = getWidth();
    int outputLabelAreaWidth = 50;
    int outputMeterStartX = windowWidth - outputLabelAreaWidth - (6 * spacing) - 10;

    g.setColour (juce::Colours::grey);

    // Input meter outlines
    for (int i = 0; i < 6; ++i)
    {
        int x = inputMeterStartX + i * spacing;
        g.drawRect (x, meterTop, barWidth, meterHeight, 1);
    }

    // Output meter outlines
    for (int i = 0; i < 6; ++i)
    {
        int x = outputMeterStartX + i * spacing;
        g.drawRect (x, meterTop, barWidth, meterHeight, 1);
    }

    g.setFont (8.0f);
    g.setColour (juce::Colours::white);

    // Left side labels (inputs)
    for (int db = 0; db >= -144; db -= 24)
    {
        float normalizedPos = (float)(db + 144) / 144.0f;
        int y = meterTop + meterHeight - (int)(normalizedPos * meterHeight);
        g.drawText (juce::String (db), 5, y - 4, 40, 8, juce::Justification::right);
    }

    // Right side labels (outputs)
    for (int db = 0; db >= -144; db -= 24)
    {
        float normalizedPos = (float)(db + 144) / 144.0f;
        int y = meterTop + meterHeight - (int)(normalizedPos * meterHeight);
        g.drawText (juce::String (db), windowWidth - 45, y - 4, 40, 8, juce::Justification::left);
    }

    g.setColour (juce::Colours::white);

    // Input meters
    for (int i = 0; i < 6; ++i)
    {
        float dBFS = 20.0f * std::log10(inputLevels[i]);
        float normalizedPos = (dBFS + 144.0f) / 144.0f;
        normalizedPos = std::max(0.0f, std::min(1.0f, normalizedPos));

        int barHeight = (int)(normalizedPos * meterHeight);
        int x = inputMeterStartX + i * spacing;
        int y = meterTop + meterHeight - barHeight;
        g.fillRect (x, y, barWidth, barHeight);
    }

    // Output meters
    for (int i = 0; i < 6; ++i)
    {
        float dBFS = 20.0f * std::log10(outputLevels[i]);
        float normalizedPos = (dBFS + 144.0f) / 144.0f;
        normalizedPos = std::max(0.0f, std::min(1.0f, normalizedPos));

        int barHeight = (int)(normalizedPos * meterHeight);
        int x = outputMeterStartX + i * spacing;
        int y = meterTop + meterHeight - barHeight;
        g.fillRect (x, y, barWidth, barHeight);
    }
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    // This is also called once on initialization, so we can set the initial position of the toggle button here.
    int centerX = getWidth() / 2 - 50;
    int centerY = getHeight() / 2 - 15;
    noiseToggle.setBounds(centerX, centerY, 100, 30);
}

void MainComponent::timerCallback()
{
    repaint();
}
