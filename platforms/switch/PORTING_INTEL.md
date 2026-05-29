# Dusklight → Nintendo Switch — Porting Intel

**Document role:** canonical brain-dump of everything we know after the May 2026 forensic deep-dive.
Read this first before touching anything in `platforms/switch/`.

**Status:** phase 0 (stub `.nro` boots and shows "Dusklight NX" on Switch) → ✅ done.
Everything from phase 1 onward depends on the strategy laid out below.

---

## TL;DR

A closed-source Dusklight Switch port already exists (`dusk.nro`, 48 MB). It was built by one person — **"Dan"** (also known as `givethesourceplox`, `dantiicu`, `Tiicu`, `ticohq`, Twitter `@tiicuapp`, Discord `discord.gg/DPgXGqJMkJ`). Dan has confirmed publicly he will not release the source for the private pieces (the Mesa fork) but has published ~90% of the surrounding stack as MIT-licensed reference.

The actual private piece is **one custom branch of Mesa 25.3.6** containing four files that don't exist upstream:
- `src/nouveau/vulkan/nvkmd/switch/nvkmd_switch_dev.c`
- `src/nouveau/vulkan/nvkmd/switch/nvkmd_switch_pdev.c`
- `src/nouveau/vulkan/nvk_loaderless_vk.c`
- `src/vulkan/wsi/wsi_common_switch.c`

These ~4 files are what we'd have to recreate to match Dan's port quality. Two completely independent Switch ports (Ship of Harkinian, Simpsons Hit & Run) prove that a **lower-performance but fully-public path** exists via `switch-mesa` OpenGL ES — and Dan himself recommended that path in his Discord.

---

## 1. The closed port and its author

### 1.1 Identity (consolidated)

| Alias | Where |
|---|---|
| Dan | Discord (`discord.gg/DPgXGqJMkJ`), email `ticolauncher@proton.me` |
| `dantiicu` | GitHub personal — 7 public repos, all relevant |
| `Tiicu` | GitHub commits author name; Twitter `@tiicuapp` |
| `givethesourceplox` | GitHub redistribution forks — `dusklight-NX`, `UnleashedRecomp-NX`, `bully-NX`. The name is a self-aware joke ("give the source plox"). |
| `ticohq` | GitHub org for the Tico emulation frontend + 16 forked emulator cores |

**Confirmed by user via Discord: `givethesourceplox` and `Dan` are the same person.** Dan has stated he will not release the Mesa fork source.

### 1.2 Confirmed shared private Mesa fork across multiple NROs

Same custom backend (`nvkmd/switch/*`, `nvk_loaderless_vk`, `wsi_common_switch`) appears in **at least three** different game NROs from Dan's hand:

| NRO | NACP author | Mesa version | Mesa commit | Notes |
|---|---|---|---|---|
| `dusk.nro` (Dusklight) | Twilit Realm | 25.3.6 | `ef43f0d203` | Full Aurora + Dawn + Vulkan path |
| `UnleashedRecomp.nro` | (hedge-dev) | 25.3.6 | `2dfb126b2d` | Different rebase of same patch series |
| `tico.nro` (Tico launcher) | ticoverse.com | 25.3.6 | (same line) | NVK + libretro core loader |
| `bully_nx.nro` (Bully:AE) | Dan | **23.3.6** `f823a866b2` | **GLES** (different toolchain — Gallium nouveau + libdrm_nouveau shim) |

The fact that `bully_nx.nro` uses Mesa 23 GLES while the others use Mesa 25 Vulkan means Dan maintains **two parallel private branches**. He's said in Discord "some games runs worse on new mesa" — confirming active version pinning for perf.

### 1.3 Distribution mechanism (confirmed private)

The private Mesa is packaged as a **Docker image `ticohq/switch-nvk-vulkan`** containing pre-compiled artifacts under `/opt/nvk-switch/`. The image **is not public on Docker Hub** (`hub.docker.com/v2/repositories/ticohq/` returns `{"count":0}`). The build script that produces the image (`nvk-image/create-nvk-image.sh`) is also not public — `gh search code 'create-nvk-image'` returns 0 matches.

