// Empty implementations of Dusklight subsystems whose PC backends pull SDL3,
// RmlUi, ImGui or other host-side APIs. The Switch build excludes those
// source files; stubs here keep the link clean. Each block should be
// replaced with a libnx-native implementation as the port matures.

#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>  // fsync
#include <sys/stat.h>  // mkdir (create the single data folder for early-boot logs)
#include <netinet/in.h>  // struct in_addr (for __nxlink_host / nxlink stdio)
#include <memory>
#include <string>
#include <string_view>
#include <exception>  // std::set_terminate / current_exception (NVK abort diagnostics)
#include <SDL3/SDL.h>  // aurora-switch's stub
#include <aurora/aurora.h>
#include <switch.h>
#include "global.h"

// ── libnx applet configuration ────────────────────────────────────────────
// Without these explicit overrides, libnx's default is LibraryApplet with
// only ~512MB heap — not enough for Dusklight (mem1=256MB + mem2=24MB +
// Dawn + game heap). AppletType_Application gives the full ~3.3GB.
// Pattern lifted from dantiicu/switch-vulkan-triangle-test.
extern "C" {
    u32 __nx_applet_type = AppletType_Application;
    size_t __nx_heap_size = 0;  // 0 = take all available DRAM
}

// ── libnx user-app init/exit hooks ────────────────────────────────────────
// Called by libnx between __libc_init_array and main(). Mount the romfs that
// elf2nro --romfsdir bundled into this NRO so `fopen("romfs:/res/...")` works
// for Aurora's RmlUi FileInterface (fonts, .rcss, logo.png). Without this,
// every asset load returns NULL and the launcher renders a black screen.
extern "C" void userAppInit(void) {
    Result rc = romfsInit();
    (void)rc;  // best-effort — if romfs missing the app continues with no UI assets
}
extern "C" void userAppExit(void) {
    romfsExit();
}

// ── single-folder data root (HayatoG/dusklight#5) ─────────────────────────
// Keep EVERYTHING the app reads/writes under one folder on the SD card. This MUST match the
// prefPath returned by the SDL_GetPrefPath shim (extern/aurora/include/SDL3/SDL.h). The early-boot
// logs below open before dusk::data resolves/creates the data dir, so create the folder here first.
#define DUSK_SD_DATA_ROOT "sdmc:/TwilitRealm/Dusklight"
static void dusk_ensure_data_dir(void) {
    mkdir("sdmc:/TwilitRealm", 0777);
    mkdir(DUSK_SD_DATA_ROOT, 0777);
}

// ── file-based logging to <data folder>/dusklight.log ─────────────────────
// svcOutputDebugString gets buffered/truncated by Eden. File logging on the
// SD card works identically in Eden and on real hardware, never drops, and
// survives the process so we can read it after a crash.
// Pattern lifted from dantiicu/aurora-switch examples/simple.c.
// NVK winsys diagnostic sink: drm_shim.c and nvkmd_nouveau_va.c emit
// `nvkmd: VA alloc FAILED ...` and other engine-side diagnostics through
// g_drm_shim_log_sink. Standalone smoke apps install their own; Dusklight
// needs to install one too or those messages vanish (heuristic: the
// vk_errorf line shows the *fact* of failure, this sink shows size/align).
extern "C" {
    extern void (*g_drm_shim_log_sink)(const char*);
    void dusk_switch_log(const char*);  // forward decl, defined below
}
static void dusk_nvk_log_sink(const char* msg) {
    dusk_switch_log(msg);
}

