# Parity with Encounter's reference build — workstream status & plans

**Date:** 2026-06-18 (overnight autonomous session). Goal: make our Switch `.nro`
behave like / be as optimized as the high-perf reference build (Ghidra-analyzed +
its source in `platforms/switch/reference/{aurora-switch,dawn-switch}`).

Constraints this session: **edits only inside this project** (D:\Projects\dusklight);
everything else (incl. D:\switch-nvk) **read-only**; no deletes, no commits; **no Switch
HW** runtime test available — so each item is **build-validated + matched to the
reference**, runtime validation is the user's HW step.

Reference stack (all confirmed): Aurora (GX→WebGPU) → Dawn (`dantiicu/dawn-switch`,
loaderless NVK ICD via `vk_icdGetInstanceProcAddr` + `vkCreateViSurfaceNN`) → NVK
(Mesa 26.2 fork `mesa-nvgpu` + NAK) → winsys `nvkmd_nvgpu` (libnx `/dev/nvhost*`,
GM20B class 0xB197, ZCULL, big-page arena) → Tegra X1.

---

## WS1 — Persistent pipeline/shader cache  ✅ DONE (build-validated)

**Root cause:** Aurora defaults `AURORA_ENABLE_GPU_CACHE=OFF` on Switch → compiled
`pipeline_cache_memory.cpp` (synchronous `cb()` on the render thread, no disk) +
`gpu_cache_null.cpp` (no-op). That is the per-frame shader-compile stutter and a big
chunk of the 5–12 ms "submit". The reference build forces it ON.

**Fix (all in `CMakeLists.txt`, this project):**
- `set(AURORA_ENABLE_GPU_CACHE ON ...)` + `set(AURORA_CACHE_USE_ZSTD ON ...)` before
  `add_subdirectory(extern/aurora)` → selects `pipeline_cache.cpp` (2 worker threads:
  `g_pipelineThread` async compile + `g_pipelineCacheWriterThread` async SQLite write;
  boot-time seed via `load_pipeline_cache`) + `gpu_cache.cpp` (Dawn BlobCache →
  `dawn_cache.db`, zstd).
- 3 devkitA64/newlib build fixes (each verified by reading the source, not guessed):
  1. **zstd target collision** — Aurora's FetchContent builds a `zstd` CLI *executable*
     target that collides with `GAME_LIBS`'s plain `zstd` portlib link. Fix: pre-define
     an IMPORTED `libzstd_static` pointing at portlib `libzstd.a` → Aurora skips the
     fetch ("Using existing zstd").
  2. **`<sys/mman.h>` missing** (sqlite3.c:39299 guard) → `target_compile_definitions(
     sqlite3 PRIVATE SQLITE_OMIT_WAL SQLITE_MAX_MMAP_SIZE=0)`.
  3. **`<dlfcn.h>` missing** (sqlite3.c:46236 guard) → `SQLITE_OMIT_LOAD_EXTENSION`.

**Validation:** full configure+build green; `dusklight.elf` (661 MB) links; symbols
present IN the elf: `g_pipelineThread`, `pipeline_cache_writer`,
`initialize_pipeline_cache`, `load_pipeline_cache_entries` (= async path, NOT the memory
variant), `CREATE TABLE pipeline_cache`, `sqlite3_open/exec`, "Failed to read bundled
pipeline cache" (seed path). `dusklight.nro` produced (43.7 MB).

