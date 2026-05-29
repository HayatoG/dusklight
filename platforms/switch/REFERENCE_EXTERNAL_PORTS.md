# Reference — external Switch ports (for future POCs / tests)

Catalog of OTHER projects' Switch ports we've looked at, what's reusable for Dusklight, and what
isn't. Dusklight = GameCube *Twilight Princess* decomp → Aurora → **GX command-IR replay** →
deko3d/native-Mesa-GLES → libnx. Anything that renders via a *different* graphics path (Fast3D, N64
RDP, etc.) is **not** directly reusable for our GX→GLES replay, but platform-level tricks often are.

---

## 1. JRickey/BattleShip — PR #185 "Optimize Switch port performance, logging, and build process"

- **URL:** https://github.com/JRickey/BattleShip/pull/185
- **What it really is:** despite the repo name, it's a **Super Smash Bros 64 decomp** port. Stack:
  N64 decomp + `libultraship` (SoH's framework) + **Fast3D** renderer. Submodules: `libultraship`,
  `decomp`; headers `include/ultra64.h`, `include/ssb_types.h` (`PR/ultratypes.h`).
- **Graphics path:** Fast3D translates **N64 RDP** display lists → **desktop GL 2.1 Core** via
  `SDL_GL_CreateContext` + glad (`SDL_GL_GetProcAddress`) on switch-mesa. A complete, mature,
  self-contained OpenGL renderer.
- **Threading:** coroutines (`port/switch/context_switch.s`, `port/coroutine_switch.cpp`) — N64
  single-thread game loop emulated via aarch64 context switching.

### Reusable for Dusklight
- ✅ **Corroboration only:** Mesa-GL on switch-mesa drives a *whole* game on real HW. Reinforces our
  Mesa-GLES bet is viable; our "only blue / invisible draws" is a *our-side geometry* problem, not a
  Mesa-viability problem.
- 🔮 **CPU/GPU overclock for performance (LATER):** `port/switch/SwitchImpl.cpp` +
  `SwitchPerformanceProfiles.h` set clocks via **`pcvSetClockRate`** (and clock presets). Relevant
  once we hit gameplay — our engine emits a *flood* of GX draws (the 470k+ that OOM'd g_uniforms);
  boosting clocks will help. libnx: `pcvInitialize()` / `pcvSetClockRate(PcvModule_CpuBus, hz)` or
  the newer `clkrst` sysmodule. (Docked Tegra X1 max ~1785 MHz CPU / 768 MHz GPU.)
- 🪤 **newlib `wint_t` landmine** (`include/ssb_types.h`): devkitPro newlib's built-in `<stddef.h>`
  doesn't define `wint_t`, but `<sys/_types.h>` (pulled by `<string.h>` etc.) needs it. Their fix:
  define `wint_t` globally early (`typedef __WINT_TYPE__ wint_t;` under `#ifdef __SWITCH__`). Same
  family as our heuristic #6 (newlib ctype macro pollution `_U _L _N ...`). Keep on the radar.

### NOT reusable
- ❌ Fast3D / RDP rendering — entirely different from our GX command-IR replay.
- ❌ Coroutine context-switching — TP uses real `OSThread`s (we see `[PC-OSThread]` in our logs).
- ❌ Controller mapping (SDL/libultraship), packaging CMake (we already have `build-docker.sh` +
  `elf2nro --romfsdir`), `BundleSwitch` SD-card layout (we netload via nxlink).

### POC / test ideas seeded by this PR
- A **clock-boost helper** TU (`pcvSetClockRate`/`clkrst`) gated behind a config flag, to A/B
  gameplay frame time once we render.
- A pre-emptive **newlib-types portability header** if we re-include more decomp `src/dusk/*` files.

---

## How to add a new entry
For each external port reviewed, note: real engine/stack, graphics path, what's reusable (✅),
platform crumbs (🔮/🪤), what's not (❌), and any POC ideas. Keep the "is the render path the same as
ours?" question front and center — that decides reusability.
