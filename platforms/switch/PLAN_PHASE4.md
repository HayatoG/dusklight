# Phase 4 — GX → deko3d translation via precompiled DKSH shader cache

**Decided 2026-05-23.** deko3d has **no runtime shader compilation** (uam is CLI-only;
no libuam, no glslang/shaderc/tint/SPIR-V runtime compiler in the toolchain; deko3d
loads only prebuilt `.dksh`). So we cannot compile Aurora's runtime-generated shaders
on-device. Chosen strategy: **precompiled DKSH cache keyed by `ShaderConfig` hash.**

This keeps the port 100% public (no private Mesa) and preserves the deko3d work
(Phases 1–3a). It is the documented fallback in `PLAN_DEKO3D.md` risk register.

## Why this is feasible

- `aurora::gx::ShaderConfig` (`extern/aurora/lib/gx/gx.hpp:438`) is a **fixed-size,
  padding-free, hashable POD** (`static_assert(std::has_unique_object_representations_v)`).
  One unique `ShaderConfig` → one shader. The variant space is finite.
- Clean entry point: `build_shader(const ShaderConfig&) -> wgpu::ShaderModule`
  (declared gx.hpp:487). The WGSL generator is `extern/aurora/lib/gx/shader.cpp` (1730
  LOC of TEV→WGSL via `fmt::format`).
- A TP playthrough produces a bounded set of `ShaderConfig`s; capture them, bake to DKSH.

## Architecture

```
BUILD TIME (host / PC):
  1. PC build runs the game; a capture hook dumps every unique (ShaderConfig hash, GLSL)
     pair to disk (the GLSL comes from the new GLSL generator, see step below).
  2. CMake bake step: for each captured shader, uam GLSL -> .dksh, collected into a
     shader-cache blob (romfs or embedded), indexed by ShaderConfig hash.

RUN TIME (Switch / deko3d):
  GX state -> aurora::gx builds ShaderConfig -> hash -> look up prebuilt DKSH in cache
  -> dkShaderInitialize -> bind. Draw commands (gfx command IR) replay into DkCmdBuf.
  Missing hash -> stub-and-warn (render nothing / placeholder), log the hash to capture
  later. Incremental: scenes fill in the cache as they're exercised.
```

## Sub-phases (sequenced)

### 4a — GLSL generator (the long pole)  ← STARTING HERE
Aurora generates **WGSL**; uam needs **GLSL 460**. Add a parallel GLSL emitter for the
same `ShaderConfig` (mirror `gx/shader.cpp`). WGSL↔GLSL differ in: type names
(`vec3f`→`vec3`, `f32`→`float`), entry points (`@vertex`/`@fragment`→`void main`),
bindings (`@group/@binding`→`layout(...)`), texture sampling (`textureSample`→`texture`),
builtins (`gl_Position` exists in both via different decoration). Validate by compiling
generated GLSL with uam (must produce valid DKSH).
- Decision: emit GLSL into a new path (e.g. `shader_glsl.cpp`) OR template the existing
  generator. Leaning toward a separate emitter to avoid destabilizing the PC WGSL path.

### 4b — Config capture (PC side)
Hook shader generation on the PC build to write `{hash -> GLSL}` to a cache dir during
play. Add a "dump mode" env/flag. Produces the corpus to bake.

### 4c — Bake pipeline (CMake)
Custom command: each `{hash}.vert/.frag` GLSL → uam → `{hash}_vsh.dksh`/`_fsh.dksh`.
Pack into a cache (romfs dir `shaders/` or a generated index header). Index by hash.

### 4d — deko render path (replace the gfx wgpu calls)

**Integration shape (discovered 2026-05-23):** the command IR (`CommandType`, `Command`,
`RenderPass`, `g_renderPasses`) is **file-internal/static to `common.cpp`**. A separate
deko module can't see it, so the deko render path = `#ifdef AURORA_BACKEND_DEKO3D`
branches inside the few functions that touch the wgpu pass:
- `gfx/common.cpp`: `begin_frame`, `end_frame`, `render`, `render_pass`, `bind_pipeline`
- `gx/pipeline.cpp`: `render(DrawData, pass)` (the per-GX-draw)  ← 34 LOC, the core unit
- `gfx/clear.cpp`: `render(...)` (the clear draw)
- `gfx/pipeline_cache.cpp`: `get_pipeline` + pipeline build → build deko pipeline (cached
  DKSH + state) instead of wgpu::RenderPipeline.