**Runtime TODO (user, HW):** confirm stutter gone. The .db lands at
`g_config.configPath` (Switch default `sdmc:/aurora`). OPTIONAL warm-start: drop the
reference's `pipeline_cache.db`/`dawn_cache.db` there — but first confirm `config_version`
/ `PipelineCacheSchema` match (else it's ignored, harmless). Otherwise our build
self-populates the cache on first run and persists across launches.

---

## WS2 — "Draw explosion" (F_SP102 / 26k draws → handle_draw_overrun)  ⚠️ HYPOTHESIS DEBUNKED — needs HW verification

A subagent proposed the root cause is `include/dusk/endian.h`: `BE(T)` being a no-op
on Switch (so big-endian STB demo data is read unswapped). **This is WRONG — do NOT
change endian.h.**

Why it's wrong (verified): `endian.h:72` gates the byteswapping `BE<T>` template on
`#ifdef TARGET_PC`. `CMakeLists.txt:363` defines `TARGET_PC` **unconditionally,
including the Switch build** (see the explicit FIXME comment at CMakeLists.txt:348-363:
"TARGET_PC is defined unconditionally and INCLUDES the Switch build … NEVER `#if
!TARGET_PC`"). So on Switch `BE(T) = BE<T>` → **byteswap DOES happen**, same as PC. The
no-op `#else` branch is for a true GameCube-native build only. If `BE()` were globally
broken the game wouldn't boot/render at all (it does — file-select, save, ~28-30 fps
gameplay). The logs the subagent cited are from **2026-02-06** (stale).

**Status:** no confirmed current root cause; no safe static change. WS2 needs a HW run
to even confirm the explosion still happens on the current NVK build (the F_SP102
cutscene specifically). If it does, instrument `command_processor.cpp::handle_draw` /
`handle_draw_overrun` and the `d_demo.cpp` JStudio path on HW — the bug is game-side
control flow, not Aurora's GX reader (which is endian-correct) and not endian.h.

---

## WS3 — Comptags / FB compression (winsys)  📋 PLAN ONLY (switch-nvk is read-only this session)

Lives in `D:\switch-nvk\winsys\drm_shim.c` — a **separate repo**, so per this session's
constraints it was analyzed read-only and the plan is recorded here to apply later.

**Reference mechanism (Ghidra `nvkmd_nvgpu_dev.c`/`va.c`):** at device init reserve a
DEDICATED big-page VA arena (128 KB pages, 4 GB) via `ALLOC_SPACE`, separate from the
4 KB general arena; route compressible render targets to it with a compressed PTE kind;
emit `L2_CLEAN_COMPTAGS`; if the big-page reserve fails, log "FB compression silently
OFF" and continue (non-fatal).

**Our current state (read):** `drm_shim.c` creates ONE arena and binds everything
small-page (4 KB), uncompressed (`vm_bind_op` ~L772-823, `map_kind = op->flags&0xff`).
No comptags.

**Low-risk gated plan (≈30 lines, drm_shim.c only):**
1. Add to `struct shim_device`: `bool comptag_enabled; uint64_t big_page_va_base,
   big_page_va_size;`.
2. In `shim_nv_up` after the small-page `AllocSpace`: `comptag_enabled = getenv("NVK_COMPTAGS")!=NULL;`
   (default OFF). If enabled, `AllocSpace(big_page_size, 0x100000000, big-page align)`;
   on failure log "FB compression silently OFF" and set `comptag_enabled=false`.
3. In `vm_bind_op` before `MapBufferEx`: if `comptag_enabled && is_compressible_kind(
   map_kind) && big_page arena valid` → allocate VA from the big-page arena and pass
   `big_page_size` (else current 4 KB path unchanged).
- `L2_CLEAN_COMPTAGS` is GPU-side (NVK command encoder), **not** winsys scope.
- **Gated by `NVK_COMPTAGS` env (default OFF) → zero change to the working WSI when
  unset; graceful fallback if the arena alloc fails.** Then repackage NVK
  (`package-nvk.sh`) and relink Dusklight.

---

## WS4 — Audio (reference HAS audio; ours is stubbed)  📋 PLAN ONLY (boot-path + HW-unverifiable → not flipped this session)

**Reference does it via SDL3 + the libnx `audren` audio driver** (Ghidra: a function
runs `audrenInitialize` → `audrvCreate` → mempool → sink `"MainAudioOut"` →
`audrenStartAudioRenderer`; plus `SDL_AUDIO_*` + `Z2AudioArcLoader`). So the reference
uses the SAME `DuskAudioSystem` (SDL3 `SDL_OpenAudioDeviceStream` → software DSP
`DspRender`) we already have on PC — but with a **real SDL3 carrying the Switch audren
driver**.

**Why ours is silent (verified):**
1. `CMakeLists.txt:528` excludes `src/dusk/audio/` on Switch (DuskAudioSystem.cpp,
   DuskDsp.cpp, Adpcm.cpp) and adds back only stubs (`DspStub.cpp`,
   `JASCriticalSection.cpp`). Comment: "PC-only sources: SDL3 audio … Functional
   replacements would come from a libnx-native layer." → **no audio output backend.**
2. `include/dusk/audio.h:9` `DUSK_AUDIO_DISABLED=1` on Switch → the whole Z2Audio/
   JAudio2 control layer is skipped (40+ `DUSK_AUDIO_SKIP` sites).
3. If (2) is flipped without (1), boot crashes in `JASDsp::setFXLine` (null FX config
   while loading the `.baa`), per the header comment + `Z2AudioArcLoader::readBFCA`.

**Re-enable plan (do on HW, incrementally — each step build-then-test):**
- **A. Audio output backend.** Either (a) add real SDL3 (with the Switch/audren audio
  driver) to the Switch build and un-exclude `DuskAudioSystem.cpp`+`DuskDsp.cpp`+
  `Adpcm.cpp` (matches the reference), OR (b) write a small libnx-native audren sink that
  consumes `DspRender()`'s stereo F32 output (the software DSP stays; only the SDL sink is
  replaced). (a) is closest to the reference; (b) avoids a big SDL3-on-Switch dependency.
