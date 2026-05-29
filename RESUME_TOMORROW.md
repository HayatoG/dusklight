# RESUME — Dusklight Switch port, pick up here next session

> Updated 2026-05-29 (or whatever the next session's date is — last edits 2026-05-28).
> Build is GREEN, game boots through `mDoMch_Create`, launcher works, "Iniciar Jogo"
> is unblocked. **CURRENT BLOCKER:** post-save-create LOOP — see "BLOCKER NOW".

---

## ONE-LINE COPY-PASTE FOR NEXT SESSION

```
Continuação Dusklight Switch port. Estamos em loop pós-criação-de-save (file_select).
Lê D:\Projects\dusklight\RESUME_TOMORROW.md + memória [[dusklight-tv-video-reached]]
e [[dusklight-debugging-heuristics]] primeiro. Build env: D:\dusklight-build-nvk;
Switch IP=192.168.1.6 (Sphaira FTP+Netloader).
```

---

## BLOCKER NOW

User clicks **Sim** at "Deseja criar um arquivo para salvar este jogo no cartão de memória?"
1. Sees "Criando…" → "O arquivo para salvar o jogo foi criado" ✓
2. Presses A/B → **loops back to the same "Deseja criar?" prompt**

**Root cause (under investigation):** Our `MemCardStatCheck` Switch branch now checks
`stat("sdmc:/dusklight/saves/zeldaTp.dat")` and routes to `MEMCARDCHECKPROC_LOAD_WAIT`
if the file exists, and our `LoadSyncNAND` reads it back. Patch is in place but the
user hasn't yet tested the final build with this fix (it was the last edit before
the session ended). If the loop persists after the build below ships:

- Verify the file actually exists at `sdmc:/dusklight/saves/zeldaTp.dat` (FTP `ls`).
- If yes, the load path may be returning 2 (size mismatch) — check `sizeof(mData)`
  vs the actual file size; we write `sizeof(mData)` bytes but `MemCardLoadWait`
  loads `sizeof(mSaveData)` bytes which may differ. Print both sizes.
- If file is missing, our `saveNAND` write may have raced or fopen failed
  silently — add a log in saveNAND on `f == nullptr`.
- **The user also suggested the engine might be reading old Dan-era files left
  on the Switch SD** — see "Cleanup Switch SD" section below. Worth doing
  before debugging further.

---

## WHAT'S WORKING AT THE TIME OF THIS WRITE

1. **Engine boots fully** through `mDoMch_Create` + `mDoGph_Create` + all init.
2. **Dusk launcher renders on TV** with real fonts/CSS/logo (romfs bundled).
3. **Joy-Con navigation** works in the launcher (A=confirm/SOUTH, B=cancel/EAST).
4. **"Iniciar Jogo" button** transitions to the game without crashing
   (after the `mDoMemCd_ThdInit` idempotency guard).
5. **Game scene NAME_SCENE / file-select** renders (after JKRAram null-heap guard).
6. **Save creation prompt** appears, user can pick Yes/No, "Criando…" message shows.
   ✗ But then loops (see BLOCKER NOW).

## RECENT FIXES (this session)

Each is `__SWITCH__`-guarded or `__SWITCH__`-equivalent.

| File | Fix | Why |
|---|---|---|
| `src/m_Do/m_Do_MemCard.cpp::ThdInit` | `static bool sAlreadyInited` idempotency guard | Launcher Play button + mDoMch_Create both called it → 2nd OSCreateThread on live thread → std::thread destruct joinable → std::terminate |
| `src/m_Do/m_Do_machine.cpp` | Reverted the (broken) `mCardCommand == COMM_NONE_e` guard I added earlier; kept `is_prelaunch_open()` original | mCardCommand gets reset by worker, so guard was always passing |
| `src/m_Do/m_Do_machine.cpp` | Reverted 256MB heap bump → 32MB original | 256MB > MEM1 available → JKRExpHeap::create returned NULL → null deref |
| `libs/JSystem/src/JKernel/JKRAram.cpp::changeGroupIdIfNeed` | NULL check on `JKRGetCurrentHeap()` return | TLS `sCurrentHeap` is NULL on threads that didn't set it; per-site guard pattern per [[dusklight-debugging-heuristics]] #17 |
| `src/d/d_file_select.cpp::MemCardStatCheck` | When `sdmc:/dusklight/saves/zeldaTp.dat` exists, route to LOAD_WAIT; else keep MAKE prompt | Detect that save was created and exit the create-prompt loop |
| `src/m_Do/m_Do_MemCard.cpp::LoadSyncNAND` | Switch branch reads the file synchronously with `fopen/fread`, returns 1 (success) or 2 (error) | Wii NAND worker path is gated `#if PLATFORM_WII || PLATFORM_SHIELD` and never fires |

