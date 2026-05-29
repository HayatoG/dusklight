# Plan: Dusklight prelaunch MENU on deko3d (RmlUi backend)

**Decided 2026-05-23 (user):** do the **menu first, then gameplay**. The Dusklight
prelaunch menu (the launcher/settings UI the user has seen on PC *and* on Dan's Switch
port) is **RmlUi** (HTML/CSS-style UI), rendered separately from the GX game path.

## Why the menu is a good first target on deko3d

- **RmlUi shaders are FIXED and known at build time** (Normal/Masked/Clip/blur/gradient/
  dropshadow/etc.) — NOT runtime-generated like GX TEV. So they can be precompiled with
  `uam` → DKSH. The menu **completely sidesteps the GX shader-cache problem**
  ([[deko3d-no-runtime-shader-compile]]).
- Gives a **real, interactive Dusklight screen** on deko3d much sooner (low risk).
- Dan's Switch port runs this exact menu (via Mesa runtime shaders) — proves it works on
  Switch; we just need a deko3d render backend instead of Mesa.
- Validates deko **textures, alpha blending, stencil/clip, multiple pipelines, render
  targets** — infra the gameplay path will also need.
- **Caveat:** the menu is a SEPARATE render path from gameplay (RmlUi, not GX). Finishing
  the menu does NOT advance GX→deko. Gameplay (Phase 4 / `PLAN_PHASE4.md`) comes after.

## Current integration facts (verified 2026-05-23)

- RmlUi is currently **OFF** in the Switch build (`AURORA_ENABLE_RMLUI=OFF`).
- Build wiring: `extern/aurora/cmake/aurora_core.cmake:43-55` — when `AURORA_ENABLE_RMLUI`
  is set, compiles `lib/rmlui.cpp` + `lib/rmlui/{RmlUi_Backend_Aurora,WebGPURenderInterface,
  SystemInterface_Aurora,FileInterface_SDL}.cpp` and links `rmlui` + `rmlui_backends`.
- The render interface is `aurora::rmlui::WebGPURenderInterface`
  (`lib/rmlui/WebGPURenderInterface.{hpp,cpp}`). The `.cpp` is **107 KB** and contains the
  **embedded WGSL shaders** + all render logic (geometry, textures, scissor, stencil clip
  masks, layers/compositing, filters: blur/dropshadow/colormatrix/opacity, gradients).
- It is **feature-rich** — far beyond "textured triangles." But all shaders are fixed.
- The Dusklight UI lives in `src/dusk/ui/` (`prelaunch.cpp`, `menu_bar.cpp`, `settings.cpp`,
  `achievements.cpp`, `controller_config.cpp`, ...) — currently EXCLUDED on Switch
  (CMakeLists FILTER `src/dusk/ui/`). `src/dusk/imgui/` = debug tools (console, save editor),
  separate concern.
- `prelaunch.cpp` loads `res/rml/prelaunch.rcss` and is the disc-selection launcher
  (`activeDiscPath`).

## Open questions to resolve before/while implementing

1. **Does the RmlUi library build for aarch64 (devkitA64)?** It's portable C++; need an
   `rmlui`/`rmlui_backends` target available to the Switch build (find_package or vendored).
2. **Which RmlUi features does the prelaunch menu actually use?** If it avoids blur/
   dropshadow/gradients/layers, a MINIMAL backend (geometry + textures + scissor + clip
   mask) renders most of it; advanced filters can be stubbed first.
3. **`FileInterface_SDL.cpp` + `SystemInterface_Aurora` use SDL** — need libnx-native
   replacements (file IO via romfs/sdmc; clock/log via libnx).
4. **How is RmlUi hooked into the frame loop?** `rmlui.cpp::render(encoder, viewport)` is
   wgpu-driven; need a deko equivalent called from the deko frame loop, BEFORE the GX path.
5. **Does the menu run before `main01`/the GX loop, or interleaved?** Determines where the
   deko RmlUi render call goes.

## Implementation sketch (subject to refinement after Q&A above)

### M0 — build RmlUi for Switch — ✅ DONE 2026-05-23 (RmlUi stack COMPILES for aarch64)

