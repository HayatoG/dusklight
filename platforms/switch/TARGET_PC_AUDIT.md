# TARGET_PC audit — full bottom-up enumeration

> Built over 4 rounds with different search patterns to ensure complete
> coverage. Companion to the higher-level `PLAN_TARGET_PC_AUDIT.md`.
> The root anti-pattern: Dusklight's CMake defines `TARGET_PC` on ALL
> non-original-GameCube builds **INCLUDING the Switch port**, so any
> `#if TARGET_PC` block is LIVE on our Switch build. Anything that
> assumes PC-class behavior (mouse, OpenGL, std::thread, dlopen,
> SDL_ShowFileSelect, etc.) will compile-and-run on Switch but fail at
> runtime — often silently.
>
> Treat this file as a *catalog*, not a fix list. Use it when planning a
> targeted patch (find your subsystem here → check each site → either
> guard with `__SWITCH__` or generalize the macro).

## Round-by-round methodology

| Round | Pattern (regex) | New sites added | Cumulative |
|---|---|---|---|
| 1 | `^#if TARGET_PC` (line-start) | 828 | 828 |
| 2 | `^#ifdef TARGET_PC` / `^#ifndef TARGET_PC` / `^#elif TARGET_PC` / `^# if TARGET_PC` | TBD | TBD |
| 3 | `TARGET_PC` in C++ expressions (`if (TARGET_PC)`, `defined(TARGET_PC)`, `&&`/`||` etc.) | TBD | TBD |
| 4 | any remaining line referencing `TARGET_PC` | TBD | TBD |

Sites are tallied as **(occurrences in file)** below.

---

## Round 1 — `^#if TARGET_PC` (line-start)

828 occurrences across 288 files. Top hotspots (≥6 occurrences):

### Game-side engine (`src/d/`)
| File | Count |
|---|---|
| `src/d/actor/d_a_alink.cpp` | 30 |
| `src/d/actor/d_a_movie_player.cpp` | 45 |
| `src/d/actor/d_a_mant.cpp` | 9 |
| `src/d/actor/d_a_mg_fshop.cpp` | 8 |
| `src/d/actor/d_a_b_gnd.cpp` | 8 |
| `src/d/actor/d_a_alink_hook.inc` | 7 |
| `src/d/actor/d_a_e_wb.cpp` | 7 |
| `src/d/actor/d_flower.inc` | 7 |
| `src/d/d_drawlist.cpp` | 9 |
| `src/d/d_kankyo_rain.cpp` | 9 |
| `src/d/d_camera.cpp` | 9 |
| `src/d/d_menu_ring.cpp` | 11 |
| `src/d/d_menu_fmap2D.cpp` | 10 |
| `src/d/d_save.cpp` | 7 (+ header `include/d/d_save.h`: 8) |
| `src/d/d_stage.cpp` | 7 |
| `src/d/d_menu_dmap.cpp` | 7 |
| `src/d/d_map_path_dmap.cpp` | 7 |
| `src/d/d_menu_window.cpp` | 6 |
| `src/d/d_menu_collect.cpp` | 6 |
| `src/d/d_name.cpp` | 6 |
| `src/d/d_meter_string.cpp` | 5 |
| `src/d/d_msg_class.cpp` | 5 |
| `src/d/d_msg_scrn_base.cpp` | 5 |
| `src/d/d_msg_scrn_howl.cpp` | 5 |
| `src/d/d_msg_scrn_place.cpp` | 4 |
| `src/d/d_demo.cpp` | 5 |
| `src/d/d_s_play.cpp` | 5 |
| `src/d/d_s_logo.cpp` | 5 |
| `src/d/d_event_data.cpp` | 5 |
| `src/d/d_menu_save.cpp` | 4 |
| `src/d/d_menu_fmap.cpp` | 5 |
| `src/d/d_bg_s.cpp` | 5 |
| `src/d/d_attention.cpp` | 2 |
| `src/d/d_vibration.cpp` | 1 |