- **B.** Flip `include/dusk/audio.h:9` `DUSK_AUDIO_DISABLED 0` for `__SWITCH__`.
- **C.** Fix the `JASDsp::setFXLine` null path (`libs/JSystem/.../JASDSPInterface.cpp`
  + `Z2AudioArcLoader::readBFCA`): confirm the `.baa` (`/Audiores/Z2Sound.baa`) loads
  from the disc and the FX-line config parses; null-guard `setFXLine` as a fallback.
- **D.** Verify `Z2AudioMgr::init` creates `JAUSectionHeap` (`Z2AudioMgr.cpp:110`) and
  the sound table, so `loadSeWave`/`startSound` stop being null.
- **Risk:** HIGH (boot path). Keep `DUSK_AUDIO_DISABLED` as the master toggle so a bad
  state is one-line revertible. Consider making it a runtime config flag so audio can be
  toggled on HW without rebuilding.

---

## WS5 (present mode / vsync) — INVESTIGATED, NO CHANGE (reference does the same)

Hypothesis: reference's `video.enableVsync:false` → Immediate present → higher fps
(avoids FIFO 60→30 quantization). **Debunked by reading the reference source:**
`platforms/switch/reference/aurora-switch/lib/webgpu/gpu.cpp` `best_present_mode` is
IDENTICAL to ours — `#ifdef __SWITCH__ (void)vsync; return wgpu::PresentMode::Fifo;`.
The reference ALSO runs Fifo on Switch; the config's `enableVsync:false` is a no-op for
present mode there. Changing it would DIVERGE from the reference, not match it. The other
reference config.json keys (`showPipelineCompilation`, `enableAdvancedSettings`) gate
ImGui overlays — and ImGui is OFF on Switch (`AURORA_ENABLE_IMGUI=OFF`) — so they're
no-ops too. No config "free win" exists.

## Aurora source parity — VERIFIED

Our Aurora fork (`extern/aurora` = HayatoG/aurora-switch@bbdc576) was diffed against the
reference (`platforms/switch/reference/aurora-switch` = dantiicu@f0f3511). Result: the
perf-critical files `lib/gfx/pipeline_cache.cpp` and `lib/gx/command_processor.cpp` are
**identical**. The only files that differ — `gpu.cpp`, `window_switch.cpp`, `aurora.cpp`
— differ **only by additions WE made** (debug `dusk_switch_log`, frame profiling
`bf_iter`/`ef_iter`, libnx→SDL gamepad translation for the launcher). Nothing the
reference has that we lack. **Conclusion: our render/cache code is at parity with the
reference; the gap was purely the WS1 build flag (now fixed) + WS3/WS4.**

## Morning summary
- ✅ **WS1 shipped & validated** (cache async + disk, the biggest "feel" win) — needs your
  HW run to confirm stutter gone.
- ⚠️ **WS2**: subagent's endian.h theory **debunked** (don't touch it); real cause unknown,
  needs HW to confirm it's even still happening.
- 📋 **WS3**: gated comptag plan ready to apply to switch-nvk (read-only this session).
- 📋 **WS4**: exact audio re-enable plan (SDL3+audren / libnx audren sink); not flipped
  because it's boot-path + unverifiable without HW.
- The tree builds green; only `CMakeLists.txt` changed (WS1). Nothing deleted/committed.
