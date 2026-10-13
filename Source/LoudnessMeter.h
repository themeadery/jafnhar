#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class LoudnessMeter
{
public:
    LoudnessMeter();
    ~LoudnessMeter();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Stereo, K-weighted per BS.1770-4. Call from the audio thread.
    void process(const float* left, const float* right, int numSamples);

    float getMomentaryLufs() const   { return momentaryLufs.load(std::memory_order_relaxed); }
    float getShortTermLufs() const   { return shortTermLufs.load(std::memory_order_relaxed); }
    float getIntegratedLufs() const  { return integratedLufs.load(std::memory_order_relaxed); }
    float getLoudnessRange() const   { return loudnessRange.load(std::memory_order_relaxed); }
    float getTruePeakDb() const      { return truePeakHoldDb.load(std::memory_order_relaxed); }
    float getShortTruePeakDb() const { return truePeakRecentDb.load(std::memory_order_relaxed); }

    static constexpr float invalidValue = -1000.0f;

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        void set (float n0, float n1, float n2, float d1, float d2)
        {
            b0 = n0; b1 = n1; b2 = n2; a1 = d1; a2 = d2;
        }

        inline float process (float x) noexcept
        {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
    };

    void buildKWeighting(double sampleRate);
    void accumulateSubBlock();
    void performReset();

    static float gatedLoudness (const std::array<int, 750>& hist);
    static float loudnessRangeFromHistogram (const std::array<int, 750>& hist);
    static int energyToBin (float energy) noexcept;
    static float binToEnergy (int bin) noexcept;

    static constexpr int numBins = 750;
    static constexpr float binMinLufs = -70.0f;
    static constexpr float binStepLufs = 0.1f;
    static constexpr int momentarySubBlocks = 4;   // 400 ms
    static constexpr int shortTermSubBlocks = 30;  // 3 s
    static constexpr int lraStepSubBlocks = 10;    // 1 s hop for LRA

    double sr = 48000.0;
    int subBlockSamples = 4800;
    int subBlockCount = 0;
    double sumL = 0.0, sumR = 0.0;

    std::array<float, shortTermSubBlocks> subZL {};
    std::array<float, shortTermSubBlocks> subZR {};
    int subWritePos = 0;
    long long totalSubBlocks = 0;

    std::array<int, numBins> integratedHist {};
    std::array<int, numBins> lraHist {};

    Biquad preL, preR, hpL, hpR;

    std::atomic<bool> resetRequested { false };
    std::atomic<float> momentaryLufs { invalidValue };
    std::atomic<float> shortTermLufs { invalidValue };
    std::atomic<float> integratedLufs { invalidValue };
    std::atomic<float> loudnessRange { invalidValue };
    std::atomic<float> truePeakHoldDb { invalidValue };
    std::atomic<float> truePeakRecentDb { invalidValue };

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<float> truePeakBuffer;
    float holdPeakLin = 0.0f;
    float recentPeakLin = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};
