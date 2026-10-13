#include "MainComponent.h"
#include "iso226_data.h"
#include "Telemetry.h"
#include "Logger.h"
#include "LogWindow.h"

//==============================================================================
MainComponent::MainComponent()
{
    logger::init();

    // Load persistent settings first so scale is known before sizing
    juce::PropertiesFile::Options pOpts;
    pOpts.applicationName = "jafnhar";
    pOpts.filenameSuffix = ".settings";
    pOpts.osxLibrarySubFolder = "Application Support";
    pOpts.folderName = "jafnhar";
    appProperties.setStorageParameters(pOpts);
    auto* props = appProperties.getUserSettings();
    uiScale = (float)props->getDoubleValue("uiScale", 1.0);

    actualPhon = (double)juce::roundToInt(props->getDoubleValue("actualPhon", 60.0));
    targetPhon = (double)juce::roundToInt(props->getDoubleValue("targetPhon", 80.0));

    spectrumMode = (SpectrumMode)juce::jlimit(0, 2, (int)props->getIntValue("spectrumMode", 2));
    spectrumAudioMode.store((int)spectrumMode, std::memory_order_relaxed);
    spectrumFftData.assign(spectrumFftSize * 2, 0.0f);
    spectrumDb.assign(spectrumNumBins, -100.0f);

    loudnessEnabled = props->getBoolValue("loudnessEnabled", false);
    loudTargetI   = (float)props->getDoubleValue("loudnessTargetI", -23.0);
    loudTargetLra = (float)props->getDoubleValue("loudnessTargetLRA", 10.0);
    loudTargetTp  = (float)props->getDoubleValue("loudnessTargetTP", -1.0);

    auto installId = props->getValue("installId");
    if (installId.isEmpty())
    {
        installId = juce::Uuid().toString();
        props->setValue("installId", installId);
    }

    setSize (juce::roundToInt(1280.0f * uiScale), juce::roundToInt(536.0f * uiScale));

    spdlog::info("jafnhar v{} starting — OS: {} — spdlog v{}.{}.{}",
                 ProjectInfo::versionString,
                 juce::SystemStats::getOperatingSystemName().toStdString(),
                 SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH
    );

    setWantsKeyboardFocus(true);

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

    actualPhonSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    actualPhonSlider.setRange(20.0, 100.0, 1.0);
    actualPhonSlider.setValue(actualPhon, juce::dontSendNotification);
    actualPhonSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    actualPhonSlider.onValueChange = [this] {
        actualPhon = (double)juce::roundToInt(actualPhonSlider.getValue());
        actualPhonLabel.setText(juce::String((int)actualPhon), juce::dontSendNotification);
        updateFreqResponse(currentSampleRate);
        repaint();
    };
    actualPhonSlider.onDragEnd = [this] {
        rebuildFilter();
        appProperties.getUserSettings()->setValue("actualPhon", juce::roundToInt(actualPhon));
        appProperties.saveIfNeeded();
    };
    addAndMakeVisible(actualPhonSlider);
    setupPhonLabel(actualPhonLabel, juce::String((int)actualPhon));
    addAndMakeVisible(actualPhonLabel);

    setupPhonLabel(actualTitle, "Actual");
    addAndMakeVisible(actualTitle);

    targetPhonSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    targetPhonSlider.setRange(20.0, 100.0, 1.0);
    targetPhonSlider.setValue(targetPhon, juce::dontSendNotification);
    targetPhonSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    targetPhonSlider.onValueChange = [this] {
        targetPhon = (double)juce::roundToInt(targetPhonSlider.getValue());
        targetPhonLabel.setText(juce::String((int)targetPhon), juce::dontSendNotification);
        updateFreqResponse(currentSampleRate);
        repaint();
    };
    targetPhonSlider.onDragEnd = [this] {
        rebuildFilter();
        appProperties.getUserSettings()->setValue("targetPhon", juce::roundToInt(targetPhon));
        appProperties.saveIfNeeded();
    };
    addAndMakeVisible(targetPhonSlider);
    setupPhonLabel(targetPhonLabel, juce::String((int)targetPhon));
    addAndMakeVisible(targetPhonLabel);

    setupPhonLabel(targetTitle, "Target");
    addAndMakeVisible(targetTitle);

    setupPhonLabel(phonUnitLabel, "phon");
    addAndMakeVisible(phonUnitLabel);

    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(1.0, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.onValueChange = [this] {
        masterVolume = volumeSlider.getValue();
        volumeLabel.setText(juce::String(juce::roundToInt(masterVolume * 100.0)), juce::dontSendNotification);
        appProperties.getUserSettings()->setValue("masterVolume", masterVolume);
        appProperties.saveIfNeeded();
    };
    addAndMakeVisible(volumeSlider);
    setupPhonLabel(volumeLabel, "100");
    addAndMakeVisible(volumeLabel);
    setupPhonLabel(volumeTitle, "Master Volume");
    addAndMakeVisible(volumeTitle);

    loudnessToggle.setButtonText("Normalize");
    loudnessToggle.setToggleState(loudnessEnabled, juce::dontSendNotification);
    loudnessToggle.onClick = [this] {
        loudnessEnabled = loudnessToggle.getToggleState();
        loudnessNormalizer.setEnabled(loudnessEnabled);
        appProperties.getUserSettings()->setValue("loudnessEnabled", loudnessEnabled);
        appProperties.saveIfNeeded();
    };
    loudnessToggle.setLookAndFeel(&loudnessLnf);
    addAndMakeVisible(loudnessToggle);

    loudnessResetButton.setLookAndFeel(&loudnessLnf);
    loudnessResetButton.onClick = [this] { resetLoudness(); };
    addAndMakeVisible(loudnessResetButton);

    auto setupTargetSlider = [this](juce::Slider& s, double lo, double hi, double step)
    {
        s.setLookAndFeel(&loudnessLnf);
        s.setSliderStyle(juce::Slider::IncDecButtons);
        s.setRange(lo, hi, step);
        s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 20);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::darkgrey.darker(0.6f));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey.withAlpha(0.5f));
        addAndMakeVisible(s);
    };
    setupTargetSlider(targetISlider,  -40.0, -5.0, 0.1);
    setupTargetSlider(targetLraSlider,  0.0, 30.0, 0.1);
    setupTargetSlider(targetTpSlider,  -9.0,  0.0, 0.1);
    targetISlider.setValue(loudTargetI, juce::dontSendNotification);
    targetLraSlider.setValue(loudTargetLra, juce::dontSendNotification);
    targetTpSlider.setValue(loudTargetTp, juce::dontSendNotification);

    targetISlider.onValueChange = [this] {
        loudTargetI = (float)targetISlider.getValue();
        loudnessNormalizer.setTargets(loudTargetI, loudTargetLra, loudTargetTp);
        appProperties.getUserSettings()->setValue("loudnessTargetI", (double)loudTargetI);
        appProperties.saveIfNeeded();
    };
    targetLraSlider.onValueChange = [this] {
        loudTargetLra = (float)targetLraSlider.getValue();
        loudnessNormalizer.setTargets(loudTargetI, loudTargetLra, loudTargetTp);
        appProperties.getUserSettings()->setValue("loudnessTargetLRA", (double)loudTargetLra);
        appProperties.saveIfNeeded();
    };
    targetTpSlider.onValueChange = [this] {
        loudTargetTp = (float)targetTpSlider.getValue();
        loudnessNormalizer.setTargets(loudTargetI, loudTargetLra, loudTargetTp);
        appProperties.getUserSettings()->setValue("loudnessTargetTP", (double)loudTargetTp);
        appProperties.saveIfNeeded();
    };
    loudnessNormalizer.setEnabled(loudnessEnabled);
    loudnessNormalizer.setTargets(loudTargetI, loudTargetLra, loudTargetTp);

    vikingFont = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(
            BinaryData::Viking_ttf, BinaryData::Viking_ttfSize)));

    appTitle.setText("jafnhar", juce::dontSendNotification);
    appTitle.setJustificationType(juce::Justification::centred);
    appTitle.setFont(vikingFont.withHeight(16.0f));
    appTitle.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(appTitle);

    auto savedDeviceId = props->getValue("midiDeviceId", "");
    midiSourceCC = props->getIntValue("midiSourceCC", -1);
    midiTargetCC = props->getIntValue("midiTargetCC", -1);
    midiVolumeCC = props->getIntValue("midiVolumeCC", -1);
    masterVolume = props->getDoubleValue("masterVolume", 1.0);
    volumeSlider.setValue(masterVolume, juce::dontSendNotification);
    volumeLabel.setText(juce::String(juce::roundToInt(masterVolume * 100.0)), juce::dontSendNotification);

    // MIDI device selection now lives in the gear settings menu
    if (savedDeviceId.isNotEmpty())
        openMidiDevice(savedDeviceId);
    if (midiInput == nullptr)
    {
        auto devices = juce::MidiInput::getAvailableDevices();
        if (!devices.isEmpty())
            openMidiDevice(devices[0].identifier);
    }

    // MIDI learn buttons
    sourceLearnBtn.onClick = [this] {
        learningForSource = true;
        learningForTarget = false;
        sourceLearnBtn.setButtonText("Learn...");
        targetLearnBtn.setButtonText("L");
    };
    addAndMakeVisible(sourceLearnBtn);

    targetLearnBtn.onClick = [this] {
        learningForTarget = true;
        learningForSource = false;
        targetLearnBtn.setButtonText("Learn...");
        sourceLearnBtn.setButtonText("L");
    };
    addAndMakeVisible(targetLearnBtn);

    volumeLearnBtn.onClick = [this] {
        learningForVolume = true;
        learningForSource = false;
        learningForTarget = false;
        volumeLearnBtn.setButtonText("Learn...");
        sourceLearnBtn.setButtonText("L");
        targetLearnBtn.setButtonText("L");
    };
    addAndMakeVisible(volumeLearnBtn);

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

    minimiseButton.setButtonText("-");
    minimiseButton.onClick = [this] {
        if (auto* rw = findParentComponentOfClass<juce::ResizableWindow>())
            rw->setMinimised(true);
    };
    addAndMakeVisible(minimiseButton);

    closeButton.setButtonText("X");
    closeButton.onClick = [this] {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    };
    addAndMakeVisible(closeButton);

    settingsButton.onClick = [this] {
        juce::PopupMenu menu;

        juce::PopupMenu scaleMenu;
        std::vector<float> scales = { 0.50f, 0.75f, 0.90f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f };
        for (auto s : scales)
        {
            auto label = juce::String((int)(s * 100.0f)) + "%";
            scaleMenu.addItem((int)(s * 100.0f), label, true, std::abs(uiScale - s) < 0.001f);
        }
        menu.addSubMenu("UI Scale", scaleMenu);

        juce::PopupMenu spectrumMenu;
        spectrumMenu.addItem(1, "Input",  true, spectrumMode == SpectrumMode::input);
        spectrumMenu.addItem(2, "Output", true, spectrumMode == SpectrumMode::output);
        spectrumMenu.addItem(3, "Off",    true, spectrumMode == SpectrumMode::off);
        menu.addSubMenu("Spectrum", spectrumMenu);

        juce::PopupMenu midiMenu;
        auto midiDevices = juce::MidiInput::getAvailableDevices();
        if (midiDevices.isEmpty())
            midiMenu.addItem(1000, "No MIDI inputs", false, false);
        else
            for (int i = 0; i < midiDevices.size(); ++i)
                midiMenu.addItem(1000 + i, midiDevices[i].name, true,
                                 midiDevices[i].identifier == selectedMidiDeviceId);
        menu.addSubMenu("MIDI Device", midiMenu);

        menu.showMenuAsync(juce::PopupMenu::Options().withMinimumWidth(100),
            [this](int result) {
                if (result >= 1000) {
                    auto devices = juce::MidiInput::getAvailableDevices();
                    auto idx = result - 1000;
                    if (idx < devices.size())
                        openMidiDevice(devices[idx].identifier);
                }
                else if (result == 1 || result == 2 || result == 3) {
                    spectrumMode = (result == 1) ? SpectrumMode::input
                                 : (result == 2) ? SpectrumMode::output
                                                 : SpectrumMode::off;
                    spectrumAudioMode.store((int)spectrumMode, std::memory_order_relaxed);
                    appProperties.getUserSettings()->setValue("spectrumMode", (int)spectrumMode);
                    appProperties.saveIfNeeded();
                    repaint();
                }
                else if (result >= 50) {
                    auto newScale = (float)result / 100.0f;
                    setUIScale(newScale);
                }
            });
    };
    addAndMakeVisible(settingsButton);

    logButton.onClick = [this] {
        if (!logWindow)
            logWindow = std::make_unique<LogWindow>();
        logWindow->centreAroundComponent(this, logWindow->getWidth(), logWindow->getHeight());
        logWindow->setVisible(true);
    };
    addAndMakeVisible(logButton);

    Telemetry::sendPing(installId);

    startTimer (42); // ~23.8 fps; both 24 and 23.976 fps round to 42 ms
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    appProperties.saveIfNeeded();
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

    loudnessNormalizer.prepare(sampleRate, samplesPerBlockExpected);
    loudnessNormalizer.setEnabled(loudnessEnabled);
    loudnessNormalizer.setTargets(loudTargetI, loudTargetLra, loudTargetTp);

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

    int spectrumAudioTap = spectrumAudioMode.load(std::memory_order_relaxed);

    auto captureSpectrumTap = [&](int chL, int chR)
    {
        auto* l = bufferToFill.buffer->getReadPointer(chL, bufferToFill.startSample);
        auto* r = bufferToFill.buffer->getReadPointer(chR, bufferToFill.startSample);
        int pos = spectrumWritePos.load(std::memory_order_relaxed);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            spectrumRing[(size_t)(pos & (spectrumFftSize - 1))] = (l[i] + r[i]) * 0.5f;
            ++pos;
        }
        spectrumWritePos.store(pos, std::memory_order_release);
    };

    if (spectrumAudioTap == (int)SpectrumMode::input)
        captureSpectrumTap(4, 5); // raw ASIO input channels 5 & 6, before they are cleared

    for (int ch = 0; ch < 6; ++ch)
    {
        auto* out = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
        juce::dsp::AudioBlock<float> block(*bufferToFill.buffer); // block contains all channels
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
        }
        else
        {
            juce::FloatVectorOperations::clear(out, bufferToFill.numSamples);
        }
    }

    // Loudness normalization: input tap (post-ISO, pre-gain) -> gain -> output tap (pre-volume)
    auto* outL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* outR = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
    loudnessNormalizer.process(outL, outR, bufferToFill.numSamples);

    // Master volume + output channel meters
    for (int ch = 0; ch < 6; ++ch)
    {
        auto* out = bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);
        float peak = 0.0f;
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            peak = std::max(peak, std::abs(out[i]));
        if (masterVolume < 1.0f && (ch == 0 || ch == 1))
        {
            juce::FloatVectorOperations::multiply(out, (float)masterVolume, bufferToFill.numSamples);
            peak *= (float)masterVolume;
        }
        outputLevels[ch] = std::max(outputLevels[ch] * 0.95f, peak);
    }

    if (spectrumAudioTap == (int)SpectrumMode::output)
        captureSpectrumTap(0, 1); // processed output channels 1 & 2
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
    g.addTransform(juce::AffineTransform::scale(uiScale));

    float w = getWidth() / uiScale;
    float h = getHeight() / uiScale;

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto* device = deviceManager.getCurrentAudioDevice();
    if (device)
    {
        auto name = device->getName();
        if (name != lastDeviceName)
        {
            spdlog::info("Audio device found: {}", name.toStdString());
            lastDeviceName = name;
        }
        noDeviceLogged = false;
    }
    else if (!noDeviceLogged)
    {
        spdlog::warn("No audio device available");
        noDeviceLogged = true;
        lastDeviceName = {};
    }

    juce::String info = device ? device->getName() : "No device";
    g.setColour (juce::Colours::white);
    g.drawText (info, 10, 10, 300, 20, juce::Justification::left);

    int meterHeight = (int)h - 80;
    int barWidth = 4;
    int spacing = 10;
    int meterTop = 40;
    int labelAreaWidth = 50;
    int inputMeterStartX = labelAreaWidth + 10;

    int windowWidth = (int)w;
    int outputLabelAreaWidth = 50;
    int outputMeterStartX = windowWidth - outputLabelAreaWidth - (6 * spacing) - 10;

    auto meterPos = [](float db) -> float
    {
        if (db > -20.0f) return 1.0f - 0.45f * (db / -20.0f);
        if (db > -60.0f) return 0.55f - 0.45f * ((db + 20.0f) / -40.0f);
        return 0.10f - 0.10f * ((db + 60.0f) / -46.0f);
    };

    const float labelDbs[] = { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -24.0f,
                               -36.0f, -48.0f, -72.0f, -96.0f };

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
    for (float db : labelDbs)
    {
        float normalizedPos = juce::jlimit(0.0f, 1.0f, meterPos(db));
        int y = meterTop + meterHeight - (int)(normalizedPos * meterHeight);
        g.drawText (juce::String ((int)db), 5, y - 4, 40, 8, juce::Justification::right);
    }

    // Right side labels (outputs)
    for (float db : labelDbs)
    {
        float normalizedPos = juce::jlimit(0.0f, 1.0f, meterPos(db));
        int y = meterTop + meterHeight - (int)(normalizedPos * meterHeight);
        g.drawText (juce::String ((int)db), windowWidth - 45, y - 4, 40, 8, juce::Justification::left);
    }

    g.setColour (juce::Colours::white);

    // Input meters
    for (int i = 0; i < 6; ++i)
    {
        float dBFS = 20.0f * std::log10(inputLevels[i]);
        float normalizedPos = juce::jlimit(0.0f, 1.0f, meterPos(dBFS));

        int barHeight = (int)(normalizedPos * meterHeight);
        int x = inputMeterStartX + i * spacing;
        int y = meterTop + meterHeight - barHeight;
        g.fillRect (x, y, barWidth, barHeight);
    }

    // Output meters
    for (int i = 0; i < 6; ++i)
    {
        float dBFS = 20.0f * std::log10(outputLevels[i]);
        float normalizedPos = juce::jlimit(0.0f, 1.0f, meterPos(dBFS));

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

        // Spectrum analyzer (own unlabeled scale, above grid, behind curve)
        if (spectrumMode != SpectrumMode::off && !spectrumDb.empty())
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(plotBounds);

            auto spectrumYForDb = [&](float db) -> float {
                float norm = juce::jlimit(0.0f, 1.0f, meterPos(db));
                return (float)plotBounds.getBottom() - norm * (float)plotBounds.getHeight();
            };

            juce::Path spectrumPath;
            int x0 = plotBounds.getX(), x1 = plotBounds.getRight();
            float freqSpan = logFreqMax - logFreqMin;
            int lastBin = spectrumNumBins - 1;
            for (int x = x0; x <= x1; ++x)
            {
                float p = (float)(x - x0) / (float)(x1 - x0);
                float f = std::pow(10.0f, logFreqMin + p * freqSpan);
                float binPos = f / (float)nyquist * (float)lastBin;
                int b0 = juce::jlimit(0, lastBin, (int)binPos);
                int b1 = juce::jmin(b0 + 1, lastBin);
                float t = binPos - (float)b0;
                float db = spectrumDb[(size_t)b0] + t * (spectrumDb[(size_t)b1] - spectrumDb[(size_t)b0]);
                float y = spectrumYForDb(db);
                if (x == x0) spectrumPath.startNewSubPath((float)x, y);
                else spectrumPath.lineTo((float)x, y);
            }

            juce::Path filled = spectrumPath;
            filled.lineTo((float)x1, (float)plotBounds.getBottom());
            filled.lineTo((float)x0, (float)plotBounds.getBottom());
            filled.closeSubPath();

            g.setColour(juce::Colours::grey.withAlpha(0.35f));
            g.fillPath(filled);
            g.setColour(juce::Colours::lightgrey.withAlpha(0.9f));
            g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
        }

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

    // Loudness normalization block (bottom-right blank area)
    {
        const int x0 = 430, xMid = 675, x1 = 935;
        const int leftRuleX = 426;
        const int topY = 404, hRuleY = 501, bottomY = 534;

        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.drawLine((float)leftRuleX, (float)topY, (float)leftRuleX, (float)bottomY, 1.0f);
        g.drawLine((float)xMid, (float)topY, (float)xMid, (float)hRuleY, 1.0f);
        g.drawLine((float)x0, (float)hRuleY, (float)x1, (float)hRuleY, 1.0f);

        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.setColour(juce::Colours::lightgrey);
        g.drawText("INPUT", 438, 404, 232, 16, juce::Justification::left);
        g.drawText("OUTPUT", 682, 404, 250, 16, juce::Justification::left);

        auto drawStat = [&](const char* name, float v, const char* unit,
                            int x, int y, int nameW, int valRight)
        {
            g.setColour(juce::Colours::grey);
            g.drawText(name, x, y, nameW, 18, juce::Justification::left);
            juce::String text = "--";
            if (v > -200.0f)
                text = juce::String(v, 1) + " " + unit;
            g.setColour(juce::Colours::white);
            g.drawText(text, x + nameW, y, valRight - (x + nameW), 18, juce::Justification::right);
        };

        const int rowY[4] = { 422, 442, 462, 482 };
        drawStat("M",   loudInM,   "LUFS", 438, rowY[0], 36, 668);
        drawStat("I",   loudInI,   "LUFS", 438, rowY[1], 36, 668);
        drawStat("LRA", loudInLra, "LU",   438, rowY[2], 36, 668);
        drawStat("TP",  loudInTp,  "dBTP", 438, rowY[3], 36, 668);

        drawStat("M",   loudOutM,   "LUFS", 682, rowY[0], 36, 930);
        drawStat("I",   loudOutI,   "LUFS", 682, rowY[1], 36, 930);
        drawStat("LRA", loudOutLra, "LU",   682, rowY[2], 36, 930);
        drawStat("TP",  loudOutTp,  "dBTP", 682, rowY[3], 36, 930);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(juce::Colours::grey);
        g.drawText("I",   430, 503, 95, 10, juce::Justification::left);
        g.drawText("LRA", 528, 503, 95, 10, juce::Justification::left);
        g.drawText("TP",  626, 503, 95, 10, juce::Justification::left);
    }
}

