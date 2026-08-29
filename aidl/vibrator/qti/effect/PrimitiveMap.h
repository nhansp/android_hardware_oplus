/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/vibrator/CompositePrimitive.h>

#include <array>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

/*
 * Maps each CompositePrimitive onto one of the effects shipped in
 * /odm/etc/vibrator/809/def. The table is indexed by the enum value, so it has
 * to stay in the order the AIDL declares and has to cover every enumerator.
 * NOOP has no effect of its own and plays nothing.
 */
struct PrimitiveEntry {
    CompositePrimitive primitive;
    int effectId;
};

static constexpr int kNoEffect = -1;

static constexpr std::array<PrimitiveEntry, 9> kPrimitiveMap = {{
        {CompositePrimitive::NOOP, kNoEffect},
        {CompositePrimitive::CLICK, 303},
        {CompositePrimitive::THUD, 6},
        {CompositePrimitive::SPIN, 12},
        {CompositePrimitive::QUICK_RISE, 5},
        {CompositePrimitive::SLOW_RISE, 47},
        {CompositePrimitive::QUICK_FALL, 11},
        {CompositePrimitive::LIGHT_TICK, 7},
        {CompositePrimitive::LOW_TICK, 9},
}};

static inline int effectIdForPrimitive(CompositePrimitive primitive) {
    for (const auto& entry : kPrimitiveMap) {
        if (entry.primitive == primitive) return entry.effectId;
    }
    return kNoEffect;
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
