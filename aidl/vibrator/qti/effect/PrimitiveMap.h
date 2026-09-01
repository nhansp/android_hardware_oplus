/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/vibrator/CompositePrimitive.h>
#include <aidl/android/hardware/vibrator/Effect.h>
#include <aidl/android/hardware/vibrator/EffectStrength.h>

#include <array>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

/*
 * Waveform ids into /odm/etc/vibrator/809/def, resolved through
 * get_effect_stream(). Anything that resolves there is uploaded as a custom
 * FIFO effect, where the kernel plays our samples verbatim and never checks the
 * id. Anything that does NOT resolve silently falls back to the 6-byte upload
 * path, whose allowlist is only the device tree's placeholder effects 0-5 --
 * flat 0x7f runs that read as a buzz rather than a tap. Every id below is
 * therefore verified present in the shipped blob list.
 *
 * Peak drive is modelled from each waveform's envelope against
 * fifo_vmax (8241 mV), and the ids are ordered into a monotonic ladder:
 *
 *   texture_tick  7    517 mV      click        2   3007 mV
 *   low_tick     11   1151 mV      pop         12   3153 mV
 *   tick          1   1866 mV      heavy_click  3   3411 mV
 *   spin         13   1866 mV      thud        10   2529 mV
 */

static constexpr int kNoEffect = -1;

struct EffectEntry {
    Effect effect;
    int effectId;
};

static constexpr std::array<EffectEntry, 6> kEffectMap = {{
        {Effect::CLICK, 2},        /* ColorOS maps its CLICK family here */
        {Effect::DOUBLE_CLICK, 3}, /* no stock two-pulse waveform; heaviest single */
        {Effect::TICK, 1},         /* ColorOS CLOCK_TICK/TEXT_HANDLE_MOVE */
        {Effect::THUD, 10},        /* flat 14ms; no true thud in the blob set */
        {Effect::POP, 12},         /* abrupt full-scale cut */
        {Effect::HEAVY_CLICK, 3},  /* top of OPlus's own ladder */
}};

/*
 * OPlus ship three rungs of their click waveform. Selecting between them is how
 * strength is meant to work: a weak waveform driven at a lower magnitude simply
 * fails to start the LRA, which is what made the previous attempt imperceptible.
 * 110 and 112 are byte-identical to 1 and 6; 111 is its own 441-byte waveform,
 * so use OPlus's ladder rather than substituting 2 for it.
 */
struct StrengthEntry {
    EffectStrength strength;
    int effectId;
};

static constexpr std::array<StrengthEntry, 3> kClickLadder = {{
        {EffectStrength::LIGHT, 110},
        {EffectStrength::MEDIUM, 111},
        {EffectStrength::STRONG, 112},
}};

struct PrimitiveEntry {
    CompositePrimitive primitive;
    int effectId;
};

/*
 * Only the primitives with a real waveform are listed. THUD, QUICK_RISE,
 * SLOW_RISE and QUICK_FALL have no stock analogue -- the blob set has nothing
 * with a clean ramp envelope at the right length -- so they are left
 * unsupported and the framework substitutes a prebaked effect. Claiming them
 * with a wrong-shaped waveform is worse than not claiming them.
 */
static constexpr std::array<PrimitiveEntry, 9> kPrimitiveMap = {{
        {CompositePrimitive::NOOP, kNoEffect},
        {CompositePrimitive::CLICK, 2},
        {CompositePrimitive::THUD, kNoEffect},
        {CompositePrimitive::SPIN, 13},
        {CompositePrimitive::QUICK_RISE, kNoEffect},
        {CompositePrimitive::SLOW_RISE, kNoEffect},
        {CompositePrimitive::QUICK_FALL, kNoEffect},
        {CompositePrimitive::LIGHT_TICK, 1},
        {CompositePrimitive::LOW_TICK, 11},
}};

static inline int effectIdForEffect(Effect effect, EffectStrength strength) {
    if (effect == Effect::CLICK) {
        for (const auto& entry : kClickLadder) {
            if (entry.strength == strength) return entry.effectId;
        }
    }
    for (const auto& entry : kEffectMap) {
        if (entry.effect == effect) return entry.effectId;
    }
    return kNoEffect;
}

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