void MainComponent::resized()
{
    auto S = [this](int v) { return juce::roundToInt(v * uiScale); };

    int meterHeight = getHeight() - S(60);
    int spacing = S(10);
    int inputMeterStartX = S(60);

    int plotX = inputMeterStartX + 6 * spacing + S(15);
    int plotY = S(40);
    int plotH = meterHeight - S(115);

    int knobY = plotY + plotH + S(34);
    int knobSize = S(80);
    int knobGap = S(34);

    int actualKnobX = plotX;
    int tgtKnobX = plotX + knobSize + knobGap;

    actualPhonSlider.setBounds(actualKnobX, knobY, knobSize, knobSize);
    targetPhonSlider.setBounds(tgtKnobX, knobY, knobSize, knobSize);

    int titleY = knobY - S(18);
    int titleH = S(16);
    actualTitle.setBounds(actualKnobX, titleY, knobSize, titleH);
    targetTitle.setBounds(tgtKnobX, titleY, knobSize, titleH);

    int labelY = knobY + knobSize + S(2);
    int labelH = S(16);
    actualPhonLabel.setBounds(actualKnobX, labelY, knobSize, labelH);
    targetPhonLabel.setBounds(tgtKnobX, labelY, knobSize, labelH);
    phonUnitLabel.setBounds(actualKnobX + knobSize, labelY, knobGap, labelH);

    int volKnobX = getWidth() - knobSize - S(135);
    volumeSlider.setBounds(volKnobX, knobY, knobSize, knobSize);
    volumeTitle.setBounds(volKnobX, titleY, knobSize, titleH);
    volumeLabel.setBounds(volKnobX, labelY, knobSize, labelH);
    volumeLearnBtn.setBounds(volKnobX - S(28) - S(4), knobY + S(30), S(28), S(20));

    sourceLearnBtn.setBounds(actualKnobX + knobSize + S(4), knobY + S(30), S(28), S(20));
    targetLearnBtn.setBounds(tgtKnobX + knobSize + S(4), knobY + S(30), S(28), S(20));

    bypassToggle.setBounds(S(8), getHeight() - S(22) - S(4), S(84), S(22));

    loudnessLnf.fontHeight = 11.0f * uiScale;
    targetISlider.sendLookAndFeelChange();
    targetLraSlider.sendLookAndFeelChange();
    targetTpSlider.sendLookAndFeelChange();

    targetISlider.setBounds(S(430), S(511), S(95), S(22));
    targetLraSlider.setBounds(S(528), S(511), S(95), S(22));
    targetTpSlider.setBounds(S(626), S(511), S(95), S(22));
    loudnessToggle.setBounds(S(732), S(511), S(104), S(22));
    loudnessResetButton.setBounds(S(846), S(511), S(88), S(22));

    int btnSize = S(20);
    int btnTop = S(8);
    int btnGap = S(4);
    closeButton.setBounds(getWidth() - btnSize - S(8), btnTop, btnSize, btnSize);
    minimiseButton.setBounds(getWidth() - 2 * btnSize - S(8) - btnGap, btnTop, btnSize, btnSize);

    int gearSize = S(24);
    int gearX = getWidth() - 2 * btnSize - gearSize - S(8) - 2 * btnGap;
    settingsButton.setBounds(gearX, btnTop - S(2), gearSize, gearSize);

    int logBtnW = S(36);
    int logBtnH = S(18);
    logButton.setBounds(getWidth() - logBtnW - S(8),
                        getHeight() - logBtnH - S(4),
                        logBtnW, logBtnH);

    appTitle.setBounds(0, S(8), getWidth(), S(20));
    appTitle.setFont(vikingFont.withHeight(16.0f * uiScale));

    auto fontOpts = juce::FontOptions(13.0f * uiScale);
    actualPhonLabel.setFont(fontOpts);
    targetPhonLabel.setFont(fontOpts);
    actualTitle.setFont(fontOpts);
    targetTitle.setFont(fontOpts);
    phonUnitLabel.setFont(fontOpts);
    volumeLabel.setFont(fontOpts);
    volumeTitle.setFont(fontOpts);
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    learningForSource = false;
    learningForTarget = false;
    learningForVolume = false;
    sourceLearnBtn.setButtonText("L");
    targetLearnBtn.setButtonText("L");
    volumeLearnBtn.setButtonText("L");

    if (event.y < juce::roundToInt(40.0f * uiScale))
        windowDragger.startDraggingComponent(getTopLevelComponent(), event);
}