### Framework (`src/m_Do/`, `src/f_pc/`, `src/f_op/`, `src/f_ap/`)
| File | Count |
|---|---|
| `src/m_Do/m_Do_graphic.cpp` | 17 |
| `src/m_Do/m_Do_ext.cpp` | 15 |
| `src/m_Do/m_Do_machine.cpp` | 6 |
| `src/m_Do/m_Do_printf.cpp` | 6 |
| `src/m_Do/m_Do_main.cpp` | 2 |
| `src/m_Do/m_Do_MemCard.cpp` | 2 |
| `src/m_Do/m_Do_MemCardRWmng.cpp` | 1 |
| `src/m_Do/m_Do_dvd_thread.cpp` | 1 |
| `src/m_Do/m_Do_lib.cpp` | 1 |
| `src/f_pc/f_pc_node.cpp` | 2 |
| `src/f_pc/f_pc_leaf.cpp` | 3 |
| `src/f_pc/f_pc_base.cpp` | 3 |
| `src/f_op/f_op_overlap_req.cpp` | 1 |
| `src/f_op/f_op_actor_mng.cpp` | 4 |
| `src/f_ap/f_ap_game.cpp` | 2 |

### JSystem (`libs/JSystem/src/`)
| File | Count |
|---|---|
| `libs/JSystem/src/JUtility/JUTResFont.cpp` | 7 |
| `libs/JSystem/src/JUtility/JUTGamePad.cpp` | 6 |
| `libs/JSystem/src/JUtility/JUTPalette.cpp` | 2 |
| `libs/JSystem/src/JUtility/JUTAssert.cpp` | 1 |
| `libs/JSystem/src/JUtility/JUTVideo.cpp` | 1 |
| `libs/JSystem/src/JKernel/JKRHeap.cpp` | 6 |
| `libs/JSystem/src/JKernel/JKRArchivePri.cpp` | 5 |
| `libs/JSystem/src/JKernel/JKRThread.cpp` | 3 |
| `libs/JSystem/src/JKernel/JKRMemArchive.cpp` | 2 |
| `libs/JSystem/src/JKernel/JKRExpHeap.cpp` | 2 |
| `libs/JSystem/src/JKernel/JKRDvdRipper.cpp` | 2 |
| `libs/JSystem/src/JKernel/JKRDecomp.cpp` | 2 |
| `libs/JSystem/src/JKernel/JKRAram.cpp` | 1 |
| `libs/JSystem/src/JKernel/JKRDvdArchive.cpp` | 1 |
| `libs/JSystem/src/JKernel/JKRCompArchive.cpp` | 1 |
| `libs/JSystem/src/JKernel/JKRAramArchive.cpp` | 1 |
| `libs/JSystem/src/J3DGraphLoader/J3DModelLoader.cpp` | 7 |
| `libs/JSystem/src/J3DGraphLoader/J3DClusterLoader.cpp` | 2 |
| `libs/JSystem/src/J3DGraphLoader/J3DAnmLoader.cpp` | 2 |
| `libs/JSystem/src/J3DGraphBase/J3DMaterial.cpp` | 7 |
| `libs/JSystem/src/J3DGraphBase/J3DShape.cpp` | 3 |
| `libs/JSystem/src/J3DGraphBase/J3DTexture.cpp` | 2 |
| `libs/JSystem/src/J3DGraphBase/J3DSys.cpp` | 2 |
| `libs/JSystem/src/J3DGraphBase/J3DPacket.cpp` | 2 |
| `libs/JSystem/src/J3DGraphBase/J3DTevs.cpp` | 1 |
| `libs/JSystem/src/J3DGraphAnimator/J3DModel.cpp` | 2 |
| `libs/JSystem/src/J3DGraphAnimator/J3DModelData.cpp` | 1 |
| `libs/JSystem/src/J3DGraphAnimator/J3DSkinDeform.cpp` | 1 |
| `libs/JSystem/src/J3DGraphAnimator/J3DCluster.cpp` | 2 |
| `libs/JSystem/src/J2DGraph/J2DTextBox.cpp` | 1 |
| `libs/JSystem/src/J2DGraph/J2DPrint.cpp` | 1 |
| `libs/JSystem/src/J2DGraph/J2DPictureEx.cpp` | 2 |
| `libs/JSystem/src/JParticle/JPABaseShape.cpp` | 7 |
| `libs/JSystem/src/JParticle/JPAParticle.cpp` | 3 |
| `libs/JSystem/src/JStudio/JStudio/jstudio-object.cpp` | 3 |
| `libs/JSystem/src/JStudio/JStudio/fvb.cpp` | 4 |
| `libs/JSystem/src/JStudio/JStudio_JStage/object-actor.cpp` | 1 |
| `libs/JSystem/src/JFramework/JFWSystem.cpp` | 1 |
| `libs/JSystem/src/JFramework/JFWDisplay.cpp` | 4 |
| `libs/JSystem/src/JHostIO/JHIRMcc.cpp` | 1 |
| `libs/JSystem/src/JHostIO/JHIMccBuf.cpp` | 1 |
| `libs/JSystem/src/JGadget/binary.cpp` | 4 |
| `libs/JSystem/src/JGadget/define.cpp` | 1 |
| `libs/JSystem/src/JAudio2/JAUInitializer.cpp` | 2 |
| `libs/JSystem/src/JAudio2/JASChannel.cpp` | 2 |
| `libs/JSystem/src/JAudio2/JASSeqParser.cpp` | 1 |
| `libs/JSystem/src/JAudio2/JASDSPInterface.cpp` | 1 |
| `libs/JSystem/src/JAudio2/JASAiCtrl.cpp` | 1 |
| `libs/JSystem/src/JAudio2/JAISe.cpp` | 1 |
| `libs/JSystem/src/JAudio2/JAIAudience.cpp` | 1 |
| `libs/JSystem/src/JAudio2/dsptask.cpp` | 1 |
| `libs/JSystem/src/JAudio2/dspproc.cpp` | 1 |