**CRITICAL — Aurora GX uses VERTEX PULLING, not fixed-function attributes.** `gx::render`
binds an **index buffer + uniform + (storage) buffers** and calls `DrawIndexed` — it
**never binds a vertex buffer**. The vertex shader reads attributes from buffers indexed
by vertex id (`attr_load(config, attr, vidx)` in `gx/shader.cpp`). Consequences:
- The triangle POC's `DkVtxAttribState` path does NOT apply to GX draws. For GX:
  bind storage/uniform/index buffers (DkGpuAddr + offset) + `dkCmdBufDrawIndexed`.
- The generated **GLSL must do vertex pulling**: read attrs from SSBOs
  (`layout(std430, binding=N) readonly buffer`) using `gl_VertexIndex`. (4a must mirror
  this, not declare `in` vertex attributes.)

**`DrawData` fields** (per GX draw): `pipeline` (ShaderConfig/Pipeline hash),
`uniformRange` (offset/size into uniform buffer — bound as dynamic offset),
`bindGroups.textureBindGroup`, `idxRange` (offset/size into index buffer), `indexCount`,
`instanceCount`, `dstAlpha` (→ blend constant).

**Buffer pools — SIMPLER on deko.** wgpu uses a mapped staging buffer + `MapAsync` +
`CopyBufferToBuffer` (begin_frame/end_frame). deko `DkMemBlockFlags_CpuUncached |
GpuCached` memory is **directly CPU-writable AND GPU-readable** → point the `g_verts/
g_uniforms/g_indices/g_storage` ByteBuffers at DkMemBlock CPU addrs; NO staging copy,
NO MapAsync. `push_*`/`map_*` (common.cpp:971-1024) are backend-agnostic ByteBuffer ops.
Buffers needed: uniform (24mb), index (1mb), storage (8mb), texture-upload (off by
default, `UseTextureBuffer=false`). Vertex buffer (3mb) likely unused for GX (pulling).
- `bind_pipeline(ref, pass)` → look up deko pipeline by hash, bind shaders + state.
- `render()` → deko bind swapchain RT + clear (EFB handling later), iterate commands.
- `render_pass()` → SetViewport/SetScissor/Draw dispatch → DkCmdBuf ops.
- Textures: aurora gfx texture cache → `DkImage`/`DkSampler` (bindGroup 2).

**First provable milestone (anti-pattern #4): 4d-lite — geometry with placeholder shader.**
Before the GLSL generator (4a) exists, render ALL GX draws with a single placeholder
shader that vertex-pulls position only (flat color). Un-stub `fapGm_Execute`. Signal:
game geometry silhouettes appear (wrong colors, no textures) — proves the entire
GX→command-IR→deko draw path works on Switch. THEN do 4a/4b/4c for correct shading.

### 4e — un-stub fapGm_Execute on Switch, iterate
Remove the `#ifdef __SWITCH__` skip in `m_Do_main.cpp` (~373, ~412). Run, fill the
shader cache from logged missing hashes, fix GX state-mapping gaps scene by scene.
- **Signal milestones**: (1) no crash with everything stubbed-to-noop; (2) first real
  GX geometry on screen with placeholder shader; (3) title screen with correct shaders.

## Key files
- `extern/aurora/lib/gx/shader.cpp` (1730) — WGSL gen (TEV→shader). Source for 4a.
- `extern/aurora/lib/gx/gx.hpp` — `ShaderConfig` (438), `PipelineConfig`, `build_shader`/
  `build_pipeline` (485-487).
- `extern/aurora/lib/gx/pipeline.cpp` (34) — `create_pipeline` (WGSL→RenderPipeline).
- `extern/aurora/lib/gfx/common.cpp` — buffer pools, command IR, `render_pass` (887),
  `begin_frame`/`end_frame`/`render`. The wgpu-bound layer to mirror for deko.
- `extern/aurora/lib/deko/gpu.cpp` — deko backend (Phases 1–3a). Render path goes here.

## Anti-patterns to respect (see [[dusklight-debugging-heuristics]])
- #4: prove the layer below first. 4d milestone (1) = no-crash noop before real shaders.
- Trace with `dusk_switch_log` (file), not svcOutputDebugString (#9).
- deko cmdbuf-clear invalidates pre-recorded DkCmdLists (#8) — record inline per frame.
