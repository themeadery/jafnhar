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
    * @struct PhonData
    * @brief Pre-calculated Sound Pressure Level (SPL) values in dB for various phon levels.
    * * Use these as target magnitudes for FIR filter design. Values are absolute SPL;
    * for DSP, you likely want to normalize these relative to the 1000 Hz reference point.
    */
    struct PhonData {
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
    };

}
