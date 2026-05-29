# AI Knowledge Dossier — Dusklight Switch Port

Curated knowledge sources for injecting expertise into AI assistants (Claude, etc.) working on this port. Use this as a paste-into-context cheatsheet when an AI is hitting domain gaps.

**Generated:** 2026-05-22, after a long Dusklight Switch port session.

---

## Domain 1 — Switch homebrew (libnx, NRO, services)

**Authoritative sources:**
- `switchbrew/libnx` + Doxygen at `switchbrew.github.io/libnx/`
- **Switchbrew Wiki** (`switchbrew.org/wiki`) — definitive reference for IPC services
- `switchbrew/switch-examples` — minimal runnable demos
- `misson20000/twili` — debug-monitor with GDB stub

**Highest-leverage files:**
- `libnx/nx/include/switch/runtime/env.h` — `__nx_applet_type` / `__nx_heap_size` mechanics
- `libnx/nx/source/runtime/init.c` — full init flow and HBABI handshake
- `switch-examples/graphics/opengl/*/source/main.cpp` — canonical EGL+NWindow+GLES boot

**Gap symptoms:** AI hand-rolls service init order, forgets `__nx_applet_type = AppletType_Application` for >512MB heap, confuses applet/title/sysmodule lifecycle.

---

## Domain 2 — Switch GPU stack (Tegra X1, deko3d, switch-mesa)

**Authoritative sources:**
- `devkitPro/deko3d` (especially `Primer.md`) — Tegra X1 GPU semantics
- `devkitPro/mesa-old` (branch `switch-18.3`) and `devkitPro/pacman-packages/switch/mesa/`
- Forum thread `devkitpro.org/viewtopic.php?f=13&t=8780` — fincs's libnx 1.4.0 announcement
- Mesa upstream `src/gallium/drivers/nouveau/`

**Highest-leverage:**
- `deko3d/Primer.md` — Tegra X1 register model, SASS SM 5.3, command-buffer semantics
- The 121 KB Mesa Switch patch in `pacman-packages` — exact EGL extensions actually implemented at runtime

**Gap symptoms:** AI assumes modern Mesa behavior; trusts `#ifdef EGL_KHR_fence_sync` header presence to mean runtime support (THE bug that cost iterations on this project); conflates Maxwell-2 (Tegra X1) with desktop Maxwell.

---

## Domain 3 — WebGPU / Dawn / Tint

**Authoritative sources:**
- `google/dawn` upstream
- `gpuweb/gpuweb` (WebGPU spec)
- Tint under `dawn/src/tint/lang/` (writers per backend)
- `webgpu_cpp.h` / `webgpu.h`

**Highest-leverage:**
- `dawn/src/dawn/native/opengl/BackendGL.cpp` — `DiscoverPhysicalDevices` + ext mandates
- `dawn/src/dawn/native/opengl/{DisplayEGL,ContextEGL,SwapChainEGL}.cpp` — surface plumbing
- `dawn/docs/dawn/codegen.md`, `dawn/docs/dawn/buildbot.md`

**Gap symptoms:** AI treats Dawn as a black box; doesn't know `PhysicalDevice → Adapter → Device → Queue` lifecycle; conflates `wgpu::Surface` with WSI swapchain.

---

## Domain 4 — Aurora-style GX compatibility layer

**Authoritative sources:**
- `encounter/aurora` upstream and `dantiicu/aurora-switch` fork
- `dolphin-emu/dolphin` — `Source/Core/VideoCommon/{BPMemory,CPMemory,TextureCacheBase,PixelShaderGen}`
- **YAGCD** (`hitmen.c02.at/files/yagcd/`) chap. 5 (registers) and chap. 8 (GX)
- `amnoid.de/gc/tev.html` — TEV explained
- `libogc/gx.h` — closest thing to an SDK reference

**Highest-leverage:**
- `aurora/lib/gfx/gx.cpp` + `aurora/lib/gfx/stream/` — GX→WebGPU pipeline batching
- YAGCD chap. 5.11 (BP/CP) — every register name JSystem writes

**Gap symptoms:** AI doesn't know TEV is fixed-function; confuses BP vs CP vs XF registers; missing intuition for VCD/VAT vertex attribute encoding.

---

## Domain 5 — GameCube/Wii decomp & JSystem

**Authoritative sources:**
- `zeldaret/tp` (100% decompiled — our upstream) and `decomp.dev/zeldaret/tp`
- `zeldaret/oot` (sister project, more libs documented)
- `projectPiki/pikmin` and `projectPiki/pikmin2` — heavy JSystem users with cleaner symbol names
- Dolphin's `Source/Core/Core/HLE/`
- `decomp.me` / `objdiff` tools

**Highest-leverage:**
- `zeldaret/tp/include/JSystem/JKernel/JKRHeap.h` + `JKRExpHeap.cpp` — heap model
- `zeldaret/tp/include/JSystem/J3DGraphBase/J3DPacket.h` — display-list packet flow

**Gap symptoms:** AI treats JKRHeap as a regular C++ allocator (it has root-heap hierarchy + tagged allocations); doesn't know J3DSys binds GX and material data.

