# Gap Audit — Dusklight Switch port (2026-05-22 night)

Systematic enumeration of assumptions we made + their verification status. Read this before committing to any next direction. Updated after multi-agent investigation.

---

## Severity legend

- 🔴 **Critical** — would block runtime even if everything else worked
- 🟡 **Suspect** — plausibly the actual root cause of the current Null-fallback
- 🟢 **Verified** — confirmed working, no further action needed
- ⚪ **Unverified but low-risk** — assumption made without proof but unlikely to be the issue

---

## Section A — Build chain

| # | Assumption | Status | Evidence |
|---|---|---|---|
| A1 | devkitA64 toolchain auto-detected by CMake | 🟢 | configure log shows toolchain detected, 2446 compile tasks emit objects |
| A2 | Aurora-switch builds on aarch64 | 🟢 | all aurora_*.a archives produced |
| A3 | Dawn full pipeline (with Tint shader compiler) builds on aarch64 | 🟢 | all dawn libs produced, GLSL writer + SPV writer ON, link succeeds |
| A4 | JSystem (20 libs) compiles unmodified for aarch64 | 🟢 | all libJSystem_*.a produced; only fix needed was JFWDisplay.cpp shim include |
| A5 | newlib `<ctype.h>` macros don't poison game headers | 🟢 | one fix at `include/d/d_event_lib.h:49` (`#undef _C` before u16 field) — no others found |
| A6 | All PC-only Dusklight source files identified for exclusion | ⚪ | filter regex `src/dusk/{audio,imgui,ui,file_select,iso_validate,autosave,gyro}` — if anything else has SDL3/RmlUi includes we'd see compile error, none seen |
| A7 | Static link order resolves all JSystem cross-refs | 🟢 | `--start-group/--end-group` wrapper at `CMakeLists.txt:463` works |
| A8 | switch-mesa link line correct (EGL→GLESv2→glapi→drm_nouveau) | 🟢 | link succeeds, no undef refs |

## Section B — libnx / Switch homebrew runtime contract

| # | Assumption | Status | Evidence |
|---|---|---|---|
| B1 | `__nx_applet_type = AppletType_Application` for full DRAM | 🟢 | added to switch_stubs.cpp this session; without it would be LibraryApplet (~512MB max — Dusklight needs more) |
| B2 | `__nx_heap_size = 0` (use all available) | 🟢 | added to switch_stubs.cpp this session |
| B3 | NRO format correct (magic NRO0, NACP attached, icon embedded) | 🟢 | `elf2nro` produces 27 MB file with NRO0 magic at offset 16, Eden recognizes title "Dusklight NX" |
| B4 | `consoleInit`/`padInitializeDefault`/`appletMainLoop` plumbing | ⚪ | Dusklight doesn't call these directly — Aurora-switch's window_switch.cpp does. Not verified each one is called. |
| B5 | libnx services we use (vi, fs, applet, hid) are init'd by libnx auto-init | ⚪ | libnx 4.x has auto-init for most services; specific ones might still need explicit |
| B6 | `argc=0` from Switch loader handled | 🟢 | confirmed via trace, our fallback installs dummy_argv before cxxopts.parse |

## Section C — Aurora-switch fork

