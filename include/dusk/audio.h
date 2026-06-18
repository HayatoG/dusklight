#ifndef DUSK_AUDIO_H
#define DUSK_AUDIO_H

// Switch audio IS wired up now: the JAudio2/DSP software pipeline runs and DuskAudioSystem outputs
// through libnx audren (platforms/switch/src/switch_audio.cpp). DuskAudioSystem::Initialize() runs
// JASDsp::initBuffer()/initAll() before the engine's Z2AudioMgr::init, so the FX-line that used to
// be null on Switch (the old reason this was disabled, heuristic #20) is set up first.
#if defined(__SWITCH__)
#define DUSK_AUDIO_DISABLED 0
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
// Audio is enabled on all platforms now, so the mDoAud_* wrappers run normally (nothing to skip).
#define DUSK_AUDIO_SKIP(...)

#endif  // DUSK_AUDIO_H
