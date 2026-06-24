# Switch performance backlog (NOT issues yet)

Two big performance levers for the Dusklight Switch port, parked here intentionally — **do not open
GitHub issues for these yet** (user request, 2026-06-24). They are the largest remaining FPS gains and
deserve their own focused effort once the community-feedback batch is shipped.

## 1. Triple-buffer WSI (minImageCount 2 → 3) + frame interpolation → real 60 fps

**Problem.** In-game runs at TP's native ~30 fps; frame interpolation is enabled
(`game.enableFrameInterpolation`) but produces no extra frames on Switch. HW work showed the engine is
**GPU/fill-rate bound** in-game, and the current double-buffered FIFO swapchain *masks* the variable
GPU work — the loop effectively caps at the present cadence instead of letting interpolated frames
land.

**Fix / recipe (HW-reasoned, from the perf roadmap).** For smooth 60:
1. **WSI `minImageCount` 2 → 3** (triple buffer) in our NVK WSI — *unmasks* the variable work so
   interpolated frames can actually be presented.
2. **Frame interpolation ON** (already wired) → renders interpolated frames at 60 while the sim stays
   30 Hz.
3. **Resolution inside the GPU budget** (Maxwell GM20B is bandwidth-bound; lower internal res / the
   optimized preset helps).

**Where.** `D:\switch-nvk` Mesa WSI: `mesa-25/src/vulkan/wsi/wsi_common_switch.c` (the zero-copy
nwindow swapchain). A 3-buffer prototype branch exists (gitignored mesa-25 tree per the perf-roadmap
memory) — revive/validate it. Rebuild NVK → `package-nvk.sh` → relink dusklight.

**Risk.** More nwindow buffers = more VRAM + the zero-copy `g_zc_owner` ownership logic must handle 3
slots cleanly (see the WSI swapchain-recreate owner-transfer fix). Validate present pacing on HW.

## 2. DRS — Dynamic Resolution Scaling

**Why.** The right tool to *hold* 60 fps: drop internal resolution under load and raise it when there's
headroom, instead of a fixed res that either tanks fps in heavy scenes or wastes it in light ones.
The GPU is fill-rate bound, so resolution is the most effective knob. Marked in the perf roadmap as
"the right tool / next major feature."

**Where.** Aurora render + present path (internal render target sizing) + a controller that watches
frame time and steps the scale. Ties into the existing `game.internalResolutionScale`
(0=Auto/1=360p…8=1080p, `src/dusk/ui/settings.cpp` graphics_tuner) — DRS would drive that value (or an
internal equivalent) dynamically. Needs a frame-time signal (the `[wsi-prof]` present profiler already
measures interval) and hysteresis to avoid res thrashing.

**Risk.** Resolution changes recreate the swapchain (the `0xf59` collision we fixed with owner
transfer) — DRS must reuse that path safely and not thrash it every frame; step on a cooldown.

---

When ready to act, promote whichever one to a GitHub issue then. Until then this file is the record.
