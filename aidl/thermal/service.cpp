/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "thermal.oplus"

#include "include/Thermal.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::android::hardware::thermal::implementation::Thermal;

int main() {
    auto thermal = ndk::SharedRefBase::make<Thermal>();
    const std::string name = std::string(Thermal::descriptor) + "/default";

    binder_status_t status =
            AServiceManager_addService(thermal->asBinder().get(), name.c_str());
    if (status != STATUS_OK) {
        LOG(FATAL) << "failed to register " << name << ": " << status;
        return EXIT_FAILURE;
    }

    ABinderProcess_setThreadPoolMaxThreadCount(0);
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  /* joinThreadPool does not return */
}
