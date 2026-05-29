# Plan: deko3d Native Backend for Switch

**Branch:** `switch-port/deko3d-backend` (off `main`, clean of opengl-core experiments)
**Strategy:** Skip Dawn entirely. Implement `aurora::deko::*` as a direct GX → deko3d translator. Same Aurora client API, different backend.

---

## Why deko3d (vs Dawn-on-Mesa-GLES which we just left)

Three failed builds of Dawn-on-GLES on Switch revealed a pattern: every Dawn-internal layer assumes Mesa 22+ behavior. Mesa-on-Switch is 20.1-rc3 (2020). Each patch unblocks one validation but reveals another. Diminishing returns.

deko3d is:
- **100% public** (devkitPro/libnx-bundled `libdeko3d.a`, MIT, no Mesa dependency)
- **No version mismatch** — written specifically for Tegra X1
- **Less CPU overhead** — direct command stream to GPU, no userspace driver
- **What Atmosphère's menus + several Switch homebrews use** — proven runtime
- **Documented**: `deko3d/Primer.md` + `switch-examples/graphics/deko3d/`

We lose:
- Dawn's WebGPU abstraction (don't need it for Switch-only target)
- Tint's WGSL→shader compilation (use uam: `SPIR-V → DKSH`)
- Code shared with PC/Android Aurora backends (already platform-specific anyway)

---

## Architecture

```
PC build (unchanged):
  Dusklight game code
    → JSystem GX calls
    → aurora::gfx (1991 LOC, platform-agnostic, KEEP)
    → aurora::gx (KEEP)
    → aurora::webgpu (~750 LOC, PC-only)
    → Dawn → WebGPU → D3D12/Vulkan/Metal

Switch build (new path):
  Dusklight game code
    → JSystem GX calls
    → aurora::gfx (1991 LOC, KEEP — minor #ifdef for backend type)
    → aurora::gx (KEEP)
    → aurora::deko (NEW, ~3-4k LOC) ─── PARALLEL to aurora::webgpu
    → libdeko3d.a → Tegra X1 NVN
```

The `aurora::gfx::*` translation layer (1991 LOC) **stays unchanged**. It already abstracts "build a render pipeline from BP/CP/XF state" — we just need to provide a different concrete backend for the abstract operations.

## What gets replaced

**Replaced (~3-4k LOC of new code):**
- `extern/aurora/lib/webgpu/gpu.cpp` (~750 LOC) → `extern/aurora/lib/deko/gpu.cpp` (instance, swapchain, frame loop)
- `extern/aurora/lib/dawn/*` Aurora's Dawn glue → `extern/aurora/lib/deko/*` (binding, shader compile via uam)
- WGSL → SPIR-V pipeline using Tint (kept) → SPIR-V → DKSH via `uam`

**Kept untouched:**
- `extern/aurora/lib/gfx/*` (~2k LOC GX→pipeline IR)
- `extern/aurora/lib/gx/*` (GX state machine, vertex format translation)
- All Dusklight client code
- JSystem entirely
- All Switch scaffolding (libnx applet config, file logging, NWindow, NACP/elf2nro)

## Implementation phases

### Phase 0 — Build skeleton (1 day)
- [ ] CMakeLists: detect `AURORA_PLATFORM_SWITCH=ON` → build `aurora::deko` instead of `aurora::webgpu`
- [ ] Add `-DAURORA_BACKEND_DEKO3D` flag
- [ ] Stub `aurora::deko::initialize/shutdown/begin_frame/end_frame` returning success
- [ ] Drop entire `extern/aurora/lib/dawn/` and `extern/aurora/lib/webgpu/` from Switch build
- [ ] Drop Dawn submodule build entirely (massive build speedup)
- [ ] **Signal**: `.nro` builds, boots in Eden, reaches game_main, hits stub render frame ~60 fps with no crashes

### Phase 1 — Clear-color (2-3 days)
- [ ] `dkDeviceCreate` + dispatch queue
- [ ] `dkMemBlockCreate` for swapchain images (3-buffer)
- [ ] `dkSwapchainCreate` from `nwindowGetDefault()`
- [ ] Per-frame: `dkQueueAcquireImage` → cmd buf record clear → `dkQueueSubmitCommands` → `dkQueuePresentImage`
- [ ] Cycle clear colors per frame (sanity check)
- [ ] **Signal**: Eden shows colored gradient. Same look as our `egl_test.nro` did, but via deko3d not EGL/Mesa.

### Phase 2 — Shader pipeline (5-7 days)
- [ ] Set up uam toolchain in build (`uam` is in devkitPro tools)
- [ ] Pre-bake a trivial vertex+fragment shader to `.dksh` at build time (CMake custom command)
- [ ] Load `.dksh` at runtime via `dkshFromMemBlock`
- [ ] Render a triangle with the precompiled shader
- [ ] **Signal**: Eden shows a triangle

### Phase 3 — Tint integration (4-7 days)
- [ ] Use Tint at runtime to compile WGSL → SPIR-V (already linked, no new deps)
- [ ] Pipe SPIR-V → uam at runtime (uam has runtime API? else: bake at build, list of known shaders)
- [ ] First Aurora-generated GX shader actually runs
- [ ] **Signal**: One TEV-stage cube rendering on Eden