void MainComponent::mouseDrag(const juce::MouseEvent& event)
{
    windowDragger.dragComponent(getTopLevelComponent(), event, nullptr);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == 'b' || key.getKeyCode() == 'B')
    {
        bypassToggle.setToggleState(!bypassToggle.getToggleState(), juce::sendNotification);
        return true;
    }
    return false;
}

void MainComponent::timerCallback()
{
    updateSpectrum();
    updateLoudnessReadouts();
    repaint();
}

void MainComponent::updateLoudnessReadouts()
{
    const auto& in = loudnessNormalizer.getInputMeter();
    const auto& out = loudnessNormalizer.getOutputMeter();

    loudInM = in.getMomentaryLufs();
    loudInI = in.getIntegratedLufs();
    loudInLra = in.getLoudnessRange();
    loudInTp = in.getTruePeakDb();

    loudOutM = out.getMomentaryLufs();
    loudOutI = out.getIntegratedLufs();
    loudOutLra = out.getLoudnessRange();
    loudOutTp = out.getTruePeakDb();

    loudGainDb = loudnessNormalizer.getAppliedGainDb();
}

void MainComponent::resetLoudness()
{
    loudnessNormalizer.reset();
    loudInM = loudInI = loudInLra = loudInTp = LoudnessMeter::invalidValue;
    loudOutM = loudOutI = loudOutLra = loudOutTp = LoudnessMeter::invalidValue;
    repaint();
}

