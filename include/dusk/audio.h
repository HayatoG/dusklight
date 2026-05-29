#ifndef DUSK_AUDIO_H
#define DUSK_AUDIO_H

#if defined(__SWITCH__)
// On Switch the JAudio2 backend isn't wired up to libnx/audren yet; mDoAud_Create
// dies with a null FX-line in JASDsp::setFXLine during Z2AudioMgr::init. Fake the
// init flag so the engine boots and the game loop runs without audio. See
// dusklight debugging heuristic #20.
#define DUSK_AUDIO_DISABLED 1
#elif TARGET_PC
#define DUSK_AUDIO_DISABLED 0
#else
#define DUSK_AUDIO_DISABLED 0
#endif

// On Switch, audio infrastructure is never instantiated (Z2AudioMgr::init is
// skipped via DUSK_AUDIO_DISABLED). Every `mDoAud_*` inline wrapper opens
// with `DUSK_AUDIO_SKIP()` expecting an early return when audio is off —
// make that return actually happen, otherwise the wrappers call
// `Z2AudioMgr::getInterface()->...` which derefs a never-built singleton.
// Optional arg lets callers like `s32 mDoAud_load1stDynamicWave()` return a
// specific value (e.g. `DUSK_AUDIO_SKIP(0)`); default for void wrappers.
//
// On PC the macro stays empty: audio is fully wired up there, nothing to skip.
#if defined(__SWITCH__)
#define DUSK_AUDIO_SKIP(...) return __VA_ARGS__;
#else
#define DUSK_AUDIO_SKIP(...)
#endif

#endif  // DUSK_AUDIO_H
