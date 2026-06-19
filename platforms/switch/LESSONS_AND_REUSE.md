# What We Learned Porting Dusklight to Switch — Reusable Knowledge

> A standalone distillation of everything the Dusklight Switch port + the `switch-nvk`
> driver effort taught us, organized by **how transferable it is to other projects**.
> Written 2026-06-18 (project at 100% — shipped). Companion: `DIARIO.md` (the full saga),
> `RESUME_REPORT_V141.md` (current state), the `dusklight-switch-port` skill.
>
> The point of this doc: the hardest parts of this project are NOT specific to Twilight
> Princess. We built a general-purpose open-source **Vulkan driver for Switch homebrew**,
> a **libnx audio backend**, and a pile of **cross-compile / real-hardware lessons** that
> apply to almost any native Switch port or homebrew app. This catalogs what's reusable
> and how.

---

## TL;DR — the three reusable assets

1. **`switch-nvk`** — a working **Mesa NVK 25.0.7 Vulkan driver** cross-compiled for
   `aarch64-none-elf` (devkitA64), with **runtime shader compilation** (NAK), a custom
   **`VK_NN_vi_surface` WSI** over libnx `nwindow` (zero-copy block-linear scanout), and a
   **`drm_shim`** that maps libdrm → libnx ioctls. This is the crown jewel: a real Vulkan
   1.x driver any Switch homebrew can link, not tied to Twilight Princess in any way.
2. **`switch_audio.cpp`** — a **libnx `audren` audio backend** behind a tiny SDL-audio surface.
   Drop-in for any project that produces F32/PCM frames.
3. **A real-hardware porting playbook** — the cross-compile, deploy, crash-triage, and
   "emulator-lies" lessons that every native Switch port re-discovers the hard way.

---

## Tier A — Universal Switch homebrew (any native port or app)

These apply to *anything* you cross-compile for Switch with devkitPro/libnx.

- **Deploy + live logs:** `nxlink -s -r N -a <ip> app.nro`. On Windows, **Defender silently
  drops the netload UDP reply** — you MUST add a Windows Firewall **inbound** rule for
  `nxlink.exe` (UDP 28280) or you get "Connection failed" despite ping working.
- **Crash triage on real HW:** Atmosphère writes `atmosphere/crash_reports/<ts>.log`; pull it
  (Sphaira FTP `:5000`) and symbolize: `aarch64-none-elf-addr2line -ifCe app.elf 0xOFFSET`
  (use `-i` for inline chains; symbolize against the SAME unstripped elf the `.nro` came from).
- **Emulators lie.** Eden/Yuzu/Ryujinx tolerate null derefs, uninitialized state, and TLS
  layouts that **real Tegra faults on**. Treat emulator success as "compiles + roughly runs,"
  never as validation. Verify on hardware.
- **TLS must be `tls_model("initial-exec")`.** libnx has no `__tls_get_addr`/DTV, so
  global-dynamic thread-locals fault at 0x0. Any `thread_local` / `__thread` in a ported
  codebase needs `__attribute__((tls_model("initial-exec")))`.
- **Applet config matters:** set `__nx_applet_type = AppletType_Application` and
  `__nx_heap_size = 0` (let libnx size it). Without these you launch as a LibraryApplet with a
  ~512MB heap cap → OOM on anything real.
- **`svcOutputDebugString` drops lines under load.** For crash-safe traces, write to a file
  sink (e.g. `sdmc:/app.log`). And **per-line flushing is a perf killer** (see Tier C).
- **Decode every libnx `Result` before theorizing.** `MAKERESULT(module, desc)`. We burned a
  session because `0xd5c` "looked like" InsufficientMemory but is `MAKERESULT(348,6)` =
  NVIDIA Timeout (channel reset). One wrong decode → a week of wrong theories.
- **Unit traps in libnx APIs.** `nvFenceWait` takes **microseconds**, not nanoseconds. Read
  the header for units on every timeout/duration call.
- **libnx FsFs is not POSIX.** `std::filesystem::rename(tmp, dst)` does NOT overwrite an
  existing `dst` (returns "File exists"). Remove-then-rename. SQLite over FsFs needs a
  no-locking VFS (unix-none) + `journal_mode=MEMORY` (no `fcntl` locking → otherwise IOERR).
- **newlib `<ctype.h>` macro pollution.** It `#define`s `_U _L _N _S _P _X _B _C`; any of those
  as a member/field/variable name fails to compile on `aarch64-none-elf`. Grep before porting.