void MainComponent::openMidiDevice(const juce::String& identifier)
{
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (d.identifier != identifier)
            continue;

        if (d.name != lastMidiDeviceName)
        {
            spdlog::info("MIDI device found: {}", d.name.toStdString());
            lastMidiDeviceName = d.name;
        }

        midiInput.reset();
        midiInput = juce::MidiInput::openDevice(d.identifier, this);
        if (midiInput)
            midiInput->start();

        selectedMidiDeviceId = d.identifier;
        appProperties.getUserSettings()->setValue("midiDeviceId", d.identifier);
        appProperties.saveIfNeeded();
        return;
    }
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!message.isController())
        return;

    auto cc = message.getControllerNumber();
    auto val = message.getControllerValue();

    if (learningForSource) {
        juce::MessageManager::callAsync([this, cc] {
            midiSourceCC = cc;
            learningForSource = false;
            sourceLearnBtn.setButtonText("L");
            appProperties.getUserSettings()->setValue("midiSourceCC", midiSourceCC);
            appProperties.saveIfNeeded();
        });
        return;
    }
    if (learningForTarget) {
        juce::MessageManager::callAsync([this, cc] {
            midiTargetCC = cc;
            learningForTarget = false;
            targetLearnBtn.setButtonText("L");
            appProperties.getUserSettings()->setValue("midiTargetCC", midiTargetCC);
            appProperties.saveIfNeeded();
        });
        return;
    }
    if (learningForVolume) {
        juce::MessageManager::callAsync([this, cc] {
            midiVolumeCC = cc;
            learningForVolume = false;
            volumeLearnBtn.setButtonText("L");
            appProperties.getUserSettings()->setValue("midiVolumeCC", midiVolumeCC);
            appProperties.saveIfNeeded();
        });
        return;
    }

    if (cc == midiSourceCC) {
        int idx = juce::jlimit(20, 100, juce::roundToInt(20.0f + val / 127.0f * 80.0f));
        juce::MessageManager::callAsync([this, idx] {
            actualPhonSlider.setValue((double)idx);
            rebuildFilter();
            appProperties.getUserSettings()->setValue("actualPhon", idx);
            appProperties.saveIfNeeded();
        });
    }
    if (cc == midiTargetCC) {
        int idx = juce::jlimit(20, 100, juce::roundToInt(20.0f + val / 127.0f * 80.0f));
        juce::MessageManager::callAsync([this, idx] {
            targetPhonSlider.setValue((double)idx);
            rebuildFilter();
            appProperties.getUserSettings()->setValue("targetPhon", idx);
            appProperties.saveIfNeeded();
        });
    }
    if (cc == midiVolumeCC) {
        double vol = juce::jmap((double)val, 0.0, 127.0, 0.0, 1.0);
        juce::MessageManager::callAsync([this, vol] {
            volumeSlider.setValue(vol);
        });
    }
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

