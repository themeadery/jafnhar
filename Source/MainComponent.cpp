#include "MainComponent.h"
#include "iso226_data.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (1280, 536);

    bypassToggle.setButtonText("Bypass");
    bypassToggle.setToggleState(false, juce::dontSendNotification); // Default to disabled
    bypassToggle.onClick = [this] { bypass = bypassToggle.getToggleState(); };
    addAndMakeVisible(bypassToggle);

    auto setupPhonLabel = [](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(13.0f));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    };

    sourcePhonSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sourcePhonSlider.setRange(0, 2, 1);
    sourcePhonSlider.setValue(1, juce::dontSendNotification);
    sourcePhonSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sourcePhonSlider.onValueChange = [this] {
        sourcePhonIdx = juce::roundToInt(sourcePhonSlider.getValue());
        static const char* phonLabels[] = { "40", "60", "80" };
        sourcePhonLabel.setText(phonLabels[sourcePhonIdx], juce::dontSendNotification);
        updateFreqResponse(currentSampleRate);
        repaint();
    };
    sourcePhonSlider.onDragEnd = [this] { rebuildFilter(); };
    addAndMakeVisible(sourcePhonSlider);
    setupPhonLabel(sourcePhonLabel, "60");
    addAndMakeVisible(sourcePhonLabel);

    setupPhonLabel(sourceTitle, "Source");
    addAndMakeVisible(sourceTitle);

    targetPhonSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    targetPhonSlider.setRange(0, 2, 1);
    targetPhonSlider.setValue(2, juce::dontSendNotification);
    targetPhonSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    targetPhonSlider.onValueChange = [this] {
        targetPhonIdx = juce::roundToInt(targetPhonSlider.getValue());
        static const char* phonLabels[] = { "40", "60", "80" };
        targetPhonLabel.setText(phonLabels[targetPhonIdx], juce::dontSendNotification);
        updateFreqResponse(currentSampleRate);
        repaint();
    };
    targetPhonSlider.onDragEnd = [this] { rebuildFilter(); };
    addAndMakeVisible(targetPhonSlider);
    setupPhonLabel(targetPhonLabel, "80");
    addAndMakeVisible(targetPhonLabel);

    setupPhonLabel(targetTitle, "Target");
    addAndMakeVisible(targetTitle);

    setupPhonLabel(phonUnitLabel, "phon");
    addAndMakeVisible(phonUnitLabel);

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
    currentSampleRate = sampleRate;
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
            if (!bypass)
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
            if (!bypass)
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
            meterHeight - 115
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
            g.drawText(juce::String((int)db), plotBounds.getX() - 32, y - 5, 30, 10, juce::Justification::right);
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
            float d = !bypass ? freqRespDb[i] : 0.0f;
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
    int plotH = meterHeight - 115;

    int knobY = plotY + plotH + 34;
    int knobSize = 80;
    int knobGap = 34;

    int srcKnobX = plotX;
    int tgtKnobX = plotX + knobSize + knobGap;

    sourcePhonSlider.setBounds(srcKnobX, knobY, knobSize, knobSize);
    targetPhonSlider.setBounds(tgtKnobX, knobY, knobSize, knobSize);

    int titleY = knobY - 18;
    int titleH = 16;
    sourceTitle.setBounds(srcKnobX, titleY, knobSize, titleH);
    targetTitle.setBounds(tgtKnobX, titleY, knobSize, titleH);

    int labelY = knobY + knobSize + 2;
    int labelH = 16;
    sourcePhonLabel.setBounds(srcKnobX, labelY, knobSize, labelH);
    targetPhonLabel.setBounds(tgtKnobX, labelY, knobSize, labelH);
    phonUnitLabel.setBounds(srcKnobX + knobSize, labelY, knobGap, labelH);

    bypassToggle.setBounds(tgtKnobX + knobSize + 25, knobY + 29, 110, 22);
}

void MainComponent::timerCallback()
{
    repaint();
}