### Phase 4 — Full GX state translation (10-14 days)
- [ ] BP register block (blend mode, depth, alpha test, fog) → `DkRasterizerState`/`DkBlendState`/`DkDepthStencilState`
- [ ] CP register block (vertex desc) → `DkVtxAttribState`/`DkVtxBufferState`
- [ ] XF registers (texture matrices, projection) → uniform buffer constants
- [ ] Texture upload: aurora gfx's texture cache → `DkImage`/`DkSampler`
- [ ] Buffer management: vertex/index buffers via `DkMemBlock`
- [ ] **Signal**: Dusklight title screen visible in Eden

### Phase 5 — Polish (TBD)
- Audio (separate concern, not blocking visual)
- Input remapping (already mostly working via aurora-switch's input_switch.cpp)
- Save persistence (sdmc:/ paths)
- 60fps stability

## Risk register

| Risk | Probability | Mitigation |
|---|---|---|
| Tint can't be linked statically without Dawn | Low | Tint is standalone in `dawn-switch/src/tint/`; we can link `tint_api` directly per Dan's `testNoDawn` recipe |
| uam runtime compilation isn't supported | Medium | Fallback: bake all GX shader variants at build time. Aurora has finite pipeline state space — possibly precompilable |
| GX → deko3d state mapping has unimplemented features | Medium-High | Hit incrementally during Phase 4. Stub-and-warn each missing op, fix as scenes hit them |
| Memory layout: deko3d uses explicit `DkMemBlock` pools, aurora gfx assumes WebGPU's implicit allocations | Medium | Wrap pool allocation in `aurora::deko::buffer_pool` similar to Vulkan's VMA |
| TEV pixel shader complexity exceeds deko3d/Tegra fragment shader capability | Low | Tegra X1 supports SM 5.3, plenty for GX TEV. Each TEV stage is ~5 instructions in SPIR-V |
| Time estimate slippage | High | Stages 4-5 historically 2-3x estimate on similar ports |

## Knowledge dependencies (load before starting each phase)

- **Phase 0-1**: `deko3d/Primer.md`, `switch-examples/graphics/deko3d/deko_basic/`
- **Phase 2-3**: `dkshFromMemBlock` docs, uam man page, Tint API in `dawn-switch/src/tint/api/`
- **Phase 4**: YAGCD chap 5 (BP/CP/XF registers), aurora's existing `gfx/common.cpp` (the 1991-line file)
- **Throughout**: anti-patterns from `dusklight_debugging_heuristics.md`

## Build invocation (target)

```bash
# Same wrapper, different cmake flags
bash platforms/switch/build-docker.sh build

# Inside build-docker.sh, the cmake call becomes:
cmake -S /dusklight -B /dusklight/build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DAURORA_PLATFORM_SWITCH=ON \
  -DAURORA_BACKEND_DEKO3D=ON   # NEW
  -DAURORA_ENABLE_RMLUI=OFF \
  -DAURORA_ENABLE_IMGUI=OFF \
  -DDUSK_MOVIE_SUPPORT=OFF \
  -DDUSK_ENABLE_UPDATE_CHECKER=OFF \
  -DDUSK_ENABLE_DISCORD=OFF
# NO DAWN_ENABLE_* flags — Dawn entirely off-path on Switch
# NO AURORA_DAWN_PROVIDER=source — Dawn not built at all
```

## Estimate summary

| Phase | Optimistic | Realistic | Pessimistic |
|---|---|---|---|
| 0. Skeleton | 1 day | 2 days | 3 days |
| 1. Clear color | 2 days | 3 days | 5 days |
| 2. Shader pipeline | 5 days | 7 days | 10 days |
| 3. Tint integration | 4 days | 7 days | 14 days |
| 4. GX state translation | 10 days | 14 days | 21 days |
| 5. Polish | 5 days | 10 days | 20 days |
| **TOTAL** | **27 days** | **43 days** | **73 days** |

**Realistic: ~6 weeks to playable Dusklight in Eden.** This is the path. Less detective work than Dawn-on-Mesa-Switch (which was 4-6 layers of stacked unknowns).

## What does NOT change from current main

- All Dusklight client code (`src/`, `include/`)
- All JSystem code (`libs/JSystem/`)
- All Switch scaffolding outside aurora: `platforms/switch/{Makefile,icon.jpg,Dusklight.nacp,src/switch_stubs.cpp,build-docker.sh}`
- Test harness: `platforms/switch/egl_test/` (kept as switch-mesa-still-works baseline)
- All memory files + skill + dossier

## How we'll know it's the right move

We declare success when:
1. `dusklight.nro` builds without Dawn (build time should drop dramatically)
2. Eden boots it and shows ANY pixel from our render code
3. Phase 4 hits sustained 60fps with a non-trivial GX scene

We declare it WRONG (and retreat to Dawn-on-Vulkan/private-Mesa) when:
1. uam can't handle our shader complexity
2. Aurora's gfx API doesn't actually abstract cleanly from WebGPU semantics
3. Phase 1 takes >1 week (signals deeper structural issues)