- **Verify which binary actually ran.** Docker bind-mount mtimes are unreliable → Ninja
  silently skips edits; `cmake --build | tee` masks `FAILED`. Confirm the `.o` recompiled, the
  ELF relinked, and the **`.nro` mtime is newer than the `.elf`** (a separate `elf2nro` step).

## Tier B — Switch graphics & audio (the `switch-nvk` driver — the big reusable win)

The headline result: **you can run real Vulkan on Switch homebrew with on-device shader
compilation.** Most Switch homebrew uses `deko3d` (no runtime shader compile — needs offline
`uam` + a precompiled DKSH cache) or GL via `switch-mesa`. We proved a third path.

- **Mesa NVK cross-compiles for Switch.** Mesa 25.0.7 NVK + NAK builds for
  `aarch64-none-elf`. Caveats that bite: cross `-Dmesa-clc=enabled` wrongly builds LLVM *for*
  the target → use a 2-phase native-tools build; NAK (Rust) needs nightly + `-Zbuild-std` for
  the tier-3 `aarch64-nintendo-switch-freestanding` target; meson needs `rustfmt` or it errors.
- **`drm_shim`** replaces libdrm with libnx ioctls (`nvGpuChannel*`, `nvMap*`, `nvFence*`).
  This is the libnx↔Mesa bridge and is the most reusable single file for anyone porting any
  Mesa driver (not just NVK) to Switch.
