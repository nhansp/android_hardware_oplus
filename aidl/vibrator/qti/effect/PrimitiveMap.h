/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/vibrator/CompositePrimitive.h>
#include <aidl/android/hardware/vibrator/Effect.h>

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
        {CompositePrimitive::LOW_TICK, 14},
}};

static inline int effectIdForPrimitive(CompositePrimitive primitive) {
    for (const auto& entry : kPrimitiveMap) {
        if (entry.primitive == primitive) return entry.effectId;
    }
    return kNoEffect;
}

/*
 * The prebaked effects were previously played by passing the AIDL enum value
 * straight through as a waveform index, which does not correspond to anything:
 * CLICK is enum 0 and there is no effect_0, so it fell back to a plain buzz.
 * These are picked from the shipped waveforms by measured length and amplitude.
 */
struct EffectEntry {
    Effect effect;
    int effectId;
};

static constexpr std::array<EffectEntry, 6> kEffectMap = {{
        {Effect::CLICK, 303},        /* 10ms, peak  99 */
        {Effect::DOUBLE_CLICK, 3},   /* 38ms, peak 127, two bursts */
        {Effect::TICK, 7},           /* 10ms, peak  82 */
        {Effect::THUD, 6},           /* 17ms, peak 127 */
        {Effect::POP, 8},            /* 16ms, peak 127 */
        {Effect::HEAVY_CLICK, 12},   /* 14ms, peak 120, the strongest short one */
}};

static inline int effectIdForEffect(Effect effect) {
    for (const auto& entry : kEffectMap) {
        if (entry.effect == effect) return entry.effectId;
    }
    return kNoEffect;
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
