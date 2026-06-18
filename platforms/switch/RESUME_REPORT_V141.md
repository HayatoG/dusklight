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
Build7 (link) GREEN. Build8 = cache-path fix (data.cpp `0a83eceed2`).

## ✅ GREEN + HW-VALIDATED + RELEASE PACKAGED (2026-06-18)
- aurora `824a1b1` (pushed), dusklight `0a83eceed2` (pushed, incl. the cachePath HW fix).
- HW: boots, runs in-game, cache loads (no open errors), **"os modelos carregaram bonitinhamente"** (pop-in fixed).
- **Cache-path fix:** `src/dusk/data.cpp` set `cachePath=prefPath` (= SDL-shim `sdmc:/game/`, a nonexistent dir → SQLite CANTOPEN 14 → reference config-13 `.db` never loaded). Fixed: on Switch `cachePath=dataPath` (the `/TwilitRealm/Dusklight/` data dir).
- **USER-READY RELEASE:** `D:\dusklight-build\Dusklight-Release\` + `Dusklight-Switch-Release.zip` (23.9MB). Layout mirrors SD: `switch/dusklight.nro`, `game/data_location.json`, `TwilitRealm/Dusklight/{config.json,dawn_cache.db(5.8MB warm NVK shaders),pipeline_cache.db(2.4MB config-13)}` + README.txt. User adds their own `game.gcm`. The dawn_cache.db = our NVK-compiled shaders (valid for all users since NVK is baked into the NRO).
- **⏳ OPEN: 30fps.** Frame interp enabled (config `enableFrameInterpolation` 1/2) but produces NO extra frames on Switch. Loop `m_Do_main:337-360` gated by `game_clock::advance_main_loop().is_interpolating`; top `VIWaitForRetrace()` (m_Do_main:329). Hypothesis: main loop iterates ~30Hz (nothing to interpolate into) — VIWaitForRetrace cadence OR ~33ms CPU/frame (present is only ~150µs). Needs HW instrumentation (log is_interpolating/sim_ticks_to_run + per-section timing).

## POST-RE-PORT FEATURES (user asked 2026-06-18: audio, pipeline-notif, (-) overlay)
User confirmed the RmlUi **FPS overlay DOES render in-game** on Switch → the RmlUi UI pipeline is live in-game (aurora renders g_context every frame via aurora.cpp:264 `rmlui::record_frame`).

### 🔊 AUDIO — IMPLEMENTED (building; HW test pending)
Reference RE (`C:\Users\Guilherme\Downloads\dusklight-switch\analysis\out\05_audio_backend.c`) shows Encounter uses **SDL3's own libnx/audren** audio driver (strings `audrenInitialize/audrvCreate/audrenStartAudioRenderer failed`, `"MainAudioOut"`, format 0x8120=F32). We mirror just the SDL audio surface `DuskAudioSystem.cpp` uses, over audren. Chain: game → JAudio2 → DuskDsp (`DspRender` → float32 stereo @ 32kHz) → SDL_PutAudioStreamData → **our audren backend** → speakers.
Files changed:
- **NEW `platforms/switch/src/switch_audio.cpp`** — audren backend: `audrenInitialize`→`audrvCreate(…,2)`→mempool add/attach→`audrvDeviceSinkAdd("MainAudioOut",2)`→`audrenStartAudioRenderer`→`audrvVoiceInit(0,2,PcmFormat_Float,32000)`→mix factors→`audrvVoiceStart`. Dedicated pthread (256KB stack) pulls PCM the game pushed into a 64KB ring, fills 4×8KB wavebufs (armDCacheFlush before queue), `audrvVoiceAddWaveBuf`+`audrvUpdate`+`audrenWaitFrame`. F32 fed direct (no convert). Implements `SDL_Init/SDL_OpenAudioDeviceStream/SDL_PutAudioStreamData/SDL_Resume|PauseAudioStreamDevice`. **KEY: stream opens PAUSED** (DuskAudioSystem runs DspInit AFTER open, resumes at end of Initialize) — render before DspInit would touch uninit DSP. Init fail → returns non-null sentinel, runs silent (no crash).
- **`extern/aurora/include/SDL3/SDL.h`** — added audio decls (SDL_AudioSpec{format,channels,freq}, SDL_AUDIO_F32=0x8120, SDL_AudioStream opaque, callback typedef, SDLCALL, the 5 fn prototypes — non-inline, defined in switch_audio.cpp).
- **`include/dusk/audio.h`** — `DUSK_AUDIO_DISABLED` 1→0 on Switch; `DUSK_AUDIO_SKIP` → empty (audio wired).
- **`CMakeLists.txt`** — re-include `src/dusk/audio/{DuskAudioSystem,DuskDsp,Adpcm}.cpp` + add `switch_audio.cpp` (keep JASCriticalSection/DspStub/switch_stubs).
- **`platforms/switch/src/switch_stubs.cpp`** — removed the `dusk::audio` stub block (real sources provide it).
- Wiring: `libs/JSystem/src/JAudio2/JAUInitializer.cpp:66` calls `dusk::audio::Initialize()` in the JAudio2 init (runs now that audio is enabled). globals (MasterVolume/EnableReverb/EnableHrtf/ChannelAux) in DuskDsp.cpp.
- **RISK (HW): the old reason audio was off** = `Z2AudioMgr::init` hit a null FX-line in `JASDsp::setFXLine`. Now `DuskAudioSystem::Initialize` runs `JASDsp::initBuffer()`/`initAll()` first — hopefully fixes it. If it still crashes on HW → trace setFXLine. Also watch audren errcodes + buffer underrun.

### 🔔 PIPELINE NOTIFICATION — ✅ IMPLEMENTED (RmlUi port; build green, HW test pending)
The pipeline-compilation progress only had an ImGui impl (ImGuiConsole.cpp, excluded on Switch). Ported to an RmlUi overlay element:
- `src/dusk/ui/overlay.cpp` — added `<pipeline-compilation id="pipeline-compilation"/>` to kDocumentSource; `Overlay::update()` shows it while `getSettings().backend.showPipelineCompilation && aurora_get_stats()->queuedPipelines > 0`, label "Compiling shaders… {createdPipelines}/{created+queued}" (throttled 0.1s), mirroring the FPS element.
- `src/dusk/ui/overlay.hpp` — `mPipelineCompilation` + `mPipelineLastUpdate` members.
- `res/rml/overlay.rcss` — `pipeline-compilation` style (bottom-center card; `[open]` shows). Bundled via make-nro romfs.
- Achievement/controller toasts already render via overlay.cpp create_toast; the unlock sound (`mDoAud_seStartMenu`) now works thanks to audio.

### 🎛️ (-) OVERLAY IN-GAME — DIAGNOSED + INSTRUMENTED (needs 1 HW trace, then fix)
Root cause narrowed: MINUS→`KI_F1`(input.cpp:207-211)→`NavCommand::Menu`(ui.cpp:333)→`Document::handle_nav_command`→`toggle()`(document.cpp:141-144). RmlUi keydowns only reach a document's listener if it's **focused** (document.cpp:45-56: passive Overlay forwards to top_document only when *it* gets the keydown). In-game the game holds input focus, so the menu keydown likely never reaches the MenuBar (works in launcher because a UI doc is focused there).
- Added a DIAG log at `document.cpp:143` (`[ui-diag] NavCommand::Menu -> toggle`, `#ifdef __SWITCH__`). HW test: press MINUS in-game — if it logs, toggle fires (look elsewhere); if it logs in launcher but NOT in-game → keydown isn't routing (focus) → fix = give the Overlay/MenuBar a focused element in-game OR poll the menu button directly in the in-game loop and call toggle. NOT fixed speculatively (would risk the launcher's working nav).
- Added `aurora::Module Log{"dusk::ui::document"}` to document.cpp for the diag.

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