| # | Assumption | Status | Evidence |
|---|---|---|---|
| C1 | `AURORA_PLATFORM_SWITCH=ON` bypasses SDL3 cleanly | 🟢 | aurora_switch.cmake routes through libnx; no SDL3 link errors |
| C2 | `aurora_initialize` reaches the backend loop | 🟢 | `[AURORA-TRACE] aurora::initialize: backend loop start` fires |
| C3 | PreferredBackendOrder list is correct after we uncommented OPENGL/OPENGLES | ⚪ | order is now [WEBGPU?, OPENGL, OPENGLES, NULL]. The trace shows `type=6` (OPENGLES) tried, fails, then `type=8` (probably NULL after OPENGLES skip). **OPENGL=5 not appearing in trace** — either it was skipped silently or order is different. |
| C4 | `window::initialize` does what Dan intended on Switch | 🟢 | trace shows "window::initialize done", no crash |
| C5 | `webgpu::initialize` for OPENGLES actually exercises Dawn's GLES backend | 🔴 | **suspect** — returns true but `g_backendType=Null`. Either Dawn fell through internally or aurora's logic accepts whatever Dawn picks regardless of requested backend. |
| C6 | aurora-switch's `SetupWindowAndGetSurfaceDescriptor` provides correct Switch surface descriptor | 🟢 | per Agent B's deep-read: it correctly returns `wgpu::SurfaceSourceSwitchNWindow{ .window = nwindowGetDefault() }` |
| C7 | `show_window()` on Switch is a no-op stub (doesn't block) | 🟢 | empty body at window_switch.cpp:121 per Agent B |

## Section D — Dawn

| # | Assumption | Status | Evidence |
|---|---|---|---|
| D1 | `DAWN_PLATFORM_IS(SWITCH)` macro actually evaluates to true in our build | 🟡 | not directly verified. If false, our SwapChainEGL Switch case at line 227-236 is dead code. **Verify by adding `#warning` or running preprocessor check.** |
| D2 | Dawn's `BackendGL::DiscoverPhysicalDevices` runs the Switch branch (uses eglGetProcAddress directly) | 🟡 | depends on D1 |
| D3 | `eglGetDisplay(EGL_DEFAULT_DISPLAY)` returns valid display on switch-mesa | ⚪ | switch-examples canonical pattern — should work; not directly traced |
| D4 | Dawn's `DisplayEGL::InitializeWithProcAndDisplay` succeeds | 🟡 | not directly traced. May fail if EGL extension check (EGL_EXT_create_context_robustness, EGL_KHR_fence_sync) doesn't find them via getProc-only loading (vs full ext string query). |
| D5 | `eglChooseConfig` finds a window-bit RGBA8 config | 🟡 | switch-mesa config set unknown for our requested format |
| D6 | `eglCreateContext` succeeds for ES profile | 🟡 | possible failure point |
| D7 | Surface→SwapChain creation reaches our patched Switch case | 🟡 | depends on D1-D6 succeeding first |
| D8 | Dawn doesn't have other Switch-specific gaps Dan left out | 🟡 | Dan tested only Vulkan path. OpenGL path is upstream-inherited and likely untested on Switch. May have other latent issues. |

## Section E — Mesa-on-Switch (devkitPro)

| # | Assumption | Status | Evidence |
|---|---|---|---|
| E1 | Mesa version is recent enough for Dawn's EGL requirements | 🔴 | **Mesa 20.1.0 (May 2020)** is OLD. Dawn might assume Mesa 22+ behaviors. Verify Dawn's actual EGL version requirement vs what 20.1 advertises. |
| E2 | `libEGL.a` + `libGLESv2.a` + `libglapi.a` + `libdrm_nouveau.a` is the complete dependency chain | ⚪ | hasn't required `libdrm` (system) or `libnv*` linkage; if missing, link would fail (it didn't) |
| E3 | Mesa nouveau gallium driver functional on Switch's Tegra X1 | 🟢 | proven by Ship of Harkinian, Simpsons HnR on real hardware |
| E4 | Required EGL extensions provided | 🟢 | EGL_EXT_create_context_robustness, EGL_KHR_fence_sync, EGL_KHR_reusable_sync, EGL_KHR_surfaceless_context, EGL_KHR_no_config_context all confirmed in eglext.h |
| E5 | `eglCreateWindowSurface` accepts `NWindow*` directly | 🟢 | confirmed by switch-examples es2gears/simple_triangle public pattern |

## Section F — Emulator (Eden)

| # | Assumption | Status | Evidence |
|---|---|---|---|
| F1 | Eden emulates Switch homebrew NRO loading | 🟢 | "Loading Dusklight NX" appears in eden_log |
| F2 | Eden emulates Switch's HID (pads) | 🟢 | log shows HID service init |
| F3 | Eden emulates Switch's VI/display | 🟢 | log shows VI service calls, CreateManagedDisplayLayer |
| F4 | Eden emulates Switch's `nv:dev`/`nv:gem`/`nv:host` (low-level GPU services Mesa nouveau needs) | 🔴 | **Per Agent C research: probably NOT.** Eden v0.2.0 startup banner reports "Vulkan-only". Yuzu/Eden's GPU emulation strategy is NVN/Vulkan translation, not raw Tegra X1. **Public switch-mesa homebrew has zero documented runs in Eden/Yuzu.** This may be why our GLES init fails silently in Eden — it'd work on real Switch hardware. |
| F5 | Eden surfaces svcOutputDebugString to log | 🟢 | our `[DUSKLIGHT-TRACE]` lines appear in eden_log.txt |

## Section G — Specific assumptions made by this codebase

| # | Assumption | Status | Evidence |
|---|---|---|---|
| G1 | `TARGET_PC` not defined on Switch | 🔴 | **WRONG** — it IS defined on Switch (line CMakeLists:317). Caused 1 wasted iteration. Use `#ifndef __SWITCH__` instead. |
| G2 | `dusk::IsRunning` etc. globals defined unconditionally | 🟢 | restored this session (was wrongly inside `#ifndef __SWITCH__`) |
| G3 | `dusk::ui::*` document subclasses callable on Switch | 🔴 | NO — guarded all push_document call sites with `#ifndef __SWITCH__` |
| G4 | Achievements `push_toast` callable on Switch | 🔴 | NO — guarded the one call site |
| G5 | `iso_validate` / `autosave` / `gyro` / `file_select` / SDL3-using UI need libnx replacements | ⚪ | currently all stubbed/excluded; functional replacements are out of scope for "first frame on screen" |

---

