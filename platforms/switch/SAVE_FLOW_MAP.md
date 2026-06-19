# Save flow map — Dusklight per platform

> Written 2026-05-28 mid-session. Purpose: stop guessing which path runs on
> Switch. The decomp gates code by `PLATFORM_GCN | PLATFORM_WII | PLATFORM_SHIELD`,
> the Dusk fork adds `TARGET_PC`, and our Switch build sets `VERSION=0` so
> **PLATFORM_GCN is TRUE on Switch**. That single fact decides everything.

---

## ⚠️ UPDATED 2026-06-18 — the save path MOVED; save + read WORK (HW-verified)

Everything below this banner is HISTORY: the `sdmc:/aurora/...` path was correct in May, but two
things changed, so the live path is different now.

1. **The memcard dir now follows `g_config.userPath`, not `configPath`.** aurora commit
   `3643a36 "Split up configPath -> userPath/cachePath"` changed `CARDInit`'s `cardWorkingDir`
   source from `g_config.configPath` → `g_config.userPath` (`card.cpp:176-187`). `GetCardFullPath`
   is unchanged: `userPath / GetCardRegion() / "Card A"`. The engine STILL only calls
   `CARDSetLoadType` + `CARDInit` (no `CARDDetectDolphin`/`CARDSetBasePath`), so the fallback fires
   and `ResolveDolphinCardPath` (the `sdmc:/game/GC/...` one) is NOT used.
2. **`userPath` now points at the data dir, redirected by `data_location.json`.** dusk sets
   `config.userPath = dusk::data::initialize_data().userPath` (`m_Do_main.cpp:643`). With the bundled
   `sdmc:/game/data_location.json` = `{"mode":"custom","customPath":"/TwilitRealm/Dusklight"}` (added
   in dusklight `b931b0ca5a`, 2026-06-18 "cache/config fixes"), `resolve_data_path` returns the custom
   path. So `userPath = /TwilitRealm/Dusklight`.

→ **The save TODAY lives at `sdmc:/TwilitRealm/Dusklight/USA/Card A/01-GZ2E-gczelda2.gci`** — NOT
`sdmc:/aurora/...`. Because the memcard and the shader cache SHARE `userPath`, redirecting it for
the cache (the config-13 "modelos bonitinhamente" fix) also moved the save.

**SAVE + READ WORK (HW-verified 2026-06-18 via the on-device HTTP server).** Both `.gci` confirmed at
32832 bytes: `sdmc:/TwilitRealm/Dusklight/USA/Card A/` (new, live) and `sdmc:/aurora/USA/Card A/`
(old, orphaned). The user's "it never reads my save" was the OLD save being orphaned by the userPath
move — NEW saves round-trip fine. `CardGciFolder` writes via `SDL_IOStream` = real
`fopen`/`fwrite`/`fflush`/`fclose` (SDL3 shim in `extern/aurora/include/SDL3/SDL.h`) and persists. To
migrate old progress: copy `sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci` → the new path.

**⚠️ TOOLING GOTCHA (cost most of the session):** Sphaira FTP AND DBI FTP — both via `curl` — returned
an EMPTY listing for `Card A/` and 0 bytes on the `.gci` download. FALSE NEGATIVE: curl/FTP can't
`CWD`/`RETR` a path with a **space** in it ("Card A"). I wrongly concluded "not saving" twice (even
"two independent FTP servers confirm"). The on-device **HTTP server** (`Card%20A`) showed the truth
instantly. **Census Switch files via HTTP, not curl-FTP, when a path has a space.** Logged in
[[dusklight-debugging-heuristics]].

---

## TL;DR — what actually runs on Switch

We compile with `-DVERSION=0` → `VERSION_GCN_USA` → `PLATFORM_GCN=1`.
`PLATFORM_WII=0`, `PLATFORM_SHIELD=0`. Plus `-DTARGET_PC` and `-D__SWITCH__`.

So the save path on Switch is the **GameCube path**, NOT the Wii NAND path.

```
file_select Yes → dataSave() [PLATFORM_GCN]
  → mDoMemCd_save()              (include/m_Do/m_Do_MemCard.h:151, inline)
  → mDoMemCd_Ctrl_c::save()      (src/m_Do/m_Do_MemCard.cpp:293)
      • memcpy buffer → mData
      • mCardCommand = COMM_STORE_e
      • signal mCond                                  ← async kick
file_select MakeGameFile polls mDoMemCd_SaveSync()
  • returns 0 while async work in flight
  • returns 1 when mCardState == WRITE_e             ← worker progressed
  • returns 2 on error
file_select STAT_CHECK reads mDoMemCd_getStatus(0)
  • status==2 (READY)  → case 2: mDoMemCd_Load() → LOAD_WAIT → proceeds
  • status==1 (NO_FILE) → MAKE prompt
  • status==0 (NO_CARD) → "no card" error
```

