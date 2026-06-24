# RESUME — Community-feedback batch (read this first, 2026-06-24 night handoff)

Branch **`switch-port/community-feedback`** in BOTH repos (`dusklight` + submodule `extern/aurora`).
**Nothing committed. Nothing built since the last edits.** User went to sleep; we test tomorrow.

## ⚠️ DO THIS FIRST TOMORROW: BUILD (it was NOT built)
The last edits (#7 shadows, #9 autosave, #3 gyro real-fix) are **in the working tree, UNCOMPILED**.
Two of them touch widely-included headers AND change a struct layout, so a normal `build-only` may leave
stale/mismatched objects (the Docker bind-mount mtime is unreliable — we hit this repeatedly today).

**Force an ABI-wide recompile, then build:**
```bash
# 1) touch the changed headers INSIDE the container so Ninja definitely rebuilds dependents
docker run --rm -v D:\Projects\dusklight:/dusklight -v D:\dusklight-build:/dusklight/build-switch \
  devkitpro/devkita64 bash -c "touch /dusklight/include/dusk/settings.h /dusklight/include/d/d_com_inf_game.h \
  /dusklight/src/dusk/autosave.cpp /dusklight/src/dusk/gyro.cpp /dusklight/src/dusk/settings.cpp \
  /dusklight/src/dusk/ui/settings.cpp /dusklight/platforms/switch/src/switch_stubs.cpp && echo touched"
# 2) build (settings.h added a field -> ABI -> expect a broad recompile, ~1400 objs)
bash /d/Projects/dusklight/platforms/switch/build-docker.sh build-only
# 3) package + deploy
docker run --rm -v D:\Projects\dusklight:/dusklight -v D:\dusklight-build:/dusklight/build-switch \
  devkitpro/devkita64 bash /dusklight/platforms/switch/make-nro.sh
/c/devkitPro/tools/bin/nxlink.exe -s -r 20 -a <SWITCH-IP> /d/dusklight-build/dusklight.nro
```
Switch IP was **192.168.1.9** today (DHCP drifts — re-check; broadcast nxlink without `-a` did NOT work,
must use `-a <ip>`). Sphaira netloader must be open. Read the full nxlink log.

**Build risk to watch:** `autosave.cpp` and `gyro.cpp` were EXCLUDED on Switch and are now built. If the
build fails on either, read the error. autosave's only known blocker was a dead `#include
"imgui/ImGuiConsole.hpp"` (removed). gyro.cpp has clean includes (no imgui/SDL) — reason for its old
exclusion is unknown, so it's the higher-risk one. Worst case: re-add that one file's
`list(FILTER DUSK_FILES EXCLUDE REGEX ...)` line in CMakeLists.txt + restore its stub block in
`switch_stubs.cpp`, and ship the rest.

---

## What was implemented THIS session but is UNBUILT/UNTESTED

### #7 — Shadows OFF (new toggle) — files changed:
- `include/dusk/settings.h` — added `ConfigVar<bool> disableShadows;` (after `disableWaterRefraction`).
- `src/dusk/settings.cpp` — `.disableShadows {"game.disableShadows", false}` + `Register(...)`.
- `include/d/d_com_inf_game.h` — gated `dComIfGd_drawShadow` + `dComIfGd_imageDrawShadow` with
  `#ifdef TARGET_PC if (!disableShadows) {...}` (mirrors `disableWaterRefraction`). Both passes (producer
  `imageDraw` + consumer `draw`) are gated. Shadow-map texture is private to `d_drawlist.cpp` and sampled
  nowhere else, so skipping is crash-free. (Do NOT use `shadowResolutionMultiplier==0` for off — 0×0
  texture / `GXCreateFrameBuffer(0,0)` would crash; that's why this is a separate bool.)
- `src/dusk/ui/settings.cpp` — "Disable Shadows" `config_bool_select` added right after "Shadow
  Resolution" in the Graphics section.
- **Test:** Settings → Graphics → "Disable Shadows" → shadows vanish, fps up. Toggle live.

### #9 — Autosave (make it actually work) — files changed:
- Root cause found: `autosave.cpp` was EXCLUDED on Switch (CMakeLists) and replaced by 3 empty stubs →
  the existing "Autosave" menu toggle did nothing.
- `CMakeLists.txt` — removed the `autosave.cpp` (and `gyro.cpp`) EXCLUDE FILTER lines.
- `platforms/switch/src/switch_stubs.cpp` — removed the 3 autosave stubs (`toggleAutoSave/updateAutoSave/
  triggerAutoSave`) → real defs from `autosave.cpp` link now.
- `src/dusk/autosave.cpp` — removed the dead `#include "imgui/ImGuiConsole.hpp"` (the only Switch blocker).
- `src/dusk/settings.cpp` — **`game.autoSave` now defaults TRUE on Switch** (`#ifdef __SWITCH__`). ⚠️
  DECISION TO CONFIRM: autosave fires on area transitions and writes the memcard — default-on means it
  saves unprompted. If that's unwanted, flip back to `false` (the toggle still works either way). The
  write path is the proven `g_mDoMemCd_control.save()` (same as manual save, OSWaitCond-fixed).
- **Test:** with autosave on, cross an area boundary → confirm it saves (log `[mc]`/store) and the
  post-save message screen does NOT hang. The `"autosave"` toast may have no RML template (cosmetic).

### #3 — Gyro REAL fix — ⚠️ earlier "validated/done" was WRONG
- `gyro.cpp` was ALSO excluded on Switch and stubbed (`switch_stubs.cpp` `getAimDeltas` returned 0,0) →
  gyro did NOTHING despite the toggle + the default-on flip I made earlier. So #3 was never actually
  working on HW.
- `CMakeLists.txt` — un-excluded `gyro.cpp` (same line as autosave).
- `platforms/switch/src/switch_stubs.cpp` — removed the `dusk::gyro` stub namespace block.
- gyro.cpp reads the Switch six-axis via aurora `pad_switch` (`PADGetSensorData` etc., already compiled)
  and feeds Link's aim. Default-on was set earlier in `settings.cpp` (`enableGyroAim`/`enableGyroRollgoal`
  true on Switch).
