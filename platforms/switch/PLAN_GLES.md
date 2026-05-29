# PLAN_GLES.md — Native Aurora OpenGL ES backend for Switch (reusable)

**Status:** PLAN (2026-05-25). Supersedes the deko3d render path (`PLAN_DEKO3D.md` / `PLAN_PHASE4.md`) for gameplay rendering. The engine/boot/heap/RmlUi infra is backend-agnostic and carries over.

## Goal & rationale

Replace the deko3d gameplay-render backend with a **native Aurora OpenGL ES backend on Mesa** (switch-mesa: `libEGL`/`libGLESv2`/`libglapi`/`libdrm_nouveau` over libnx nv services), designed to be **clean, self-contained, and REUSABLE/distributable** for other GameCube/Wii→Switch ports built on Aurora.

**Why GLES, not deko3d:** deko3d cannot compile shaders on-device (`uam` is offline CLI-only) → forces an offline precompiled-DKSH cache keyed by `ShaderConfig` hash, whose real cost is an open-ended **shader-COVERAGE long tail** (every TEV/material config must be captured by exercising the whole game, or a runtime draw has no shader). **Mesa-GLES compiles GLSL on-device at runtime (`glCompileShader`) → the entire precompile + coverage problem disappears.** That is the decisive win.

**Why NATIVE GLES, not Dawn-on-GLES:** the prior Dawn-on-GLES attempt fell back to the Null backend because Dawn's `BackendGL.cpp:84-92` hard-requires `EGL_EXT_create_context_robustness` + (`EGL_KHR_fence_sync`|`EGL_KHR_reusable_sync`), and Mesa 20.1 advertises only 4 EGL extensions (`GAP_AUDIT.md:117-121`). A native backend issues `gl*` directly against the context — never asks for those extensions — so that failure class is structurally eliminated.