### Audio (`src/Z2AudioLib/`)
| File | Count |
|---|---|
| `src/Z2AudioLib/Z2WolfHowlMgr.cpp` | 1 |
| `src/Z2AudioLib/Z2Audience.cpp` | 4 |

### Dolphin / Aurora dolphin shims
| File | Count |
|---|---|
| `extern/aurora/lib/dolphin/card_switch.cpp` | 1 |
| `extern/aurora/include/dolphin/types.h` | 1 |
| `extern/aurora/include/dolphin/card.h` | 2 |
| `libs/dolphin/include/dolphin/types.h` | 2 |
| `libs/revolution/include/revolution/types.h` | 1 |
| `platforms/switch/reference/aurora-switch/...` | 4 |

### Headers (`include/`)
| File | Count |
|---|---|
| `include/d/d_save.h` | 8 |
| `include/d/d_com_inf_game.h` | 6 |
| `include/m_Do/m_Do_ext.h` | 5 |
| `include/d/d_camera.h` | 4 |
| `include/m_Do/m_Do_graphic.h` | 3 |
| `include/SSystem/SComponent/c_math.h` | 2 |
| `include/m_Do/m_Do_machine.h` | 1 |
| `include/m_Do/m_Do_lib.h` | 1 |
| `include/m_Do/m_Do_audio.h` | 1 |
| `include/global.h` | 1 |
| `include/os_report.h` | 1 |
| `include/dusk/string.hpp` | 2 |
| `include/dusk/offset_ptr.h` | 1 |
| `include/dusk/memory.h` | 1 |
| `include/dusk/logging.h` | 1 |
| `include/f_pc/f_pc_manager.h` | 1 |
| `include/SSystem/SComponent/c_cc_s.h` | 1 |
| `include/SSystem/SComponent/c_angle.h` | 1 |
| `include/d/d_tresure.h` | 1 |
| `include/d/d_s_name.h` | 1 |
| `include/d/d_s_logo.h` | 1 |
| `include/d/d_simple_model.h` | 2 |
| `include/d/d_msg_scrn_howl.h` | 1 |
| `include/d/d_msg_object.h` | 1 |
| `include/d/d_meter2_draw.h` | 1 |
| `include/d/d_menu_ring.h` | 1 |
| `include/d/d_menu_map_common.h` | 1 |
| `include/d/d_menu_fmap2D.h` | 2 |
| `include/d/d_menu_dmap.h` | 2 |
| `include/d/d_menu_collect.h` | 2 |
| `include/d/d_map.h` | 1 |
| `include/d/d_file_select.h` | 1 |
| `include/d/d_event_data.h` | 1 |
| `include/d/d_drawlist.h` | 2 |
| `include/d/d_cam_param.h` | 1 |
| `include/d/actor/d_grass.h` | 1 |
| `include/d/actor/d_flower.h` | 1 |
| `include/d/actor/d_a_b_gnd.h` | 1 |
| `include/d/actor/d_a_alink.h` | 1 |
| `include/d/actor/d_a_player.h` | 2 |
| `include/d/actor/d_a_e_db.h` | 1 |
| `include/d/actor/d_a_e_hb.h` | 1 |
| `include/d/actor/d_a_e_mb.h` | 1 |
| `include/d/actor/d_a_e_s1.h` | 1 |
| `include/d/actor/d_a_e_yh.h` | 1 |
| `include/d/actor/d_a_e_wb.h` | 1 |
| `include/d/actor/d_a_e_yd.h` | 1 |
| `include/d/actor/d_a_e_yg.h` | 1 |
| `include/d/actor/d_a_mirror.h` | 1 |
| `include/d/actor/d_a_movie_player.h` | 3 |
| `include/d/actor/d_a_horse.h` | 1 |
| `include/d/actor/d_a_obj_fchain.h` | 2 |
| `include/d/actor/d_a_obj_klift00.h` | 2 |
| `include/d/actor/d_a_obj_lv8Lift.h` | 1 |
| `include/angle_utils.h` | 1 |
| `include/Z2AudioLib/Z2Instances.h` | 1 |
| (JSystem public headers listed below) | |
| `libs/JSystem/include/JSystem/JStudio/JStudio/fvb.h` | 4 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DMatBlock.h` | 7 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DTexture.h` | 6 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DSys.h` | 4 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DMaterial.h` | 3 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DShape.h` | 1 |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DVertex.h` | 2 |
| `libs/JSystem/include/JSystem/J3DGraphAnimator/J3DAnimation.h` | 2 |
| `libs/JSystem/include/JSystem/J3DGraphAnimator/J3DCluster.h` | 4 |
| `libs/JSystem/include/JSystem/J3DGraphAnimator/J3DModel.h` | 1 |
| `libs/JSystem/include/JSystem/J3DGraphAnimator/J3DModelData.h` | 2 |
| `libs/JSystem/include/JSystem/J3DGraphAnimator/J3DSkinDeform.h` | 1 |
| `libs/JSystem/include/JSystem/J3DGraphLoader/J3DShapeFactory.h` | 1 |
| `libs/JSystem/include/JSystem/J3DGraphLoader/J3DModelLoader.h` | 2 |
| `libs/JSystem/include/JSystem/J3DGraphLoader/J3DClusterLoader.h` | 1 |
| `libs/JSystem/include/JSystem/JParticle/JPAParticle.h` | 1 |
| `libs/JSystem/include/JSystem/JParticle/JPABaseShape.h` | 1 |
| `libs/JSystem/include/JSystem/JAudio2/JAUSoundAnimator.h` | 2 |
| `libs/JSystem/include/JSystem/JAudio2/JASTrack.h` | 1 |
| `libs/JSystem/include/JSystem/JAudio2/JASHeapCtrl.h` | 5 |
| `libs/JSystem/include/JSystem/JAudio2/JASCriticalSection.h` | 1 |
| `libs/JSystem/include/JSystem/JAudio2/JASAramStream.h` | 1 |
| `libs/JSystem/include/JSystem/JKernel/JKRHeap.h` | 6 |
| `libs/JSystem/include/JSystem/JKernel/JKRArchive.h` | 4 |
| `libs/JSystem/include/JSystem/JKernel/JKRThread.h` | 1 |
| `libs/JSystem/include/JSystem/JKernel/JKRExpHeap.h` | 2 |
| `libs/JSystem/include/JSystem/JKernel/JKRDvdRipper.h` | 2 |
| `libs/JSystem/include/JSystem/JKernel/JKRDecomp.h` | 1 |
| `libs/JSystem/include/JSystem/JKernel/JKRAram.h` | 1 |
| `libs/JSystem/include/JSystem/JFramework/JFWSystem.h` | 1 |
| `libs/JSystem/include/JSystem/JUtility/JUTGamePad.h` | 1 |
| `libs/JSystem/include/JSystem/JUtility/JUTFont.h` | 3 |
| `libs/JSystem/include/JSystem/JUtility/JUTResFont.h` | 2 |
| `libs/JSystem/include/JSystem/JUtility/JUTVideo.h` | 3 |
| `libs/JSystem/include/JSystem/JUtility/JUTPalette.h` | 1 |
| `libs/JSystem/include/JSystem/JUtility/TColor.h` | 1 |