extern "C" {
    static FILE* dusk_log_file = nullptr;
    static bool  dusk_log_inited = false;
    static bool  dusk_nxlink_up = false;  // streaming logs back to a `nxlink -s` PC

    void dusk_switch_log_init(void) {
        if (dusk_log_inited) return;
        dusk_log_inited = true;
        dusk_ensure_data_dir();
        dusk_log_file = fopen(DUSK_SD_DATA_ROOT "/dusklight.log", "w");
        if (dusk_log_file) {
            fputs("[DUSKLIGHT-LOG] init\n", dusk_log_file);
            fflush(dusk_log_file);
        }
        // Route NVK winsys diagnostics through this same log.
        g_drm_shim_log_sink = dusk_nvk_log_sink;
        // nxlink live logs: if this .nro was netloaded by `nxlink -s <switch-ip>`,
        // libnx records the host PC in __nxlink_host. Bring up sockets and redirect
        // stdout/stderr back to that PC so every dusk_switch_log line streams live to
        // the terminal on real hardware. Gated on __nxlink_host so SD-card launches
        // pay nothing (no socket init, no behavior change).
        if (__nxlink_host.s_addr != 0) {
            if (R_SUCCEEDED(socketInitializeDefault())) {
                int fd = nxlinkStdio();  // dup2's stdout/stderr onto the nxlink socket
                if (fd >= 0) {
                    dusk_nxlink_up = true;
                    printf("[DUSKLIGHT-LOG] nxlink stdio up (live logs over network)\n");
                    fflush(stdout);
                }
            }
        }
    }
    void dusk_switch_log(const char* msg) {
        // Always also send to debug-stream for Eden's live view
        svcOutputDebugString(msg, strlen(msg));
        if (!dusk_log_inited) dusk_switch_log_init();
        if (dusk_log_file) {
            fputs(msg, dusk_log_file);
            // Lazy flush: every ~64 messages drop the OS buffer to disk.
            // Removed per-line fsync(): killed ~5-10ms SD-write per log line,
            // tanking the framerate to <1fps. Crash reports still come from
            // Atmosphere (sdmc:/atmosphere/crash_reports/) so we don't need
            // per-line durability on dusklight.log.
            // EXCEPTION: low-volume diagnostic lines ([mc]/[mw]/[ms]/[talk]/[cof]
            // and [main01] boot breadcrumbs) force an immediate flush so a hang
            // (e.g. the post-save black screen) leaves them on disk for FTP pull,
            // instead of being stuck in the unflushed tail. These are rare, so the
            // SD-write cost is negligible (not per-frame).
            bool critical = (msg[0] == '[' &&
                             (msg[1] == 'm' || msg[1] == 't' || msg[1] == 'c'));
            static unsigned log_lazy_ctr = 0;
            if (critical || (++log_lazy_ctr & 0x3f) == 0) {
                fflush(dusk_log_file);
            }
        }
        if (dusk_nxlink_up) {            // live stream to the nxlink PC (real hardware)
            fputs(msg, stdout);
            fflush(stdout);
        }
    }
}

// ── audio ──────────────────────────────────────────────────────────────────
// Audio is now enabled on Switch: dusk/audio/{DuskAudioSystem,DuskDsp,Adpcm}.cpp are compiled and
// provide dusk::audio::* (Initialize/SetMasterVolume/SetPaused/SetEnableReverb/VolumeFromU16 +
// the MasterVolume/EnableReverb/EnableHrtf/ChannelAux globals). Output goes through libnx audren in
// platforms/switch/src/switch_audio.cpp. No stubs here anymore.

// gyro: real implementation in src/dusk/gyro.cpp (no longer excluded — it reads the Switch six-axis
// via aurora's pad_switch and feeds Link's aim). See HayatoG/dusklight#3.

// ── iso validation (iso_validate.cpp replacement) ─────────────────────────
// log_verification_state takes `dusk::DiscVerificationState` (enum class : u8
// from include/dusk/settings.h). We include settings.h to get the exact
// mangled signature — otherwise the linker fails to match the call site.
// iso_validate.hpp owns the real DiscInfo / ValidationError definitions
// (filtered TU iso_validate.cpp is what we're replacing); include it here
// instead of redefining the types locally.
#include "dusk/settings.h"
#include "dusk/iso_validate.hpp"

