/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "thermal.oplus"

#include "include/Thermal.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parsedouble.h>
#include <android-base/strings.h>
#include <dirent.h>

#include <cmath>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::base::ReadFileToString;
using ::android::base::Split;
using ::android::base::Trim;

namespace {

constexpr char kConfigPath[] = "/vendor/etc/thermal_sensors.conf";
constexpr char kThermalDir[] = "/sys/class/thermal";
constexpr int kPollIntervalMs = 2000;
constexpr size_t kSeverityCount = static_cast<size_t>(ThrottlingSeverity::SHUTDOWN) + 1;

const std::map<std::string, TemperatureType> kTypeNames = {
        {"cpu", TemperatureType::CPU},       {"gpu", TemperatureType::GPU},
        {"battery", TemperatureType::BATTERY}, {"skin", TemperatureType::SKIN},
        {"usb_port", TemperatureType::USB_PORT}, {"npu", TemperatureType::NPU},
        {"display", TemperatureType::DISPLAY}, {"modem", TemperatureType::MODEM},
        {"soc", TemperatureType::SOC},       {"camera", TemperatureType::CAMERA},
        {"power_amplifier", TemperatureType::POWER_AMPLIFIER},
};

/* thermal_zone indices are not stable across boots, so resolve by type. */
std::map<std::string, std::string> discoverZones() {
    std::map<std::string, std::string> zones;
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(kThermalDir), closedir);
    if (!dir) return zones;

    struct dirent* entry;
    while ((entry = readdir(dir.get())) != nullptr) {
        std::string name(entry->d_name);
        if (name.rfind("thermal_zone", 0) != 0) continue;

        std::string type;
        std::string path = std::string(kThermalDir) + "/" + name;
        if (!ReadFileToString(path + "/type", &type)) continue;
        zones[Trim(type)] = path + "/temp";
    }
    return zones;
}

}  // namespace

Thermal::Thermal() {
    auto zones = discoverZones();

    std::string config;
    if (!ReadFileToString(kConfigPath, &config)) {
        LOG(ERROR) << "no " << kConfigPath << "; reporting nothing";
        return;
    }

    for (const auto& rawLine : Split(config, "\n")) {
        std::string line = Trim(rawLine);
        if (line.empty() || line[0] == '#') continue;

        /* type  zone  name  light moderate severe critical emergency shutdown */
        auto f = Split(line, " \t");
        f.erase(std::remove(f.begin(), f.end(), ""), f.end());
        if (f.size() != 3 + kSeverityCount) {
            LOG(ERROR) << "bad config line, ignoring: " << line;
            continue;
        }

        auto it = kTypeNames.find(f[0]);
        if (it == kTypeNames.end()) {
            LOG(ERROR) << "unknown sensor type '" << f[0] << "', ignoring";
            continue;
        }

        auto zone = zones.find(f[1]);
        if (zone == zones.end()) {
            /* A sensor named in config but absent on this kernel is a config
             * bug, not a runtime condition -- say so rather than reporting a
             * sensor that will always read zero. */
            LOG(ERROR) << "thermal_zone '" << f[1] << "' not present, ignoring";
            continue;
        }

        SensorConfig sensor{};
        sensor.type = it->second;
        sensor.zone = f[1];
        sensor.name = f[2];
        sensor.path = zone->second;
        sensor.multiplier = 0.001f;
        sensor.lastSeverity = ThrottlingSeverity::NONE;
        for (size_t i = 0; i < kSeverityCount; i++) {
            if (!::android::base::ParseFloat(f[3 + i], &sensor.thresholds[i]))
                sensor.thresholds[i] = NAN;
        }
        mSensors.push_back(sensor);
        LOG(INFO) << "sensor " << sensor.name << " <- " << sensor.zone;
    }

    mPollThread = std::thread(&Thermal::pollLoop, this);
}

Thermal::~Thermal() {
    {
        std::lock_guard<std::mutex> lock(mPollLock);
        mStop = true;
    }
    mPollCv.notify_all();
    if (mPollThread.joinable()) mPollThread.join();
}

ThrottlingSeverity Thermal::severityFor(const SensorConfig& sensor, float value) {
    ThrottlingSeverity severity = ThrottlingSeverity::NONE;
    for (size_t i = 0; i < kSeverityCount; i++) {
        if (!std::isnan(sensor.thresholds[i]) && value >= sensor.thresholds[i])
            severity = static_cast<ThrottlingSeverity>(i + 1);
    }
    return severity;
}

