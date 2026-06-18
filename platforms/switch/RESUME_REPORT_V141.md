# RESUME — Re-port to dusklight v1.4.1 + encounter/aurora@13

**Date:** 2026-06-18. Read this first to continue the re-port. Companion memory:
`dusklight-report-v141-progress`, `dusklight-cache-and-crash-fixes`,
`dusklight-reference-build-analysis` (in the agent memory dir).

## BUILD-FIX LOG (live — iterating build → fix → repeat, 2026-06-18)
Builds run from `/d/Projects/dusklight` via `bash /d/Projects/dusklight/platforms/switch/build-docker.sh build`
(absolute path — cwd may drift). Logs: `/d/dusklight-build/report_buildN.log`. Build dir persists
(only changed TUs recompile; touching `SDL3/SDL.h` forces a broad aurora+dusk recompile).
Fixes applied so far (all verified against actual v13 source/struct, not guessed):
1. **CMake generate**: `aurora_gx` links `PNG::PNG` (new `png_io.cpp`) → defined IMPORTED `PNG::PNG`
   (portlib libpng16 + zlib) in `extern/aurora/CMakeLists.txt` Switch block.
2. **aurora_core sources**: added v13's `lib/webgpu/gpu_prof.cpp` + `lib/dawn/TracyPlatform.cpp` to
   `aurora_core.cmake` (both `TRACY_ENABLE`-guarded → trivial stubs on Switch). CONFIRMED compiling.
3. **configPath removed (v13)**: dropped `config.configPath=` in `src/m_Do/m_Do_main.cpp` (cachePath
   already set; aurora cache now keyed off cachePath).