## 🔥 BREAKTHROUGH (2026-05-22 late night): F4 INVALIDATED, real culprit found

Built a **standalone EGL probe** (`platforms/switch/egl_test/`) that links directly against devkitPro's switch-mesa with **zero Dawn/Aurora/Dusklight code**. Ran in Eden. Result:

```
EGL 1.4 / Mesa Project
nouveau / NV120 (Tegra X1)
OpenGL ES 3.2 Mesa 20.1.0-rc3
eglCreateContext OK / eglMakeCurrent OK
600 frames cycled clear color + swapped successfully
```

**This proves:**
- ✅ Eden DOES emulate Switch's `nv:*` services well enough for switch-mesa GLES
- ❌ **Gap F4 is INVALIDATED** — Eden is not the bottleneck
- ✅ Switch-mesa nouveau gallium driver works on Tegra X1 in Eden
- ✅ The whole public-GLES path is viable for end-to-end development without real hardware

**New root cause identified — Gap D4 confirmed and refined:**

Mesa 20.1.0-rc3 on Switch advertises ONLY 4 EGL extensions:
- `EGL_KHR_config_attribs`
- `EGL_KHR_create_context`
- `EGL_KHR_get_all_proc_addresses`
- `EGL_KHR_surfaceless_context`

**Dawn's `BackendGL.cpp:84-92` mandates two extensions that Mesa 20.1 doesn't advertise:**
```cpp
if (!display->egl->HasExt(EGLExt::CreateContextRobustness)) {
    return DAWN_VALIDATION_ERROR("EGL_EXT_create_context_robustness is required.");
}
if (!display->egl->HasExt(EGLExt::FenceSync) && !display->egl->HasExt(EGLExt::ReusableSync)) {
    return DAWN_INTERNAL_ERROR("EGL_KHR_fence_sync or EGL_KHR_reusable_sync must be supported");
}
```

This is why `webgpu::initialize(BACKEND_OPENGLES)` returned false silently in our prior runs — Dawn rejected Mesa 20.1, Aurora caught the error and fell through to BACKEND_NULL.

**Earlier agent research (Section E of `mesa-on-switch`) said all 5 EGL extensions Dawn needs ARE in switch-mesa's `eglext.h` header.** That was a **header presence check, not runtime advertisement check.** Mesa builds with these macros DEFINED but doesn't IMPLEMENT them at runtime. Important nuance we missed.

**Fix path (in progress):**
1. Patch `BackendGL.cpp` Switch case to skip the two mandatory checks → continue with degraded mode
2. Map every Dawn callsite that uses these extensions at runtime (`eglCreateSyncKHR`, `EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT`, etc.) and stub them with `glFinish()` / strip-from-attrib-list fallbacks on Switch
3. Rebuild Dusklight, retest in Eden

## Highest-priority unknowns (next session focus)

### Unknown #1 — Is `DAWN_PLATFORM_IS(SWITCH)` actually defined?

**Test:** add a static_assert or `#error` inside our SwapChainEGL Switch case. If the build fails, macro is defined. If it builds, macro might be undefined and the case is dead. Or build with `-E` flag on that .cpp and grep the preprocessed output.

### Unknown #2 — What's the exact `webgpu::initialize(BACKEND_OPENGLES)` failure mode?

**Test:** add traces inside `extern/aurora/lib/dawn/BackendBinding.cpp` (the Switch surface descriptor builder) and `gpu.cpp:447 RequestAdapter` callback (capture the `message` string Dawn passes when adapter fails). Currently we only know it returned false — not WHY.

### Unknown #3 — Will OpenGL Core work in Eden where ES doesn't?

**Test:** branch `switch-port/opengl-core` flipping flags. If Eden's stub `nv:*` services are deeper than we think, OpenGL might fail the same way. But if Dawn's OpenGL backend exercises different code paths in Eden's emulation, this could leapfrog.

### Unknown #4 — Does this work on real Switch hardware?

**Test:** copy the .nro to `sdmc:/switch/Dusklight/Dusklight.nro` on real Switch with Atmosphère, launch from hbmenu. **This is the only way to fully validate our pipeline absent Eden's Mesa-emulation gap.**

---

## Recommendations (in priority order)

1. **Branch `switch-port/opengl-core`** — flip DAWN_ENABLE_OPENGLES OFF + DAWN_ENABLE_OPENGL ON, rebuild, retest in Eden. Cheap experiment (~10min).

2. **Add Dawn-internal traces** on `main` — instrument BackendGL.cpp/DisplayEGL.cpp to capture which EGL call fails. Pin down Unknown #2 with precision.

3. **Test on real Switch hardware** — if you have one, this is the highest-signal next test. May reveal F4 (Eden mesa gap) is the entire problem.

4. **Web-search Mesa 20.1 Switch known issues** — check whether devkitPro shipped any newer Mesa to Pacman (the package might have updated since 20.1.0-5).
