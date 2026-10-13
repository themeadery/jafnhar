#include "LoudnessNormalizer.h"

LoudnessNormalizer::LoudnessNormalizer() = default;

void LoudnessNormalizer::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    sourceMeter.prepare(sampleRate, maxBlockSize);
    inputMeter.prepare(sampleRate, maxBlockSize);
    outputMeter.prepare(sampleRate, maxBlockSize);
    appliedGainDb.store(0.0f, std::memory_order_relaxed);
}

void LoudnessNormalizer::reset()
{
    sourceMeter.reset();
    inputMeter.reset();
    outputMeter.reset();
    resetRequested.store(true, std::memory_order_release);
}

void LoudnessNormalizer::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;

    if (resetRequested.exchange(false, std::memory_order_acq_rel))
        appliedGainDb.store(0.0f, std::memory_order_relaxed);

    inputMeter.process(left, right, numSamples);

    if (enabled.load(std::memory_order_relaxed))
    {
        float gainDb = appliedGainDb.load(std::memory_order_relaxed);
        float shortTerm = inputMeter.getShortTermLufs();

        if (shortTerm > -70.0f)
        {
            float loudnessGain = targetIntegrated.load(std::memory_order_relaxed) - shortTerm;
            float tpGain = targetTruePeak.load(std::memory_order_relaxed)
                         - inputMeter.getShortTruePeakDb();
            float targetDb = juce::jlimit(-20.0f, 20.0f, juce::jmin(loudnessGain, tpGain));

            float tc = (targetDb < gainDb) ? 0.5f : 2.0f; // faster down, slower up
            float coeff = (float) std::exp(-(double) numSamples / ((double) tc * sr));
            gainDb = targetDb + (gainDb - targetDb) * coeff;
            appliedGainDb.store(gainDb, std::memory_order_relaxed);
        }

        float gain = std::pow(10.0f, gainDb / 20.0f);
        if (std::abs(gain - 1.0f) > 1.0e-6f)
        {
            juce::FloatVectorOperations::multiply(left, gain, numSamples);
            juce::FloatVectorOperations::multiply(right, gain, numSamples);
        }
    }
    else
    {
        appliedGainDb.store(0.0f, std::memory_order_relaxed);
    }

    outputMeter.process(left, right, numSamples);
}
