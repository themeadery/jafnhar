#include "MainComponent.h"
#include "iso226_data.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (854, 358);

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

    initializeISO226Filter(sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlockExpected;
    spec.sampleRate = sampleRate;
    spec.numChannels = 1;

    firFilterL.prepare(spec);
    firFilterR.prepare(spec);

    firFilterL.reset();
    firFilterR.reset();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // input channel meters
    for (int ch = 0; ch < 6; ++ch)
    {
        auto* inData = bufferToFill.buffer->getReadPointer(ch, bufferToFill.startSample);
        float peak = 0.0f;
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            peak = std::max(peak, std::abs(inData[i]));
        inputLevels[ch] = std::max(inputLevels[ch] * 0.95f, peak);
    }

    for (int ch = 0; ch < 6; ++ch)
    {
        auto* out = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
        juce::dsp::AudioBlock<float> block(*bufferToFill.buffer); // block contains all channels
        float peak = 0.0f;
        if (ch == 0)
        {
            auto* in = bufferToFill.buffer->getReadPointer(4, bufferToFill.startSample);
            juce::FloatVectorOperations::copy(out, in, bufferToFill.numSamples);
            if (iso226Enabled)
            {
                auto singleChannelBlock = block.getSingleChannelBlock(ch); // gets just channel 0
                juce::dsp::ProcessContextReplacing<float> context(singleChannelBlock);
                firFilterL.process(context);
            }
            for (int i = 0; i < bufferToFill.numSamples; ++i)
                peak = std::max(peak, std::abs(out[i]));
        }
        else if (ch == 1)
        {
            auto* in = bufferToFill.buffer->getReadPointer(5, bufferToFill.startSample);
            juce::FloatVectorOperations::copy(out, in, bufferToFill.numSamples);
            if (iso226Enabled)
            {
                auto singleChannelBlock = block.getSingleChannelBlock(ch); // gets just channel 1
                juce::dsp::ProcessContextReplacing<float> context(singleChannelBlock);
                firFilterR.process(context);
            }
            for (int i = 0; i < bufferToFill.numSamples; ++i)
                peak = std::max(peak, std::abs(out[i]));
        }
        else
        {
            juce::FloatVectorOperations::clear(out, bufferToFill.numSamples);
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

    iso226Toggle.setBounds(centerX, centerY, 100, 30);
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::initializeISO226Filter(double sampleRate)
{
    // Extend frequencies to Nyquist following the ISO 226 pattern
    double nyquist = sampleRate / 2.0;
    std::vector<double> frequencies(iso226::frequencies.begin(), iso226::frequencies.end());
    std::vector<double> delta;

    for (size_t i = 0; i < 29; ++i)
        delta.push_back(iso226::PhonData::phon80[i] - iso226::PhonData::phon60[i]);

    // Continue pattern
    double ratio = std::pow(10.0, 0.1);
    double freq = iso226::frequencies[28];
    while (freq * ratio <= nyquist) {
        freq *= ratio;
        frequencies.push_back(freq);
        delta.push_back(delta[28]); // Hold 12,500 Hz value
    }

    // not even sure how to explain this, vector is of size 29 + up to nyquist
    std::vector<float> coeffs(frequencies.size());

    // Normalize to linear magnitude response relative to 1000 Hz
    double refGainDb = delta[17];
    double sum = 0.0;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        double gainDb = delta[i] - refGainDb;
        coeffs[i] = std::pow(10.0f, gainDb / 20.0f);
        sum += coeffs[i];
    }

    for (size_t i = 0; i < coeffs.size(); ++i)
        coeffs[i] /= sum;

    // Set coefficients for both filters
    firFilterL.coefficients = juce::dsp::FIR::Coefficients<float>::Ptr(new juce::dsp::FIR::Coefficients<float>(coeffs.data(), coeffs.size()));
    firFilterR.coefficients = juce::dsp::FIR::Coefficients<float>::Ptr(new juce::dsp::FIR::Coefficients<float>(coeffs.data(), coeffs.size()));
}