namespace dusk::iso {
// Lightweight on-device disc check. The real nod-based iso_validate.cpp is excluded on Switch (it
// pulls the nod library + SDL that aren't built here), so these used to return Success for ANY file —
// which is why the data-folder auto-scan happily picked a 303-byte ".controller" as the "disc". Mirror
// aurora's createImageReader magic detection (lib/dolphin/dvd/dvd_switch.cpp): reject too-small files,
// accept known container formats by their offset-0 magic, or a raw GCM/ISO by the GameCube magic
// 0xC2339F3D at offset 0x1C. See HayatoG/dusklight#6.
static ValidationError lightweight_disc_check(const char* path, DiscInfo& info) {
    if (path == nullptr || path[0] == '\0') {
        return ValidationError::IOError;
    }
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return ValidationError::IOError;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return ValidationError::IOError;
    }
    const long size = std::ftell(f);
    // A real GameCube/Wii disc image (raw or compressed) is at least tens of MB; reject companions
    // like config.json, *.controller, cache .db and .gci saves up front.
    if (size < 16L * 1024 * 1024) {
        std::fclose(f);
        return ValidationError::InvalidImage;
    }
    unsigned char hdr[0x20] = {};
    std::fseek(f, 0, SEEK_SET);
    const size_t n = std::fread(hdr, 1, sizeof hdr, f);
    std::fclose(f);
    if (n < sizeof hdr) {
        return ValidationError::InvalidImage;
    }
    const uint32_t magicLE = static_cast<uint32_t>(hdr[0]) | (static_cast<uint32_t>(hdr[1]) << 8) |
                             (static_cast<uint32_t>(hdr[2]) << 16) |
                             (static_cast<uint32_t>(hdr[3]) << 24);
    switch (magicLE) {
    case 0xB10BC001u:  // GCZ
    case 0x4F534943u:  // CISO
    case 0xA2380FAEu:  // TGC
    case 0x53464257u:  // WBFS
    case 0x01414957u:  // WIA
    case 0x015A5652u:  // RVZ
    case 0x53474745u:  // NFS
        return ValidationError::Success;  // container format — aurora_dvd_open decodes it
    default:
        break;
    }
    // Raw GCM/ISO: the GameCube magic word 0xC2339F3D sits at offset 0x1C (big-endian on disc).
    const uint32_t gcMagic = (static_cast<uint32_t>(hdr[0x1C]) << 24) |
                             (static_cast<uint32_t>(hdr[0x1D]) << 16) |
                             (static_cast<uint32_t>(hdr[0x1E]) << 8) | static_cast<uint32_t>(hdr[0x1F]);
    if (gcMagic == 0xC2339F3Du) {
        info.isPal = (hdr[3] == 'P');  // game-id byte 3: E=USA, P=PAL/EUR, J=JPN
        return ValidationError::Success;
    }
    return ValidationError::WrongGame;
}

ValidationError inspect(const char* path, DiscInfo& info) { return lightweight_disc_check(path, info); }
void log_verification_state(std::string_view, dusk::DiscVerificationState) {}
} // namespace dusk::iso

// ── ImGui console & engine (ImGuiConsole.cpp etc. replacement) ────────────
// Out-of-line definitions, so the linker actually emits symbols. The Switch
// build excludes the real ImGuiConsole.cpp + ImGuiBloomWindow.cpp etc.
namespace dusk {

class ImGuiConsoleStub {
public:
    void HandleSDLEvent(const SDL_Event&) {}
    void PreDraw() {}
    void PostDraw() {}
};
// ImGuiConsole symbols expected by callers. We can't easily import the real
// header (it transitively pulls ImGuiMenuTools etc.), so we provide just the
// mangled symbols by means of a class with matching name + namespace.
// The Switch build never touches the object members; only method dispatch.
} // namespace dusk

