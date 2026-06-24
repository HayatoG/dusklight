# Dusklight — Nintendo Switch — Installation Guide (en-US)

Homebrew build of **Dusklight** (open-source reimplementation of *The Legend of Zelda: Twilight
Princess*) running on Switch via our own NVK (Vulkan) graphics stack. The `.nro` is
**self-provisioning**: it ships a **pre-warmed shader cache** baked in and writes everything into its
data folder on first boot — so there is little to no texture/3D-model "pop-in" and installation is
basically just the one file.

## Requirements
1. A Switch on **CFW (Atmosphère)** with a homebrew launcher (hbmenu) or Sphaira.
2. Your **own legally-dumped** Twilight Princess GameCube disc image (NOT included — you must provide
   it). US release (GZ2E01), ~1.46 GB.
   - **Any format/name works:** `.gcm`, `.iso`, `.rvz`, `.ciso`, `.gcz`, `.wbfs`... — no renaming
     needed. Dusklight auto-detects it.
   - Tip: raw `.gcm`/`.iso` is fastest; compressed formats (e.g. `.rvz`) work but pay a
     decompression cost while playing.

## Install — basically just the .nro
1. Copy `dusklight.nro` to `sdmc:/switch/`.
2. Launch it once from hbmenu. It creates `sdmc:/TwilitRealm/Dusklight/` and writes the pre-warmed
   shader cache there **automatically**.
3. Put your TP dump (any name/format) into `sdmc:/TwilitRealm/Dusklight/`.
4. Launch again — Dusklight auto-detects the disc and runs.

> **No** manual folder layout, **no** `data_location.json`, **no** `game/` folder. The NRO bundles
> everything and provisions itself on first boot.

> **Everything lives in one folder:** settings, saves, caches and logs all sit under
> `sdmc:/TwilitRealm/Dusklight/`. The only exceptions are the `.nro` itself in `sdmc:/switch/`
> (required by hbmenu) and CFW crash reports in `sdmc:/atmosphere/`.

## What works (this build)
- **Audio** (JAudio2/DSP → libnx audren).
- **Gyro on by default** (bow/slingshot/clawshot aiming + look mode). Tune the sensitivity or turn
  it off in **Settings → Gyro**.
- **In-game menu**: press **MINUS (−)** during gameplay (warp, settings, etc.).
- **Automatic disc detection** in the data folder (any name/format).
- **Pipeline-compilation notification** + achievement toasts.
- **Changing Internal Resolution / aspect ratio (4:3)** no longer crashes (launcher and in-game).
- **Pre-warmed shader cache** → little/no first-run model/texture pop-in.

## Upgrading from an older version
- Your **save is preserved**: it already lived at `sdmc:/TwilitRealm/Dusklight/USA/...`.
- If you played a much older build (which saved to `sdmc:/aurora/USA/Card A/`), Dusklight
  **auto-imports** that save into the new folder on first boot (it copies, never deletes the old one).
- The `sdmc:/game/data_location.json` from old packages becomes useless and can be deleted (ignored).

## Notes
- Launcher/menus run at 60fps. In-game is ~30fps (Twilight Princess's native GameCube rate). Frame
  interpolation exists but does not yet produce extra frames on Switch (work-in-progress).
- An on-screen FPS overlay is on by default.
- **Per-file self-heal:** on boot the NRO restores from romfs **any seed file that is missing** —
  `config.json`, `dawn_cache.db` or `pipeline_cache.db` — individually. The folder does not have to
  be empty: delete just `config.json` and it comes back; delete just one `.db` and only that one
  comes back. Files you already have are **never** overwritten.
- The cache is read-write: missing shaders compile once and persist. Deleting the `.db` files won't
  break anything (it recompiles on demand, with pop-in until it re-warms; and the seed is restored on
  the next boot as above).
- Saves use the GameCube memory-card format (`.gci`).

## Legal
This package does NOT contain the game. You must dump your own copy of the GameCube Twilight Princess
that you legally own.
