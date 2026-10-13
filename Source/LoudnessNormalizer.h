#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "LoudnessMeter.h"

class LoudnessNormalizer
{
public:
    LoudnessNormalizer();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Call from the audio thread. Measures the input, applies the dynamic gain,
    // then measures the output. Works on channels 0/1 in place.
    void process(float* left, float* right, int numSamples);

    // Call from the audio thread with the raw source channels (before the ISO 226
    // filter) to measure the true input loudness. Independent of process().
    void processSource(const float* left, const float* right, int numSamples)
    {
        sourceMeter.process(left, right, numSamples);
    }

    void setEnabled(bool shouldBeEnabled) { enabled.store(shouldBeEnabled, std::memory_order_relaxed); }
    bool isEnabled() const               { return enabled.load(std::memory_order_relaxed); }

    void setTargets(float integratedLufs, float lra, float truePeakDb)
    {
        targetIntegrated.store(integratedLufs, std::memory_order_relaxed);
        targetLra.store(lra, std::memory_order_relaxed);
        targetTruePeak.store(truePeakDb, std::memory_order_relaxed);
    }

    const LoudnessMeter& getInputMeter() const  { return inputMeter; }
    const LoudnessMeter& getOutputMeter() const { return outputMeter; }
    const LoudnessMeter& getSourceMeter() const { return sourceMeter; }
    float getAppliedGainDb() const              { return appliedGainDb.load(std::memory_order_relaxed); }

private:
    LoudnessMeter sourceMeter, inputMeter, outputMeter;
    std::atomic<float> targetIntegrated { -23.0f };
    std::atomic<float> targetLra { 10.0f };
    std::atomic<float> targetTruePeak { -1.0f };
    std::atomic<bool> enabled { false };
    std::atomic<bool> resetRequested { false };
    std::atomic<float> appliedGainDb { 0.0f };
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessNormalizer)
};