### dusk runtime
| File | Count |
|---|---|
| `src/dusk/offset_ptr.cpp` | 1 |
| `src/c/c_dylink.cpp` | 2 |
| `src/SSystem/SComponent/c_math.cpp` | 1 |
| `src/SSystem/SComponent/c_cc_d.cpp` | 3 |

(All ≥6-count sites are flagged. Lower-count sites are still load-bearing
and need review when their subsystem is exercised.)

### Round 1 hotspot interpretation

- **`d_a_movie_player.cpp` (45)** — STB movie/cutscene playback. Largest single
  hotspot. Highly relevant for cutscene freezes (next big bug after the
  TV-settings one).
- **`d_a_alink.cpp` (30)** — Link player actor. Many TARGET_PC branches handle
  modern input/animation. Mis-firing here breaks combat.
- **`m_Do_graphic.cpp` (17) + `m_Do_ext.cpp` (15)** — engine graphics +
  extensions layer. Touch nearly every frame.
- **`d_menu_*` family (~50 across 11 files)** — pause/inventory/map UI.
  TARGET_PC patches widescreen, touch input, HD assets.
- **`d_a_e_*` family (enemies)** — every enemy actor has 1-7 TARGET_PC sites.
- **JSystem `J3DGraphBase/J3DMatBlock.h` (7)** — material rendering state.
  Wrong branches → wrong materials → glitched models.