void MainComponent::initializeISO226Filter(double sampleRate)
{
    currentSampleRate = sampleRate;
    auto impulse = buildIR(sampleRate);

    juce::AudioBuffer<float> irBuffer((int)impulse.size() > 1 ? 1 : 1, (int)impulse.size());
    irBuffer.copyFrom(0, 0, impulse.data(), (int)impulse.size());

    auto loadIR = [&](juce::dsp::Convolution& conv, juce::AudioBuffer<float> bufferToUse) {
        conv.loadImpulseResponse(
            std::move(bufferToUse),
            sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no
        );
    };

    loadIR(convolutionL, irBuffer);
    loadIR(convolutionR, std::move(irBuffer));
}

std::vector<float> MainComponent::buildIR(double sampleRate)
{
    if (sourcePhonIdx == targetPhonIdx)
    {
        const int halfTaps = 1024;
        freqRespFrequencies.resize(halfTaps + 1);
        freqRespDb.resize(halfTaps + 1);
        for (int i = 0; i <= halfTaps; ++i)
        {
            freqRespFrequencies[i] = (float)(i * sampleRate / 2048);
            freqRespDb[i] = 0.0f;
        }
        nyquist = sampleRate / 2.0;
        return { 1.0f };
    }

    double nyquistRate = sampleRate / 2.0;
    std::vector<double> frequencies(iso226::frequencies.begin(), iso226::frequencies.end());
    std::vector<double> delta;

    const std::array<double, 29>* phonData[] = {
        &iso226::PhonData::phon40,
        &iso226::PhonData::phon60,
        &iso226::PhonData::phon80
    };

    for (size_t i = 0; i < 29; ++i)
        delta.push_back((*phonData[sourcePhonIdx])[i] - (*phonData[targetPhonIdx])[i]);

    double extensionDelta = delta.back();
    for (double xf : iso226::xfreqs) {
        if (xf > nyquistRate)
            break;
        frequencies.push_back(xf);
        delta.push_back(extensionDelta);
    }

    if (frequencies.back() < nyquistRate) {
        frequencies.push_back(nyquistRate);
        delta.push_back(extensionDelta);
    }

    const int fftOrder = 11;
    juce::dsp::FFT fft(fftOrder);
    const int numTaps = fft.getSize();
    std::vector<juce::dsp::Complex<float>> freqDomain(numTaps);
    std::vector<juce::dsp::Complex<float>> timeDomain(numTaps);
    double refGainDb = delta[17];

    for (int i = 0; i <= numTaps / 2; ++i) {
        double binFreq = (double)i * sampleRate / numTaps;

        auto it = std::lower_bound(frequencies.begin(), frequencies.end(), binFreq);

        double gainDb;
        if (it == frequencies.begin()) {
            gainDb = delta[0] - refGainDb;
        } else if (it == frequencies.end()) {
            gainDb = delta.back() - refGainDb;
        } else {
            size_t idx2 = std::distance(frequencies.begin(), it);
            size_t idx1 = idx2 - 1;
            double f1 = frequencies[idx1], f2 = frequencies[idx2];
            double d1 = delta[idx1], d2 = delta[idx2];
            double t = (binFreq - f1) / (f2 - f1);
            gainDb = d1 + t * (d2 - d1) - refGainDb;
        }

        float magnitude = std::pow(10.0f, (float)gainDb / 20.0f);

        freqDomain[i] = { magnitude, 0.0f };
        if (i > 0 && i < numTaps / 2)
            freqDomain[numTaps - i] = { magnitude, 0.0f };
    }

    fft.perform(freqDomain.data(), timeDomain.data(), true);

    std::vector<float> impulse(numTaps);
    for (int i = 0; i < numTaps; ++i)
        impulse[(i + numTaps / 2) % numTaps] = timeDomain[i].real();

    juce::dsp::WindowingFunction<float> window(numTaps, juce::dsp::WindowingFunction<float>::hamming);
    window.multiplyWithWindowingTable(impulse.data(), numTaps);

    float sum = 0.0f;
    for (auto c : impulse) sum += std::abs(c);
    for (auto& c : impulse) c /= sum;

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

    nyquist = nyquistRate;
    return impulse;
}

void MainComponent::updateFreqResponse(double sampleRate)
{
    buildIR(sampleRate);
}

void MainComponent::rebuildFilter()
{
    if (currentSampleRate > 0.0)
        initializeISO226Filter(currentSampleRate);
}