// Define symbols at exact mangled names: dusk::ImGuiConsole::*. We
// declare the class minimally here. ODR violation versus the real header is
// accepted because the real header is not included on this side.
namespace dusk {
class ImGuiConsole {
public:
    ImGuiConsole();
    void HandleSDLEvent(const SDL_Event&);
    void PreDraw();
    void PostDraw();
};
ImGuiConsole::ImGuiConsole() {}
void ImGuiConsole::HandleSDLEvent(const SDL_Event&) {}
void ImGuiConsole::PreDraw() {}
void ImGuiConsole::PostDraw() {}

ImGuiConsole g_imguiConsole;

void ImGuiEngine_Initialize(float) {}
void ImGuiEngine_AddTextures() {}

// AuroraBackend helpers used by Dusklight elsewhere
std::string_view backend_name(AuroraBackend) { return "switch"; }
bool try_parse_backend(std::string_view, AuroraBackend&) { return false; }

// ── Bloom + StubLog (ImGuiBloomWindow.cpp / ImGuiStubLog.cpp replacement) ─
void ApplyBloomOverride() {}
void SendToStubLog(AuroraLogLevel, const char*, const char*) {}

// ── ui (ui.cpp + documents replacement) ───────────────────────────────────
namespace ui {
bool initialize() noexcept { return true; }
void shutdown() noexcept {}
void handle_event(const SDL_Event&) noexcept {}
void update() noexcept {}
bool is_prelaunch_open() noexcept { return false; }
bool any_document_visible() noexcept { return false; }
} // namespace ui

} // namespace dusk

// autosave: real implementation in src/dusk/autosave.cpp (no longer excluded — uses the same proven
// g_mDoMemCd_control save path as manual save). See HayatoG/dusklight#9.

// ── POSIX shims missing from libnx newlib ─────────────────────────────────
extern "C" int execv(const char*, char* const[]) {
    errno = ENOSYS;
    return -1;
}

// ── NVK driver runtime env (M-DV-2) ───────────────────────────────────────
// Our REAL Mesa NVK provides vk_icdGetInstanceProcAddr (in libvulkan.a, --whole-archive'd at the
// final link). The old null stub is GONE: it would double-define the symbol and, under
// --allow-multiple-definition, a null could win and kill the driver. Instead, set the NVK runtime
// env BEFORE Aurora creates the Vulkan instance. The GM20B is a non-conformant SOC device, so NVK
// hides it from vkEnumeratePhysicalDevices unless NVK_I_WANT_A_BROKEN_VULKAN_DRIVER is set; also
// disable the on-disk shader cache (no writable cache dir). A constructor runs before main().
extern "C" int setenv(const char* name, const char* value, int overwrite);
extern "C" void dusk_switch_log(const char* msg);

// std::terminate handler: the post-title crash is a silent "User Break" (abort) with no Mesa log
// and an unsymbolized report. The most likely cause is an UNCAUGHT C++ EXCEPTION (Dawn throws on
// vk errors; std::bad_alloc on OOM). Log its type/what() so we finally see the reason.
static void dusk_terminate_handler(void) {
    dusk_switch_log("[TERMINATE] std::terminate -- uncaught exception or abort\n");
    std::exception_ptr e = std::current_exception();
    if (e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) {
            char b[384]; snprintf(b, sizeof b, "[TERMINATE] uncaught std::exception: %s\n", ex.what());
            dusk_switch_log(b);
        } catch (...) {
            dusk_switch_log("[TERMINATE] uncaught non-std exception\n");
        }
    } else {
        dusk_switch_log("[TERMINATE] no active exception (raw abort()/unreachable)\n");
    }
    // fall through to the default terminate (abort) so the crash report is still produced
}

// --wrap=abort (in the dusklight link): the post-title crash is a raw abort()/assert (NOT a C++
// exception -- the terminate handler never fired). Log WHO called abort so we can addr2line it:
//   elf_offset = caller_RA - (dusk_switch_log@ - nm(dusk_switch_log))
extern "C" void __real_abort(void) __attribute__((noreturn));
extern "C" void __wrap_abort(void) {
    char b[160];
    snprintf(b, sizeof b, "[ABORT] abort() caller RA=%p (base anchor dusk_switch_log@=%p)\n",
             __builtin_return_address(0), (void*)&dusk_switch_log);
    dusk_switch_log(b);
    __real_abort();
}

