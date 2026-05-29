# Dusklight NX — Switch port (work in progress)

This folder bootstraps a Nintendo Switch port of Dusklight.

**Phase 0 (current):** stub `.nro` that proves the toolchain. No game code, no GPU,
no Aurora — just a libnx console app launched under the name "Dusklight NX".

## Phases

| Phase | Goal | Status |
|---|---|---|
| 0 | Stub `.nro` builds & boots on Switch / Ryujinx | scaffolded here |
| 1 | Aurora platform layer ported to libnx (no SDL3); null GPU backend; Dusklight `main()` runs but renders nothing | TODO |
| 2 | Dawn (WebGPU) + Mesa NVK Vulkan backend cross-built and linked | TODO — hardest step |
| 3 | Game code (src/d, src/m_Do, src/SSystem, …) compiled for `aarch64-none-elf`, ISO loading works | TODO |
| 4 | Polish: handheld/docked resolution, suspend/resume, Joy-Con rumble, gyro, save persistence on SD | TODO |

## How the closed-source port (`dusk.nro`) was built — reverse-engineered

String analysis of the public `dusk.nro` binary reveals:

- **Mesa 25.3.6** (commit `ef43f0d203`) cross-built against devkitA64
- **NVK** Vulkan driver with a custom `nvkmd/switch/` backend
  (files `nvkmd_switch_dev.c`, `nvkmd_switch_pdev.c`) that wraps libnx `nv:*` services
  instead of Linux DRM/KMS
- `nvk_loaderless_vk.c` — skips the Vulkan loader, ICD entry points called directly
- **Dawn (WebGPU)** kept intact, configured to use its Vulkan backend
- **Aurora** kept intact
- **SDL3 replaced** by libnx-native primitives (`hid`, `audren`/`audout`, framebuffer)
- GPU target: NVIDIA Tegra X1 (GM20B / Maxwell). Vanilla NVK got Maxwell support in
  Mesa 25.1 (May 2025), which is why this only became feasible recently.

None of the above (`nvkmd/switch`, `nvk_loaderless_vk`, the libnx Aurora shim) exists
in any public Mesa branch or homebrew project as of May 2026. Recreating that work
is the actual cost of this port; the rest is glue.

## Build (phase 0)

From the MSYS2 devkitPro shell (not PowerShell — the `/opt/devkitpro` mount only
exists inside MSYS2):

```bash
cd /d/Projects/dusklight/platforms/switch
make
```

Output: `Dusklight.nro` (plus `Dusklight.elf`, `Dusklight.nacp`).

## Test

- **Ryujinx** (recommended for first smoke test — Eden stubs SSL, irrelevant here
  but Ryujinx is just more reliable for newer libnx).
- **Real Switch with Atmosphère**: copy `Dusklight.nro` to `/switch/Dusklight/`
  on the SD card and launch from the Homebrew Menu.
- **nxlink**: run `nxlink -s` on the PC, call `nxlinkStdio()` near the top of
  `main` if you want printf to come back over Wi-Fi.
