# PLAN — TARGET_PC anti-pattern audit & cleanup

## Background

`CMakeLists.txt:317` defines `TARGET_PC` unconditionally on every non-original-
GameCube build: PC (Windows/Linux/macOS), Android, iOS, **and Switch**. The
codebase predates the Switch port and uses `#if TARGET_PC` as a generic
"non-GameCube" marker, baking PC-specific assumptions into branches that the
Switch now silently inherits.

Symptoms:

- Switch hits every `#if TARGET_PC` branch (no carve-out).
- Switch hits NO `#if PLATFORM_WII || PLATFORM_SHIELD` branch (neither
  defined). When a function's GameCube path is gated by
  `#if PLATFORM_GCN` and its non-GC path is gated by
  `#if PLATFORM_WII || PLATFORM_SHIELD`, Switch falls through both and the
  caller may crash on whatever default state remains.
- Trying to exclude PC-specific code with `#if !TARGET_PC` doesn't work on
  Switch (still false). The correct exclusion is `#ifndef __SWITCH__`
  (devkitA64 toolchain define).

## Concrete crashes already fixed (each driven by this anti-pattern)

| Site | Bug | Fix landed |
|---|---|---|
| `src/m_Do/m_Do_machine.cpp:1024` | `mDoMemCd_ThdInit()` double-init: Play button calls it, then `#if TARGET_PC` branch in `mDoMch_Create` calls it again on the same global thread struct → `OSCreateThread` on live thread → `svcBreak`. | Added `mCardCommand == COMM_NONE_e` guard (2026-05-28). |
| `src/dusk/iso_validate.hpp` redefined in `switch_stubs.cpp` | Local stub `enum class ValidationError` + `struct DiscInfo` collided with the real header after `iso_validate.hpp` was included from another block. | Moved the include up, dropped the local stub (2026-05-28). |
| `src/d/d_file_select.cpp::MemCardStatCheck` | Status-0 `#else` branch called `loadFileNAND()` whose return state index (NAND_STAT_CHECK = 27) is past the `MemCardCheckProc[]` table on Switch (slots 27-37 are `#if PLATFORM_WII || PLATFORM_SHIELD`). OOB function-pointer call → infinite loop. | Added `#ifdef __SWITCH__` branch routing status 0 directly to MAKE_GAMEFILE_SEL. |
| `src/d/d_file_select.cpp::MemCardMakeGameFileCheck` | Same OOB: sets `mNextCardCheckProc = NAND_STAT_CHECK` on non-GCN. | Wrapped GCN branch with `#if PLATFORM_GCN || defined(__SWITCH__)`. |
| `src/m_Do/m_Do_MemCard.cpp::saveNAND` | Posts `COMM_STORE_NAND_e` to a worker-thread switch whose cases are gated `#if PLATFORM_WII || PLATFORM_SHIELD` — never fires on Switch, save status never advances. | Added Switch branch that writes the save synchronously to `sdmc:/dusklight/saves/zeldaTp.dat` and fakes the READY state. |
| `libs/JSystem/src/JKernel/JKRHeap.cpp` | TLS model `global-dynamic` is broken on libnx (no `__tls_get_addr` / DTV), so `sCurrentHeap` resolves to 0x0 the first time `becomeCurrentHeap` touches it. | `tls_model("initial-exec")` for `__SWITCH__`. |
| `include/dusk/audio.h::DUSK_AUDIO_SKIP()` | Empty macro on both branches. `mDoAud_*` wrappers open with `DUSK_AUDIO_SKIP()` expecting an early return but it expanded to nothing → wrappers reached `Z2AudioMgr::getInterface()->...` and dereffed a never-built singleton. | `#if defined(__SWITCH__) DUSK_AUDIO_SKIP(...) return __VA_ARGS__;` |

(See `[[dusklight-debugging-heuristics]]` for the full anti-pattern list.)

## Survey of TARGET_PC / WII-SHIELD sites in the build

Approximate counts after the 2026-05-28 reset (vanilla source + our
Switch-required patches):

- `#if TARGET_PC` branches: ~140
- `#if PLATFORM_WII || PLATFORM_SHIELD` branches: ~86
- `#if PLATFORM_GCN` branches: ~140

