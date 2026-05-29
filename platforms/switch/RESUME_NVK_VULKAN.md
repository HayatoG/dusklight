# RESUME — Dusklight Switch port via our NVK (Vulkan) — M-DV-2

**Branch `switch-port/nvk-vulkan`. 2026-05-27. Read the memory `dusklight-nvk-vulkan-m2-state` first.**

## STATE: Dusklight BUILDS + BOOTS with our NVK as Dawn's Vulkan backend — but does NOT render the game.

### ✅ Working
- Clean branch `switch-port/nvk-vulkan` (off deko3d-backend = main + boot infra; deko/GLES not selected).
- Our NVK (`D:\switch-nvk\nvk-switch\lib\libvulkan.a`, built by `D:\switch-nvk\package-nvk.sh`) wired into
  Aurora's Vulkan path. The full Dusklight compiles + links + boots on real Tegra (IP 192.168.1.11).
- On HW: window -> wgpu **Vulkan** adapter+device (`RequestAdapter ok`) -> gfx -> disc opened -> engine
  init -> main loop -> `fapGm_Execute`. **NVK presents CLEARS** (TV shows gray then black).
- B fix (timeline syncobj, in `D:\switch-nvk\winsys\drm_shim.c`: drmGetCap timeline cap + EXEC sig sets
  s->value + drmSyncobjQuery fence-aware) FIXED the `nvk_upload_queue_reserve` null crash (9 -> 26000 draws).

### ❌ The blocker (the real remaining work) = the Aurora GX->wgpu REPLAY, NOT NVK
`extern/aurora/lib/gx/command_processor.cpp`. The engine renders the 2D title (frames 1-18, GX cmd=0x80)
then enters 3D stage **F_SP102** (frame 19, windowNum=1, cmd=0x98) and emits **26000 draws in ONE frame**
(anomalous) then **aborts (User Break)**. **NO game geometry is ever visible** (gray/black = only clears;
the title logo never appears). Crash is almost certainly **`handle_draw_overrun`** (`command_processor.cpp`
:1496 / :1576 -- `[[noreturn]] FATAL` "draw vertex data overrun") OR a CHECK in `handle_draw`: a
**mis-parsed vertex format** (vtxCount*vtxSize overruns the fifo). NOT a C++ exception (set_terminate never
fired), NOT a Mesa log (mesa_nvk.log empty). Same unsolved GX-replay problem as the deko/GLES era.

## BUILD/RUN
- `DUSK_BUILD_DIR=/d/dusklight-build-nvk bash platforms/switch/build-docker.sh build-only`
- `.elf` is 717MB (debug); `strip` OOMs the container -> `bash platforms/switch/make-nro.sh` runs
  `elf2nro` on the unstripped `.elf` directly (PT_LOAD only) -> 36MB `dusklight.nro`. Bump nacp version.
- `curl -T /d/dusklight-build-nvk/dusklight.nro ftp://192.168.1.11:5000/sdmc:/switch/`
- Read after: `sdmc:/dusklight.log`, `sdmc:/dusk_stderr.log` (Aurora FATAL/asserts), `sdmc:/mesa_nvk.log`,
  `sdmc:/atmosphere/crash_reports/<newest>`. OBS mirrors the TV (capture PC screen -> PNG -> Read).

## NEXT (immediate)
1. The v0.6 diag build (`0.6.0-nvk-abort`: stderr->dusk_stderr.log + --wrap=abort + [NVK-BUILD] stamp with
   `dusk_switch_log@`) is BUILT (in /d/dusklight-build-nvk) but its `.nro` was NOT produced/uploaded (docker
   was killed). -> `make-nro.sh` + FTP + run -> dusk_stderr.log gets the FATAL overrun message (file:line)
   and the abort caller RA (addr2line via base = stamp_addr - nm(dusk_switch_log) on dusklight.elf).
2. **Find where Aurora `FATAL`/`Log.error` routes on Switch** (so the overrun message is captured) if the
   stderr redirect doesn't catch it.
3. **Fix the gx-replay vertex parsing** (`command_processor.cpp` calculate_last_vtx_size / read_u16 bigEndian
   / vtxDesc-vtxFmt state): stop the overrun + produce VISIBLE geometry (correct pipeline + swapchain target).
   This is the core remaining work. Crashes are gx-replay, not NVK.

## Crash-type cheat: "User Break" reports do NOT symbolize the NRO (only hbl); Instruction/Data Abort
reports DO ("dusklight + 0x..."). For User Break, use the [NVK-BUILD] stamp + --wrap=abort to addr2line.
