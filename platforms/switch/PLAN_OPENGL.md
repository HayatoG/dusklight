# Plan: OpenGL Core experiment on Switch

**Branch:** `switch-port/opengl-core` (off `main`)
**Created:** 2026-05-22 night
**Hypothesis:** Dawn's OpenGL core backend on Switch (via switch-mesa) might succeed where the OpenGL ES path failed silently.

---

## Why this might work where ES didn't

The ES path failure in Eden is suspected to be one of:
1. Mesa-on-Switch's GLES context creation hits an Eden emulation gap (`nv:dev`/`nv:gem` services)
2. `eglChooseConfig` fails for ES-specific config
3. Dawn's ES context attribs don't match switch-mesa's ES advertised capabilities

OpenGL Core path differs in:
- `eglBindAPI(EGL_OPENGL_API)` instead of `EGL_OPENGL_ES_API`
- Different context profile bit (`EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT`)
- Different version constraints (GL 3.3+ vs ES 3.0+)
- Dawn enables more features (storage textures, atomic ops, etc.) on core

If switch-mesa's nouveau driver has better core-profile support than ES (true for Tegra X1 — it advertises GL 4.3 core but ES 3.2), AND/OR Eden's stub services handle the GL_API path differently, this could leapfrog the ES failure.

**Reasonable confidence:** 30%. Even if Dawn picks the core backend successfully in Eden, we still face the open question of whether Eden actually emulates switch-mesa's underlying nv:* services. If F4 from `GAP_AUDIT.md` is the real issue, neither GLES nor GL core will work in Eden — both need real Switch hardware.

---

## What's already done on this branch

1. **`platforms/switch/build-docker.sh`** — flipped flags:
   - `DAWN_ENABLE_OPENGLES=OFF`
   - `DAWN_ENABLE_OPENGL=ON`
   - (Vulkan still OFF, Null inherited as fallback)
   - TINT_BUILD_GLSL_WRITER stays ON (Dawn transpiles WGSL → GLSL for both paths)

2. **No other code changes** — the goal is to ISOLATE the variable. Aurora's PreferredBackendOrder picks BACKEND_OPENGL when DAWN_ENABLE_BACKEND_OPENGL is defined. SwapChainEGL's Switch case at line 227-236 handles both GL and GLES (same `egl.CreateWindowSurface` path).

---

## Steps to run the experiment

### 1. Sanity-check Dawn's PreferredBackendOrder accepts GL

In `extern/aurora/lib/aurora.cpp` PreferredBackendOrder list:

```cpp
#ifdef DAWN_ENABLE_BACKEND_DESKTOP_GL    ← Dawn's macro name for "core GL"
    BACKEND_OPENGL,
#endif
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    BACKEND_OPENGLES,
#endif
```

`DAWN_ENABLE_BACKEND_DESKTOP_GL` is defined by Dawn's CMakeLists when `DAWN_ENABLE_OPENGL=ON`. So the list will include `BACKEND_OPENGL` first. ✓ Already wired.

### 2. Build

```bash
bash platforms/switch/build-docker.sh build
```

Expected outcomes:
- **Compile fails** with missing GL symbols → revisit Dawn's OpenGL backend, check what `libGLESv2.a` exports (should cover core too via libglapi)
- **Compile succeeds** → proceed to step 3

### 3. Strip + package + test

```bash
MSYS_NO_PATHCONV=1 docker run --rm \
  -v "D:\Projects\dusklight:/dusklight" \
  -w "/dusklight" devkitpro/devkita64 bash -c \
  "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-strip -o /tmp/d.elf /dusklight/build-switch/dusklight.elf && \
   /opt/devkitpro/tools/bin/elf2nro /tmp/d.elf /dusklight/build-switch/dusklight.nro \
   --icon=/dusklight/platforms/switch/icon.jpg --nacp=/dusklight/platforms/switch/Dusklight.nacp"

cp build-switch/dusklight.nro platforms/switch/Dusklight.nro

# Eden test
/d/Eden-Windows-v0.2.0-amd64-gcc-standard/eden-cli.exe \
  "D:/Projects/dusklight/platforms/switch/Dusklight.nro" &
sleep 40
kill %1

# Inspect traces
grep -aE "DUSKLIGHT-TRACE|AURORA-TRACE|GPU-TRACE|trying backend" \
  /c/Users/Guilherme/AppData/Roaming/Eden/log/eden_log.txt | tail -50
```

### 4. Interpret results

**Best case:** Trace shows `trying backend type=5 (OPENGL)` → `webgpu init SUCCESS backend=5 g_backendType=7` (where 7 = wgpu::BackendType::OpenGL). Then we know GL core works on Switch via Dawn — proceed to wire up the render loop.

**Same as main:** Trace shows `trying backend type=5` failing silently → falls to Null. Confirms the failure is deeper (Mesa or Eden, not the API choice). Decision: instrument Dawn internals on `main`, or move to real Switch hardware testing.

**New error mode:** Trace shows an explicit error message. Capture it — that's the diagnostic we couldn't get from the ES path.

### 5. Decide next move

- If GL works in Eden → continue on this branch, wire up render loop and merge to main.
- If GL fails like GLES did → switch back to `main`, instrument Dawn internals to find the precise failure point.
- If both fail in Eden but maybe work on real hw → test on real Switch.

---

## What might go wrong (risk register)

| Risk | Probability | Mitigation |
|---|---|---|
| Dawn's OpenGL backend has more host-platform deps than ES (X11/Wayland) | Medium | Check Dawn `BackendGL.cpp` — both ES and core go through same DisplayEGL+ContextEGL classes. Should share code paths. |
| Tegra X1 GL core is buggy in Mesa 20.1 | Medium | Old Mesa = known bugs. Worst case: rebuild devkitPro Mesa from a newer source. |
| libGLESv2.a exports only ES symbols, GL core needs separate lib | Low | Mesa dispatches both via libglapi; libGLESv2's symbol table includes core GL fns (verified `glBegin` symbols present). |
| Eden lacks nv:* services for BOTH paths | High | Same hypothesis as GLES — only real hardware can distinguish. |
| Tint GLSL writer emits ES syntax → core GL parser rejects | Medium | Tint should emit appropriate GLSL version based on backend. Verify by tracing actual emitted shader source. |

---

## Aborting back to main

```bash
git checkout main
# All changes on opengl-core stay in that branch.
# To go back to opengl-core later: git checkout switch-port/opengl-core
```

The `build-switch/` directory is shared between branches (it's gitignored). To force clean rebuild after switching branches:

```bash
docker run --rm -v "D:\Projects\dusklight:/dusklight" -w /dusklight devkitpro/devkita64 bash -c "rm -rf build-switch/CMakeCache.txt build-switch/CMakeFiles/dusklight.dir build-switch/dusklight.elf"
```

(Just nukes the dusklight target's cache + CMakeCache. Dawn's compiled objects stay cached.)
