# Dusklight → Switch Port Plan

**Scope:** open-source Switch port of Dusklight without depending on Dan's private Mesa NVK fork.
**Status as of 2026-05-22:** phase 0 done (stub `.nro` boots and shows in hbmenu).
**Background:** read `PORTING_INTEL.md` first — this file is just the action plan.

---

## Two parallel tracks

| Track | Stack | Effort | Risk | Status |
|---|---|---|---|---|
| **A. Public GLES** | Aurora-switch → Dawn(OpenGL backend) → public switch-mesa GLES → libnx | 2-4 months | Low | Recommended primary |
| **B. Brute-force NVK** | Aurora-switch → Dawn(Vulkan backend, as-is) → our Mesa NVK Switch fork → libnx | 4-6 months | Medium-high | Parallel reach goal |

Track A always succeeds in shipping something (lower fps). Track B, if completed, swaps in for performance parity with Dan's port.

---

## 5 milestones (sequential)

### M1 — Aurora-switch builds locally (1 week)
Clone `dantiicu/aurora-switch`. Cross-compile `examples/simple.c` with devkitA64. Use stub GPU backend.
- ✅ Signal: `simple.elf` produced for aarch64; link errors enumerate missing libs.
- Output: list of dependencies (Dawn flavor, SDL3/libnx, etc.)

### M2 — Cross-build Mesa upstream for Switch, GLES only (1-2 weeks)
Clone Mesa 25.x. Write meson cross-file targeting devkitA64. Build with **only GLES driver enabled** (path documented by `fincs`'s historic `switch-mesa-20.1.0-5.patch`).
- ✅ Signal: `libGLESv2.a` + `libEGL.a` produced for aarch64.

### M3 — Enable NVK Vulkan in Mesa build (Track B start) — 1 week
Switch the Mesa build to Vulkan+NVK. Build fails because `nvkmd/switch/` doesn't exist.
- ✅ Signal: compile errors point exactly at the 4 files we need to write.

### M4 — Write `nvkmd/switch/` + `wsi_common_switch.c` + `nvk_loaderless_vk.c` (4-8 weeks, Track B)
Brute-force translate ~2-4k LOC from Linux DRM backend (`nvkmd/nouveau/*.c`) to libnx `nvIoctl`/`nv:*`.
- ✅ Signal: TriangleTest.nro compiles and renders a triangle on Switch / Ryujinx.

### M5 — Dusklight integration (4-8 weeks)
Replace `extern/aurora` submodule in Dusklight repo with `dantiicu/aurora-switch`. Cross-compile game code (`src/d`, `src/m_Do`, `src/SSystem`, …) for aarch64. Resolve endian/threading/memory-layout issues. Map TP heap via `virtmemReserve`.
- ✅ Signal: Dusklight splash screen visible on real Switch hardware.

---

## Decision points along the way

**After M1**: if Aurora-switch has bugs we can't quickly fix, escalate to upstream PRs to `dantiicu/aurora-switch` (still open contribution, public repo).

**After M3**: if NVK turns out to require >6 months of work, drop Track B and commit fully to Track A.

**After M4**: if Vulkan works, decide whether to land Dusklight on it or stay on GLES for compatibility. Probably ship both backends.

---

## Concrete first-week tasks

1. `git submodule add https://github.com/dantiicu/aurora-switch.git extern/aurora-switch`
2. Add `platforms/switch/CMakeLists.txt` that uses `Switch.cmake` toolchain + pulls in `aurora-switch`
3. Try to build `examples/simple.c` from aurora-switch standalone — capture errors
4. Inventory missing deps (probably: SDL3 cross-build, Dawn cross-build, libjpeg-turbo cross-build, mbedtls)
5. Decide order to tackle each missing dep

---

## Reference repos to clone (already done in `reference/`)

```
git clone https://github.com/dantiicu/aurora-switch.git           # ⭐ central
git clone https://github.com/dantiicu/dawn-switch.git
git clone https://github.com/dantiicu/switch-vulkan-triangle-test.git  # done
git clone https://github.com/KhronosGroup/Vulkan-Headers.git           # done
git clone https://github.com/HarbourMasters/Shipwright-Switch.git      # GLES reference
git clone https://github.com/HarbourMasters/libultraship-switch.git
git clone https://github.com/devkitPro/pacman-packages.git             # fincs's Mesa patch
```

---

## Numbers we've proven (not speculation)

- 63 `vkXxx` functions needed by TriangleTest (upstream Mesa already implements all)
- 8 libnx symbols (all public)
- 2-4k LOC to translate for `nvkmd/switch/*` + WSI + loaderless
- `dusk.nro` confirmed using Mesa 25.3.6 commit `ef43f0d203`
- `tico.nro` confirmed using SAME private Mesa fork (cross-binary evidence)
- Two unrelated ports (SoH, Simpsons HnR) prove public `switch-mesa` GLES path works for AAA-scale games

**See `PORTING_INTEL.md` section 2-3 for the full evidence base.**