void MainComponent::setUIScale(float scale)
{
    uiScale = scale;
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->setSize(juce::roundToInt(1280.0f * scale), juce::roundToInt(536.0f * scale));
    appProperties.getUserSettings()->setValue("uiScale", (double)scale);
    appProperties.saveIfNeeded();
    resized();
    repaint();
}

std::vector<float> MainComponent::buildIR(double sampleRate)
{
    if (actualPhon == targetPhon)
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

    auto getSPL = [](int freqIdx, double phon) -> double {
        const double pVals[] = { 20.0, 40.0, 60.0, 80.0, 100.0 };
        const std::array<double, 29>* pData[] = {
            &iso226::PhonData::phon20, &iso226::PhonData::phon40,
            &iso226::PhonData::phon60, &iso226::PhonData::phon80,
            &iso226::PhonData::phon100
        };
        if (phon <= pVals[0]) return (*pData[0])[freqIdx];
        if (phon >= pVals[4]) return (*pData[4])[freqIdx];
        for (int i = 0; i < 4; ++i)
            if (phon >= pVals[i] && phon <= pVals[i+1]) {
                double t = (phon - pVals[i]) / (pVals[i+1] - pVals[i]);
                return (*pData[i])[freqIdx] + t * ((*pData[i+1])[freqIdx] - (*pData[i])[freqIdx]);
            }
        return 0.0;
    };

    for (size_t i = 0; i < 29; ++i)
        delta.push_back(getSPL((int)i, actualPhon) - getSPL((int)i, targetPhon));

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

void MainComponent::updateSpectrum()
{
    if (spectrumMode == SpectrumMode::off)
        return;

    const int mask = spectrumFftSize - 1;
    int writePos = spectrumWritePos.load(std::memory_order_acquire);
    int start = writePos - spectrumFftSize;

    for (int i = 0; i < spectrumFftSize; ++i)
        spectrumFftData[(size_t)i] = spectrumRing[(size_t)((start + i) & mask)];

    spectrumWindow.multiplyWithWindowingTable(spectrumFftData.data(), (size_t)spectrumFftSize);
    spectrumFft.performFrequencyOnlyForwardTransform(spectrumFftData.data(), true);

    const float norm = (float)spectrumFftSize * 0.5f;
    for (int i = 0; i < spectrumNumBins; ++i)
    {
        float db = juce::Decibels::gainToDecibels(spectrumFftData[(size_t)i] / norm, -100.0f);
        float& s = spectrumDb[(size_t)i];
        float coeff = (db > s) ? 0.6f : 0.15f;
        s += (db - s) * coeff;
    }
}
