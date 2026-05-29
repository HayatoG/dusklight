# PLAN_NVK.md — Port Mesa NVK (Vulkan) to the Switch GM20B + libnx nv services

**Status:** PLAN (2026-05-25). Goal: a working open-source **Vulkan driver for Switch homebrew**
(HOS/Atmosphère `.nro`), by porting Mesa's **NVK** to the Tegra X1 **GM20B** over **libnx nv
services** — NO deko3d, NO NVIDIA L4T blob, NO dependence on Dan's private `ticohq/switch-nvk-vulkan`.
End state: Aurora → Dawn-Vulkan → our NVK → GM20B, so TP renders at native Vulkan perf
(the result Dan has via his private Mesa fork). See [[vulkan-nvk-switch-path]].

## Why this is bounded (not "write a driver from scratch")
NVK is open Mesa (`src/nouveau/`), already Vulkan-1.4 conformant on discrete Maxwell; GM20B is
Maxwell 2nd-gen (same ISA family — the **NAK** shader compiler already targets it). The GPU is the
same Tegra part libnx's `nv` services already drive. So the port = **a winsys translation layer**
(swap nouveau's Linux-DRM backend for libnx nv) + GM20B device-info + a Mesa cross-build. The
nouveau↔libnx mapping is nearly 1:1 (both wrap the same Tegra kernel GPU iface).

## The interface we reimplement (`src/nouveau/winsys/`) ↔ libnx (`switch/nvidia/`)
| NVK winsys (3 files: device/bo/context) | libnx nv primitive |
|---|---|
| `nouveau_ws_device` { fd, `nv_device_info`, bos } — `_new/_destroy/_timestamp/_vram_used` | `nvidia/gpu.h` (NvGpu); fill `nv_device_info` for GM20B |
| `nouveau_ws_bo` — `_new/_new_mapped/_new_tiled/_map/_unmap/_wait/_destroy`, **`_bind_vma/_unbind_vma`**, `_dma_buf` | `nvidia/map.h` (nvmap alloc) + `nvidia/address_space.h` (GPU VA bind) |
| `nouveau_ws_context` — `_create/_destroy/_killed`, engines | `nvidia/channel.h` + `gpu_channel.h` (GPU channel, pushbuf submit) |
| BO/exec sync (`_wait`) | `nvidia/fence.h` (NvFence) |
| WSI surface/swapchain/present | `services/vi.h` + `nvidia/graphic_buffer.h` (ref: Dan's public `vulkan-triangle-test-switch`) |

`nouveau_ws_device.fd` (a DRM fd) → our libnx nv handle. `nouveau_private.h` is the DRM-specific
guts we replace. NAK/NIL/compiler/`vulkan/` (NVK core) stay upstream-as-is.

## What's already done / de-risked
- **WSI**: PUBLIC in `github.com/dantiicu/vulkan-triangle-test-switch` (VI surface + swapchain → nwindow).
- **Dawn-Vulkan wiring**: our `extern/aurora/cmake/aurora_core.cmake` already sets `DAWN_SWITCH_NVK_ROOT=/opt/nvk-switch` + links `libvulkan.a` (the old `FATAL_ERROR` = "driver not built yet"). `aurora-switch` builds Dawn with `DAWN_ENABLE_VULKAN=ON` on Switch.
- **Shader compiler**: NVK's NAK targets Maxwell already.
- **Fallback**: Dawn-GL ([[dawn-over-gl-already-patched]]) renders Aurora's full pipeline today without any of this — keep as plan B.

## Phased milestones
- **M0 — Mesa/NVK cross-builds for Switch.** meson cross-file → devkitA64 (aarch64-none-elf, newlib). Reconstruct Dan's `create-nvk-image.sh`. Expect to fight: Mesa's Linux/glibc/POSIX assumptions, the DRM/libdrm deps (we cut them), build only `nouveau` Vulkan + NAK. Install prefix `/opt/nvk-switch`. **This is the foundation + likely the fiddliest part.**
- **M1 — winsys over nv + GM20B enum.** Reimplement device/bo/context (3 files) over libnx nv; fill `nv_device_info` for GM20B. Milestone: `vkCreateInstance` + `vkEnumeratePhysicalDevices` returns the GM20B on real HW (no render yet — prove the layer below, anti-pattern #4).
- **M2 — memory + submission.** nvmap BOs + GPU VA bind (address_space) + channel/pushbuf submit (gpu_channel) + NvFence sync. Milestone: a trivial `vkQueueSubmit` completes without fault.
- **M3 — first triangle.** WSI (ref Dan's public TriangleTest) + render pass/pipeline. Milestone: our NVK draws a triangle on the TV.
- **M4 — Aurora integration.** Point `DAWN_SWITCH_NVK_ROOT` at our build, build dusklight Vulkan backend, run TP → game renders via Dawn-Vulkan-NVK.

## PROGRESS (2026-05-25) — path fully de-risked
- **M0 Rust std: DONE.** Rust std builds+links for the Switch. Standalone project at `D:\switch-nvk\`
  (toolchain image `switch-nvk-build`, custom target `aarch64-switch-horizon.json` = freestanding spec
  + `target-family=["unix"]` + `env=newlib`, build-std + RUSTC_BOOTSTRAP + `-Zunstable-options` +
  newlib `-L`). 45 std errors → 0. This is Dan's "std shimmed". (See [[vulkan-nvk-switch-path]].)
- **M1 winsys: precisely spec'd + reference in hand.** NVK's winsys (`src/nouveau/winsys/nouveau_{device,bo,context}.c`, Mesa 26.2 clone at `D:\switch-nvk\mesa`) talks to the kernel via **raw DRM nouveau ioctls** (`drmCommandWrite/Read/WriteRead` + the structs in `drm-uapi/nouveau_drm.h`), NOT the libdrm_nouveau high-level API. So M1 = **a libdrm shim that translates the DRM-nouveau uABI → libnx nv services**, with NVK's 3 winsys .c files running UNMODIFIED on top.
  - **Shim surface (the exact ioctls to handle):** `GETPARAM`, `GET_ZCULL_INFO` → `nvGpuGetCharacteristics`; `NVIF` → NVIF object calls; `VM_INIT` → `nvAddressSpaceCreate`; `VM_BIND`(MAP/UNMAP) → `nvAddressSpaceMap/Unmap`; `GEM_NEW`/`GEM_INFO` → `nvMapCreate`; `GEM_CPU_PREP` → `nvFenceWait`; `GEM_PUSHBUF` → channel+pushbuf; `CHANNEL_ALLOC`/`FREE` → `nvGpuChannel`. Plus `drmCommand*`, `drmGetVersion` (fake nouveau version), `drmPrime*` (dmabuf — stub).
  - **Reference for the nv side of every translation:** `D:\switch-nvk\libdrm_nouveau\source\{nouveau,pushbuf,bufctx,bomap}.c` (devkitPro's working Switch nouveau winsys — `nvInitialize/nvGpuInit/nvGpuGetCharacteristics/nvAddressSpaceCreate/nvMapCreate/nvAddressSpaceMap/nvFenceWait`). NOTE: NVK uses the NEW nouveau uABI (VM_INIT/VM_BIND/EXEC); libdrm_nouveau covers the OLD GEM API — same nv primitives, newer ioctl shapes.
- **Mesa version:** cloned 26.2-devel; per Dan's "breaking changes" warning, may pin to a stable NVK-era 24.x/25.x for M1. NVK winsys interface is stable enough across 24–26.

## Open risks
1. **Mesa cross-build on libnx/newlib** (M0) — Mesa assumes Linux. Biggest unknown.
2. **GM20B vs discrete Maxwell** — integrated/unified memory, GPU class IDs, firmware; patch NVK device init + memory.
3. **nouveau_ws semantics vs libnx nv** — VA binding model, pushbuf/submit format, sync — mostly 1:1 but verify per-call.
4. **Mesa version** — pick a branch where NVK+Maxwell is solid (Mesa ≥ 25.x), but old enough to build with our toolchain.

## Critical files
- Mesa: `src/nouveau/winsys/nouveau_{device,bo,context}.{c,h}`, `nouveau_private.h`, `src/nouveau/vulkan/` (NVK core — keep), `meson.build` + cross file.
- libnx: `$DEVKITPRO/libnx/include/switch/nvidia/{gpu,channel,gpu_channel,address_space,map,fence}.h`, `services/{nv,vi}.h`.
- Ours: `extern/aurora/cmake/aurora_core.cmake` (NVK link, ready), Dan's `vulkan-triangle-test-switch` (WSI ref).
