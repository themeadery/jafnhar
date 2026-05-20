#pragma once
#include <array>

namespace iso226 {

    /**
     * @brief Preferred frequencies (Hz) for equal-loudness contours
     * as specified in ISO 226:2003.
     */
    inline constexpr std::array<double, 29> frequencies = {
        20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0,
        200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0,
        1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0,
        10000.0, 12500.0
    };

    /**
     * @brief Extended preferred frequencies (Hz) using R10 spacing.
     */
    inline constexpr std::array<double, 9> xfreqs = {
        16000.0,  // (1.25 * 10^4) -> 16,000
        20000.0,  // (2.0 * 10^4)  -> 20,000
        25000.0,  // (2.5 * 10^4)  -> 25,000
        31500.0,  // (3.15 * 10^4) -> 31,500
        40000.0,  // (4.0 * 10^4)  -> 40,000
        50000.0,  // (5.0 * 10^4)  -> 50,000
        63000.0,  // (6.3 * 10^4)  -> 63,000
        80000.0,  // (8.0 * 10^4)  -> 80,000
        100000.0  // (1.0 * 10^5)  -> 100,000
    };

    /**
    * @struct PhonData
    * @brief Pre-calculated Sound Pressure Level (SPL) values in dB for various phon levels.
    * * Use these as target magnitudes for FIR filter design. Values are absolute SPL;
    * for DSP, you likely want to normalize these relative to the 1000 Hz reference point.
    */
    struct PhonData {
        /** @brief Equal loudness contour for 20 Phons */
        static constexpr std::array<double, 29> phon20 = {
            89.5781, 82.6513, 75.9764, 69.6171, 64.0178, 58.5520, 53.1898, 48.3809,
            43.9414, 39.3702, 35.5126, 31.9922, 28.6866, 25.6703, 23.4263, 21.4825,
            20.1011, 20.0052, 21.4618, 21.4013, 18.1515, 15.3844, 14.2559, 15.1415,
            18.6349, 25.0196, 31.5227, 34.4256, 33.0444
        };

        /** @brief Equal loudness contour for 40 Phons */
        static constexpr std::array<double, 29> phon40 = {
            99.8539, 93.9444, 88.1659, 82.6287, 77.7849, 73.0825, 68.4779, 64.3711,
            60.5855, 56.7022, 53.4087, 50.3992, 47.5775, 44.9766, 43.0507, 41.3392,
            40.0618, 40.0100, 41.8195, 42.5076, 39.2296, 36.5090, 35.6089, 36.6492,
            40.0077, 45.8283, 51.7968, 54.2841, 51.4859
        };

        /** @brief Equal loudness contour for 60 Phons */
        static constexpr std::array<double, 29> phon60 = {
            109.5113, 104.2279, 99.0779, 94.1773, 89.9635, 85.9434, 82.0534, 78.6546,
            75.5635, 72.4743, 69.8643, 67.5348, 65.3917, 63.4510, 62.0512, 60.8150,
            59.8867, 60.0116, 62.1549, 63.1894, 59.9616, 57.2552, 56.4239, 57.5699,
            60.8882, 66.3613, 71.6640, 73.1551, 68.6308
        };

        /** @brief Equal loudness contour for 80 Phons */
        static constexpr std::array<double, 29> phon80 = {
            118.9900, 114.2326, 109.6457, 105.3367, 101.7214, 98.3618, 95.1729, 92.4797,
            90.0892, 87.8162, 85.9166, 84.3080, 82.8934, 81.6786, 80.8634, 80.1736,
            79.6691, 80.0121, 82.4834, 83.7408, 80.5867, 77.8847, 77.0748, 78.3124,
            81.6182, 86.8087, 91.4062, 91.7361, 85.4068
        };

        /** @brief Equal loudness contour for 100 Phons */
        static constexpr std::array<double, 29> phon100 = {
            128.4100, 124.1500, 120.1100, 116.3800, 113.3500, 110.6500, 108.1600, 106.1700,
            104.4800, 103.0300, 101.8500, 100.9700, 100.300, 99.8300, 99.6200, 99.500,
            99.4400, 100.0100, 102.8100, 104.2500, 101.1800, 98.4800, 97.6700, 99.00,
            102.300, 107.2300, 111.1100, 110.2300, 102.0700
        };
    };

}