**Why not NVK-Vulkan (Dan's path):** NVK barely supports Maxwell/Tegra X1; depends on a private Mesa fork (kills open-source/reuse); its only edge (runtime compile) is also GLES's. See `[[dusklight-gles-backend-plan]]`.

## What's already de-risked (don't reinvent)

1. **Raw EGL + GLES 3.2 works on Eden + Tegra X1** — proven by `platforms/switch/egl_test/` (`source/main.c:69-137`): full EGL bring-up on a libnx `NWindow`. Runtime-confirmed (`GAP_AUDIT.md:101-107`): `EGL 1.4 / Mesa`, `nouveau / NV120 (Tegra X1)`, **`OpenGL ES 3.2 Mesa 20.1.0-rc3`**, 600 frames presented. → GLES 3.1 features (SSBO, compute, `gl_VertexID`) are on the table.
2. **The deko backend already carved the exact seams** a GLES backend needs (see "Seams" below). The GX command IR is fully captured and backend-agnostic.
3. **Link recipe is known** (`egl_test/Makefile:30`): `-lEGL -lGLESv2 -lglapi -ldrm_nouveau` (+ `nx m z`). Gotcha: `libEGL.a` contains C++ symbols (Mesa's `nv50_ir`) → final link must use the C++ driver — Aurora already does.

## Architecture — the seams (mirror the deko backend under a NEW `AURORA_BACKEND_GLES` flag)

The deko backend lives in `namespace aurora::webgpu` and re-implements the public surface in `lib/webgpu/gpu.hpp`; `gfx/` and `gx/` call `webgpu::*` unchanged — only the impl TU swaps. Add a parallel `AURORA_BACKEND_GLES` (keep both backends shippable; keeps the GLES path self-contained for reuse).

| Seam | deko site | GLES replacement |
|---|---|---|
| Backend identity | `aurora.cpp:185-233`; deko fakes `g_backendType=OpenGLES` (`deko/gpu.cpp:39,256`) | request `BACKEND_OPENGLES` (=6, `aurora.h:15-25`); same fake |
| Context create | `deko_initialize()` `deko/gpu.cpp:147-229` (Dk{Device,Swapchain,Queue}) | EGL bring-up from `egl_test/source/main.c:59-137` → `lib/gles/gpu.cpp::initialize()` |
| Per-frame begin/end | `aurora.cpp:330-342` / `399-408` (`deko_begin/end_frame`) | `glClear` + run GLES replay + `eglSwapBuffers`. FIFO drain (`gx::fifo::drain()`) is backend-agnostic — reuse |
| gfx setup | `gfx::deko_initialize()` via `aurora.cpp:259-270` | `gfx::gles_initialize()` — create UBO/2×SSBO/EBO + bind |
| Replay | `gfx::deko_replay()` `gfx/common.cpp:829-911` (logs only) | `gfx::gles_replay()` — walk `g_renderPasses`, issue real `gl*` draws |

**Recommendation:** new TUs `lib/gles/gpu.cpp` + `lib/gles/gfx_gles.cpp` + `lib/gx/shader_glsl.cpp`; new CMake flag `AURORA_BACKEND_GLES` branching at `aurora_core.cmake:89` and `:129`.

## Reusable from the GX IR (unchanged)

Everything above the backend is backend-agnostic and reused as-is:
- Command IR (`gfx/common.cpp:40-150`): `RenderPass`/`Command`/`ShaderDrawCommand`, `g_renderPasses`.
- `gx::fifo::drain()` (`aurora.cpp:404`) turns GX register writes into `gx::DrawData`.
- `gx::DrawData` (`gx/pipeline.hpp:7-17`): `pipeline` (xxh3 hash of `PipelineConfig`, `pipeline_cache.cpp:154`), `vertRange`/`idxRange`/`uniformRange` (offsets into shared byte buffers), `vtxCount`/`indexCount`/`instanceCount`, `bindGroups.textureBindGroup`, `dstAlpha`.
- Buffer pools `g_verts`/`g_uniforms`/`g_indices`/`g_storage` (`gfx/common.hpp:174-177`: uniform 24MB, vertex/storage 3MB, index 1MB, storage 8MB; `UseTextureBuffer=false`).

**GLES per-draw replay** = mirror the canonical 16-line `gx::render` (`gx/pipeline.cpp:17-33`):
- `glUseProgram(program_for(data.pipeline))` + depth/cull/blend from `PipelineConfig`.
- uniform: `glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, uniformRange.offset, .size)`.
- vertex-pull: `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vbo)` + `(…,1,sbo)` (the VS reads `vbuf`/`abuf` SSBOs indexed by `gl_VertexID` — no VAO attribs).
- textures from `data.bindGroups.textureBindGroup`.
- `glDrawElementsInstanced(prim, indexCount, GL_UNSIGNED_SHORT, (void*)idxRange.offset, instanceCount)` (indices are **Uint16**, `pipeline.cpp:27`).
- `dstAlpha != UINT32_MAX` → `glBlendColor` (`pipeline.cpp:28-31`).

## Shader path — direct GX→GLSL ES generator (Path B, recommended)

Aurora generates **WGSL only** (`shader.cpp:678-1729`, TEV→WGSL via `fmt::format` → `CreateShaderModule(wgpu::ShaderSourceWGSL)`). Two options:
- **Path A (rejected):** GX→WGSL→Tint→GLSL at runtime (`TINT_BUILD_GLSL_WRITER=ON` exists) — drags the heavy Dawn/Tint runtime into the link (the thing we're dropping) and the WGSL→GLSL-ES mapping of storage byte-pulling is fragile.
- **Path B (RECOMMENDED):** a parallel `lib/gx/shader_glsl.cpp` mirroring `shader.cpp`, emitting GLSL ES 3.10/3.20, compiled at runtime (`glCreateShader`/`glShaderSource`/`glCompileShader`/`glLinkProgram`), cached as `GLuint` by the **same xxh3 ShaderConfig hash** already used as `PipelineRef`. This is Phase 4a's generator **minus** 4b/4c (capture+bake deleted — GLES compiles on-device). Mechanical WGSL→GLSL deltas (per the research): `vec3f`→`vec3`, `@builtin(vertex_index)`→`gl_VertexID`, `textureSample`→`texture`, `bitcast<f32>`→`uintBitsToFloat`, `extractBits`→`bitfieldExtract`, `select(a,b,c)`→`(c?b:a)`; bindings → `layout(std430/std140, binding=N)`. Byte-pull helpers (`load_u8/16/24/32`, `bswap*`, `shader.cpp:1380-1426`) port near-verbatim (integer ops core in ES 3.1).

## RmlUi (2D / prelaunch menu)

Selected at `RmlUi_Backend_Aurora.cpp:13-25` / `aurora_core.cmake:52-62`. RmlUi ships a stock **`RmlUi_Renderer_GL3`** (`Backends/`, already on the include path via `rmlui_backends`). Add a third `#ifdef AURORA_BACKEND_GLES` arm: adapt `RmlUi_Renderer_GL3` to ES (`#version 330`→`310 es` + precision quals + drop desktop-only calls), sharing the EGL context. Fallback: model a thin interface on `DekoRenderInterface.cpp` (already does libpng load + offscreen-layer opacity on Switch).

## Build / linking

1. CMake flag `AURORA_BACKEND_GLES` (parallel to `AURORA_BACKEND_DEKO3D`), branch at `aurora_core.cmake:89,129`.
2. Link: `target_link_libraries(aurora_core PRIVATE EGL GLESv2 glapi drm_nouveau nx m z)` (drop `deko3d` for this backend).
3. Mesa libs = devkitPro portlib `switch-mesa` (+ `switch-libdrm_nouveau`); install in the `devkitpro/devkita64` container (`dkp-pacman -S switch-mesa` — **verify exact pkg name, open item**). egl_test already links them from `$(PORTLIBS)/lib`.
4. Dawn can be dropped from the GLES link eventually (reuse `lib/webgpu/wgpu.hpp` type stubs like deko does); easiest first = keep Dawn dead-linked like deko, get GLES working, strip later. `TINT_BUILD_GLSL_WRITER` becomes irrelevant under Path B.
5. Applet/heap already set in `switch_stubs.cpp` (`AppletType_Application`, `__nx_heap_size=0`).

## Phased implementation

- **G0 — Re-prove the POC + query caps (½ day).** Rebuild/run `egl_test.nro` on the current container; `glGetIntegerv` for `GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS`, `GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS` (**gate the whole effort on ≥2 vertex-stage SSBOs**), `GL_MAX_SHADER_STORAGE_BLOCK_SIZE`, `GL_MAX_UNIFORM_BLOCK_SIZE`, `GL_MAX_TEXTURE_IMAGE_UNITS`. (Anti-pattern #4: prove the layer below first.)
- **G1 — EGL context as an Aurora backend skeleton. ✅ DONE + PROVEN ON REAL HW (2026-05-25).** Added `AURORA_BACKEND_GLES` + `lib/gles/gpu.cpp` (namespace `aurora::webgpu`, EGL bring-up from egl_test → ES 3.1 context → clear-color present); wired `aurora.cpp` (3 frame seams generalized to `DEKO3D || GLES`), `webgpu/gpu.hpp`, `aurora_core.cmake` (GLES branch + portlibs include + Mesa link), `build-docker.sh` (GLES default, `DUSK_GLES`/`DUSK_DEKO`/`DUSK_RMLUI` toggles). On real Tegra: `[aurora::webgpu(gles)] NV120 | OpenGL ES 3.2 Mesa 20.1.0-rc3 | GLSL ES 3.20 ... gles_initialize: ALL OK`, Aurora selected the GLES backend, **10 frames presented the clear color (user saw BLUE on the TV)** before the (backend-agnostic) vibration crash #5. Two G1 stubs in gles/gpu.cpp (`aurora_switch_begin_gx_capture`, `dusk_switch_disable_menu`) — REMOVE in G2/G6. Built with `DUSK_RMLUI=OFF` (GLES RmlUi interface is G6).
- **G2 — GL buffer pools + IR replay w/ placeholder shader.** 🔶 **STEP 1 DONE (2026-05-25):** capture wired to GLES (generalized the deko `#ifdef` guards incl. common.hpp decls; texture.cpp 5 GLES stubs; `find_pipeline_impl` hash-only) → engine boots to TITLE on HW + **GX capture works (`gx=1`, `[title] Draw`)**. ❌ but NO pixels — `deko_replay` is still LOG-ONLY. 🔲 **STEP 2 (the pixel work, NEXT):** (a) fix the ByteBuffer `push`/`map` crash — a UINT32_MAX sentinel length → `resize(huge)`→`realloc` null→`memset(0x0)` (guard it). (b) `gles_initialize()` (GL SSBO `g_verts`/EBO `g_indices`/UBO `g_uniforms`) + `gles_replay()` (per draw: bind, runtime flat GLSL ES, `glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT)`). **REAL layout (`[gfx-deko][S2DUMP]`): verts = BIG-ENDIAN XYZ, `stride=12`, position-only; uint16 triangle indices; NO per-draw uniform (`uni size=UINT32_MAX`) → MVP/transform source is the open question.** Un-stub stays as-is (fapGm already runs). **Milestone:** first flat game-geometry on screen.
- **G3 — Runtime GX→GLSL generator (long pole).** `lib/gx/shader_glsl.cpp`; compile/link/cache by ShaderConfig hash; validate byte-pull + TEV math. **Milestone:** title screen with correct (untextured) shading.
- **G4 — Textures.** aurora already decodes all formats → RGBA8 (`texture_convert.cpp:500`); `glTexImage2D(GL_RGBA8)` + `glGenSamplers` keyed off `textureBindGroup`; wire `build_bind_groups` (`gx.cpp:808`) GLES arm. **Milestone:** textured gameplay.
- **G5 — Pipeline state + offscreen/EFB.** Map `PipelineConfig` → `glEnable(GL_DEPTH_TEST)`/`glDepthFunc` (honor `UseReversedZ=true`), cull, blend, masks; offscreen passes via FBOs + EFB→XFB fullscreen-triangle blit.
- **G6 — RmlUi GLES interface.** Adapt `RmlUi_Renderer_GL3` to ES; prelaunch HOME parity with deko.
- **G7 — Drop Dawn from the GLES link + package as reusable backend.** Document the `AURORA_BACKEND_GLES` flag + Mesa portlib requirement for other ports.

## Open risks / unknowns

1. **✅ RESOLVED (G0, 2026-05-25, Eden/switch-mesa nouveau):** vertex-stage SSBO is supported with huge margin — `GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 16` (needed ≥2), `MAX_SHADER_STORAGE_BUFFER_BINDINGS=80`, `MAX_SHADER_STORAGE_BLOCK_SIZE=128MB`, `MAX_UNIFORM_BLOCK_SIZE=64KB`, GLES 3.2 / GLSL ES 3.20. **The vertex-pulling architecture is GO.** Bonus extensions present: `GL_EXT_buffer_storage` (persistent-mapped buffers → mitigates risk #4), `GL_KHR_parallel_shader_compile` (mitigates shader-compile hitch), `GL_OES_get_program_binary` (can cache compiled programs to disk). Re-confirm on real HW (likely identical — same Mesa nouveau driver; Eden only emulates the nv kernel iface).
2. **Mesa 20.1 is old (2020).** GLSL ES compiler bugs / `#version` quirks → generate the lowest ES with SSBO (3.10); test incrementally.
3. **Eden vs hardware.** POC proves basic GLES on Eden; heavy SSBO/draw loads may differ. Test on Atmosphère HW at G2 and G3 (use the nxlink + FTP crash-report loop, heuristics #19).
4. **Per-frame buffer re-upload cost** (24+8+3+1 MB): use `glMapBufferRange(WRITE|UNSYNCHRONIZED)` / orphaning; persistent-mapped if `GL_EXT_buffer_storage` advertised.
5. **EGL extension scarcity** (4 only, no fence_sync): use core ES `glFenceSync`/`glClientWaitSync` (ES 3.0), not EGL fences.
6. **RmlUi GL3→GLES layer/filter delta** (opacity fades): if messy, model on `DekoRenderInterface`.
7. **Index format:** replay assumes `GL_UNSIGNED_SHORT` (`pipeline.cpp:27`) — verify no Uint32 index path.

## Critical files

- `platforms/switch/egl_test/source/main.c` (+ `Makefile`) — proven EGL/GLES bring-up + link line.
- `extern/aurora/lib/deko/gpu.cpp` — backend-impersonation pattern to copy into `lib/gles/gpu.cpp`.
- `extern/aurora/lib/gfx/common.cpp` (772-919) — GX IR, buffer pools, replay/begin/end seam.
- `extern/aurora/lib/gx/shader.cpp` (1330-1426, 1680-1728) — WGSL TEV generator to mirror as `shader_glsl.cpp`.
- `extern/aurora/lib/gx/pipeline.cpp`+`.hpp` — canonical per-draw replay + `DrawData`/`PipelineConfig`.
- `extern/aurora/cmake/aurora_core.cmake` + `platforms/switch/build-docker.sh` — backend `#ifdef`/link wiring.

**PREREQUISITE:** the engine must first boot to steady-state gameplay frames AND issue GX draws (`gx>0`) — currently blocked by the CPU bring-up crash cascade (backend-agnostic; see `RESUME_HERE.md`, immediate next = the vibration `m_gamePad[]` null-guard). GLES work begins once draws flow.