- **`JKernel/JKRHeap` (6 src + 6 header)** — heap subsystem. Already
  patched for thread-local model; remaining branches still untouched.
- **`JUTGamePad.cpp` (6) + `JUTGamePad.h` (1)** — gamepad. Currently aurora
  shims most of this; verify there are no remaining vanilla GC paths firing.

---

## Round 2 — alternative preprocessor patterns

Pattern: `^#ifdef TARGET_PC` | `^#ifndef TARGET_PC` | `^#elif TARGET_PC` | `^#elif defined(TARGET_PC)` | `^#  *if TARGET_PC` (whitespace-indented preprocessor).

**Result: 231 occurrences across 113 files. ~75 files NEW vs Round 1.**

### New hotspot files (not in Round 1)
| File | Count | Note |
|---|---|---|
| `src/d/d_drawlist.cpp` | 14 | already had 9 in R1; **combined: 23**. Display-list emission |
| `src/d/d_select_cursor.cpp` | 8 | Save-slot cursor / dataSelect UI |
| `libs/JSystem/src/JUtility/JUTException.cpp` | 7 | Exception handler — relevant to our addr2line flow |
| `src/d/actor/d_a_npc_ne.cpp` | 6 | NPC "Ne" dialogue |
| `src/d/d_map_path.cpp` | 5 | already had 3 in R1; **combined: 8** |
| `extern/aurora/lib/dolphin/pad/pad_switch.cpp` | 3 | Switch gamepad shim |
| `extern/aurora/include/dolphin/pad.h` | 3 | Pad header |
| `src/d/d_meter_haihai.cpp` | 3 | Meter "haihai" placement UI |
| `src/d/d_meter_button.cpp` | 3 | Meter button UI |
| `src/d/actor/d_a_alink.cpp` | 8 | (R1 had 30; **combined: 38**) |
| `src/d/actor/d_a_alink_wolf.inc` | 3 | (R1 had 3; combined: 6) |
| `libs/JSystem/src/JKernel/JKRAramStream.cpp` | 2 | ARAM streaming (audio) |
| `libs/JSystem/src/JAudio2/JASTaskThread.cpp` | 2 | Audio task thread |
| `libs/JSystem/src/JAudio2/JAISeqMgr.cpp` | 1 | Audio sequence manager |
| `src/d/d_home_button.cpp` | 1 | **Home button handler — Switch-relevant!** |
| `src/d/d_menu_calibration.cpp` | 1 | **Calibration UI — Switch-relevant!** |
| `src/d/d_msg_scrn_light.cpp` | 2 | Light message screen |
| `src/d/d_msg_out_font.cpp` | 2 | Message font output |
| `src/d/d_resorce.cpp` | 1 | (R1 had 3; combined: 4) |
| `src/d/d_ovlp_fade2.cpp` | 1 | Overlap fade-2 |
| `src/d/d_meter2_draw.cpp` | 2 | Meter2 draw |
| `src/d/d_menu_letter.cpp` | 1 | Letter menu |
| `src/d/actor/d_a_obj_tp.cpp` | 2 | obj_tp |
| `src/d/actor/d_a_obj_gb.cpp` | 2 | obj_gb |
| `src/d/actor/d_a_obj_brg.cpp` | 1 | obj_brg |
| `src/d/actor/d_a_dshutter.cpp` | 1 | dshutter |
| `src/d/actor/d_a_door_boss.cpp` + 3 variants | 1+1+1+1 | Boss doors |
| `src/d/actor/d_a_balloon_2D.cpp` | 2 | 2D balloon |
| `src/d/actor/d_a_npc_ne.cpp` | 6 | NPC Ne |
| `src/d/actor/d_a_alink_damage.inc` | 2 | combined with R1: 4 |
| `src/d/actor/d_a_alink_effect.inc` | 1 | Link effects |
| `libs/JSystem/src/JUtility/JUTTexture.cpp` | 2 | JUT texture upload |
| `libs/JSystem/src/JUtility/JUTFader.cpp` | 2 | Fader subsystem |
| `libs/JSystem/src/J2DGraph/J2DTevs.cpp` | 1 | J2D TEV stages |
| `libs/JSystem/src/J3DGraphBase/J3DShapeMtx.cpp` | 1 | J3D shape matrix |
| `libs/dolphin/src/gx/GXMisc.c` | 3 | GX Misc (state set) |
| `libs/dolphin/include/dolphin/gx/GXStruct.h` | 2 | GX structs |
| `libs/dolphin/include/dolphin/gx/GXGeometry.h` | 1 | GX geometry |
| `libs/dolphin/include/dolphin/os.h` | 1 | Dolphin OS |
| `extern/aurora/include/dolphin/gx/*` (8 files) | 1-2 each | All GX-related |
| `libs/JSystem/include/JSystem/J3DGraphBase/J3DFifo.h` | 3 | J3D fifo |
| `include/dusk/endian.h` | 2 | **Endian helpers — Switch is aarch64 LE** |
| `include/dusk/gx_helper.h` | 1 | GX helper |
| `include/dusk/audio.h` | 1 | (We already added our DUSK_AUDIO_SKIP here) |