Top files by `#if PLATFORM_WII || PLATFORM_SHIELD` count (most-likely
Switch-affected because Switch usually wants the Wii/non-GC behaviour):

| File | gated blocks | Switch status |
|---|---|---|
| `src/d/d_file_select.cpp` | many | ⚠️ partially patched (memcard state machine) |
| `src/m_Do/m_Do_graphic.cpp` | 13 | unverified |
| `src/m_Do/m_Do_audio.cpp` | 5 | ✅ skipped via `DUSK_AUDIO_SKIP`; see also Z2/JAU guards |
| `src/m_Do/m_Do_MemCard.cpp` | 3 | ⚠️ saveNAND patched; load + format paths still broken |
| `src/m_Do/m_Do_MemCardRWmng.cpp` | 2 | unverified |
| `src/d/d_event.cpp` | 2 | unverified |
| `src/d/actor/d_a_title.cpp` | 1 | reached at runtime, no crash observed |
| `src/d/actor/d_a_movie_player.cpp` | 1 | likely affected by ARAM streaming |
| `src/d/actor/d_a_horse.cpp` | 1 | unverified |

## End-state target

Replace the `TARGET_PC` blanket with explicit platform markers:

- `TARGET_PC` keeps meaning "PC desktop" (Windows / Linux / macOS / WSL).
- New `TARGET_SWITCH` defined when building for the Switch (in
  `extern/aurora/cmake/aurora_core.cmake` or top `CMakeLists.txt`).
- New `TARGET_MOBILE` for Android/iOS (currently also caught by
  `TARGET_PC`).

Rewrite rules in source:

| Today | Tomorrow |
|---|---|
| `#if TARGET_PC` (Switch-affected) | `#if TARGET_PC \|\| TARGET_SWITCH` if behavior is the same, otherwise split |
| `#if !TARGET_PC` (Switch-affected) | `#ifndef __SWITCH__` (interim) or carve out `TARGET_SWITCH` explicitly |
| `#if PLATFORM_WII \|\| PLATFORM_SHIELD` | `#if PLATFORM_WII \|\| PLATFORM_SHIELD \|\| TARGET_SWITCH` when Switch should behave like Wii |

## Migration phases

1. **Phase 0 — STOP THE BLEED (now).** Every newly-touched `#if TARGET_PC`
   block that affects Switch gets an explicit `#ifdef __SWITCH__` carve-out.
   Done greedily as crashes are discovered. ← current state.

2. **Phase 1 — Define `TARGET_SWITCH`.** Add `-DTARGET_SWITCH=1` to
   `GAME_COMPILE_DEFS` when `AURORA_PLATFORM_SWITCH`. Equivalent to
   `__SWITCH__` but matching the `TARGET_*` family. No source changes yet.

3. **Phase 2 — Mechanical audit.**
   - Generate a list of every `#if TARGET_PC` site:
     ```bash
     grep -rn '^\s*#if.*TARGET_PC' src libs include
     ```
   - Triage each site:
     - Switch behaves like PC here → leave alone.
     - Switch needs different behavior → split into
       `#if TARGET_PC && !defined(__SWITCH__)` or a dedicated branch.
   - Same for `#if !TARGET_PC` (most likely need `#if !TARGET_PC || TARGET_SWITCH`).

4. **Phase 3 — Mechanical audit of `#if PLATFORM_WII || PLATFORM_SHIELD`.**
   For each: if Switch should adopt Wii behavior (save data, scene flow,
   etc.), append `|| TARGET_SWITCH`. Most file-select sites fall here.

5. **Phase 4 — Drop the carve-outs.** After Phase 2/3, `#ifdef __SWITCH__`
   appearing alongside `TARGET_PC` should be reduced to whatever truly is
   libnx-specific. Move those to dedicated Switch source files when
   possible.

## Tracking

Each per-site fix references this doc + the `[[dusklight-debugging-heuristics]]`
memory. When a Switch crash is diagnosed as "another TARGET_PC trap", add a
row to the table above so the audit list grows monotonically.