bool Thermal::readSensor(SensorConfig& sensor, Temperature* out) {
    std::string buf;
    if (!ReadFileToString(sensor.path, &buf)) return false;

    double raw;
    if (!::android::base::ParseDouble(Trim(buf), &raw)) return false;

    out->type = sensor.type;
    out->name = sensor.name;
    out->value = static_cast<float>(raw) * sensor.multiplier;
    out->throttlingStatus = severityFor(sensor, out->value);
    return true;
}

ndk::ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* out) {
    return getTemperaturesWithType(TemperatureType::UNKNOWN, out);
}

ndk::ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType type,
                                                    std::vector<Temperature>* out) {
    std::lock_guard<std::mutex> lock(mSensorLock);
    out->clear();
    for (auto& sensor : mSensors) {
        if (type != TemperatureType::UNKNOWN && sensor.type != type) continue;
        Temperature t;
        if (readSensor(sensor, &t)) out->push_back(t);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholds(std::vector<TemperatureThreshold>* out) {
    return getTemperatureThresholdsWithType(TemperatureType::UNKNOWN, out);
}

ndk::ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType type, std::vector<TemperatureThreshold>* out) {
    std::lock_guard<std::mutex> lock(mSensorLock);
    out->clear();
    for (const auto& sensor : mSensors) {
        if (type != TemperatureType::UNKNOWN && sensor.type != type) continue;

        TemperatureThreshold t;
        t.type = sensor.type;
        t.name = sensor.name;
        t.hotThrottlingThresholds.resize(kSeverityCount + 1);
        t.coldThrottlingThresholds.resize(kSeverityCount + 1, NAN);
        t.hotThrottlingThresholds[0] = NAN; /* NONE has no threshold */
        for (size_t i = 0; i < kSeverityCount; i++)
            t.hotThrottlingThresholds[i + 1] = sensor.thresholds[i];
        out->push_back(t);
    }
    return ndk::ScopedAStatus::ok();
}

/*
 * Cooling devices are reported for completeness but never driven: thermal-engine
 * owns mitigation on this platform, and a second writer would fight it.
 */
ndk::ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out) {
    out->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType, std::vector<CoolingDevice>* out) {
    out->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& callback) {
    return registerThermalChangedCallbackWithType(callback, TemperatureType::UNKNOWN);
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& callback, TemperatureType type) {
    if (callback == nullptr)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);

    std::lock_guard<std::mutex> lock(mCallbackLock);
    for (const auto& entry : mCallbacks) {
        if (entry.callback->asBinder() == callback->asBinder())
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mCallbacks.push_back({callback, type != TemperatureType::UNKNOWN, type});
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& callback) {
    if (callback == nullptr)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);

    std::lock_guard<std::mutex> lock(mCallbackLock);
    for (auto it = mCallbacks.begin(); it != mCallbacks.end(); it++) {
        if (it->callback->asBinder() == callback->asBinder()) {
            mCallbacks.erase(it);
            return ndk::ScopedAStatus::ok();
        }
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus Thermal::registerCoolingDeviceChangedCallbackWithType(
        const std::shared_ptr<ICoolingDeviceChangedCallback>&, CoolingType) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::unregisterCoolingDeviceChangedCallback(
        const std::shared_ptr<ICoolingDeviceChangedCallback>&) {
    return ndk::ScopedAStatus::ok();
}

/*
 * No thermal model here, so the only honest forecast is the present value.
 * Reporting a made-up trend would be worse than reporting no trend.
 */
ndk::ScopedAStatus Thermal::forecastSkinTemperature(int32_t, float* out) {
    std::lock_guard<std::mutex> lock(mSensorLock);
    for (auto& sensor : mSensors) {
        if (sensor.type != TemperatureType::SKIN) continue;
        Temperature t;
        if (!readSensor(sensor, &t)) break;
        *out = t.value;
        return ndk::ScopedAStatus::ok();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

void Thermal::pollLoop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mPollLock);
            mPollCv.wait_for(lock, std::chrono::milliseconds(kPollIntervalMs),
                             [this] { return mStop; });
            if (mStop) return;
        }

        std::vector<Temperature> changed;
        {
            std::lock_guard<std::mutex> lock(mSensorLock);
            for (auto& sensor : mSensors) {
                Temperature t;
                if (!readSensor(sensor, &t)) continue;
                if (t.throttlingStatus == sensor.lastSeverity) continue;
                sensor.lastSeverity = t.throttlingStatus;
                changed.push_back(t);
            }
        }
        if (changed.empty()) continue;

        std::lock_guard<std::mutex> lock(mCallbackLock);
        for (const auto& t : changed) {
            for (const auto& entry : mCallbacks) {
                if (entry.filtered && entry.type != t.type) continue;
                entry.callback->notifyThrottling(t);
            }
        }
    }
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