---

## Domain 6 — Cross-compilation & low-level

**Authoritative sources:**
- **AAPCS64** — `ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst`
- devkitPro toolchain: `devkitPro/buildscripts`
- newlib upstream — especially `libc/include/ctype.h` (the `_C`/`_U` macro pollution)
- LLD docs for static-link semantics

**Highest-leverage:**
- AAPCS64 §6 (parameter passing) + §5.4 (register usage) — explains GCC 10.1 warnings
- newlib `libc/include/ctype.h` lines 18-40 — the macro pollution catalog

**Gap symptoms:** AI suggests `-Wno-psabi` blindly; can't explain `--start-group/--end-group`; treats GCC parameter-passing warnings as noise; doesn't know newlib's ctype macros clash with C++ member names.

---

## Domain 7 — Emulation testing (Eden, Yuzu, Ryujinx)

**Authoritative sources:**
- `eden-emu/eden` (`git.eden-emu.dev`) and `yuzu-emu/yuzu` mirrors
- `Ryujinx` LDN forks
- `misson20000/twili` GDB stub + `jam1garner`'s GDB cheatsheet gist
- Yuzu blog explaining NVN→Vulkan HLE strategy
- Switchbrew wiki "Services" page

**Highest-leverage:**
- Eden source `src/core/hle/service/nvdrv/` — exactly which `nv:*` ioctls are emulated
- `jam1garner`'s GDB gist — remote-attach commands

**Gap symptoms:** AI claims emulators are "Vulkan-only" without checking `nvdrv` source; doesn't know Eden's logger encoding (UTF-16; needs `grep -a` to read homebrew prints).

---

## Single-URL drop-in dossier

| Domain | One link to paste into AI context |
|---|---|
| 1. libnx | https://github.com/switchbrew/libnx/blob/master/nx/include/switch/runtime/env.h |
| 2. Switch GPU | https://github.com/devkitPro/deko3d/blob/master/Primer.md |
| 3. Dawn/Tint | https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/opengl/BackendGL.cpp |
| 4. Aurora/GX | http://www.amnoid.de/gc/tev.html + https://hitmen.c02.at/files/yagcd/yagcd/chap5.html |
| 5. TP decomp | https://github.com/zeldaret/tp/tree/main/include/JSystem |
| 6. AAPCS64 | https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst |
| 7. Emulators | https://gist.github.com/jam1garner/c9ba6c0cff150f1a2480d0c18ff05e33 |

---

## Anti-patterns observed in this codebase's AI sessions

Concrete instances where AI assistants demonstrated gaps in this project:

1. **Header-presence treated as runtime support.** Agent claimed "all 5 EGL extensions Dawn needs ARE in switch-mesa's `eglext.h`" — true for the header, false at runtime (Mesa 20.1 advertises only 4). Cost multiple iterations to discover via standalone EGL probe. **Discipline needed:** `eglQueryString(EGL_EXTENSIONS)` over `#ifdef`.

2. **`TARGET_PC` polarity wrongly inferred.** Multiple agents assumed `!TARGET_PC ⇒ Switch`. Reality: `TARGET_PC` is defined on Switch too. The canonical guard for "not on Switch" is `#ifndef __SWITCH__`. (See memory file `dusklight-switch-target-pc-convention`.)

3. **Silent-failure tolerance in init returns.** Aurora's `webgpu::initialize` returns bool; agents accepted "true" without checking `g_backendType`. Null backend won silently. **Replace** mental model: "init succeeded" → "init succeeded, check which adapter was chosen."

4. **Emulator-vs-hardware substitution.** Agents earlier blamed Eden's `nv:*` emulation; later wanted to pivot to real hardware. The standalone EGL probe inverted both conclusions. **Lesson:** isolate one layer at a time with a minimal harness; don't reason about layer N's bug while layer N-1 is unproven.

5. **Dawn's PreferredBackendOrder traversal misread.** Agents saw `type=6 → type=8` in traces and guessed at enum positions. **Lesson:** when ordinals appear in traces, the first action is `grep -n 'enum.*Backend' extern/aurora/include/`, never inference.

6. **Newlib gotchas under-anticipated.** The `_C` macro clash with a u16 field was caught reactively after compile failures. Newlib `ctype.h` also defines `_U`, `_L`, `_N`, `_S`, `_P`, `_X`, `_B` — any of these could clash with C++ identifiers. An AI primed on the full macro list would pre-emptively grep.

---

## How to use this dossier

When starting a new AI session for Switch port work:

1. Paste the **Anti-patterns** section first — primes the model not to repeat known mistakes.
2. Paste relevant **Domain** sections based on what task is being approached.
3. Reference the **Single-URL drop-in** column when the AI needs to look up something specific.
4. If the AI starts making vague claims about Switch services or Mesa or Dawn, point it at the corresponding GitHub source URL — it should fetch and read before speculating.

For tasks that span multiple domains (e.g. "patch Dawn to accept Mesa 20.1"), paste Domains 2 + 3 simultaneously.