- **The FECS wall (homebrew can't write privileged GR regs).** NVK's 3D init writes priv
  registers via the FECS falcon (`SET_FALCON04` / `nvk_mme_set_priv_reg`); Horizon blocks that
  → MMU fault notifier type 31 → channel reset. **No-op those writes** — they're non-essential
  robustness tweaks (Mesa 20's `nvc0_magic_3d_init` proves GM20B works with zero FECS writes).
  *General lesson for any GPU-driver-on-Switch effort: the OS sandboxes privileged GPU regs.*
- **Fence completion + coherency need the builtin FENCE CMDLIST appended** after the syncpt
  incr (mirrors libdrm_nouveau `pushbuf.c:226`); its `syncpt|(1<<20)|(1<<16)` dword does the
  syncpt-incr + GPU L2 flush that makes the CPU see GPU writes.
- **Depth/tiling on Horizon:** Mesa sets `drm_format_mod=INVALID` only `#if LINUX||BSD`; a
  Horizon port falls through to MOD_LINEAR → the GPU faults clearing tiled Z. Add
  `DETECT_OS_HORIZON` to that guard and carry the PTE kind through `vm_bind` (`flags & 0xff`).
- **WSI from scratch (`VK_NN_vi_surface`).** Mesa has no `VK_NN_vi_surface` impl — only the
  registry entry. We wrote `wsi_common_switch.c`: acquire = `nwindowDequeueBuffer`→slot→image;
  present = pull the native `NvMultiFence` from the `VkFence` → `nwindowQueueBuffer`. **Reusable
  pattern for any Vulkan-on-Switch WSI.**
- **Zero-copy present = render straight into the compositor buffer** (block-linear, `kind=0xfe`).
  Cut present from 9.6ms → 153µs (64×). Gotchas: `NvGraphicBuffer.header.num_ints` MUST be set
  or dequeue hangs forever on a marshalled-empty buffer; `framebufferMakeLinear` is an illusion
  (it swizzles hidden — the compositor only accepts block-linear).
- **`switch_audio.cpp` — libnx `audren` backend.** `audrenInitialize`→`audrvCreate`→
  `audrvDeviceSinkAdd("MainAudioOut")`→`audrvVoiceInit`→`audrenStartAudioRenderer`; a worker
  thread fills wavebufs (`armDCacheFlush` before queue). **audren rejects `PcmFormat_Float` —
  only `Int16` accepted** → convert F32→s16. Drop-in for any project with PCM output.

## Tier C — Decomp / engine porting (GC/Wii decomp, or any large C/C++ port)

- **Platform-macro polarity is the #1 crash source.** In this codebase `TARGET_PC` is defined
  *on Switch too* (it just means "not the original GameCube binary"). PC-launcher-lifecycle code
  guarded by `#if TARGET_PC` gets inherited by Switch unchanged. The general rule: **audit every
  platform macro's truth table on your new target**, and prefer the toolchain's own macro
  (`__SWITCH__`) for true platform carve-outs. Companion trap: macros that are *false* on your
  target (`PLATFORM_WII||PLATFORM_SHIELD`) leave dispatch tables with OOB function pointers.
- **GC OS-primitive emulation is subtle.** A reimplemented `OSWaitCond` that unlocks the mutex
  *before* `cv.wait()` has a **lost-wakeup** race (signal lands on an empty door → worker sleeps
  forever). Hold one lock level atomically into the wait. *Any* ported condvar/mutex emulation
  is a prime suspect when a posted worker op intermittently never runs.
- **SQLite caches on Switch** need the FsFs treatment (unix-none VFS + MEMORY journal); a
  pipeline/shader cache is the single biggest stutter-killer for a GPU-heavy port.
- **Deep recursion overflows libnx threads.** Tint's `AnalyzeUniformity` (and similar compiler
  passes) overflow the small default `std::thread` stack → run them on an 8MB **pthread**.
- **A GX→WebGPU layer (Aurora) is reusable across GC/Wii decomps.** The whole graphics
  abstraction (GX/TEV → WebGPU → our Vulkan/NVK) is engine-agnostic; another GC/Wii port could
  reuse the entire Aurora→Dawn→NVK chain.

## Tier D — Methodology (the meta-lessons that saved the project)

- **Measure before optimizing.** Profiling the present revealed it was **98% CPU memcpy**, not
  the "weak GPU" everyone assumed (GPU was 20µs). That one log redirected months of guessing.
- **Verify by the artifact (the TV), not the logs.** "`gx=1`, draw captured" was true while the
  screen was blank — the replay was log-only. Pixels are the only proof.
- **Build a POC of the layer below before instrumenting the layer above.** A 100-line
  `egl_test.nro` / `nvk_tri.c` settles "does the layer under me even work" in an hour.
- **Read the reference binary.** Ghidra on the working binary proved our backend was
  byte-equivalent to it → the bug was upstream in the engine, not the renderer. Don't rewrite a
  layer that a decompile shows is identical to a known-good one.
- **Heartbeat probes, not snapshot probes.** On-change logging is blind precisely in the stuck
  state; per-frame heartbeats reveal oscillation/livelock.
- **The instrumentation can be the bug.** Per-frame double-flush logging *was* the stutter.
  Measuring costs; budget for it.

---

## Reuse analysis — can this help Silent Hill / emulators / other ports?

> The sections below are grounded in web research (see citations). Short version:
> **the libnx/hardware playbook transfers to literally any Switch homebrew; the NVK Vulkan
> driver + WSI + audren backend are the high-value, genuinely novel reusable pieces; the
> Aurora GX layer transfers to other GC/Wii decomps.**

### Silent Hill / `ps-switch-kit` (PS1 native recomp) — HIGH overlap, same author
The PS1→Switch effort (`D:\Projects\ps-switch-kit`, the `ps1-switch-port` skill) is the most
direct beneficiary — same toolchain, same hardware, same dev loop:
- **Tier A (all of it)** transfers verbatim — it's the same libnx/devkitPro/nxlink/Eden world.
- **`switch_audio.cpp`** is a drop-in for SPU output (PS1 audio → F32/PCM → audren Int16).
- **WSI / present** transfers directly: PS1 is software-rasterized (ABGR1555 VRAM), so the final
  step is "blit a framebuffer to the screen" — exactly what our zero-copy nwindow present does
  (and the `num_ints`/block-linear gotchas are already solved).
- **NVK is optional there** (a software rasterizer doesn't need a GPU driver), but it *could*
  accelerate upscaling/post or a hardware-rasterizer path later.
- The `ps1-switch-port` skill already declares it "adapts the libnx/devkitPro/Eden hardware
  knowledge from the NVK-on-Switch effort." This doc is the concrete checklist behind that.

### Emulators & other homebrew on Switch — the NVK driver is the unlock

**The big finding from research: there is NO public Vulkan driver for Switch homebrew, and
upstream Mesa NVK does not even support Tegra.** So what we built is genuinely novel, not a
re-tread:

- **Upstream NVK explicitly excludes Tegra.** The Mesa NVK docs state it's conformant on
  *discrete* NVIDIA GPUs (Kepler→Blackwell) and that **"Tegra/integrated GPUs are not currently
  supported."** [[mesa-nvk]] Our driver runs NVK on the Switch's **GM20B (Tegra X1)** — a target
  upstream doesn't claim to support. The FECS-wall / depth-tiling / WSI work above is exactly
  what that gap requires.
- **The homebrew community has only *speculated* about this.** Mesa/nouveau gives Switch
  homebrew OpenGL; for Vulkan, the standing assumption is "Nouveau doesn't support it, but a
  custom Vulkan driver could theoretically be written." [[mesa-nvk]][[deko3d]] deko3d itself
  notes "there's even the chance of a homebrew Vulkan driver being developed in the future." We
  built that — and **directly on the hardware via libnx**, not as a translation layer on top of
  deko3d (the approach the community usually imagines).
- **deko3d — today's only GPU option — has the limitation we routed around.** It's the homebrew
  answer to NVN, low-level and fast, but it has **no on-device shader compilation** (offline
  `uam` only). [[deko3d]] Our NVK path compiles shaders at runtime via NAK, which is precisely
  why a GX→shader engine (or any emulator with dynamic shaders) is viable on it.

**Concrete reuse target — PS1 emulation on Switch (directly relevant to Silent Hill):**
- **DuckStation already has a Switch port** (`SumavisionQ5/duckstation-switch`) and DuckStation's
  hardware renderer **natively supports Vulkan** (alongside D3D/GL/Metal), with internal-resolution
  upscaling + PGXP geometry correction. [[duckstation-switch]][[duckstation-gpu]] Today a Switch
  homebrew build can only use its deko3d/GL path; **our NVK driver is the missing piece that
  could let DuckStation's existing, mature Vulkan renderer run on real Switch hardware** — i.e.
  hardware-accelerated PS1 (Silent Hill) with upscaling, without rewriting the renderer for deko3d.
- The same applies to other emulators with Vulkan renderers that lack a Switch GPU path:
  **PPSSPP, beetle-psx-vulkan, Flycast, ParaLLEl-N64** all render via Vulkan; RetroArch on Switch
  currently has **no Vulkan video driver** (only the long-discussed "Vulkan-on-deko3d someday").
  [[retroarch-switch]] Our driver is a candidate backend for exactly that gap.

**Honest caveats (so this doesn't oversell):**
- Our NVK is **"works for our workload," not Vulkan-conformant.** We no-op'd privileged FECS
  registers, simplified WSI present sync, and only exercised the feature set Aurora/Dawn use.
  Reusing it for an emulator means **hardening** (more of the API surface, more formats, CTS-ish
  testing), not a turnkey drop-in.
- **GM20B is weak.** Even with a perfect driver, the Tegra X1 in handheld/docked clocks limits
  what upscaling/post is affordable — measure, as always.
- **For PC-side Switch emulators (Eden/Yuzu/Ryujinx) the driver does NOT help** — they *emulate*
  the Switch GPU on a host, the opposite direction. The only marginal transfer is our reverse-
  engineered knowledge of the **VI compositor / `nwindow` / `NvGraphicBuffer` marshalling**
  (the `num_ints`, block-linear `kind=0xfe`, `VK_NN_vi_surface` extent `0xFFFFFFFF` behavior
  [[vk-nn-vi-surface]]), which could inform the accuracy of how they emulate homebrew WSI.

**Bottom line:** the reuse value is real and concentrated in three things — (1) the **libnx/HW
playbook** transfers to *any* Switch homebrew (Silent Hill included), (2) the **audren audio
backend** is a drop-in, and (3) the **NVK Vulkan driver + nwindow WSI** is a one-of-a-kind asset
that could unlock Vulkan-renderer emulators (DuckStation/PPSSPP/RetroArch cores) on real Switch
hardware — a thing the community has wanted but nobody has publicly shipped. The biggest lever
for the Silent Hill project specifically is the audio + present + HW playbook now; the NVK driver
is the option that opens up if/when it wants hardware-accelerated rendering.

### Android-wrapper ports (soloader) — e.g. Dan's Bully-NX — the SAME stack we mastered
This is the strongest "not Dusklight-only" case, because Dan's own Bully port leans on exactly
the platform tech we now know cold.
- **What Bully-NX is:** a wrapper/loader, NOT a reimplementation — *"It loads the original
  Android native game library, patches the runtime, and runs it inside a minimal Switch-side
  compatibility layer."* It loads the Android arm64-v8a `libGame.so` + `libc++_shared.so` +
  `assets/` from the official APK. Built on fgsfds + Andy Nguyen's (TheFloW's) Vita
  **so-loader** base. [[bully-nx]][[bully-x]] Still WIP with *"temporary hacks/workarounds in the
  runtime"* (timing_workaround_ms, button layout, "more cleanup and stabilization still needed").
- **Its backend = the PUBLIC stack, not Dan's private NVK.** README build deps: `devkitA64`,
  `libnx`, `SDL2`, `OpenAL`, `minizip`, `zlib`, `zstd`, and **"Switch Mesa / Nouveau userspace
  libraries"** — i.e. **GLES via switch-mesa/nouveau** for graphics, **OpenAL** (→ SDL2 audio)
  for sound. [[bully-nx]] That is the *same Mesa-on-Switch userspace we cross-compiled and
  debugged* (we built Mesa 25.0.7 + the `drm_shim`/winsys for it) — just the GL frontend instead
  of the Vulkan one.
- **So yes, we could attack this — and here are the concrete "do better than Dan" levers:**
  1. **Vulkan instead of nouveau-GLES (our unique asset).** Bully-NX renders through Mesa's
     nouveau **GLES**. We have a runtime-shader-compiling **NVK Vulkan** driver + zero-copy WSI
     that nobody else on Switch has. Route the game's GL through **Zink (GL/GLES-on-Vulkan, also
     in Mesa)** → our NVK, OR use the engine's Vulkan path if it has one. *Honest caveat:* GLES
     via nouveau already works and may be fine; Vulkan/Zink only wins if GLES is the bottleneck,
     and it adds a layer — measure first (per our own Tier D rule).
  2. **Zero-copy present (proven 64× present speedup).** Any wrapper that blits via libnx
     `Framebuffer`/CPU-copy pays the 9.6ms present we already killed. Our block-linear nwindow
     present (`num_ints`, `kind=0xfe`) drops it to ~150µs — a free win for *any* Switch port,
     GLES or Vulkan.
  3. **Direct audren audio.** OpenAL-soft on Switch backends onto SDL2 audio (extra latency/copy).
     Our `switch_audio.cpp` talks to **audren** directly (Int16) — lower-latency, fewer layers.
  4. **Stabilization discipline.** Bully-NX self-describes as WIP with temporary hacks. Our
     crash-triage playbook (Sphaira FTP → addr2line, "emulators lie", measure-before-optimize,
     heartbeat probes) is exactly what turns a "boots with workarounds" port into a clean one.
- **The bigger strategic point (what makes this NOT Dusklight-specific at all):** the **so-loader
  pattern is general** — load an Android/Vita arm64 `.so`, resolve its dynamic symbols against
  shims (bionic libc → newlib/libnx, GLES → Mesa, OpenSL/OpenAL → audren, input → hid), patch,
  run. Combine that public loader base with **our platform stack** (Mesa-on-Switch build infra +
  NVK Vulkan + audren + the HW playbook) and you have a reusable capability to port *many* Android
  games to Switch — including titles with **no existing port at all**, which is more valuable than
  out-competing Dan on one he already shipped. The loader/dynamic-linking layer is the one piece
  we'd be building fresh (it's adjacent to our decomp/driver work, not identical), but the base is
  open (TheFloW/fgsfds) and well-trodden.
