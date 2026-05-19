#include "MainComponent.h"
#include "iso226_data.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (1280, 536);

    iso226Toggle.setButtonText("ISO 226 Filter");
    iso226Toggle.setTooltip("60 - 80 Phon delta");
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
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlockExpected;
    spec.numChannels = 1;

    convolutionL.prepare(spec);
    convolutionR.prepare(spec);

    initializeISO226Filter(sampleRate);
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
                auto singleChannelBlock = block.getSingleChannelBlock(0); // gets just channel 0
                juce::dsp::ProcessContextReplacing<float> context(singleChannelBlock);
                convolutionL.process(context);
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
                auto singleChannelBlock = block.getSingleChannelBlock(1); // gets just channel 1
                juce::dsp::ProcessContextReplacing<float> context(singleChannelBlock);
                convolutionR.process(context);
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

    // Frequency response plot
    if (!freqRespFrequencies.empty() && nyquist > 0.0)
    {
        juce::Rectangle<int> plotBounds(
            inputMeterStartX + 6 * spacing + 15,
            meterTop,
            outputMeterStartX - 15 - (inputMeterStartX + 6 * spacing + 15),
            meterHeight - 40
        );

        g.setColour(juce::Colours::darkgrey.darker(0.7f));
        g.fillRect(plotBounds);
        g.setColour(juce::Colours::lightgrey);
        g.drawRect(plotBounds, 1);

        float dbMin = 0.0f, dbMax = 0.0f;
        {
            float minVal = std::numeric_limits<float>::max();
            float maxVal = -std::numeric_limits<float>::max();
            for (auto v : freqRespDb) {
                minVal = std::min(minVal, v);
                maxVal = std::max(maxVal, v);
            }
            float padding = std::max(3.0f, (maxVal - minVal) * 0.1f);
            dbMin = std::floor((minVal - padding) / 5.0f) * 5.0f;
            dbMax = std::ceil((maxVal + padding) / 5.0f) * 5.0f;
            if (dbMax - dbMin < 20.0f) {
                float center = (dbMin + dbMax) / 2.0f;
                dbMin = center - 10.0f;
                dbMax = center + 10.0f;
            }
        }

        const float freqMin = 20.0f, freqMax = (float)nyquist;
        const float logFreqMin = std::log10(freqMin);
        const float logFreqMax = std::log10(freqMax);

        auto xForFreq = [&](float f) -> float {
            return plotBounds.getX() + (std::log10(std::max(f, freqMin)) - logFreqMin) / (logFreqMax - logFreqMin) * (float)plotBounds.getWidth();
        };
        auto yForDb = [&](float d) -> float {
            return (float)plotBounds.getBottom() - (d - dbMin) / (dbMax - dbMin) * (float)plotBounds.getHeight();
        };

        g.setFont(8.0f);
        float dashPattern[] = { 3.0f, 3.0f };

        // Horizontal grid lines
        for (float db = dbMin; db <= dbMax; db += 5.0f)
        {
            int y = (int)yForDb(db);
            g.setColour(juce::Colours::grey.withAlpha(0.4f));
            juce::Path dashPath;
            dashPath.startNewSubPath((float)plotBounds.getX(), (float)y);
            dashPath.lineTo((float)plotBounds.getRight(), (float)y);
            juce::PathStrokeType(0.5f).createDashedStroke(dashPath, dashPath, dashPattern, 2);
            g.strokePath(dashPath, juce::PathStrokeType(0.5f));

            g.setColour(juce::Colours::lightgrey);
            g.drawText(juce::String((int)db), plotBounds.getX() + 2, y - 5, 30, 10, juce::Justification::left);
        }

        // Vertical grid lines (REW-style log frequency scale)
        std::map<float, bool> tickMap;
        for (int exp = 0; exp <= 5; ++exp)
        {
            float decadeStart = std::pow(10.0f, (float)exp);
            for (int n = 1; n <= 9; ++n)
            {
                float f = n * decadeStart;
                if (f >= freqMin && f <= freqMax)
                    tickMap[f] = (n == 1 || n == 2 || n == 5);
            }
            float decadeEnd = 10.0f * decadeStart;
            if (decadeEnd >= freqMin && decadeEnd <= freqMax)
                tickMap[decadeEnd] = true;
        }
        if (tickMap.find(freqMax) == tickMap.end())
            tickMap[freqMax] = true;

        for (auto& [freq, major] : tickMap)
        {
            int x = (int)xForFreq(freq);

            if (major)
            {
                g.setColour(juce::Colours::grey.withAlpha(0.4f));
                juce::Path dashPath;
                dashPath.startNewSubPath((float)x, (float)plotBounds.getY());
                dashPath.lineTo((float)x, (float)plotBounds.getBottom());
                juce::PathStrokeType(0.5f).createDashedStroke(dashPath, dashPath, dashPattern, 2);
                g.strokePath(dashPath, juce::PathStrokeType(0.5f));

                g.setColour(juce::Colours::lightgrey);
                juce::String label;
                if (freq >= 1000.0f)
                    label = juce::String((int)(freq / 1000.0f)) + "k";
                else
                    label = juce::String((int)freq);
                g.drawText(label, x - 12, plotBounds.getBottom() + 2, 24, 10, juce::Justification::centred);
            }
            else
            {
                g.setColour(juce::Colours::grey.withAlpha(0.15f));
                g.drawVerticalLine(x, (float)plotBounds.getY(), (float)plotBounds.getBottom());
            }
        }

        // 0 dB reference line
        int zeroY = (int)yForDb(0.0f);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        juce::Path zeroPath;
        zeroPath.startNewSubPath((float)plotBounds.getX(), (float)zeroY);
        zeroPath.lineTo((float)plotBounds.getRight(), (float)zeroY);
        juce::PathStrokeType(0.5f).createDashedStroke(zeroPath, zeroPath, dashPattern, 2);
        g.strokePath(zeroPath, juce::PathStrokeType(0.5f));

        // Curve
        juce::Path curvePath;
        bool first = true;
        for (size_t i = 0; i < freqRespFrequencies.size(); ++i)
        {
            float f = freqRespFrequencies[i];
            float d = freqRespDb[i];
            if (f < freqMin || f > freqMax) continue;

            float px = xForFreq(f);
            float py = yForDb(d);

            if (first)
            {
                curvePath.startNewSubPath(px, py);
                first = false;
            }
            else
            {
                curvePath.lineTo(px, py);
            }
        }

        g.setColour(juce::Colours::orange);
        g.strokePath(curvePath, juce::PathStrokeType(1.5f));
    }
}

