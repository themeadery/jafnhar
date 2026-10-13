#include "LoudnessMeter.h"

LoudnessMeter::LoudnessMeter() = default;
LoudnessMeter::~LoudnessMeter() = default;

void LoudnessMeter::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    subBlockSamples = juce::jmax(1, (int) std::round(sampleRate * 0.1));
    buildKWeighting(sampleRate);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing((size_t) juce::jmax(1, maxBlockSize));
    truePeakBuffer.assign((size_t) juce::jmax(1, maxBlockSize) * 2, 0.0f);

    performReset();
}

void LoudnessMeter::reset()
{
    resetRequested.store(true, std::memory_order_release);
}

void LoudnessMeter::buildKWeighting(double sampleRate)
{
    // Stage 1: high-shelf pre-filter (BS.1770-4, coefficients re-derived for the sample rate)
    {
        const double f0 = 1681.974450955533;
        const double Q  = 0.7071752369554196;
        const double G  = 3.999843853973347;
        const double K  = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate);
        const double Vh = std::pow(10.0, 0.05 * G);
        const double Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;

        float b0 = (float)((Vh + Vb * K / Q + K * K) / a0);
        float b1 = (float)(2.0 * (K * K - Vh) / a0);
        float b2 = (float)((Vh - Vb * K / Q + K * K) / a0);
        float a1 = (float)(2.0 * (K * K - 1.0) / a0);
        float a2 = (float)((1.0 - K / Q + K * K) / a0);

        preL.set(b0, b1, b2, a1, a2);
        preR.set(b0, b1, b2, a1, a2);
    }

    // Stage 2: RLB high-pass (BS.1770-4)
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan(juce::MathConstants<double>::pi * f0 / sampleRate);
        const double a0 = 1.0 + K / Q + K * K;

        float b0 = 1.0f, b1 = -2.0f, b2 = 1.0f;
        float a1 = (float)(2.0 * (K * K - 1.0) / a0);
        float a2 = (float)((1.0 - K / Q + K * K) / a0);

        hpL.set(b0, b1, b2, a1, a2);
        hpR.set(b0, b1, b2, a1, a2);
    }
}

void LoudnessMeter::process(const float* left, const float* right, int numSamples)
{
    if (resetRequested.exchange(false, std::memory_order_acq_rel))
        performReset();

    for (int i = 0; i < numSamples; ++i)
    {
        float l = hpL.process(preL.process(left[i]));
        float r = hpR.process(preR.process(right[i]));
        sumL += (double)(l * l);
        sumR += (double)(r * r);

        if (++subBlockCount >= subBlockSamples)
            accumulateSubBlock();
    }

    if (oversampling != nullptr && numSamples > 0)
    {
        auto* tp = truePeakBuffer.data();
        for (int i = 0; i < numSamples; ++i)
        {
            tp[i] = left[i];
            tp[(size_t) numSamples + (size_t) i] = right[i];
        }

        float* channelPtrs[2] = { tp, tp + numSamples };
        juce::dsp::AudioBlock<float> block(channelPtrs, 2, (size_t) numSamples);
        auto up = oversampling->processSamplesUp(block);

        float peak = 0.0f;
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* p = up.getChannelPointer(ch);
            for (size_t n = 0; n < up.getNumSamples(); ++n)
                peak = juce::jmax(peak, std::abs(p[n]));
        }

        holdPeakLin = juce::jmax(holdPeakLin, peak);

        double decay = std::exp(-(double) numSamples / (2.0 * sr));
        recentPeakLin = (float) juce::jmax((double) peak, (double) recentPeakLin * decay);

        truePeakHoldDb.store(holdPeakLin > 0.0f ? 20.0f * std::log10(holdPeakLin) : invalidValue,
                             std::memory_order_relaxed);
        truePeakRecentDb.store(recentPeakLin > 0.0f ? 20.0f * std::log10(recentPeakLin) : invalidValue,
                               std::memory_order_relaxed);
    }
}

int LoudnessMeter::energyToBin(float energy) noexcept
{
    if (energy <= 0.0f)
        return -1;
    float L = -0.691f + 10.0f * std::log10(energy);
    return (int) std::lround((L - binMinLufs) / binStepLufs);
}

float LoudnessMeter::binToEnergy(int bin) noexcept
{
    float L = binMinLufs + (float) bin * binStepLufs;
    return std::pow(10.0f, (L + 0.691f) / 10.0f);
}