Worker thread (`mDoMemCd_Ctrl_c::main`, line 147) picks up `COMM_STORE_e` and
calls `store()` (line 304). `store()` is the one that issues the actual
`CARDCreate`/`CARD_OPEN`/`mDoMemCdRWm_Store`/`CARDClose` sequence.

The bytes that hit disk come from **Aurora's memcard emulation** in
`extern/aurora/lib/dolphin/card.cpp` + `extern/aurora/lib/card/` (Kabukibu /
gci-folder backend).

## Per-platform table

`PLATFORM_GCN | PLATFORM_WII | PLATFORM_SHIELD | TARGET_PC | __SWITCH__`
combinations actually used in `dataSave`-and-below:

| File:Line | What | GCN | Wii | Shield | PC (Win/Mac/Lin) | Switch (our build) |
|---|---|---|---|---|---|---|
| `src/d/d_file_select.cpp:5458 dataSave()` | branches | calls `mDoMemCd_save` | calls `mDoMemCd_saveNAND` | calls `mDoMemCd_saveNAND` | calls `mDoMemCd_save` (TARGET_PC+VERSION=0) | **calls `mDoMemCd_save`** |
| `src/d/d_file_select.cpp:4274 MemCardStatCheck` | branches | GCN switch on `getStatus(0)` | `loadFileNAND()` (returns early) | GCN switch (no Shield branch) | GCN switch | **GCN switch** (after our revert) |
| `include/m_Do/m_Do_MemCard.h:151 mDoMemCd_save` | inline wrapper | exists (no gate) | exists (no gate) | exists (no gate) | exists | exists |
| `include/m_Do/m_Do_MemCard.h:208 mDoMemCd_saveNAND` | inline wrapper | `#if WII\|\|SHIELD` — **absent on Switch** | exists | exists | absent | **absent** |
| `src/m_Do/m_Do_MemCard.cpp:293 save()` | impl | runs | runs | runs (mutex+queue) | runs | **runs** |
| `src/m_Do/m_Do_MemCard.cpp:304 store()` | gated `#if !PLATFORM_SHIELD` | runs (CARDCreate "gczelda2") | runs (CARDCreate "zeldaTp.dat") | absent | runs | **runs (uses Aurora's GCI-folder backend)** |
| `src/m_Do/m_Do_MemCard.cpp:472 attach()` | gated `#if !PLATFORM_SHIELD` | CARDProbeEx → mount → loadfile → checkspace | same | absent | same | **same** |
| `src/m_Do/m_Do_MemCard.cpp:698 saveNAND()` | gated `#if WII\|\|SHIELD` (lines 620-983) | absent | runs | runs | absent | **absent** ← my earlier instrumentation here was dead code |
| `src/m_Do/m_Do_MemCard.cpp:722 storeNAND()` | gated same | absent | runs (NANDCreate / NAND_OPEN / mDoMemCdRWm_StoreBannerNAND / mDoMemCdRWm_StoreNAND) | runs | absent | **absent** |
| `src/m_Do/m_Do_MemCard.cpp:125 init` | sets `mCardCommand = COMM_ATTACH_e` if TARGET_PC else COMM_NONE_e | NONE (vanilla) | NONE | NONE | **ATTACH** | **ATTACH** (auto-mounts at boot) |
| `src/m_Do/m_Do_MemCard.cpp:165 worker switch` | dispatches commands | `STORE_e` → `store()`, `RESTORE_e` → `restore()`, `ATTACH_e` → `attach()` | same + STORE_NAND_e → storeNAND | only NAND cases | same as GCN | **same as GCN** |
| `extern/aurora/lib/dolphin/card_switch.cpp` | full no-op stubs | (not built) | (not built) | (not built) | (not built — `aurora_card.cmake` lists `card.cpp` only) | **NOT BUILT** ← red herring, ignore the file |
| `extern/aurora/lib/dolphin/card.cpp` | real CARD impl | runs on Linux/Mac/Win | runs | runs | **runs** | **runs** |

### Aurora `card.cpp` paths

```
aurora::g_config.configPath = "sdmc:/aurora"            ← Switch fallback (lib/aurora.cpp:151)
                              (only if user didn't pass it; Dusklight does NOT pass configPath,
                               so the fallback fires)
GetCardFullPath(working, SlotA) = "sdmc:/aurora/USA/Card A"
                                  ^^^^^^^^^^^ depends on game ID 'E' (3rd byte) → "USA"
EnsureCardStorageDirectory: std::filesystem::create_directories("sdmc:/aurora/USA/Card A")
CARDCreate(slot, "gczelda2", 524288) → writes `<maker>-<game>-<name>.gci`
                                   → "01-GZ2E-gczelda2.gci" (32832 bytes on disk after store)
```

**Confirmed on hardware** 2026-05-28 22:35: file present at
`sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci` size 32832 bytes ✓.

## Where Dan's stale `.gci` lived

`sdmc:/game/USA/Card A/01-GZ2E-gczelda2.gci` — Dan's dusk.nro must have passed
`configPath = "sdmc:/game"` explicitly. We don't. Ours uses Aurora's
`sdmc:/aurora` fallback. The two paths don't talk to each other.

We deleted Dan's tree during cleanup, which was correct (it wasn't ours).
The recreated `sdmc:/game/USA/Card A` and `Card B` we made earlier this
session are *irrelevant* to our build — Aurora writes under `sdmc:/aurora`.
Leave them empty; harmless.