- **Reality check:** Dan is excellent and Bully-NX already "runs really well." Realistically we'd
  (a) match it on the same public tech, (b) *maybe* beat its perf via Vulkan/Zink + zero-copy
  present + direct audren (needs measurement to confirm), (c) clean up its known workarounds, and
  (d) — the best use of the capability — point the same pattern at games nobody has ported yet.

### Sources
- [givethesourceplox/bully-NX (the Switch wrapper/loader)](https://github.com/givethesourceplox/bully-NX) — `[[bully-nx]]`
- [Bully-NX coverage / "Android wrapper-loader, runs well"](https://x.com/TheNathanNS/status/2041505805929721945) — `[[bully-x]]`
- [Mesa NVK driver docs (Tegra not supported)](https://docs.mesa3d.org/drivers/nvk.html) — `[[mesa-nvk]]`
- [devkitPro/deko3d README (homebrew GPU API, no runtime shader compile)](https://github.com/devkitPro/deko3d/blob/master/README.md) — `[[deko3d]]`
- [DuckStation GPU rendering (Vulkan among HW backends)](https://deepwiki.com/stenzek/duckstation/2.2-gpu-rendering) — `[[duckstation-gpu]]`
- [DuckStation Switch port](https://github.com/SumavisionQ5/duckstation-switch) — `[[duckstation-switch]]`
- [RetroArch Switch (Vulkan video driver discussion)](https://gbatemp.net/threads/retroarch-switch.492920/page-384) — `[[retroarch-switch]]`
- [VK_NN_vi_surface (Khronos refpage)](https://docs.vulkan.org/refpages/latest/refpages/source/VK_NN_vi_surface.html) — `[[vk-nn-vi-surface]]`
