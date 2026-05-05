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
        /** @brief Equal loudness contour for 40 Phons (Quiet/Reference level). */
        static constexpr std::array<double, 29> phon40 = {
            93.2, 86.1, 79.2, 72.5, 66.3, 60.6, 55.4, 51.0, 47.1, 43.6,
            40.7, 38.3, 36.3, 34.8, 33.7, 33.1, 33.2, 34.0, 34.4, 33.7,
            32.3, 31.3, 31.1, 32.2, 35.5, 40.7, 46.8, 52.8, 57.3
        };

        /** @brief Equal loudness contour for 60 Phons (Moderate conversation level). */
        static constexpr std::array<double, 29> phon60 = {
            103.7, 97.4, 91.2, 85.3, 79.9, 74.9, 70.4, 66.5, 63.1, 60.1,
            57.6, 55.6, 54.0, 52.9, 52.3, 52.1, 52.5, 53.7, 54.5, 54.4,
            53.5, 53.1, 53.5, 55.0, 58.2, 62.9, 68.6, 74.3, 78.4
        };

        /** @brief Equal loudness contour for 80 Phons (Loud/Mixing level). */
        static constexpr std::array<double, 29> phon80 = {
            114.2, 108.7, 103.2, 98.2, 93.5, 89.2, 85.5, 82.1, 79.1, 76.6,
            74.6, 73.0, 71.8, 71.0, 70.8, 71.1, 71.9, 73.4, 74.6, 75.1,
            74.8, 75.0, 75.9, 77.7, 80.9, 85.1, 90.3, 95.8, 99.4
        };
    };

}