Public consumers (`switch-vulkan-triangle-test`, `vulkan-smoke-test-switch`) reference it via:
- env var `DOCKER_IMAGE=ticohq/switch-nvk-vulkan`
- pkg-config name `nvk-switch-vulkan`
- prefix `/opt/nvk-switch`

---

## 2. What Dan published (the ~90% MIT-licensed stack)

### 2.1 `dantiicu/aurora-switch` ⭐ central piece

- Fork of `encounter/aurora` (the engine Dusklight uses)
- Single squashed "Switch Port" commit: **+5527 / −130 LOC**, 70 files modified
- Adds CMake option `AURORA_PLATFORM_SWITCH=ON` which:
  - Replaces SDL3 with libnx-native primitives
  - Adjusts heap/threading/file paths for Horizon
  - Conditionally turns off ImGui (`_aurora_default_imgui OFF`) and GPU cache (`_aurora_default_gpu_cache OFF`) for Switch
- Pinned Dawn version: `v20260423.175430`
- Pinned SDL3 version: `3.4.4`
- License: **MIT**

### 2.2 `dantiicu/dawn-switch`

- Fork of `google/dawn` (Chromium's WebGPU implementation)
- 432 MB repo (full Dawn source + Switch adaptations)
- "Switch Port" commit: 2206 additions, 60 deletions, 25 top files
- Switch changes are **Vulkan-backend only** — the Switch Port commit only touches `src/dawn/native/vulkan/*` and platform helpers
- Dawn upstream OpenGL backend code (`src/dawn/native/opengl/*`) is INHERITED in the fork but NOT activated for Switch by Dan — **this is our opening for the public GLES path**

### 2.3 Vulkan sample apps (three small repos)

- `dantiicu/switch-vulkan-triangle-test` ← same code as `vulkan-triangle-test-switch`. 42 KB `TriangleTest.cpp`, draws rotating triangle, **uses ONLY standard Vulkan + `VK_NN_vi_surface`**
- `dantiicu/vulkan-smoke-test-switch` — 52 KB minimal instance/device/swapchain probe
- `dantiicu/vulkan-compute-shader-test-switch` — compute pipeline sample

All three follow identical build pattern: standard Vulkan code + linkage via private Docker image. The application-layer code is **100% portable Vulkan** — nothing Switch-specific in the source.

### 2.4 The `ticohq` emulator core forks (16 repos)

All MIT/permissive. Switch patches in single "tico port" / "tico integration" commits. Most relevant patterns:
- **`tico-dolphin`** — adds `MemArenaSwitch.cpp` (505 LOC), the libnx-native memory arena. Direct reference for handling Horizon's virtual memory.
- `tico-flycast` — Vulkan via Dan's private Mesa
- `tico-ppsspp` — same

### 2.5 The "open source" of the troll forks

- `givethesourceplox/dusklight-NX` — 1:1 clone of upstream `TwilitRealm/dusklight`, 2 commits ahead with only README mentioning Switch. **TROLL — no Switch source.**
- `givethesourceplox/UnleashedRecomp-NX` — 0 ahead, 1 behind upstream. **TROLL.**
- `givethesourceplox/bully-NX` — **REAL OPEN-SOURCE PORT** of Bully:AE via Android `.so` loader. Forks `fgsfdsfgs/max_nx`. Has functional Makefile that documents `CUSTOM_MESA_ROOT=/mesa-new` build pattern. MIT.

---

## 3. The alternative public stack (proven by other ports)

### 3.1 Ship of Harkinian (OoT decomp port to Switch)

- Repo: `HarbourMasters/Shipwright-Switch` (12 stars) + submodule `HarbourMasters/libultraship-switch`
- Stack: **SDL2 + EGL + OpenGL ES via `switch-mesa` (public Mesa Gallium nouveau)**
- Public Dockerfile in the repo using `devkitpro/devkita64` base
- Fingerprint of `soh.nro`: 117× `EGL`, 25× `GLES`, 22× `OpenGL`, 13× `mesa`, 9× `nouveau`, **0× `nvk_`, 0× `VkPhysicalDevice`**

### 3.2 Simpsons Hit & Run

- Repo: `ZenoArrows/The-Simpsons-Hit-and-Run` (380 stars, fork of `Svxy/The-Simpsons-Hit-and-Run` — the 2003 leaked source)
- Stack: **SDL2/3 + Pure3D engine + OpenGL ES via switch-mesa**
- `SimpsonsPAL.nro` / `SimpsonsNTSC.nro` fingerprint: 113× EGL, 35× GLES, 28× OpenGL, 13× mesa, 9× nouveau, **0× nvk_**

### 3.3 Implication

Two completely independent, popular Switch homebrew ports of real games use the public `switch-mesa` OpenGL ES path. **The Vulkan NVK route is NOT required** for a Dusklight Switch port — it's a performance optimization, not a feature gate.

---

## 4. Dan's own roadmap (Discord 2026-01-30)

Dan posted a complete porting recipe in his Discord. Two key passages, paraphrased + decoded:

### 4.1 "The hard part is memory mapping, not graphics"

> "just the memory mapping thing that is different (...) when ported to android, someone did proper cpu adjustments, memory mapping and all this (...) libnx expose memory access, w^x pointers (...) i used this to port swanstation and ppsspp as well"

Linked libnx headers (these are public devkitPro headers — `nx/include/switch/...`):

| Header | Critical APIs | Relevance to Dusklight |
|---|---|---|
| `arm/cache.h` | `armDCacheFlush`, `armICacheInvalidate` | LOW — Dusklight has no self-modifying code |
| `kernel/jit.h` | `jitCreate`, `jitTransitionToWritable`, `jitTransitionToExecutable` | MEDIUM — Dawn might do shader specialization codegen at runtime |
| `kernel/svc.h` | `svcMapMemory`, `svcMirrorMemory`, `svcMapPhysicalMemory` | HIGH — needed to map GC memory regions at fixed VA |
| `kernel/virtmem.h` | `virtmemReserve`, `virtmemAddReservation` | **CRITICAL** — Horizon has ASLR; TP code assumes predictable pointers |

### 4.2 "Graphics is solved — pick GLES (easier) or deko3d (faster)"

> "also, since libnx 1.4.0 switch supports open gl core (same from pc) - https://devkitpro.org/viewtopic.php?f=13&t=8780"
> "and someone really interested can also create an custom video adapter for switch using deko3d - https://github.com/devkitPro/deko3d"
> "which is more performatic for switch by reducing cpu overhead but obvious, things that needs a lot of work"

**Dan himself recommends `switch-mesa` OpenGL Core as the public path.** His private NVK Vulkan branch is a perf optimization — not the only way.

### 4.3 "I used Claude to brute-force port emulators"

In another Discord message: "eu usando claude mythos / portando emulador via brute force." This is direct confirmation that **AI-assisted brute-force port iteration is the method Dan used**. The same method is available to us.

### 4.4 "Information is in the project repository"

Dan: "if you care enough about this, you should know that the information is in the project repository." → he believes his published repos contain enough breadcrumbs. He's signalling RTFM but not actively gatekeeping.

---

## 5. The libnx-side technical map

### 5.1 Memory regions a GameCube game expects

The original game (TP) was compiled against:
- **MEM1** = 24 MB main RAM at fixed PowerPC physical/virtual addresses
- **MEM2** = 64 MB ARAM at fixed addresses
- Stack and locator pages at known offsets

For Dusklight on PC, Aurora abstracts this. For Switch, we need to use `virtmemReserve` to claim a stable VA range before any allocation happens, then `svcMapMemory` or aligned `malloc` inside that range.

### 5.2 Heap override for large applications

Dan's TriangleTest demonstrates the pattern:
```c
extern "C" {
    u32    __nx_applet_type = AppletType_Application;  // not LibraryApplet — full memory
    size_t __nx_heap_size   = 0;                       // 0 = take all available
}
```
This is **mandatory** for Dusklight — the default applet heap (~512 MB) is plenty for TP, but the default profile is the small LibraryApplet which gives only ~256 MB. Without this override, `malloc` fails early.

### 5.3 Required libnx services

From Borealis/NexPS playbook (already documented in `SWITCH_HOMEBREW_GUIDE.md`):
`socket`, `romfs`, `pl:u`, `setsys`, `nifm`, `psm`, `lbl`, `applet`, `hid`. Aurora-switch already initializes most of these — verify by reading `dantiicu/aurora-switch` `cmake/aurora_os.cmake`.

---

## 6. The strategy: two parallel tracks

### Track A — Public GLES (lower risk, ship-able)

```
Dusklight game code
  → Aurora (dantiicu/aurora-switch with AURORA_PLATFORM_SWITCH=ON)
  → Dawn (dantiicu/dawn-switch with OpenGL backend ENABLED — not done by Dan)
  → switch-mesa OpenGL ES via Mesa nouveau Gallium (PUBLIC pacman package)
  → libnx → Horizon
```

**Effort estimate: 2–4 months solo with AI assistance.**

Risks: Dawn's OpenGL backend is less performant than Vulkan and may need tweaks to work against Mesa nouveau (which targets Tegra Maxwell — older GPU spec). Some Dawn features (storage buffers, compute, etc.) may not be available through OpenGL ES 3.2.

### Track B — Brute-force the Mesa NVK fork (high reward, Dan's path)

```
Mesa 25.3.6 + recreate ~4 files
  → Vulkan NVK Switch (PRIVATE → ours)
  → Dawn (dantiicu/dawn-switch as-is)
  → Aurora (dantiicu/aurora-switch)
  → Dusklight
```

The 4 files to write (file names confirmed from binary forensics):
1. **`src/nouveau/vulkan/nvkmd/switch/nvkmd_switch_dev.c`** — Switch nvkmd device backend. Reference: `nvkmd/nouveau/nvkmd_nouveau_dev.c` (Linux DRM, public)
2. **`src/nouveau/vulkan/nvkmd/switch/nvkmd_switch_pdev.c`** — Physical device. Reference: `nvkmd/nouveau/nvkmd_nouveau_pdev.c`
3. **`src/nouveau/vulkan/nvk_loaderless_vk.c`** — Vulkan loader bypass. Standard ICD entry-point glue.
4. **`src/vulkan/wsi/wsi_common_switch.c`** — VI surface integration. Reference: `wsi_common_wayland.c` + Nintendo `VK_NN_vi_surface` documentation.

Each ~500-2000 LOC. Pattern: translate every `drmIoctl(NV_*)` call to its libnx `nvIoctl` equivalent. Backed by `nv:*` services via libnx headers `services/nv.h`.

**Effort estimate: 3–6 months solo with AI assistance (matches Dan's "brute force" claim).**

### Recommended: run both

Track A delivers a working Dusklight in months. Track B, if completed, upgrades the backend and matches Dan's quality. Track B failure does not block Track A.

---

## 7. Public reference repos (all need git clone)

```
# Core
git clone https://github.com/dantiicu/aurora-switch.git
git clone https://github.com/dantiicu/dawn-switch.git

# Vulkan samples (Track B reference)
git clone https://github.com/dantiicu/switch-vulkan-triangle-test.git
git clone https://github.com/dantiicu/vulkan-smoke-test-switch.git
git clone https://github.com/dantiicu/vulkan-compute-shader-test-switch.git

# GLES reference (Track A model)
git clone https://github.com/HarbourMasters/Shipwright-Switch.git
git clone https://github.com/HarbourMasters/libultraship-switch.git

# Public Mesa Switch foundation (historic, GLES baseline)
git clone https://github.com/devkitPro/pacman-packages.git
# → see switch/mesa/switch-mesa-20.1.0-5.patch (121 KB, fincs's original work)

# Loader pattern reference (open MIT)
git clone https://github.com/givethesourceplox/bully-NX.git
git clone https://github.com/fgsfdsfgs/max_nx.git
```

---

## 8. What's already done in this repo

`platforms/switch/` currently contains:
- `Makefile` — devkitPro classic, builds against libnx; outputs `Dusklight.nro`
- `source/main.c` — libnx-only stub showing system info; exits with +
- `icon.jpg` — 256×256 JPEG resized from `res/icon.png`
- `README.md` — phase plan + recipe of the closed port
- `.gitignore`

Validation:
- Built successfully from MSYS2 devkitPro shell on Windows
- `Dusklight.nro` (230 KB) with NACP `Title="Dusklight NX"`, `Author="Dusklight NX port (community)"`, `Version="0.0.1-stub"`
- ASET section intact, icon embedded
- Ready to copy to `sdmc:/switch/Dusklight/Dusklight.nro`

---

## 9. Known unknowns (the things we still need to figure out)

1. **Dawn OpenGL backend on Switch**: does it actually work against switch-mesa nouveau out of the box? Or does it need Switch-specific patches? Need to try.
2. **Aurora's `aurora::gfx` IR**: how tightly bound is it to Dawn? Can `dantiicu/aurora-switch` swap Dawn for direct GLES if needed?
3. **TP heap requirements**: how much memory does the game actually want? Dusklight on PC reports it — but Switch handheld mode is constrained to ~3.3 GB usable.
4. **Endian**: TP is Big-Endian on GC. Aurora's PC port already handles this for x86/ARM little-endian — but the Switch path inherits the same fixes. Should "just work" but expect edge cases.
5. **Save state**: Dusklight saves via `.gci` to Application Support dir. Switch needs SD card path via libnx fsdev.
6. **Audio**: GC has DSP+ARAM. Aurora abstracts to host audio. Switch path = `audrenInitialize` per libnx. Already in aurora-switch.

---

## 10. Next concrete steps

### Immediate (now)
1. Clone `dantiicu/switch-vulkan-triangle-test` into `platforms/switch/reference/` ✅
2. Compile `TriangleTest.cpp` to `.o` (no link) with devkitA64 to extract Vulkan symbol surface
3. Run `aarch64-none-elf-nm -u` on the `.o` → finite list of Vulkan API functions our Mesa fork must export
4. Cross-reference against Mesa NVK `nvkmd/nouveau` public source → quantify "how much code do we need to write"

### Short-term (next 1–2 weeks)
5. Clone `dantiicu/aurora-switch` and try building `examples/simple.c` against it using stub GPU backend → validate Aurora compiles for aarch64 at all
6. Investigate Dawn OpenGL backend feasibility in `dantiicu/dawn-switch`
7. Try building Shipwright-Switch via its Docker → independent confirmation that public GLES toolchain works on this machine

### Medium-term (next 1–3 months)
8. Either:
   - Track A: Patch dawn-switch to enable OpenGL backend, link against switch-mesa, get Dusklight's splash screen rendering
   - Track B: Start writing `nvkmd/switch/*.c` files against Mesa 25.3.6, iterate until triangle test compiles and runs

---

## 11. Why this is realistic now (a sanity check)

- Dan portyed multiple PC-class engines (Dolphin, Sonic Unleashed Recomp, Aurora, Dawn) to Switch **solo with AI assistance** in ~6 months
- All four files he wrote that don't exist upstream have known sizes (typically 500-2000 LOC each)
- The Linux DRM equivalent is fully public and ~the same shape, so translation (not invention) is the work
- Dusklight is structurally simpler than Dolphin (no JIT, no emulated CPU)
- The user (you) shipped NexPS — a Switch homebrew with `libcurl + multi-thread download + AES decrypt + Borealis UI` — comparable complexity to a Mesa backend port

The risk isn't "is this possible." The risk is "how many months of focused effort before the first frame renders."

---

**Last updated:** 2026-05-22, after Discord screenshot disclosure that Dan used Claude to brute-force port emulators.