### Round 2 hotspot interpretation

- **`d_drawlist.cpp` combined 23 sites** — the display-list emission layer
  generates the GX commands the renderer plays back. Almost half are
  TARGET_PC; getting these right matters for any render correctness.
- **`d_select_cursor.cpp` 8 sites** — already lit up the file-select flow;
  any selection-cursor bug at save selection traces here.
- **`JUTException.cpp` 7 sites** — covers fatal-error / abort dispatch.
  Make sure aborts route to our `dusk_switch_log` + crash report path.
- **`d_home_button.cpp`** + **`d_menu_calibration.cpp`** — small but
  Switch-specific UX (Home button = Sphaira sleep, calibration is the
  brightness check we already neutralised in `dScnName`).
- **`pad_switch.cpp` 3 sites** — our Joy-Con mapping. Already reviewed in
  earlier session but verify the alternative-preprocessor branches.
- **endian.h** — aarch64 is little-endian like x86 but GameCube data is
  BIG-endian. Any unguarded `TARGET_PC` swap there matters for save-data
  + DVD reads.

## Round 3 — TARGET_PC in C++ expressions (`if (TARGET_PC)`, `defined(TARGET_PC)`, ...)

TBD.

## Round 4 — final sweep

TBD.

---

## Fix strategy (per `PLAN_TARGET_PC_AUDIT.md`)

Two valid patterns, choose per site:

1. **Narrow `#ifndef __SWITCH__` around the PC-specific code inside the
   `#if TARGET_PC` block.** Keeps the original `TARGET_PC` semantics for
   real-PC builds, carves the Switch out:
   ```cpp
   #if TARGET_PC
   #ifndef __SWITCH__
       sdl_only_thing();
   #endif
       portable_stuff();
   #endif
   ```
2. **Migrate the macro to `TARGET_PC && !defined(__SWITCH__)` (or to a new
   `TARGET_PC_DESKTOP` symbol).** Use when the entire `#if TARGET_PC` block
   is desktop-specific.

NEVER strip the `#if TARGET_PC` entirely on Switch — many of these
blocks contain CORRECT Dusk fixes that the GC original lacked (e.g.,
heap-fallback rules in JKRHeap, the file-select state-machine init order
in m_Do_machine, the `mCardCommand = COMM_ATTACH_e` auto-mount in
m_Do_MemCard). Treat each site individually.