4. **pipeline_cache.cpp (took v13's)**: re-applied Switch fixes — 8MB-stack **pthread** worker
   (`#ifdef __SWITCH__`, Tint recursion overflows libnx std::thread) + **unix-none** VFS +
   `journal_mode=MEMORY` for the writable DB open. `gpu_cache.cpp` already had ours (cachePath/unix-none/MEMORY).
5. **allowTextureReplacements removed (v13)**: dropped the assignment in m_Do_main (auto-loads from resourcesPath).
6. **rmlui.cpp (took v13's, dropped v11 Switch guards)**: restored `#ifndef AURORA_PLATFORM_SWITCH`
   around `#include <RmlUi_Platform_SDL.h>` + local `RmlSDL::{GetKeyModifierState,ConvertMouseButton,
   InputEventHandler}` stubs. `rmlui_backends` is INTERFACE-only on Switch (no real SDL / no .cpp).
7. **SDL3 shim** (`extern/aurora/include/SDL3/SDL.h`): added `SDL_RegisterEvents` (aurora.cpp:124),
   then for dusk `mouse.cpp`: `SDL_EVENT_WINDOW_FOCUS_GAINED`, `SDL_WINDOW_INPUT_FOCUS`,
   `SDL_GetWindowRelativeMouseMode`, `SDL_SetWindowRelativeMouseMode`, `SDL_SetWindowMouseGrab`,
   `SDL_WarpMouseInWindow`. + ImGui stub (`extern/aurora/include/imgui.h`): added `ImGuiIO::MouseDelta`.
8. **dusk `mouse.cpp`** (v1.4.1 PC mouse-camera): added 6 SDL stubs to the shim
   (`SDL_EVENT_WINDOW_FOCUS_GAINED`, `SDL_WINDOW_INPUT_FOCUS`, `SDL_GetWindowRelativeMouseMode`,
   `SDL_SetWindowRelativeMouseMode`, `SDL_SetWindowMouseGrab`, `SDL_WarpMouseInWindow`) +
   `ImGuiIO::MouseDelta` to the imgui stub. (Inert on Switch — window never reports INPUT_FOCUS.)
9. **dusk `icon_provider.cpp`**: implemented real RGBA32 surface ops in the shim — `SDL_CreateSurface`
   (heap pixels), `SDL_DestroySurface` (now frees; only icon_provider makes real surfaces on Switch
   so no double-free), `SDL_LockSurface`/`SDL_UnlockSurface`/`SDL_MUSTLOCK`, `SDL_SetSurfaceBlendMode`,
   `SDL_BlitSurfaceScaled` (nearest-neighbour + src-over alpha), `SDL_ScaleMode`/`SDL_BlendMode`.
   Verified audio/imgui/file_select files are EXCLUDED on Switch (CMakeLists 524-541) → not real errors.
10. **aurora `pad_switch.cpp`** signature conflicts (v13 changed pad.h): `PADControlMotor(s32→u32 chan)`,
    `PADSetPortForIndex(...,s32→u32 port)`. + added 6 v1.4.1-new PAD stubs (`PADSetVirtualStatus`,
    `PADClearVirtualStatus`, `PADHasLED`, `PADCanForceDeviceRumble`, `PADGetForceDeviceRumble`,
    `PADSetForceDeviceRumble`) used by touch_controls/gamepad_color/controller_config.
11. **LINK stage** (dusklight.elf, 16 undefined symbols): (a) aurora.cpp guards ImGui calls with
    AURORA_ENABLE_GX (on) not AURORA_ENABLE_IMGUI (off→imgui.cpp not built) + system_info.cpp not
    built on Switch → new `lib/switch_aurora_stubs.cpp` (imgui::{create_context,initialize,shutdown,
    new_frame,freeze,render} + log_system_information), wired via `elseif(AURORA_PLATFORM_SWITCH)` in
    aurora_core.cmake; (b) v13's `lib/rmlui/RuntimeTextureProvider.cpp` (load_runtime_texture/
    register_texture_provider/unregister_texture_provider) wasn't in our RMLUI source list → added it.
Build7 (link) in progress.
NOTE: NONE of these re-port branches are committed yet — commit aurora + dusklight once the build is green.

## Why we're doing this
A reference Switch build (Encounter's, `v1.4.1-32-dirty`) runs much better
(~40-85fps) than our v1.3.1 (~20fps). The gap is mostly **v1.4.1's newer aurora
renderer** (draw-call merging, full GPU vertex parsing, render_worker,
multi-staging-buffer, gpu_prof) + frame interpolation — NOT the cache. Updating to
v1.4.1 + `encounter/aurora@13` (GXPipelineConfigVersion=13) brings that renderer and
makes the reference's `pipeline_cache.db` loadable (config 13). NOTE: the reference's
`dawn_cache.db` (compiled shader blobs) will NOT transfer (keyed by NVK/shader-gen) —
proven by it growing +2.4MB on our runs.

## Git state
- **CHECKPOINT (safe, pushed) — bail here if needed:**
  - dusklight `main` = `b931b0ca5a` (HayatoG/dusklight) — working v1.3.1 + cache/async/crash fixes.
  - aurora `dusklight-switch-port` = `64cb652` (HayatoG/aurora-switch).
- **RE-PORT branches (foundational merges committed, NOT pushed, NOT building yet):**
  - aurora `switch-port/v13-report` = `3d749ff` (merged remote `enc` = encounter/aurora main).
  - dusklight `switch-port/v1.4.1-report` = `10c47bd891` (merged remote `upstream` = TwilitRealm/dusklight tag v1.4.1; submodule → 3d749ff).
- Resume: `git -C extern/aurora checkout switch-port/v13-report && git checkout switch-port/v1.4.1-report`.

## BUILD-FIX LOOP — the remaining work (iterative: build → fix → repeat)
First build log: `/d/dusklight-build/report_build1.log`. Build cmd (from
`/d/Projects/dusklight`, NOT the build dir): `bash platforms/switch/build-docker.sh build`.
Expected fixes (from API analysis; confirm against actual build errors):
1. **aurora cmake** (we kept OURS for AuroraDawnProvider/aurora_core/aurora_dvd/extern
   CMakeLists to preserve the Switch build): re-add v13's new sources —
   `lib/webgpu/gpu_prof.cpp` + `lib/dawn/TracyPlatform.cpp` to `aurora_core` target;
   provide `PNG::PNG` target + zlib for Switch (v13's gx links them).
2. **configPath removed in v13** (AuroraConfig now userPath/cachePath/resourcesPath):
   fix any `config.configPath`/`g_config.configPath` refs → cachePath/userPath. Our
   `src/m_Do/m_Do_main.cpp` set `config.configPath` — remove/port it. (`dusk::ConfigPath`
   is a SEPARATE dusk-side var — stays.)
3. **pipeline_cache.cpp** (we took v13's, dropping our Switch fixes): re-apply on v13's
   structure — (a) the 8MB-stack **pthread** worker (v13 uses std::thread on Vulkan →
   Tint AnalyzeUniformity recursion overflows the small libnx stack → crash); (b)
   **unix-none VFS** + **journal_mode=MEMORY** for the writable DB open (FsFs has no
   fcntl locking → SQLITE_IOERR). v13's SDL VFS is read-only (bundled cache only).
   gpu_cache.cpp auto-merged keeping our Switch fixes — verify they survived + use cachePath.
4. **m_Do_main.cpp / Switch code**: adapt to v1.4.1 API changes the build surfaces
   (texture-replacement API, etc.). We kept OURS for m_Do_main (the Switch boot path).
5. Rebuild until green → `make-nro.sh` → nxlink to Switch.

## Switch HW setup (for testing)
- Switch IP **192.168.1.11**, Sphaira netloader on. nxlink:
  `/c/devkitPro/tools/bin/nxlink.exe -s -r 20 -a 192.168.1.11 /d/dusklight-build/dusklight.nro`
  (needs Windows Firewall inbound rule for nxlink.exe). Logs stream to the nxlink stdout.
- Sphaira **FTP on :5000** (anonymous): `curl ftp://192.168.1.11:5000/sdmc:/...` (flaky while app runs).
- **Data layout on SD** = `sdmc:/TwilitRealm/Dusklight/`: game.gcm (1.46GB), config.json
  (full reference settings), dawn_cache.db/pipeline_cache.db (config 13), USA/Card A/
  01-GZ2E-gczelda2.gci (save), achievements.json.
- Netloaded build's prefPath = `sdmc:/game/` (hardcoded SDL shim). We placed
  `sdmc:/game/data_location.json` = `{"mode":"custom","customPath":"/TwilitRealm/Dusklight"}`
  (in-repo at platforms/switch/data_location.json) to redirect the data dir. NOTE: v13
  uses cachePath (not configPath) for the cache — re-verify the redirect still applies.
- Card/save path is HARDCODED `sdmc:/game/GC/...` (aurora DolphinCardPath) — separate
  from the data dir; the save isn't found unless that's also handled.

## Fixes already done (on the checkpoint, carried into the re-port)
- WS1 cache enabled (AURORA_ENABLE_GPU_CACHE+ZSTD, libzstd_static from portlib, sqlite3
  OMIT_WAL/MAX_MMAP_SIZE=0/OMIT_LOAD_EXTENSION).
- Cache opens: configPath fix + /-path descriptor + unix-none VFS + MEMORY journal.
- Crash fix: async pipeline worker on 8MB pthread stack (was Tint stack overflow).
- These need re-applying/verifying on the v13 base (see build-fix #2,#3).

## Still open (post-re-port)
- WS2 draws (game-side, maybe fixed in v1.4.1 — re-test). WS3 comptags (switch-nvk,
  read-only repo — plan in PARITY_WORKSTREAMS.md). WS4 audio (reference uses SDL3+audren;
  ours stubbed — v1.4.1's audio path may differ). Frame interpolation (compiled+enabled;
  verify it runs on Switch — the 60-85fps lever). (-) menu in-game (input path is
  launcher-only). Pipeline-compilation overlay (ImGui, off on Switch — needs RmlUi or v13's).
