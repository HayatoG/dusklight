# 👉 RESUME HERE — Switch port (GLES backend)

**Branch:** `switch-port/deko3d-backend`. Session end 2026-05-25 (GLES bring-up). Local commits only — **NOT pushed**.

## 🎯 STATE: GLES backend proven on HW; engine boots THROUGH to the renderer. Next = implement the GLES gfx layer (G2).

The graphics backend was pivoted **deko3d → native Aurora Mesa-GLES** (runtime shader compile, no precompile/coverage tail; reusable for other Switch ports). See `PLAN_GLES.md` + memory `[[dusklight-gles-backend-plan]]`. Build defaults to GLES now (`DUSK_GLES=ON DUSK_DEKO=OFF DUSK_RMLUI=OFF`; toggles in build-docker.sh).

### ✅ Done this session
- **G0** — `egl_test` re-proven: switch-mesa/nouveau = **GLES 3.2, `GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS=16`** (vertex-pulling GO), 128MB SSBO, `GL_EXT_buffer_storage`/`GL_KHR_parallel_shader_compile`/`GL_OES_get_program_binary`.
- **G1** — native GLES backend `extern/aurora/lib/gles/gpu.cpp` (EGL→ES3.1→clear present), aurora.cpp seams (`DEKO3D||GLES`), gpu.hpp, aurora_core.cmake (`AURORA_BACKEND_GLES`), build-docker.sh. **PROVEN ON REAL TEGRA** (user saw BLUE clear on the TV; `[aurora::webgpu(gles)] OpenGL ES 3.2 Mesa 20.1.0-rc3 ... gles_initialize: ALL OK`).
- **Boot cascade cleared (backend-agnostic null-guards), engine now boots THROUGH to gameplay + the renderer:**
  1. TLS — `sCurrentHeap` `tls_model("initial-exec")` not global-dynamic (JKRHeap.cpp:50).
  2. audio — `Z2SceneMgr::loadSeWave/loadBgmWave` `#if DUSK_AUDIO_DISABLED return false`.
  3. fade — `fopOvlpM_SceneIsStart/SceneIsStop` null `overlap_task` guard.
  4. vibration — `mDoCPd_c` 5 motor accessors null-guard `m_gamePad[]` (m_Do_controller_pad.h).
  5. kankyo/player — `daPy_py_c::checkNowWolfEyeUp()` (d_a_player.cpp:454) null-guards `daAlink_getAlinkActorClass()` (fixes the whole checkNowWolf* family).
  6. camera/Midna — `dCamera_c::nextType` (d_camera.cpp:1953) null-guards `getMidnaActor()`.
- **RESULT: player (ALINK) SPAWNS, BG_OBJ/tags load, 25+ draw tags REGISTER, and the GX FIFO drains 211 KB/frame** (`[fifo] drain this=211856`) = the gameplay scene EMITS REAL DRAWS (the old `gx=0` is resolved).

### ✅ G2 STEP 1 DONE: engine boots to TITLE on GLES + GX capture works (gx=1). ❌ but NOTHING visible (replay is log-only).
The boot cascade + the per-site gfx guards are done. On real HW the engine now boots THROUGH to the title scene (F_SP102), runs steadily (frame 23+), `[title] Draw` fires, and **the GX capture records draws: `[gfx-deko] frame=N passes=4 draws=1 gx=1`**. Guards that got us here: texture.cpp 5 stubs; resolve_pass skips its uniform push; **`pipeline_cache_memory.cpp find_pipeline_impl` returns hash-only on GLES** (the first-GX-draw crash). **HONEST: the TV shows ONLY the GLES blue clear — the captured draws are NOT drawn because `gfx::deko_replay` is LOG-ONLY (no glDrawElements yet).**

### 🔨 CURRENT BLOCKER = (a) a ByteBuffer push crash + (b) no actual GLES draw
- **(a) `push_storage`/`push_uniform` → `ByteBuffer::append_zeroes` → `memset` @0x0** (common.hpp:111/162) from `handle_draw_unmerged` (command_processor.cpp:1637) on certain draws (shadow shape, GX cmd=0x98). ROOT: a **UINT32_MAX sentinel length** reaches `push` (the frame stats show `uniBytes=4294967295` = the g_uniforms length sentinel) → `resize(huge)` → `realloc` null → `memset(0x0)`. **FIX: guard `push`/`map` (common.cpp ~1135-1163) against a UINT32_MAX/absurd length (treat as 0 / skip the push).**
- **(b) the replay is log-only → no pixels.**

### ▶ NEXT (the pixel-producing work — write the replay from the REAL S2DUMP layout):
**S2DUMP (captured): `draw#0 vtx=16 idx=24 stride=12 vert{s=192} idx{s=48} uni{s=UINT32_MAX}`** → each vertex = **3 floats XYZ, BIG-ENDIAN** (byteswap in shader); indices = **uint16 triangles**; **NO per-draw uniform** (uni=UINT32_MAX) → the MVP/transform source is the OPEN question (positions are large, e.g. x=608 → need a transform to reach clip space; the title may be 2D/ortho).
1. Fix the push UINT32_MAX guard (a).
2. `gfx::gles_initialize`: create GL SSBO (g_verts), EBO (g_indices), UBO (g_uniforms).
3. `gfx::gles_replay`: per captured GX draw — upload buffers, `glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,vbo)`, a runtime-compiled GLSL ES program (vertex: read 3 floats by `gl_VertexID*stride` + `bswap`→`gl_Position`, applying MVP if a valid uniform exists else identity/ortho; fragment: flat color), `glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, idxOffset)`. → first flat geometry on screen (G2 milestone).
4. Iterate: real GX→GLSL shader (G3), textures (G4), pipeline state/EFB (G5).
Reference the wgpu vertex-pull shader (`lib/gx/shader.cpp` byte-pull helpers ~1380-1426) for the exact byteswap/addressing.

## HW workflow (unchanged — heuristic #19)
Build (`DUSK_GLES=ON DUSK_DEKO=OFF DUSK_RMLUI=OFF bash platforms/switch/build-docker.sh build`) → **WAIT for real completion (docker idle / task notification), verify build.log no FAILED + ELF relinked + the edited .o compiled** → strip+elf2nro **WITH `--romfsdir`** → **verify `.nro` mtime NEWER than `dusklight.elf`** (I shipped stale .nro twice — heuristic #16) → `nxlink -s -r 25 -a 192.168.1.7 dusklight.nro` (Sphaira open, Windows Firewall rule for nxlink.exe) → on crash, pull `ftp://192.168.1.7:5000/atmosphere/crash_reports/<newest>.log` (reopen Sphaira after each crash) → `aarch64-none-elf-addr2line -ifCe dusklight.elf 0xOFFSET` (against the SAME elf the .nro was built from). Disc = `sdmc:/switch/dusk/TLoZ - Princesa do Crepusculo (BR).gcm` (loads as GZ2E01; auto-boots since launcher/RmlUi is off).

Dusklight flow: launcher (RmlUi) → game. RmlUi is OFF in the GLES build (launcher skipped, disc auto-boots) — RmlUi-GLES interface is G6.

See `BUILD_STATUS.md`, memories `[[dusklight-switch-build-status]]`, `[[dusklight-debugging-heuristics]]` #16-20, `[[dusklight-gles-backend-plan]]`, and `PLAN_GLES.md`.