(Plus all the earlier-session fixes documented in `[[dusklight-tv-video-reached]]`.)

## DOCS / MEMORY UPDATED THIS SESSION

- `[[dusklight-debugging-heuristics]]` anti-pattern #2 expanded with the
  concrete double-`mDoMemCd_ThdInit` example.
- `[[dusklight-debugging-heuristics]]` new anti-pattern #22 for
  `#if PLATFORM_WII || PLATFORM_SHIELD` fall-through (dispatch-table OOB
  and worker-switch no-op).
- `[[dusklight-tv-video-reached]]` "Progress 2026-05-28" section added.
- `CMakeLists.txt:317` — FIXME comment explaining the TARGET_PC trap.
- `platforms/switch/PLAN_TARGET_PC_AUDIT.md` — NEW doc tracking the
  TARGET_PC / WII-SHIELD migration plan (phases 0-4).
- Skill `dusklight-switch-port` anti-pattern #2 expanded.
- `MEMORY.md` index entry refreshed.

## WORKING-TREE STATE (uncommitted)

`D:\Projects\dusklight`:
- M `.gitignore` `CMakeLists.txt`
- M `include/d/d_event_lib.h` `include/dusk/audio.h`
- M `libs/JSystem/src/JKernel/JKRAram.cpp` `JKRHeap.cpp`
- M `src/Z2AudioLib/Z2AudioMgr.cpp` `Z2SceneMgr.cpp`
- M `src/d/d_event_lib.cpp` `d_file_select.cpp` `d_s_logo.cpp`
- M `src/dusk/ui/graphics_tuner.cpp` `graphics_tuner.hpp` `prelaunch.cpp` `settings.cpp`
- M `src/m_Do/m_Do_MemCard.cpp` `m_Do_machine.cpp` `m_Do_main.cpp`
- M `extern/aurora` (pointer)
- ?? `platforms/switch/` (Switch build infra)
- ?? `RESUME_TOMORROW.md` (this file)

`D:\Projects\dusklight\extern\aurora`:
- M `CMakeLists.txt` `cmake/aurora_core.cmake` `cmake/aurora_dvd.cmake`
- M `include/SDL3/SDL.h` `include/aurora/aurora.h`
- M `lib/aurora.cpp` `lib/imgui.hpp` `lib/window_switch.cpp`
- M `lib/rmlui/WebGPURenderInterface.cpp` `lib/webgpu/gpu.cpp`
- M `lib/switch/tracy_stub/tracy/Tracy.hpp`
- ?? `include/SDL3/SDL_audio.h` `include/imgui.h` `include/misc/`
- ?? `lib/switch/tracy_stub/client/` `lib/switch/tracy_stub/common/`

`D:\switch-nvk`:
- M `winsys/drm_shim.c` (timeline syncobj B-fix from M-DV-1)
- M `mesa-25/src/nouveau/vulkan/nvk_image.c` (diagnostic instrumentation —
  optional, can be reverted)
- ?? `dan-re/` Ghidra workspace + scripts (gitignored already)

## BUILD / DEPLOY RECIPE

Switch IP: `192.168.1.6` (Sphaira running with FTP + Netloader available).
Build dir: `D:\dusklight-build-nvk`.

```bash
# Incremental build
MSYS_NO_PATHCONV=1 docker run --rm \
    -v "D:\Projects\dusklight:/dusklight" \
    -v "D:\dusklight-build-nvk:/dusklight/build-switch" \
    -v "D:\switch-nvk:/switch-nvk" \
    devkitpro/devkita64 \
    bash -c 'cd /dusklight/build-switch && ninja 2>&1 | tail -5'

# Package .nro (bundles res/ as romfs)
MSYS_NO_PATHCONV=1 docker run --rm \
    -v "D:\Projects\dusklight:/dusklight" \
    -v "D:\dusklight-build-nvk:/dusklight/build-switch" \
    devkitpro/devkita64 \
    bash /dusklight/platforms/switch/make-nro.sh

# Send to Switch (Sphaira must be in Netloader)
timeout 130 /c/devkitPro/tools/bin/nxlink.exe -s -a 192.168.1.6 \
    /d/dusklight-build-nvk/dusklight.nro
```

