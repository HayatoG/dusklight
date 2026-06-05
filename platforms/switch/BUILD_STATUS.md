# Dusklight → Switch Build Status

**Last update:** 2026-05-25 — 🎯 **FIRST REAL-HARDWARE session + backend pivot to Mesa-GLES.** Via nxlink live logs + Sphaira FTP crash-report→addr2line (see `RESUME_HERE.md` for the loop), the engine now boots on real Tegra → `mDoMch_Create` → main loop → logo scene draws → **`dScnPly_Create`** (gameplay/opening scene), **~12 frames presenting the deko blue clear ON THE TV** (deko3d native present confirmed on hardware), `gx=0` still. Eden-masked crash cascade cleared: **TLS `tls_model("initial-exec")` not global-dynamic** (heuristic #18); audio/fade null-guards; vibration `m_gamePad[]` null-guard = **immediate next code task (diagnosed, not applied)**. **🔀 DECISION: pivot graphics backend deko3d → native Aurora Mesa-GLES** (runtime shader compile = no precompile/coverage tail; reusable for other Switch ports) — plan in `PLAN_GLES.md`, decision in memory `[[dusklight-gles-backend-plan]]`. The deko3d render notes below remain accurate history (boot/heap/RmlUi infra is backend-agnostic).

---

**Last update (prior):** 2026-05-23 LATE NIGHT — 🎯 **Phase 2 COMPLETE.** Engine main loop runs infinitely on Switch + Eden with deko3d backend presenting per-frame cycling clear color. main01 init runs through every step. First main-loop iter cycles cleanly: `aurora_update` → `aurora_begin_frame` (deko) → `VIWaitForRetrace` → `updateRenderSize` → `dusk::ui::update` → `advance_main_loop` → `mDoCPd_c::read` → `fapGm_Execute` (SKIPPED on Switch, Phase 3 work) → `mDoAud_Execute` → `aurora_end_frame` (deko present). Frame pacing + Limiter + frame_interp all validated end-to-end. User confirmed cor azul/ciclando visível.

> ## 🚨 IF YOU'RE STARTING A NEW SESSION
> Read `RESUME_HERE.md` first. It has the exact step-by-step to continue.
> Active branch: `switch-port/deko3d-backend`. Plan: `PLAN_DEKO3D.md`.

## 🎯 Headline (2026-05-23 LATE NIGHT — Phase 2 COMPLETE)

- **Build:** Cross-compiles + links clean for aarch64 Switch. `Dusklight.nro` ~27 MB.
- **Runs in Eden:** ✅ Engine boots, all init traces fire, main01 completes setup, main loop runs continuously.
- **Engine state:** ✅ Main loop iterating at full speed; `fapGm_Execute` (game logic + GX render) stubbed on Switch — Phase 3 work.
- **GPU pipeline:** ✅ deko3d `begin_frame`/`end_frame` integrated into aurora's `aurora_begin_frame`/`aurora_end_frame` via `#if AURORA_BACKEND_DEKO3D` short-circuit (bypasses broken Dawn surface path). Cycling color visible.
- **The "Unmapped Read64" mystery (prior session):** Confirmed RED HERRING. It was `svcOutputDebugString` getting dropped by Eden under heavy bg-thread noise. Switched engine traces to `dusk_switch_log` → `sdmc:/dusklight.log` (file-based, crash-safe, fflush per line). All previously hidden traces now visible.

## Archive: prior Phase 1 headline (2026-05-23 night)

- **Build:** Cross-compiles + links clean for aarch64 Switch with deko3d backend + Dawn-as-type-source. `Dusklight.nro` ~27 MB.
- **Runs in Eden:** ✅ boots, ALL libnx services init, ALL `[DUSKLIGHT-TRACE]` markers fire through full game_main init.
- **Engine state:** ✅ ALIVE past `dComIfG_ct` (JSystem global game context created).
- **Disc image:** ✅ User's `tloz.gcm` loaded from `sdmc:/dusklight/` via aurora-switch's `dvd_switch.cpp` (Dan's 2324-LOC reimpl). aurora_dvd_open returned success.
- **GPU pipeline:** ✅ **`dkDeviceCreate` → `dkMemBlockCreate` (framebuffer) → `dkSwapchainCreate` (from `nwindowGetDefault()`) → `dkQueueCreate` → per-frame `dkQueueAcquireImage` + `dkCmdBufClearColorFloat` + `dkQueueSubmitCommands` + `dkQueuePresentImage`** — all working. 480+ frames cycled R→G→B→R color presented through real deko3d. The libnx-NWindow → Tegra-X1 path is end-to-end alive.
- **Game logic (main01):** ⏭️ Still stubbed (alive loop instead). Next blocker is `mDoMch_Create` (JSystem heap setup with PowerPC arena APIs). Fixing that is Phase 2.
- **Renders Dusklight content:** ❌ Not yet — currently a clear-color cycle. Bridging `aurora::gfx` (GX→pipeline IR) to deko3d primitives is Phase 3+.

## Previous state (2026-05-22 night, archived)

- **Build:** Cross-compiles + links clean for aarch64 Switch. `Dusklight.nro` = 27 MB, valid NRO0 magic.
- **Runs in Eden:** boots, all libnx services init, all `[DUSKLIGHT-TRACE]` lifecycle markers fire through main → game_main → aurora_initialize.
- **Renders:** nothing yet. **Dawn fell back to the Null backend** (`g_backendType=1` in our trace) — our `BACKEND_OPENGLES` init failed silently and Dawn picked the next entry in PreferredBackendOrder, which is BACKEND_NULL (no-op).
- **Root cause unconfirmed** but two strong leads (see "Gaps & open questions").

## Phase 1 deko3d trace (2026-05-23 night)

```
[aurora::webgpu(deko)] initialize entry
[aurora::webgpu(deko)] deko_initialize entry
[aurora::webgpu(deko)] dkDeviceCreate OK
[aurora::webgpu(deko)] dkMemBlockCreate fb OK              ← framebuffer memblock (kFbNum=2)
[aurora::webgpu(deko)] dkSwapchainCreate OK                ← from nwindowGetDefault()
[aurora::webgpu(deko)] dkQueueCreate OK                    ← graphics queue
[aurora::webgpu(deko)] deko_initialize: ALL OK
... aurora init continues, game_main returns ...
[DUSKLIGHT-TRACE] deko frame=60  color=(0.67,0.33,0.00)    ← laranja  → present OK
[DUSKLIGHT-TRACE] deko frame=120 color=(0.34,0.66,0.00)    ← verde-amarelo
[DUSKLIGHT-TRACE] deko frame=180 color=(0.01,0.99,0.00)    ← verde puro
[DUSKLIGHT-TRACE] deko frame=240 color=(0.00,0.67,0.33)    ← turquesa
[DUSKLIGHT-TRACE] deko frame=300 color=(0.00,0.34,0.66)    ← azul-aço
[DUSKLIGHT-TRACE] deko frame=360 color=(0.00,0.01,0.99)    ← azul puro
[DUSKLIGHT-TRACE] deko frame=420 color=(0.33,0.00,0.67)    ← roxo
[DUSKLIGHT-TRACE] deko frame=480 color=(0.66,0.00,0.34)    ← magenta
```

**480 frames presented = ~8s of cycling clear color, end-to-end through libnx→deko3d→Tegra X1.**

## Phase 2 progress (2026-05-23 night, continuing from Phase 1)

After visual confirmation, we let `main01()` actually run (removed the alive-loop guard). With dense traces in `mDoMch_Create`, `JFWSystem::init`, `mDoGph_Create`, we found and bypassed:

| Call | Status | Notes |
|---|---|---|
| `mDoMch_Create` entry → JKRHeap, setSysHeapSize, setRenderMode, firstInit, createDbPrintHeap, JUTDbPrint::start, createAssertHeap | ✅ all complete |
| `JFWSystem::init`: JKRAram::create → JKRThread → JUTVideo → JUTCreateFifo → JUTGamePad → JUTAssertion → JUTResFont → JUTDbPrint → JUTConsoleManager → JUTConsole | ✅ all complete |
| `JFWSystem::init`: `JUTException::createConsole` | ⏭️ SKIPPED on `__SWITCH__` (hangs — debug-only exception console using GameCube-specific signal hooks). Patch in `libs/JSystem/src/JFramework/JFWSystem.cpp` |
| `JFWSystem::init` returns DONE | ✅ |
| `mDoMch_Create`: createCommandHeap, createArchiveHeap, createJ2dHeap, createGameHeap, createZeldaHeap, JKRSetAramTransferBuffer, JKRThreadSwitch::createManager, JKRDvdRipper setup, `mDoDvdThd::create`, `mDoDvdErr_ThdInit`, `mDoMemCd_ThdInit` | ✅ all complete; `returning 1` |
| `mDoGph_Create`: createSolidHeap, `mDoGph_gInf_c::create`, `dComIfGd_init`, adjustSolidHeap, restoreCurrentHeap | ✅ all complete; `returning 1` |
| **Background thread `Unmapped Read64 @ 0x00000001039762B0`** | ⚠️ MYSTERY — repeated identical-address read64s from a background thread (one of: mDoDvdThd / mDoDvdErr / mDoMemCd / JKRAram / JKRThreadSwitch). Starts during `mDoGph_Create` execution, continues after. Main thread `main01: mDoCPd_c::create` trace doesn't fire afterward, so likely the bad-pointer thread eventually causes a fatal that stops main thread logging too. |
| `mDoCPd_c::create` | ⏭️ Phase 2 skip on `__SWITCH__` (controller pad — aurora-switch's `input_switch.cpp` should handle HID instead). Patch in `src/m_Do/m_Do_main.cpp` main01 body. |

**Next session strategy (per user's directive 2026-05-23 night):** stop iterating one-blocker-at-a-time. Instead, **dense-trace the entire remaining flow** (all calls from main01 onward including LOAD_COPYDATE, fapGm_Create, fopAcM_initManager, cDyl_InitAsync, JKRCreateSolidHeap audio, game_clock init, main loop body with aurora_update/begin_frame/end_frame). Single rebuild → run → see every checkpoint that fires → fix all silent blockers in one pass.

### 🎯 VISUAL CONFIRMED (2026-05-23 late night)

After fixing a `dkCmdBufClear` invalidating pre-recorded `s_cmdsBindFb` lists in the same cmdbuf (now records bind+viewport+scissor+clear INLINE per frame), the cycling colors are **VISIBLE ON BOTH REAL SWITCH HARDWARE AND EDEN EMULATOR**.

User confirmed:
- Real Switch (via DBI FTP @ 192.168.1.5:5000): "Aeeeeee, foi sim, a tela ta colorida!!"
- Eden emulator (post-fix re-test): "No emulador tambem fooooooii"

The deko3d render pipeline is **proven end-to-end on real Tegra X1 hardware**:
```
libnx NWindow
  → nwindowGetDefault()
  → dkDeviceCreate
  → dkMemBlockCreate (framebuffer, 2x)
  → dkSwapchainCreate
  → dkQueueCreate (Graphics)
  → per-frame: dkQueueAcquireImage → dkCmdBufClear → dkCmdBufBindRenderTarget +
    dkCmdBufSetViewports + dkCmdBufSetScissors + dkCmdBufClearColorFloat +
    dkCmdBufFinishList → dkQueueSubmitCommands → dkQueuePresentImage
```

**The bug that made early Eden tests show black:** mixing pre-recorded `DkCmdList` tokens with `dkCmdBufClear` in the same `DkCmdBuf` invalidates the pre-recorded lists (the underlying memory is reused). Fix: record full per-frame command list inline. `dkQueueAcquireImage` blocks until the GPU is done with the previous use of the slot, so per-frame cmdbuf reuse is safe.

## Boot trace from earlier run (2026-05-23, with tloz.gcm at sdmc:/dusklight/)

```
[DUSKLIGHT-TRACE] static-ctor pri101 / pri65535 / main:entry / game_main:entry
[DUSKLIGHT-TRACE] game_main: pre/post registerSettings / FinishRegistration
[DUSKLIGHT-TRACE] game_main: cxxopts::Options ctor / add_options / parse_positional
[DUSKLIGHT-TRACE] argc=0 → faking argv (Switch has no real argv)
[DUSKLIGHT-TRACE] game_main: cxxopts parsed
[DUSKLIGHT-TRACE] game_main: data::initialize_data / InitializeFileLogging
[DUSKLIGHT-TRACE] game_main: LoadFromUserPreferences / crash_reporting::initialize
[DUSKLIGHT-TRACE] game_main: SDL_AddGamepadMappingsFromFile / SDL_SetAppMetadata
[DUSKLIGHT-TRACE] game_main: aurora_initialize call
[AURORA-TRACE] aurora::initialize entry → window::initialize → initialize_event_watch
[AURORA-TRACE] backend loop: trying type=6 (OpenGLES)
[aurora::webgpu(deko)] initialize: STUB returning true     ← deko façade active
[AURORA-TRACE] webgpu init SUCCESS backend=6 g_backendType=8 (OpenGLES sentinel)
[AURORA-TRACE] window+webgpu OK → show_window
[AURORA-TRACE] gfx::initialize SKIPPED (deko3d Phase 0)    ← intentional
[AURORA-TRACE] get_window_size → returning to caller
[DUSKLIGHT-TRACE] game_main: aurora_initialize returned
[DUSKLIGHT-TRACE] game_main: VISetWindowTitle / AuroraSetViewportPolicy / VISetFrameBufferScale
[DUSKLIGHT-TRACE] game_main: audio config done
[DUSKLIGHT-TRACE] auroraInfo.backend=6 (NULL=8)            ← Aurora returned OpenGLES, not Null
[DUSKLIGHT-TRACE] game_main: dusk::ui::initialize done
[DUSKLIGHT-TRACE] game_main: SWITCH scanning sdmc:/dusklight/ for disc image
[DUSKLIGHT-TRACE] SWITCH found disc: sdmc:/dusklight/tloz.gcm   ← directory scan worked
[DUSKLIGHT-TRACE] game_main: SWITCH ISO validated, calling aurora_dvd_open
[DUSKLIGHT-TRACE] game_main: SWITCH ISO opened OK              ← aurora-switch DVD layer works!
[DUSKLIGHT-TRACE] game_main: dusk::version::init / LanguageInit / OSInit / OSGetTime
[DUSKLIGHT-TRACE] game_main: dComIfG_ct                        ← JSystem global game context
[DUSKLIGHT-TRACE] game_main: dComIfG_ct done                   ← all foundations alive
[DUSKLIGHT-TRACE] game_main: SWITCH skipping main01 -> alive loop
[DUSKLIGHT-TRACE] post-main01 alive loop frame=60 / 120 / ... / 1140
                                                               ← engine spinning, alive ~19s
```

## What's been built end-to-end

| Layer | aarch64-elf | Notes |
|---|---|---|
| devkitA64 toolchain | ✅ | newlib + libstdc++ aarch64-none-elf |
| libnx | ✅ | Linked statically. `__nx_applet_type=AppletType_Application`, `__nx_heap_size=0` (full DRAM) set in `platforms/switch/src/switch_stubs.cpp`. |
| Aurora-switch (`dantiicu/aurora-switch@f0f3511`) | ✅ | `AURORA_PLATFORM_SWITCH=ON` cleanly bypasses SDL3 |
| Dawn (`dantiicu/dawn-switch`) | ✅ | OpenGL+OpenGLES+Null backends compiled. Vulkan OFF. Tint with GLSL writer ON, SPV writer ON. |
| switch-mesa (`/opt/devkitpro/portlibs/switch/lib/`) | ✅ linked | `libEGL.a`, `libGLESv2.a`, `libglapi.a`, `libdrm_nouveau.a`. Mesa 20.1.0 (devkitPro pacman package). |
| JSystem (all 20 libs) | ✅ | JKernel/JParticle/JAudio2/J3DGraph*/J2DGraph/etc. |
| Dusklight game code | ✅ | `src/d/`, `src/m_Do/`, `src/f_op/`, `src/SSystem/`, `src/Z2AudioLib/` etc. |
| Link | ✅ | Single static binary `dusklight.elf` (663 MB w/ debug → 27 MB after strip+elf2nro) |
| Boot | ✅ | All `[DUSKLIGHT-TRACE]` lifecycle markers fire through registerSettings, cxxopts (with `argc=0` workaround), data init, SDL metadata. |
| Aurora init | ✅ | window::initialize, event watch, backend loop. Picks 2nd backend in preferred order (Null). |
| Dawn init | ⚠️ | `webgpu::initialize` returns true BUT `g_backendType = wgpu::BackendType::Null`. **OpenGL ES failed silently and Dawn fell through to Null.** |
| Renders pixels | ❌ | Null backend draws nothing. |

## Patches applied (uncommitted, all on `main`)

### Toolchain / build glue
- `CMakeLists.txt` — Switch branch: exclude PC src + re-include `JASCriticalSection.cpp`/`DspStub.cpp` + link `EGL/GLESv2/glapi/drm_nouveau`
- `platforms/switch/build-docker.sh` — Docker wrapper, adds `DAWN_ENABLE_VULKAN=OFF DAWN_ENABLE_OPENGLES=ON DAWN_ENABLE_OPENGL=OFF TINT_BUILD_GLSL_WRITER=ON TINT_BUILD_SPV_WRITER=ON`
- `extern/aurora` submodule → `dantiicu/aurora-switch@f0f3511` (Switch branch)
- `extern/aurora/cmake/AuroraDawnProvider.cmake` — respect external `DAWN_FETCH_DEPENDENCIES`
- `extern/aurora/cmake/aurora_dvd.cmake` — portlibs include path
- `extern/aurora/lib/aurora.cpp` — uncommented BACKEND_OPENGL/OPENGLES in PreferredBackendOrder; added `[AURORA-TRACE]` instrumentation throughout `initialize()`
- `extern/aurora/lib/webgpu/gpu.cpp` — added `[GPU-TRACE]` around WaitAny(adapter), WaitAny(device), create_surface
- `extern/aurora/lib/switch/tracy_stub/` — Tracy shim (FrameMark, TracyLockable, etc.)
- `platforms/switch/reference/dawn-switch/`:
  - `src/dawn/native/opengl/BackendGL.cpp` — skip dlopen on Switch, pass `&::eglGetProcAddress` directly
  - `src/dawn/native/opengl/SwapChainEGL.cpp` — Switch case calls `egl.CreateWindowSurface(display, config, NWindow*, attribs)` (line 227-236)
  - `third_party/EGL-Registry/src/api/EGL/eglplatform.h` — added `__SWITCH__` branch with `void*` types
- Abseil portability (4 files): sysinfo.cc, thread_identity.cc, elf_mem_image.h, time_zone_libc.cc
- `platforms/switch/stub-nvk/lib/libvulkan.a` — stub archive (NOT used now since Vulkan OFF, harmless to keep)

### Dusklight code
- `include/d/d_event_lib.h:49` — `#undef _C` before u16 field (newlib ctype macro clash)
- `src/dusk/main.cpp` — `[DUSKLIGHT-TRACE]` via `svcOutputDebugString` + static-ctor priority traces
- `src/m_Do/m_Do_main.cpp` — extensive traces, `argc=0` fallback, `__SWITCH__` guards on UI doc construction
- `src/dusk/achievements.cpp:1126` — `#ifndef __SWITCH__` around push_toast
- 18 source files: `#ifndef __SWITCH__` guards on SDL3 includes
- `include/dusk_pch.hpp` — Switch shim auto-include

### Switch-specific scaffolding
- `platforms/switch/src/switch_stubs.cpp` — applet config, stub bodies for filtered subsystems, free-fn stubs (execv, vk_icdGetInstanceProcAddr), ICD shim, ImGuiConsole stub
- `platforms/switch/src/dusk_sdl3_shim.h` — minimal SDL3 surface
- `platforms/switch/src/dusk_rmlui_shim.hpp` — Rml type forward decls
- `platforms/switch/src/dusk_imgui_shim.h` — ImGui type forward decls

## Boot trace (latest run, 2026-05-22 night)

```
[DUSKLIGHT-TRACE] static-ctor pri101: VERY EARLY
[DUSKLIGHT-TRACE] static-ctor pri65535: LATE
[DUSKLIGHT-TRACE] main: entry
[DUSKLIGHT-TRACE] game_main: entry → registerSettings → FinishRegistration
[DUSKLIGHT-TRACE] game_main: cxxopts ctor → add_options → parse_positional → parse
[DUSKLIGHT-TRACE] argc=0 argv=0x82..  (Switch passes argc=0!)
[DUSKLIGHT-TRACE] game_main: faking argv (Switch has no real argv)
[DUSKLIGHT-TRACE] game_main: parse done → cxxopts parsed
[DUSKLIGHT-TRACE] game_main: data::initialize_data → InitializeFileLogging → LoadFromUserPreferences
[DUSKLIGHT-TRACE] game_main: crash_reporting::initialize → SDL_AddGamepadMappingsFromFile
[DUSKLIGHT-TRACE] game_main: SDL_SetAppMetadata → pre-AuroraConfig → aurora_initialize call

[AURORA-TRACE] aurora::initialize entry
[AURORA-TRACE] aurora::initialize: window::initialize → done
[AURORA-TRACE] aurora::initialize: initialize_event_watch
[AURORA-TRACE] aurora::initialize: backend loop start → trying PreferredBackendOrder
[AURORA-TRACE] trying backend type=6  (BACKEND_OPENGLES per AuroraBackend enum)
[AURORA-TRACE] aurora::initialize: window created, calling webgpu::initialize
[AURORA-TRACE] aurora::initialize: webgpu::initialize failed, destroying window  ← SILENT FAILURE
[AURORA-TRACE] trying backend type=8  (next entry, probably BACKEND_NULL)
[AURORA-TRACE] aurora::initialize: window created, calling webgpu::initialize
[AURORA-TRACE] webgpu init SUCCESS backend=8 g_backendType=1  ← g_backendType=1 = wgpu::BackendType::Null
[AURORA-TRACE] aurora::initialize: window+webgpu OK
[AURORA-TRACE] post-OK g_backendType=1 (Null=1 OpenGL=7 Vulkan=6)
```

**Eden was killed manually at this point (45s timeout). No further traces.**

## Gaps & open questions (READ BEFORE NEXT CODE EDIT)

### Gap 1 — GLES init fails silently inside `webgpu::initialize(BACKEND_OPENGLES)`

The most important unknown. Possible specific causes (need instrumentation to confirm):
- **`eglGetDisplay(EGL_DEFAULT_DISPLAY)` returns `EGL_NO_DISPLAY` on switch-mesa** — possible but unlikely (canonical switch-examples code uses exactly this).
- **`eglChooseConfig` returns no matching EGLConfig** for Dawn's requested format — switch-mesa's config set might not advertise the right combo (RGBA8 + window-bit + ES2/ES3 conformant).
- **`eglCreateContext` fails** because Dawn requests `EGL_KHR_create_context_robustness` but switch-mesa accepts it only with specific attribs.
- **Adapter `WaitAny` times out** at 5s in `gpu.cpp:456` because Dawn's GL physical-device probing tries to create a throwaway context that hangs.

Confirmation strategy: add traces inside `DisplayEGL::InitializeWithProcAndDisplay` and `BackendGL::DiscoverPhysicalDevices`.

### Gap 2 — Eden may not support switch-mesa at all (most important blocker)

Per Agent C's research (parallel investigation):
- Eden v0.2.0 reports itself as "Vulkan-only" in startup logs.
- No `nv:*` service emulation visible in Eden's source paths (only NVN, the high-level Nintendo SDK).
- Mesa-on-Switch's nouveau gallium driver speaks to the low-level Tegra X1 hardware via `nv:dev`/`nv:gem`/`nv:host` kernel services — same services Eden's NVN-translation layer needs but at a different abstraction level.
- **Public switch-mesa homebrew (Ship of Harkinian, Simpsons HnR) is documented as working on REAL Switch hardware. There's NO public evidence it works in Eden or Yuzu.**
- Our hang/Null-fallback may be Eden's fault, not ours.

**Action implied:** test on real Switch before declaring GLES broken.

### Gap 3 — aurora-switch's `webgpu::initialize` may not even try OpenGL ES code paths

Worth verifying — looking at Dan's aurora-switch, the original SwapChainEGL never had a Switch case (Agent A confirmed it was missing and added it). If aurora-switch was never tested with the GLES backend by Dan (his goal was Vulkan-only), more silent gaps may exist downstream (Dawn's PhysicalDeviceGL, ContextEGL, swap chain configure).

### Gap 4 — `__SWITCH__` macro propagation

Verify in the build log that DAWN_PLATFORM_IS(SWITCH) actually evaluates true. The dawn-switch `Platform.h` should define it when compiling with `__SWITCH__`. If something upstream is undef-ing it, our SwapChainEGL Switch case would be dead code.

### Gap 5 — Mesa 20.1.0 (2020) might predate functional EGL on Switch

Switch-mesa's age (5 years old) means it may have known bugs. Worth grepping the devkitPro mesa patch for any TODO/FIXME/INCOMPLETE markers.

## Pivot option: try OpenGL Core on Switch (still cross-compiled, not PC)

Dan said in Discord (2026-01-30):
> "since libnx 1.4.0 switch supports open gl core (same from pc)"

He meant: switch-mesa exposes a desktop-style OpenGL core profile on Switch hardware via the same Mesa nouveau backend that powers libGLESv2. Same target (aarch64-none-elf, NRO), same Mesa, different API surface.

devkitPro provides:
- `libEGL.a` ✓ (shared with GLES)
- `libGLESv2.a` (ES path — what we tried)
- `libglapi.a` (Mesa internal dispatch — shared)
- `libGL` (core/desktop API on Switch) — **need to verify presence**

Branch `switch-port/opengl-core` will:
1. Switch `DAWN_ENABLE_OPENGLES=OFF DAWN_ENABLE_OPENGL=ON` in build-docker.sh
2. Verify devkitPro has `libGL.a` for Switch (in docker container)
3. Build, instrument, test in Eden
4. See if OpenGL core avoids whatever's killing the OpenGL ES init

## Resume strategy

If the next session opens cold:
1. Read this file end-to-end first.
2. The build cmd is still `bash platforms/switch/build-docker.sh build`.
3. ELF is at `build-switch/dusklight.elf`; NRO produced via `aarch64-none-elf-strip` + `elf2nro` inside docker (see history of `build-docker.sh` invocations).
4. Eden log location: `C:\Users\HayatoG\AppData\Roaming\Eden\log\eden_log.txt`.
5. Filter trace lines: `grep -aE "DUSKLIGHT-TRACE|AURORA-TRACE|GPU-TRACE" /c/Users/HayatoG/AppData/Roaming/Eden/log/eden_log.txt`.
6. Next step is either: instrument Dawn's BackendGL/DisplayEGL to find where GLES fails, OR pivot to `switch-port/opengl-desktop` branch.