## Where the loop user reported came from

User flow:
1. STAT_CHECK on first entry → `getStatus(0)` returns 1 (NO_FILE) — Aurora's
   `attach()` couldn't find an existing .gci → mCardState=NO_FILE.
2. GCN switch case 1: not present — there's no `case 1:` in MemCardStatCheck.
3. **My earlier `#ifdef __SWITCH__` override** at line 4285-4310 hijacked the
   whole function: it `stat()`-ed `sdmc:/dusklight/saves/zeldaTp.dat` (a
   path I invented assuming Wii flow). That file never existed because the
   Wii path was never wired up; on the GCN path that file is irrelevant.
4. Override fell through to "no save → MAKE prompt".
5. User clicks Yes → dataSave→save→worker→store→Aurora CARDCreate succeeds
   → `sdmc:/aurora/USA/Card A/01-GZ2E-gczelda2.gci` written.
6. mCardState=WRITE_e → SaveSync=1 → "Arquivo criado" shown.
7. MakeGameFileCheck → MEMCARDCHECKPROC_STAT_CHECK (with my override still
   intercepting).
8. Override stat-s `sdmc:/dusklight/saves/zeldaTp.dat` again → still missing
   → MAKE prompt **again** → user reports the loop.

The fix is to remove the override entirely and let the GCN switch run.
`getStatus(0)` now returns 2 (READY, mCardState was set to READY by store
finishing) → `case 2:` → `mDoMemCd_Load()` → LOAD_WAIT → restore() reads
the .gci → `mCardState=READ` → LoadSync=1 → data_select.

## What Aurora's GCI-folder backend writes

`extern/aurora/lib/card/CardGciFolder.cpp:88`:

```cpp
std::string gciFilename = fmt::format("{}-{}-{}.gci", m_maker, m_game, filename);
```

Where `m_maker = "01"` (Nintendo), `m_game = "GZ2E"` (TP USA), `filename =
"gczelda2"` (passed by store()). So → `"01-GZ2E-gczelda2.gci"`. Exactly what
we see on disk.

The file is a standard Dolphin `.gci` (32 KB header + payload). Total 32832
bytes = 64 sectors × 512 byte blocks (GCN memcard block size).

## Cross-reference for future bugs

If save behavior breaks again, decide which layer to debug from:

1. **Engine state machine** — `dFile_select_c::*` in `src/d/d_file_select.cpp`.
   State enum `MEMCARDCHECKPROC_*`. Look for `mCardCheckProc =` assignments.
2. **Engine memcard control object** — `mDoMemCd_Ctrl_c` in
   `src/m_Do/m_Do_MemCard.cpp`. State enum `CARD_STATE_*`.
   `getStatus(0)` translates internal state → external (engine-visible) code.
3. **Worker thread** — `mDoMemCd_Ctrl_c::main` (line 147). Cmd enum `COMM_*`.
   Async kicks from engine; sync read-out via `*Sync()` functions.
4. **Aurora dolphin wrapper** — `extern/aurora/lib/dolphin/card.cpp`.
   `CARDCreate`/`CARDProbeEx`/`CARDMount`/`CARD_OPEN`/`CARDWrite`/
   `CARDClose`/etc. Maps to `aurora::card::ICard` (next layer).
5. **Aurora gci-folder impl** — `extern/aurora/lib/card/CardGciFolder.cpp`.
   Writes actual `.gci` files via `std::filesystem`.

## Heuristics worth saving (will fold into [[dusklight-debugging-heuristics]])

- **VERSION=0 trap**: on Switch we set `-DVERSION=0` (GCN-USA) so
  `PLATFORM_GCN=1`. Any `#if PLATFORM_WII || PLATFORM_SHIELD` block is
  dead code on Switch. Conversely, any `#if PLATFORM_GCN` block IS LIVE.
  Before patching a state machine on Switch, grep for the matching
  `PLATFORM_*` gates and confirm which branch our build takes. Mis-guessing
  this cost me a full session of "saveNAND" instrumentation that was never
  reached.
- **Aurora `g_config.configPath` defaults to `sdmc:/aurora` on Switch** when
  Dusklight doesn't pass it (which it doesn't — only `userPath`/`cachePath`
  are set). Memcard files therefore live under `sdmc:/aurora/USA/Card A/`,
  NOT under any "game"/"dusklight"/"saves" tree.
- **Stat-on-the-wrong-path looks like a save loop**. If "Arquivo criado"
  shows but STAT_CHECK keeps re-prompting, suspect a stat() against the
  wrong file before suspecting the write itself.