void MainComponent::resized()
{
    int meterHeight = getHeight() - 60;
    int spacing = 10;
    int inputMeterStartX = 60;

    int plotX = inputMeterStartX + 6 * spacing + 15;
    int plotY = 40;
    int plotH = meterHeight - 40;

    int toggleY = plotY + plotH + 5;
    int toggleH = 22;
    iso226Toggle.setBounds(plotX, toggleY, 110, toggleH);
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::initializeISO226Filter(double sampleRate)
{
    // 1. Calculate the ISO 226 Magnitude Curve
    double nyquistRate = sampleRate / 2.0;
    std::vector<double> frequencies(iso226::frequencies.begin(), iso226::frequencies.end());
    std::vector<double> delta;

    for (size_t i = 0; i < 29; ++i)
        delta.push_back(iso226::PhonData::phon60[i] - iso226::PhonData::phon80[i]);

    // Store the last valid delta value (12.5 kHz) to extend toward Nyquist
    double extensionDelta = delta.back();
    for (double xf : iso226::xfreqs) {
        if (xf > nyquistRate)
            break; // stop if the extended frequency exceeds Nyquist
        frequencies.push_back(xf); // add extended frequencies with R10 spacing
        delta.push_back(extensionDelta); // hold the last delta value constant for frequencies beyond 12.5 kHz
    }

    // Always add a point at nyquist
    if (frequencies.back() < nyquistRate) {
        frequencies.push_back(nyquistRate);
        delta.push_back(extensionDelta);
    }

    // 2. Prepare for programmatic Impulse Response generation
    const int fftOrder = 11; // 2^11 = 2048 taps
    juce::dsp::FFT fft(fftOrder);
    const int numTaps = fft.getSize();
    std::vector<juce::dsp::Complex<float>> freqDomain(numTaps);
    std::vector<juce::dsp::Complex<float>> timeDomain(numTaps);
    double refGainDb = delta[17]; // 1000 Hz reference

    // 3. Map Magnitudes to FFT Bins
    for (int i = 0; i <= numTaps / 2; ++i) {
        double binFreq = (double)i * sampleRate / numTaps;

        auto it = std::lower_bound(frequencies.begin(), frequencies.end(), binFreq);
        size_t idx = std::distance(frequencies.begin(), it);
        idx = std::min(idx, delta.size() - 1);

        double gainDb = delta[idx] - refGainDb;
        float magnitude = std::pow(10.0f, (float)gainDb / 20.0f);

        freqDomain[i] = { magnitude, 0.0f };
        if (i > 0 && i < numTaps / 2)
            freqDomain[numTaps - i] = { magnitude, 0.0f };
    }

    // 4. Transform to Time Domain (Inverse FFT)
    fft.perform(freqDomain.data(), timeDomain.data(), true);

    // 5. Center (Linear Phase), Window, and Normalize
    std::vector<float> impulse(numTaps);
    for (int i = 0; i < numTaps; ++i)
        impulse[(i + numTaps / 2) % numTaps] = timeDomain[i].real();

    // Create the windowing function object
    juce::dsp::WindowingFunction<float> window(numTaps, juce::dsp::WindowingFunction<float>::hamming);

    // Apply it to the data
    window.multiplyWithWindowingTable(impulse.data(), numTaps);

    float sum = 0.0f;
    for (auto c : impulse) sum += std::abs(c);
    for (auto& c : impulse) c /= sum;

    // Capture frequency response for visualization
    const int halfTaps = numTaps / 2;
    freqRespFrequencies.resize(halfTaps + 1);
    freqRespDb.resize(halfTaps + 1);

    std::vector<juce::dsp::Complex<float>> actualFreq(numTaps);
    std::vector<juce::dsp::Complex<float>> actualTime(numTaps);
    for (int i = 0; i < numTaps; ++i)
        actualTime[i] = { impulse[i], 0.0f };
    fft.perform(actualTime.data(), actualFreq.data(), false);

    for (int i = 0; i <= halfTaps; ++i)
    {
        freqRespFrequencies[i] = (float)(i * sampleRate / numTaps);
        freqRespDb[i] = 20.0f * std::log10(std::max(std::abs(actualFreq[i]), 1e-30f));
    }

    this->nyquist = nyquistRate;

    // 1. Create the buffer
    juce::AudioBuffer<float> irBuffer(1, (int)impulse.size());
    irBuffer.copyFrom(0, 0, impulse.data(), (int)impulse.size());

    // 2. Define the lambda to accept a buffer by value
    auto loadIR = [&](juce::dsp::Convolution& conv, juce::AudioBuffer<float> bufferToUse) {
        conv.loadImpulseResponse(
            std::move(bufferToUse),
            sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no
        );
        };

    // 3. Load them
    loadIR(convolutionL, irBuffer);            // Pass a COPY to the Left channel
    loadIR(convolutionR, std::move(irBuffer)); // Let the Right channel OWN the original
}