**Findings (M0 investigation + test build):**
- ✅ **RmlUi library compiles for aarch64** (205 `rmlui_core` objects) — the key unknown.
  Brought in via FetchContent (pinned tarball), `RMLUI_CUSTOM_RTTI=ON` on Switch.
- ✅ **All aurora RmlUi glue compiles** on the Switch+deko build: `rmlui.cpp`,
  `RmlUi_Backend_Aurora.cpp`, `SystemInterface_Aurora.cpp`, `FileInterface_SDL.cpp`, and
  `WebGPURenderInterface.cpp`.
- ✅ **SDL was NOT a blocker**: aurora ships its own `extern/aurora/include/SDL3/SDL.h`
  shim, so the SDL3 includes (`SDL_GetTicksNS`, file IO) resolve at compile time. (Runtime
  link of SDL3 symbols still unverified — see open item below.)
- ✅ The whole RmlUi-on-Switch path was **designed but never compiled before** — it has
  `#ifdef AURORA_PLATFORM_SWITCH` branches throughout (timer, png-based texture load, RTTI,
  INTERFACE backends w/o SDL). We are the first to actually build it.
- ✅ Deps all present in devkitPro: Freetype v2.13.3 + png16/harfbuzz/bz2/z portlibs.

**Fixes applied:**
- `platforms/switch/build-docker.sh`: `AURORA_ENABLE_RMLUI=ON` (was OFF).
- `extern/aurora/cmake/aurora_core.cmake`: added `$DEVKITPRO/portlibs/switch/include` to
  `aurora_core` includes on Switch (WebGPURenderInterface.cpp needs `<png.h>`). This was
  the only code-side fix needed.

**Only non-code blocker hit:** transient `ar: ... libwebgpu_dawn.a; Input/output error`
archiving Dawn's huge static lib over the Windows Docker bind mount — environmental, not
code. Retried.

**IMPORTANT nuance for M1:** `WebGPURenderInterface.cpp` *compiles* on deko (Dawn `wgpu::*`
types are linked) but will NOT *work* at runtime — it creates wgpu device/pipelines that
are null/stub on the deko backend. So M1 still needs a real deko render interface; the
compiling code is a useful structural reference, not a working renderer.

**Open items for M1:**
- Replace/augment `WebGPURenderInterface` with a deko3d render interface (the real work).
- Verify SDL3 symbols don't break the final link (the SDL3 shim is headers-only?). If the
  link needs SDL3 funcs (`SDL_GetTicksNS`, `SDL_IOFromFile`), provide libnx/stdio versions.
- Re-include `src/dusk/ui/` (or the minimal prelaunch subset) in the Switch build + wire
  the prelaunch into the deko frame loop.

### M1 — DekoRenderInterface (minimal)
- New `lib/rmlui/DekoRenderInterface.{hpp,cpp}` implementing `Rml::RenderInterface`:
  `CompileGeometry`/`RenderGeometry` (vtx+idx → DkMemBlock, draw), `LoadTexture`/
  `GenerateTexture` (→ DkImage/DkSampler), `EnableScissorRegion`/`SetScissorRegion`,
  `SetTransform` (MVP uniform). Stub clip mask/layers/filters initially.
- Precompile the RmlUi "Normal" + textured shaders: port the embedded WGSL → GLSL → uam →
  DKSH (embed like the triangle). One/few fixed shaders.
- Signal: prelaunch menu renders (text + boxes + images), maybe missing fancy effects.

### M2 — clip masks + scissor correctness
- Stencil-based clip mask (RmlUi `EnableClipMask`/`RenderToClipMask`) → deko stencil.
- Signal: rounded/clipped UI elements correct.

### M3 — input + interactivity
- Wire libnx HID → RmlUi input events (pointer/keys) via `SystemInterface`/Backend.
- Signal: menu navigable with the controller; disc selection works.

### M4 — filters/gradients (as needed)
- Port blur/dropshadow/gradient/colormatrix shaders only if the menu uses them.

## Anti-patterns to respect
- #4 prove layer below first: M1 minimal (geometry+texture) before clip/filters.
- Reuse the proven triangle DKSH-embed pattern for the fixed RmlUi shaders.
- Trace with `dusk_switch_log` (#9). Record inline per frame; deko cmdbuf-clear caveat (#8).