- **Test:** in-game, aim the bow / enter look mode → the reticle should now move with the console tilt.
  If gyro.cpp fails to build, that's the build risk noted above.

---

## Already DONE earlier this session + HW-VALIDATED (on this branch, uncommitted)
- Disc auto-detect rejects non-discs (size+magic in the Switch `iso::inspect` stub) and finds `game.gcm`.
- **Cache CANTOPEN fixed**: strip `sdmc:` prefix before `sqlite3_open_v2` in `gpu_cache.cpp` +
  `pipeline_cache.cpp` (the unix VFS mangled the devoptab path). Warm cache loads now.
- **Single folder**: SDL prefPath shim → `sdmc:/TwilitRealm/Dusklight/`; logs moved into the folder;
  self-provision seed (config + caches bundled in `romfs:/seed/`, restored per-file on boot via manual
  `copy_file_raw` — `std::filesystem::copy_file` is ENOSYS on libnx).
- **Save migration** (#4): `card.cpp` copies a legacy save from `sdmc:/aurora` etc. into the data folder.
- **Boost**: "CPU + GPU Boost" moved below the FPS toggle; new **"Boost+ (overclock)"** (CPU 1785 / GPU
  921 / EMC 1600) — HW-confirmed 921+1600 applied (user has sys-clk-OC). The two are **mutually
  exclusive**, and turning the active one off returns to stock. `set_boost(boost, boostPlus)` in
  `switch_perf.cpp` (tier 0/1/2), persisted + applied at boot (`m_Do_main`).
- **Release packaging** (#5): `platforms/switch/package-release.sh` + `platforms/switch/release/*`
  (single-folder, self-provision; no `game/` folder / no `data_location.json`).

## GitHub issues (HayatoG/dusklight) status
- ✅ #3 gyro (real fix this turn, UNTESTED) · ✅ #4 save migration · ✅ #5 single folder · ✅ #6 disc scan
  — all on the branch, validated except gyro's un-exclude.
- ⏳ #7 shadows off (implemented, unbuilt) · ⏳ #9 autosave (implemented, unbuilt)
- ❌ #10 mipmap (still just needs a visual HW check; code is fine — likely close as invalid)
- ❌ #8 pipeline-cache-clean-on-game-change (optional, not started)
- ❌ **#11 multi-language UI (PT/EN/ES) — NOT STARTED** (see below)

## #11 — multi-language UI — plan ready, not started
Design (confirmed feasible; `Rml::String == std::string`, so `tr("key")` is a drop-in at every
`.key/.helpText/.text/.title` call site):
1. New module `src/dusk/i18n.{hpp,cpp}` (add to `files.cmake` near `config.cpp`): `i18n::load(lang)`
   reads `romfs:/res/lang/<code>.json` (nlohmann/json, via SDL_IOFromFile/fopen) into a map;
   `tr(key)` returns active → English fallback → key.
2. Setting: add `ConfigVar<int> uiLanguage` to `backend` (NOT `GameLanguage` — that's disc text and
   lacks Portuguese). Selector in the Interface tab; live-reload on change. Call `i18n::load()` in
   `m_Do_main` right after `config::LoadFromUserPreferences()`.
3. Strings live in `res/lang/{en,pt,es}.json` → ship as `romfs:/res/lang/*.json` automatically
   (make-nro symlinks `res/`; **path is `res/lang/`, the symlink nests under `res/`**).
4. ~230 strings across `ui/settings.cpp` (~180), `ui/prelaunch.cpp` (~35), `ui/overlay.cpp`,
   `ui/menu_bar.cpp`. Migrate prelaunch + menu_bar FIRST (most visible, proves the pipeline), then
   settings.cpp tab-by-tab. The 7 `constexpr const char*` label arrays in settings.cpp:55-98 need a
   small index helper. Watch `fmt::format` strings (keep placeholders) and return a stable ref from
   `tr()`. Effort ≈ 4.5–5.5 days of migration.

## Other
- `platforms/switch/PERF_BACKLOG.md` — created: triple-buffer WSI (minImageCount 2→3) + DRS, the two
  biggest FPS levers. **No GitHub issue yet** (user request).
- **Commit plan** (when validated, with user OK — standing rule): aurora submodule first (SDL prefPath,
  card.cpp migration, gpu_cache/pipeline_cache strip), then dusklight (everything else) + bump submodule
  pointer. Push only to HayatoG forks. Don't commit the EOL churn in aurora (only `git add` the real files).
- Diary entry for today still pending (`D:\dusklight-diario\DIARIO.md`).
