#include "MainComponent.h"
#include "iso226_data.h"

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

    iso226Toggle.setButtonText("ISO 226 Filter");
    iso226Toggle.setTooltip("80 - 60 Phon equal-loudness curve difference");
    iso226Toggle.setToggleState(true, juce::dontSendNotification); // Default to enabled
    iso226Toggle.onClick = [this] { iso226Enabled = iso226Toggle.getToggleState(); };
    addAndMakeVisible(iso226Toggle);

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

    initializeISO226Filter();
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

        if (ch == 0 || ch == 1)
        {
            auto* inData = bufferToFill.buffer->getReadPointer(ch + 4, bufferToFill.startSample);
            auto& buffer = filterBuffers[ch];
            
            if (iso226Enabled)
            {
                for (int i = 0; i < bufferToFill.numSamples; ++i)
                {
                    // Shift buffer and add new sample (circular buffer style)
                    for (size_t j = buffer.size() - 1; j > 0; --j)
                    {
                        buffer[j] = buffer[j - 1];
                    }
                    buffer[0] = inData[i];
                
                    // Apply FIR convolution: sum(sample[n] * coeff[n])
                    float filtered = 0.0f;
                    for (size_t j = 0; j < buffer.size(); ++j)
                    {
                        filtered += buffer[j] * firCoefficients[j];
                    }
                
                    outData[i] = filtered;
                    peak = std::max(peak, std::abs(filtered));
                }
            }
            else
            {
                // Pass through unfiltered (bit perfect)
                for (int i = 0; i < bufferToFill.numSamples; ++i)
                {
                    outData[i] = inData[i];
                    peak = std::max(peak, std::abs(inData[i]));
                }
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
    int centerX = getWidth() / 2 - 100;
    int centerY = getHeight() / 2 - 15;

    noiseToggle.setBounds(centerX, centerY, 100, 30);
    iso226Toggle.setBounds(centerX + 110, centerY, 100, 30); // Beside noise toggle
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::initializeISO226Filter()
{
    // Calculate delta: 80 phon - 60 phon (difference in dB)
    std::array<double, 29> delta;
    for (size_t i = 0; i < delta.size(); ++i)
    {
        delta[i] = iso226::PhonData::phon80[i] - iso226::PhonData::phon60[i];
    }
    
    // Normalize to linear magnitude response (convert dB to linear)
    // and normalize relative to 1000 Hz (index 17)
    double refGainDb = delta[17]; // 1000 Hz reference
    
    double sum = 0.0;
    for (size_t i = 0; i < firCoefficients.size(); ++i)
    {
        double gainDb = delta[i] - refGainDb; // Normalize
        firCoefficients[i] = std::pow(10.0f, gainDb / 20.0f); // Convert to linear
        sum += firCoefficients[i];
    }
    
    // Normalize coefficients to sum to 1.0 (preserve signal level)
    for (size_t i = 0; i < firCoefficients.size(); ++i)
    {
        firCoefficients[i] /= sum;
    }
    
    // Clear filter buffers
    for (auto& buffer : filterBuffers)
        buffer.fill(0.0f);
}
