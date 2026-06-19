# PLAN — Fix in-game resolution/aspect change crash (WSI swapchain recreate)

**Status:** ✅ DONE — Option A IMPLEMENTED + HW-VALIDATED ("Funcionou perfeito!"). Created 2026-06-18.
Fix landed in switch-nvk `6c58d9a` (HayatoG/switch-nvk master) — `g_zc_owner` owner-transfer in
`mesa-25/src/vulkan/wsi/wsi_common_switch.c` (regenerated into `patches/switch-nvk-mesa-25.0.7.patch`).
dusklight relinked against the new `libvulkan.a`. HW: every resolution/aspect recreate now
`nwindowConfigureBuffer -> 0x0`, zero-copy re-enabled, no `0xf59`, no crash — launcher AND in-game.
The plan below is kept for history.
**Symptom (HW-confirmed):** toggling `video.lockAspectRatio` (4:3) IN-GAME crashes; the saved
setting then crash-loops on boot. Log:
```
[wsi-zc] img0 nwindowConfigureBuffer -> 0xf59   (was 0x0 at first boot)
[wsi-zc] zero-copy FAILED -> CPU-copy fallback
exiting ...
```
Boot was un-stuck by FTP-resetting `video.lockAspectRatio: false` in
`sdmc:/TwilitRealm/Dusklight/config.json`. Works in the launcher (aspect changed before the
zero-copy swapchain is active), crashes in-game.

## CONFIRMED (2026-06-18, HW capture nxlink_rescrash.log): RESOLUTION change hits the same bug
Changing `game.internalResolutionScale` in the LAUNCHER menu crashes too (graphics_tuner.cpp:96-97
applies `VISetFrameBufferScale` LIVE on every selector step → swapchain recreate). Log proves it:
boot configures OK (`nwindowConfigureBuffer -> 0x0`, zero-copy ENABLED), then on the resolution
change `nwindowConfigureBuffer -> 0xf59` → zero-copy FAILED → exiting. So this bug is NOT just the
in-game 4:3 edge case — it breaks any extent change (resolution + aspect), launcher or in-game.
Option A (WSI recreate handling) fixes all of them at once.

## Root cause
`wsi_switch_surface_create_swapchain` (in **switch-nvk**:
`mesa-25/src/vulkan/wsi/wsi_common_switch.c`, ~L469-568, compiled into `libvulkan.a` linked by
dusklight) **ignores `pCreateInfo->oldSwapchain`**. Vulkan recreate order: create NEW swapchain
(with `oldSwapchain` = current) → THEN destroy old. So the NEW swapchain runs
`nwindowConfigureBuffer` (L537) on the surface's single shared `NWindow` while the OLD swapchain's
buffers are still registered (released only in `wsi_switch_swapchain_destroy` → `nwindowReleaseBuffers`,
L457, which runs later) → `nwindowConfigureBuffer` returns `0xf59` (buffers already registered) →
`zero_copy=false` → fallback/crash.

## Option A — fix the WSI (RECOMMENDED; fixes ALL runtime resolution changes)
Track the nwindow's owning swapchain (one NWindow per surface on Switch → a single static pointer is
enough) and transfer ownership on recreate:
```c
/* file scope */
static struct wsi_switch_swapchain *g_zc_owner = NULL;

/* in wsi_switch_surface_create_swapchain, BEFORE the configure loop (~L530), after
   nwindowSetDimensions(L528) which already picks up the new extent: */
if (g_zc_owner != NULL && g_zc_owner != chain) {
    nwindowReleaseBuffers(chain->window);  /* release previous owner's buffers */
    g_zc_owner->zero_copy = false;         /* so its destroy won't double-release (would kill ours) */
    g_zc_owner = NULL;
}
/* ... configure loop ... */
if (chain->zero_copy) g_zc_owner = chain;

/* in wsi_switch_swapchain_destroy (L456): only release if still the owner */
if (chain->zero_copy && g_zc_owner == chain) {
    nwindowReleaseBuffers(chain->window);
    g_zc_owner = NULL;
}
```
Why it works: the new swapchain steals the nwindow (release old → configure new) in the right order
→ no `0xf59`; the old swapchain's later destroy sees it's no longer the owner → skips the release →
no double-release of the new buffers.
**Risks:** brief overlap if the old swapchain's presents are still in flight at recreate — Vulkan
should have drained them before destroy; verify no tearing/glitch. Confirm `0xf59` really means
"already configured" (decode the Result; adjust if it's something else like buffer-in-use).
**Build chain (the heavy part):**
1. Edit `wsi_common_switch.c` in switch-nvk (also the `winsys/wsi/` copy if that's the built one).
2. Rebuild Mesa → `libvulkan.a` (meson/ninja in switch-nvk — slow).
3. Repackage into dusklight's NVK install (`package-nvk.sh` / copy to `DAWN_SWITCH_NVK_ROOT/lib`).
4. Relink dusklight (`build-docker.sh build`) → make-nro → HW test: change aspect in-game → swapchain
   recreates → no crash; toggle back and forth.

## Option B — avoid the recreate (LIGHTER; dusklight/aurora-side, no Mesa rebuild)
Apply 4:3 as a **letterbox/viewport in the present blit** (black bars, keep the swapchain at native
1280×720) instead of changing the swapchain extent. No extent change → no recreate → WSI bug never
triggered. Narrower (only covers aspect, not arbitrary resolution changes) but no Mesa rebuild.
Investigate where aurora/dusk turns `lockAspectRatio` into a swapchain resize vs a viewport
(`mDoGph_gInf_c::updateRenderSize` / aurora present viewport) and make it a viewport-only change.

## Recommendation
A is the correct fix (the bug is genuinely in the WSI recreate path and it covers all runtime
resolution changes). B is the quick win if we just want 4:3 in-game without touching Mesa.
Decide at implementation time. Until then: set aspect in the launcher, not in-game (and the
crash-looped config is recoverable by resetting `video.lockAspectRatio:false` over FTP).