__attribute__((constructor)) static void dusk_nvk_env(void) {
    dusk_ensure_data_dir();  // keep the early-boot logs inside the single data folder (#5)
    setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
    setenv("MESA_SHADER_CACHE_DISABLE", "1", 1);
    // Route Mesa/NVK's own log (mesa_loge / unreachable / errorf) to a file so a driver-side
    // abort (the "User Break" crashes that don't symbolize the NRO) leaves a readable reason.
    setenv("MESA_LOG_FILE", DUSK_SD_DATA_ROOT "/mesa_nvk.log", 1);
    std::set_terminate(dusk_terminate_handler);
    // Capture stderr (assert() messages "file:line: cond", and anything printed there) to a file --
    // newlib's __assert_func writes the failing condition + location to stderr before aborting.
    freopen(DUSK_SD_DATA_ROOT "/dusk_stderr.log", "w", stderr);
    // Stamp the build + the runtime address of a known function so a crash report's raw return
    // addresses can be turned into ELF offsets: offset = RA - (this_addr - nm(dusk_switch_log)).
    char b[160];
    snprintf(b, sizeof b, "[NVK-BUILD] dusklight clean-reset  dusk_switch_log@=%p\n",
             (void*)&dusk_switch_log);
    dusk_switch_log(b);
}

// ── Backend glue no-op fallbacks (Dawn-GL experiment) ─────────────────────
// game_main() (m_Do_main.cpp) calls these unconditionally on Switch, but their real definitions
// live only in the NATIVE backends:
//   - aurora_switch_begin_gx_capture : gfx/common.cpp  (#if AURORA_BACKEND_DEKO3D || GLES) — arms
//                                      the native GX command-IR capture.
//   - dusk_switch_disable_menu       : lib/gles/gpu.cpp (GLES) — disables the RmlUi prelaunch menu.
// In the real-Dawn (AURORA_BACKEND_DAWN_GL) build neither native backend is compiled, so provide
// no-op definitions here to satisfy the link. Compiled ONLY when no native backend is active, so
// the strong native definitions are used (and never double-defined) in the deko/GLES builds.
#if !defined(AURORA_BACKEND_DEKO3D) && !defined(AURORA_BACKEND_GLES)
extern "C" void aurora_switch_begin_gx_capture(void) {}
extern "C" void dusk_switch_disable_menu(void) {}
#endif

extern "C" void aurora_set_resampler(AuroraSampler) {}
extern "C" void aurora_set_texture_replacements_enabled(bool) {}

namespace dusk::crash_handler {
// Host crash_handler.cpp uses POSIX dlfcn/unwind/link — not available on
// newlib. Switch crash reporting is delivered through __wrap_abort and the
// std::set_terminate handler installed in the constructor above.
void install() {}
}  // namespace dusk::crash_handler

// dusk::ui:: settings + prelaunch pull in host-side helpers we filtered out
// (file_select.cpp = SDL_ShowOpenFile* dialogs, iso_validate.cpp = full hash
// of the GameCube disc). Stub them so the link closes; on Switch the disc
// path is selected differently and a hash check would block boot for ~20s.
#include "dusk/file_select.hpp"
namespace dusk {
void ShowFileSelect(FileCallback, void*, SDL_Window*, const SDL_DialogFileFilter*, int, const char*, bool) {}
void ShowFolderSelect(FileCallback, void*, SDL_Window*, const char*) {}
std::string display_name_for_path(std::string_view path) { return std::string(path); }
}  // namespace dusk
namespace dusk::iso {
// Same lightweight check as inspect() (full nod hash verification isn't available on Switch). Better
// than the old always-Success, which let any file masquerade as a verified disc.
ValidationError validate(const char* path, VerificationStatus&, DiscInfo& info) {
    return lightweight_disc_check(path, info);
}
}  // namespace dusk::iso