void LoudnessMeter::accumulateSubBlock()
{
    float zL = (float)(sumL / (double) subBlockSamples);
    float zR = (float)(sumR / (double) subBlockSamples);
    sumL = sumR = 0.0;
    subBlockCount = 0;

    subZL[(size_t) subWritePos] = zL;
    subZR[(size_t) subWritePos] = zR;
    subWritePos = (subWritePos + 1) % shortTermSubBlocks;
    ++totalSubBlocks;

    if (totalSubBlocks >= momentarySubBlocks)
    {
        double zLm = 0.0, zRm = 0.0;
        for (int i = 0; i < momentarySubBlocks; ++i)
        {
            int idx = (subWritePos - 1 - i + shortTermSubBlocks) % shortTermSubBlocks;
            zLm += subZL[(size_t) idx];
            zRm += subZR[(size_t) idx];
        }
        float energy = (float)((zLm + zRm) / momentarySubBlocks);
        momentaryLufs.store(energy > 0.0f ? (float)(-0.691 + 10.0 * std::log10(energy)) : invalidValue,
                            std::memory_order_relaxed);

        if (energy > 0.0f)
        {
            float L = -0.691f + 10.0f * std::log10(energy);
            if (L >= binMinLufs)
            {
                int bin = energyToBin(energy);
                if (bin >= 0 && bin < numBins)
                    integratedHist[(size_t) bin]++;
            }
        }
    }

    if (totalSubBlocks >= shortTermSubBlocks)
    {
        double zLs = 0.0, zRs = 0.0;
        for (int i = 0; i < shortTermSubBlocks; ++i)
        {
            int idx = (subWritePos - 1 - i + shortTermSubBlocks) % shortTermSubBlocks;
            zLs += subZL[(size_t) idx];
            zRs += subZR[(size_t) idx];
        }
        float energy = (float)((zLs + zRs) / shortTermSubBlocks);
        shortTermLufs.store(energy > 0.0f ? (float)(-0.691 + 10.0 * std::log10(energy)) : invalidValue,
                            std::memory_order_relaxed);

        if (totalSubBlocks % lraStepSubBlocks == 0 && energy > 0.0f)
        {
            float L = -0.691f + 10.0f * std::log10(energy);
            if (L >= binMinLufs)
            {
                int bin = energyToBin(energy);
                if (bin >= 0 && bin < numBins)
                    lraHist[(size_t) bin]++;
            }
        }
    }

    integratedLufs.store(gatedLoudness(integratedHist), std::memory_order_relaxed);
    loudnessRange.store(loudnessRangeFromHistogram(lraHist), std::memory_order_relaxed);
}

float LoudnessMeter::gatedLoudness(const std::array<int, 750>& hist)
{
    double sum = 0.0;
    long long n = 0;
    for (int i = 0; i < numBins; ++i)
        if (hist[(size_t) i] > 0)
        {
            sum += (double) binToEnergy(i) * (double) hist[(size_t) i];
            n += hist[(size_t) i];
        }

    if (n == 0)
        return invalidValue;

    double mean = sum / (double) n;
    double threshold = mean * std::pow(10.0, -10.0 / 10.0);

    double sum2 = 0.0;
    long long n2 = 0;
    for (int i = 0; i < numBins; ++i)
        if (hist[(size_t) i] > 0 && (double) binToEnergy(i) >= threshold)
        {
            sum2 += (double) binToEnergy(i) * (double) hist[(size_t) i];
            n2 += hist[(size_t) i];
        }

    if (n2 == 0)
        return invalidValue;

    return (float)(-0.691 + 10.0 * std::log10(sum2 / (double) n2));
}

float LoudnessMeter::loudnessRangeFromHistogram(const std::array<int, 750>& hist)
{
    double sum = 0.0;
    long long n = 0;
    for (int i = 0; i < numBins; ++i)
        if (hist[(size_t) i] > 0)
        {
            sum += (double) binToEnergy(i) * (double) hist[(size_t) i];
            n += hist[(size_t) i];
        }

    if (n == 0)
        return invalidValue;

    double mean = sum / (double) n;
    double threshold = mean * std::pow(10.0, -20.0 / 10.0);

    long long n2 = 0;
    for (int i = 0; i < numBins; ++i)
        if (hist[(size_t) i] > 0 && (double) binToEnergy(i) >= threshold)
            n2 += hist[(size_t) i];

    if (n2 < 2)
        return invalidValue;

    double loTarget = 0.10 * (double) n2;
    double hiTarget = 0.95 * (double) n2;

    long long cum = 0;
    float lo = binMinLufs, hi = binMinLufs;
    bool loSet = false;
    for (int i = 0; i < numBins; ++i)
    {
        if (hist[(size_t) i] == 0 || (double) binToEnergy(i) < threshold)
            continue;

        cum += hist[(size_t) i];
        float binLufs = binMinLufs + (float) i * binStepLufs;

        if (!loSet && (double) cum >= loTarget)
        {
            lo = binLufs;
            loSet = true;
        }
        if ((double) cum >= hiTarget)
        {
            hi = binLufs;
            break;
        }
    }

    return hi - lo;
}

void LoudnessMeter::performReset()
{
    preL.reset(); preR.reset(); hpL.reset(); hpR.reset();

    subBlockCount = 0;
    sumL = sumR = 0.0;
    subZL.fill(0.0f);
    subZR.fill(0.0f);
    subWritePos = 0;
    totalSubBlocks = 0;

    integratedHist.fill(0);
    lraHist.fill(0);

    holdPeakLin = 0.0f;
    recentPeakLin = 0.0f;

    if (oversampling != nullptr)
        oversampling->reset();

    momentaryLufs.store(invalidValue, std::memory_order_relaxed);
    shortTermLufs.store(invalidValue, std::memory_order_relaxed);
    integratedLufs.store(invalidValue, std::memory_order_relaxed);
    loudnessRange.store(invalidValue, std::memory_order_relaxed);
    truePeakHoldDb.store(invalidValue, std::memory_order_relaxed);
    truePeakRecentDb.store(invalidValue, std::memory_order_relaxed);
}