After a crash, with Sphaira FTP on:

```bash
# Pull latest crash report and addr2line it
LATEST=$(curl -sS "ftp://192.168.1.6:5000/sdmc:/atmosphere/crash_reports/" | \
    grep '\.log$' | tail -1 | awk '{print $NF}')
curl -sS "ftp://192.168.1.6:5000/sdmc:/atmosphere/crash_reports/$LATEST" \
    -o /d/dusklight-build-nvk/crash_latest.log

# Compute load base from dusk_switch_log address in runtime log:
#   nm dusklight.elf | grep " dusk_switch_log$"  -> static offset
#   runtime_log "dusk_switch_log@=<addr>"        -> runtime address
#   base = runtime - static_offset
# Then subtract base from every ReturnAddress in crash_latest.log and feed to
# aarch64-none-elf-addr2line -e dusklight.elf -i -f -C <hex>
```

## CLEANUP SWITCH SD (user's suggestion — NOT YET DONE)

The user hypothesized the engine may be reading stale files left by Dan's
public `dusk.nro`. Paths the engine touches on Switch:

- `sdmc:/game/config.json` — Dusklight settings (read at boot via
  `SDL_GetPrefPath()` stub which returns `"sdmc:/game/"`).
  Could be Dan's or ours.
- `sdmc:/game/achievements.json` — Dusklight achievements.
- `sdmc:/game/texture_replacements/` and `texture_dumps/` — optional asset
  override dirs. Dan might have populated.
- `sdmc:/game/USA/`, `sdmc:/game/logs/` — Dusklight-internal.
- `sdmc:/dusklight/saves/zeldaTp.dat` — OUR save file (just added this session).
- `sdmc:/switch/dusk/TLoZ - Princesa do Crepusculo (BR).gcm` — the disc image
  (referenced by `config.json:backend.isoPath`).
- `sdmc:/dusklight.log` and `sdmc:/dusk_stderr.log` — our logs.
- `sdmc:/atmosphere/crash_reports/` — Atmosphere crashes (system path).
- `romfs:/res/...` — inside the NRO, fonts/CSS/logo.

**Safe to wipe on Switch (user-approved):**
- `sdmc:/game/` entirely (Dusklight settings; will regenerate with defaults).
- `sdmc:/dusklight/` (our save; the game will prompt to create on next boot).
- `sdmc:/dusklight.log`, `sdmc:/dusk_stderr.log` (logs).

**KEEP:**
- `sdmc:/switch/dusklight.nro` (our current build) — but re-upload after wipe.
- `sdmc:/switch/dusk/TLoZ - Princesa do Crepusculo (BR).gcm` (the disc).
- `sdmc:/atmosphere/` (CFW).
- Everything else under `sdmc:/switch/` (other homebrews).

If wiping doesn't fix the save-create loop, the issue is in our code (most
likely `LoadSyncNAND` returns 2 because the size or path mismatches), not stale
config. Either way, do the wipe BEFORE iterating further — eliminates one
variable.

## KEY MEMORIES TO READ

- `[[dusklight-tv-video-reached]]` — overall milestone state + checklist.
- `[[dusklight-debugging-heuristics]]` — 22 anti-patterns; #2, #17, #18, #20, #22
  most relevant to this current scene.
- `[[dusklight-clean-build-fixes]]` — the 6 load-bearing CMake fixes since
  the 2026-05-28 reset.
- `[[dusklight-switch-target-pc-convention]]` — `TARGET_PC` semantics.
- `platforms/switch/PLAN_TARGET_PC_AUDIT.md` — long-term plan for the
  TARGET_PC carve-out.

## NOT DONE THIS SESSION

- Commit anything (3 working trees still uncommitted).
- Verify the latest `LoadSyncNAND` + STAT_CHECK fix on the TV.
- Strip instrumentation logs (still verbose).
- JStudio TParse aarch64 fix (cutscenes remain skipped via the d_s_logo.cpp
  OPENING_SCENE→NAME_SCENE workaround).
- Real audio (still all-stub).
- Wipe stale Dan-era files from Switch SD.
