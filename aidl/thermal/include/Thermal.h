/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/thermal/BnThermal.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

/*
 * One configured sensor: which thermal_zone to read, what to call it, and the
 * six hot thresholds indexed by ThrottlingSeverity (LIGHT..SHUTDOWN).
 */
struct SensorConfig {
    TemperatureType type;
    std::string name;     /* reported to the framework */
    std::string zone;     /* thermal_zone *type* to match, not an index */
    std::string path;     /* resolved at startup */
    float thresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN) + 1];
    float multiplier;     /* sysfs millidegrees -> degrees, usually 0.001 */
    ThrottlingSeverity lastSeverity;
};

class Thermal : public BnThermal {
  public:
    Thermal();
    ~Thermal();

    ndk::ScopedAStatus getCoolingDevices(std::vector<CoolingDevice>* out) override;
    ndk::ScopedAStatus getCoolingDevicesWithType(CoolingType type,
                                                 std::vector<CoolingDevice>* out) override;
    ndk::ScopedAStatus getTemperatures(std::vector<Temperature>* out) override;
    ndk::ScopedAStatus getTemperaturesWithType(TemperatureType type,
                                               std::vector<Temperature>* out) override;
    ndk::ScopedAStatus getTemperatureThresholds(std::vector<TemperatureThreshold>* out) override;
    ndk::ScopedAStatus getTemperatureThresholdsWithType(
            TemperatureType type, std::vector<TemperatureThreshold>* out) override;
    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& callback) override;
    ndk::ScopedAStatus registerThermalChangedCallbackWithType(
            const std::shared_ptr<IThermalChangedCallback>& callback,
            TemperatureType type) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& callback) override;
    ndk::ScopedAStatus registerCoolingDeviceChangedCallbackWithType(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& callback,
            CoolingType type) override;
    ndk::ScopedAStatus unregisterCoolingDeviceChangedCallback(
            const std::shared_ptr<ICoolingDeviceChangedCallback>& callback) override;
    ndk::ScopedAStatus forecastSkinTemperature(int32_t forecastSeconds,
                                               float* out) override;

  private:
    struct CallbackEntry {
        std::shared_ptr<IThermalChangedCallback> callback;
        bool filtered;
        TemperatureType type;
    };

    bool readSensor(SensorConfig& sensor, Temperature* out);
    static ThrottlingSeverity severityFor(const SensorConfig& sensor, float value);
    void pollLoop();

    std::vector<SensorConfig> mSensors;
    std::vector<CallbackEntry> mCallbacks;
    std::mutex mCallbackLock;
    std::mutex mSensorLock;

    std::thread mPollThread;
    std::condition_variable mPollCv;
    std::mutex mPollLock;
    bool mStop = false;
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
