/**
 * m_Do_main.cpp
 * Main Initialization
 * PC Port Version - based on Aurora integration from Vorversion
 */

#include "m_Do/m_Do_main.h"
#include <dolphin/vi.h>
#include <cstring>
#include <cstdio>
#ifdef __SWITCH__
extern "C" void dusk_switch_log(const char*);
#endif
#include "DynamicLink.h"
#include "JSystem/JAudio2/JASAudioThread.h"
#include "JSystem/JAudio2/JAUSectionHeap.h"
#include "JSystem/JAudio2/JAUSoundTable.h"
#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JHostIO/JORServer.h"
#include "JSystem/JKernel/JKRAram.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JUtility/JUTConsole.h"
#include "JSystem/JUtility/JUTException.h"
#include "JSystem/JUtility/JUTProcBar.h"
#include "JSystem/JUtility/JUTReport.h"
#include "SSystem/SComponent/c_counter.h"
#include "SSystem/SComponent/c_API_graphic.h"
#include "Z2AudioLib/Z2WolfHowlMgr.h"
#include "c/c_dylink.h"
#include "d/d_com_inf_game.h"
#include "d/d_debug_pad.h"
#include "d/d_s_logo.h"
#include "d/d_s_menu.h"
#include "d/d_s_play.h"
#include "dusk/perf.hpp"
#include "dusk/time.h"
#include "f_ap/f_ap_game.h"
#include "f_op/f_op_msg.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext2.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_machine.h"
#include "m_Do/m_Do_printf.h"
#include "m_Do/m_Do_ext2.h"
#include "SSystem/SComponent/c_counter.h"
#include <cstring>

#include <filesystem>
#include <system_error>
#include <thread>
#include "SSystem/SComponent/c_API.h"
#include "dusk/app_info.hpp"
#include "dusk/crash_handler.h"
#include "dusk/crash_reporting.h"
#include "dusk/data.hpp"
#include "dusk/dusk.h"
#include "dusk/frame_interpolation.h"
#include "dusk/game_clock.h"
#include "dusk/gyro.h"
#include "dusk/imgui/ImGuiConsole.hpp"
#include "dusk/imgui/ImGuiEngine.hpp"
#include "dusk/i18n.hpp"
#include "dusk/iso_validate.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "dusk/ui/graphics_tuner.hpp"
#include "dusk/ui/menu_bar.hpp"
#include "dusk/ui/overlay.hpp"
#include "dusk/ui/prelaunch.hpp"
#include "dusk/ui/preset.hpp"
#include "dusk/ui/ui.hpp"
#include "dusk/texture_replacements.hpp"
#include "version.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <aurora/dvd.h>
#include <dolphin/dvd.h>

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_misc.h"
#include "cxxopts.hpp"
#include "d/actor/d_a_movie_player.h"
#include "dusk/audio/DuskAudioSystem.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/config.hpp"
#include "dusk/speedrun.h"
#include "dusk/settings.h"
#include "dusk/io.hpp"
#include "dusk/version.hpp"
#include "dusk/discord_presence.hpp"
#include "tracy/Tracy.hpp"
#include "f_pc/f_pc_draw.h"
#include "tracy/Tracy.hpp"
#include <RmlUi/Core.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if DUSK_ENABLE_SENTRY_NATIVE
#include "dusk/ui/reporting.hpp"
#endif

// --- GLOBALS ---
s8 mDoMain::developmentMode = -1;
OSTime mDoMain::sPowerOnTime;
OSTime mDoMain::sHungUpTime;
u32 mDoMain::memMargin = 0xFFFFFFFF;
char mDoMain::COPYDATE_STRING[18] = "??/??/?? ??:??:??";
#if TARGET_PC
const int audioHeapSize = 0x14D800 * 2;
#else
const int audioHeapSize = 0x14D800;
#endif

// =========================================================================
// LOAD_COPYDATE - PC Version
// =========================================================================
#define COPYDATE_PATH "/str/Final/Release/COPYDATE"

#if TARGET_PC
bool dusk::IsRunning = true;
bool dusk::IsShuttingDown = false;
bool dusk::IsGameLaunched = false;
bool dusk::RestartRequested = false;
std::filesystem::path dusk::ConfigPath;
std::filesystem::path dusk::CachePath;
#endif

void dusk::RequestRestart() noexcept {
    RestartRequested = SupportsProcessRestart;
    IsRunning = false;
}

s32 LOAD_COPYDATE(void*) {
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    DVDFileInfo fi;
    if (DVDOpen(COPYDATE_PATH, &fi)) {
        u32 readLen = (fi.length < sizeof(buffer) - 1) ? fi.length : sizeof(buffer) - 1;
        // DVDReadPrio requires 32-byte aligned buffer and length rounded up to 32
        u32 alignedLen = (readLen + 31) & ~31;
        alignas(32) char readBuf[64];
        DVDReadPrio(&fi, readBuf, alignedLen, 0, 2);
        DVDClose(&fi);

        memcpy(buffer, readBuf, readLen);
        buffer[readLen] = '\0';
    } else {
        SAFE_STRCPY(buffer, "PC PORT BUILD");
        DuskLog.warn("COPYDATE file not found at {}", COPYDATE_PATH);
    }

    memcpy(mDoMain::COPYDATE_STRING, buffer, sizeof(mDoMain::COPYDATE_STRING) - 1);
    mDoMain::COPYDATE_STRING[sizeof(mDoMain::COPYDATE_STRING) - 1] = '\0';

    DuskLog.info("COPYDATE=[{}]", mDoMain::COPYDATE_STRING);
    return 1;
}

AuroraInfo auroraInfo;
AuroraStats dusk::lastFrameAuroraStats;
float dusk::frameUsagePct = 0.0f;

bool launchUILoop() {
#ifdef __SWITCH__
    unsigned dusk_ui_frame_no = 0;
#endif
    while (dusk::IsRunning && !dusk::IsGameLaunched) {
#ifdef __SWITCH__
        if (dusk_ui_frame_no < 8) {
            char b[80];
            snprintf(b, sizeof b, "[dusk] launchUILoop iter=%u IsRunning=%d IsGameLaunched=%d\n",
                     dusk_ui_frame_no, (int)dusk::IsRunning, (int)dusk::IsGameLaunched);
            ::dusk_switch_log(b);
        }
        ++dusk_ui_frame_no;
#endif
        const AuroraEvent* event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            switch (event->type) {
            case AURORA_SDL_EVENT:
                dusk::ui::handle_event(event->sdl);
                dusk::g_imguiConsole.HandleSDLEvent(event->sdl);
                break;
            case AURORA_DISPLAY_SCALE_CHANGED:
                dusk::ImGuiEngine_Initialize(event->windowSize.scale);
                break;
            case AURORA_EXIT:
#ifdef __SWITCH__
                ::dusk_switch_log("[dusk] launchUILoop AURORA_EXIT received -> returning false\n");
#endif
                return false;
            }

            event++;
        }

        if (!aurora_begin_frame()) {
            DuskLog.debug("aurora_begin_frame returned false, skipping draw this frame");
            continue;
        }

        dusk::ui::update();

        dusk::g_imguiConsole.PreDraw();
        dusk::g_imguiConsole.PostDraw();

        aurora_end_frame();
    }

    return dusk::IsRunning;
}

void main01(void) {
#ifdef __SWITCH__
#  define DLOG(s) ::dusk_switch_log("[main01] " s "\n")
#else
#  define DLOG(s) ((void)0)
#endif
    DLOG("entry");
    OS_REPORT("\x1b[m");

    // 1. Setup
    DLOG("mDoMch_Create ...");
    mDoMch_Create();
    DLOG("mDoMch_Create done");
    DLOG("mDoGph_Create ...");
    mDoGph_Create();
    DLOG("mDoGph_Create done");
    DLOG("mDoCPd_c::create ...");
    mDoCPd_c::create();
    DLOG("mDoCPd_c::create done");

    // Console Setup
    JUTConsole* console = JFWSystem::getSystemConsole();
    if (console) {
        console->setOutput(mDoMain::developmentMode ? JUTConsole::OUTPUT_OSR_AND_CONSOLE :
                                                      JUTConsole::OUTPUT_NONE);
        console->setPosition(32, 42);
    }
    DLOG("console setup done");

    // Loader Init
    mDoDvdThd_callback_c::create((mDoDvdThd_callback_func)LOAD_COPYDATE, NULL);
    DLOG("mDoDvdThd_callback_c::create LOAD_COPYDATE done");

    DLOG("fapGm_Create ...");
    OSReport("Calling fapGm_Create()...\n");
    fapGm_Create();
    DLOG("fapGm_Create done");

    DLOG("fopAcM_initManager ...");
    OSReport("Calling fopAcM_initManager()...\n");
    fopAcM_initManager();
    DLOG("fopAcM_initManager done");

    DLOG("cDyl_InitAsync ...");
    OSReport("Calling cDyl_InitAsync()...\n");
    cDyl_InitAsync();
    DLOG("cDyl_InitAsync done");

    DLOG("audio heap create ...");
    g_mDoAud_audioHeap = JKRCreateSolidHeap(audioHeapSize, JKRGetCurrentHeap(), false);
    JKRHEAP_NAME(g_mDoAud_audioHeap, "g_mDoAud_audioHeap");
    DLOG("audio heap done");

    if (DUSK_AUDIO_DISABLED) {
        // Pretend the audio engine initialized already. This is a lie, but needed to boot.
        DLOG("DUSK_AUDIO_DISABLED -> onInitFlag");
        mDoAud_zelAudio_c::onInitFlag();
    }

    OSReport("Entering Main Loop (main01)...\n");
    DLOG("entering main game-loop");

    dusk::game_clock::ensure_initialized();

#ifdef __SWITCH__
    unsigned main01_iter = 0;
#endif
    do {
#ifdef __SWITCH__
        if (main01_iter < 10) {
            char b[80];
            snprintf(b, sizeof b, "[main01] iter=%u top\n", main01_iter);
            ::dusk_switch_log(b);
        }
        ++main01_iter;
#endif
        // 1. Update Window Events
        const AuroraEvent* event = aurora_update();
        while (true) {
            switch (event->type) {
            case AURORA_NONE:
                goto eventsDone;
            case AURORA_PAUSED:
                dusk::audio::SetPaused(true);
                break;
            case AURORA_UNPAUSED:
                dusk::audio::SetPaused(false);
                dusk::game_clock::reset_frame_timer();
                break;
            case AURORA_SDL_EVENT:
                dusk::ui::handle_event(event->sdl);
                dusk::g_imguiConsole.HandleSDLEvent(event->sdl);
                break;
            case AURORA_DISPLAY_SCALE_CHANGED:
                dusk::ImGuiEngine_Initialize(event->windowSize.scale);
                break;
            case AURORA_EXIT:
                goto exit;
            }

            event++;
        }

        eventsDone:;

        if (!aurora_begin_frame()) {
            DuskLog.debug("aurora_begin_frame returned false, skipping draw this frame");
            continue;
        }

        VIWaitForRetrace();

        dusk::lastFrameAuroraStats = *aurora_get_stats();
        mDoGph_gInf_c::updateRenderSize();

        dusk::ui::update();

        const auto pacing = dusk::game_clock::advance_main_loop();
        if (pacing.is_interpolating) {
            if (pacing.sim_ticks_to_run > 0) {
                dusk::frame_interp::begin_frame(dusk::getSettings().game.enableFrameInterpolation, true, 0.0f);
                dusk::frame_interp::set_ui_tick_pending(true);

                for (int sim_tick = 0; sim_tick < pacing.sim_ticks_to_run; ++sim_tick) {
                    dusk::frame_interp::begin_sim_tick();
                    mDoCPd_c::read();
                    dusk::gyro::read(pacing.sim_pace);
                    fapGm_Execute();
                    mDoAud_Execute();
                    dusk::game_clock::commit_sim_tick();
                }
            }

            dusk::frame_interp::begin_frame(dusk::getSettings().game.enableFrameInterpolation, false,
                                            dusk::game_clock::sample_interpolation_step());
            dusk::frame_interp::interpolate();
            dusk::frame_interp::begin_presentation_camera();
            // run draw functions for anything specially marked to handle interp
            fpcM_DrawIterater((fpcM_DrawIteraterFunc)fpcM_Draw);
            cAPIGph_Painter();
            dusk::frame_interp::end_presentation_camera();
            dusk::frame_interp::set_ui_tick_pending(false);
        } else {
            dusk::frame_interp::begin_frame(dusk::FrameInterpMode::Off, true, 0.0f);
            dusk::frame_interp::set_ui_tick_pending(true);

            // Game Inputs
            mDoCPd_c::read();
            dusk::gyro::read(pacing.presentation_dt_seconds);

            // EXECUTE GAME LOGIC & RENDER
            // This calls mDoGph_Painter -> JFWDisplay -> GX Functions
            fapGm_Execute();

            mDoAud_Execute();
        }

        static Limiter main_loop_limiter;
        static double last_fps_setting = 0.0;
        static Limiter::duration_t target_ns = 0;

        if (dusk::getSettings().game.enableFrameInterpolation.getValue() == dusk::FrameInterpMode::Capped && !dusk::getTransientSettings().skipFrameRateLimit) {
            double current_fps = dusk::getSettings().video.maxFrameRate.getValue();
            if (current_fps != last_fps_setting) {
                last_fps_setting = current_fps;
                target_ns = static_cast<Limiter::duration_t>(1'000'000'000.0 / current_fps);
            }

            Limiter::duration_t sleepTime = main_loop_limiter.Sleep(target_ns);
            dusk::frameUsagePct = 100.0f * (1.0f - static_cast<float>(sleepTime) / static_cast<float>(target_ns));
        } else {
            main_loop_limiter.Reset();
        }

#ifdef __SWITCH__
        if (main01_iter <= 10) {
            char b[80];
            snprintf(b, sizeof b, "[main01] iter=%u -> aurora_end_frame\n", main01_iter - 1);
            ::dusk_switch_log(b);
        }
#endif
        aurora_end_frame();
#ifdef __SWITCH__
        if (main01_iter <= 10) {
            char b[80];
            snprintf(b, sizeof b, "[main01] iter=%u end_frame done\n", main01_iter - 1);
            ::dusk_switch_log(b);
        }
#endif


        FrameMark;

#ifdef DUSK_DISCORD
        dusk::discord::run_callbacks();
        dusk::discord::update_presence();
#endif
    } while (dusk::IsRunning);

    exit:;
    DLOG("game-loop exit (IsRunning=false or AURORA_EXIT)");
    dusk::ui::shutdown();
#undef DLOG
}

static bool IsBackendAvailable(AuroraBackend backend) {
    if (backend == BACKEND_AUTO) {
        return true;
    }

    size_t availableBackendCount = 0;
    const AuroraBackend* availableBackends = aurora_get_available_backends(&availableBackendCount);
    for (size_t i = 0; i < availableBackendCount; ++i) {
        if (availableBackends[i] == backend) {
            return true;
        }
    }

    return false;
}

static AuroraBackend ResolveDesiredBackend(const cxxopts::ParseResult& parsedArgOptions) {
    AuroraBackend desiredBackend = BACKEND_AUTO;

    if (parsedArgOptions.count("backend") != 0) {
        const std::string backendArg = parsedArgOptions["backend"].as<std::string>();
        if (!dusk::try_parse_backend(backendArg, desiredBackend)) {
            fmt::print(stderr, "Unknown backend: {}\n", backendArg);
            exit(1);
        }
    } else if (!dusk::try_parse_backend(
                   static_cast<const std::string&>(dusk::getSettings().backend.graphicsBackend),
                   desiredBackend))
    {
        DuskLog.warn("Unknown configured backend '{}', falling back to Auto",
                     static_cast<const std::string&>(dusk::getSettings().backend.graphicsBackend));
        desiredBackend = BACKEND_AUTO;
    }

    if (!IsBackendAvailable(desiredBackend)) {
        DuskLog.warn("Requested backend '{}' is unavailable, falling back to Auto",
                     dusk::backend_name(desiredBackend));
        desiredBackend = BACKEND_AUTO;
    }

    return desiredBackend;
}

static void aurora_imgui_init_callback(const AuroraWindowSize* size) {
    dusk::ImGuiEngine_Initialize(size->scale);
    dusk::ImGuiEngine_AddTextures();
}

static void ApplyCVarOverrides(const cxxopts::OptionValue& option) {
    if (option.count() == 0) {
        return;
    }

    const auto& cVars = option.as<std::vector<std::string>>();
    for (const auto& cvarArg : cVars) {
        const auto sep = cvarArg.find('=');
        if (sep == std::string::npos) {
            DuskLog.fatal("--cvar argument has no '=': '{}'", cvarArg);
            continue;
        }

        const auto name = std::string_view(cvarArg).substr(0, sep);
        const auto value = std::string_view(cvarArg).substr(sep + 1);

        const auto cVar = dusk::config::GetConfigVar(name);
        if (!cVar) {
            DuskLog.fatal("Unknown --cvar name: '{}'", name);
        }

        try {
            cVar->getImpl()->loadFromArg(*cVar, value);
        } catch (const std::exception& e) {
            DuskLog.fatal("Unable to parse: '{}': {}", value, e.what());
        }
    }
}

static constexpr PADDefaultMapping defaultPadMapping = {
    .buttons = {
        {SDL_GAMEPAD_BUTTON_SOUTH, PAD_BUTTON_A},
        {SDL_GAMEPAD_BUTTON_EAST, PAD_BUTTON_B},
        {SDL_GAMEPAD_BUTTON_WEST, PAD_BUTTON_X},
        {SDL_GAMEPAD_BUTTON_NORTH, PAD_BUTTON_Y},
        {SDL_GAMEPAD_BUTTON_START, PAD_BUTTON_START},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_TRIGGER_Z},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_L},
        {PAD_NATIVE_BUTTON_INVALID, PAD_TRIGGER_R},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_BUTTON_UP},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_BUTTON_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_BUTTON_LEFT},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_BUTTON_RIGHT},
    },
    .axes = {
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_POS},
        {{SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_X_NEG},
        // SDL's gamepad y-axis is inverted from GC's
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_POS},
        {{SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_LEFT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_X_NEG},
        // see above
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_NEGATIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_POS},
        {{SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_RIGHT_Y_NEG},
        {{SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_L},
        {{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_INVALID, PAD_AXIS_TRIGGER_R},
    },
};

static bool mainCalled = false;

static u8 selectedLanguage;

u8 OSGetLanguage() {
    return selectedLanguage;
}

static void LanguageInit() {
    // Keep language at 0 (English) if not on a PAL disc.
    // Doubt this matters, but avoid funky shit.
    if (!dusk::version::isRegionPal()) {
        return;
    }

    // Cache this to avoid funky shenanigans.
    selectedLanguage = static_cast<u8>(dusk::getSettings().game.language.getValue());
}

static std::string asset_path(const char* assetName) {
    const char* basePath = SDL_GetBasePath();
    if (basePath != nullptr && basePath[0] != '\0') {
        return std::string(basePath) + "res/" + assetName;
    }
    return std::string("res/") + assetName;
}

static void log_build_info() {
    DuskLog.info("Build: {} (rev {}, built {}, type {})", DUSK_WC_DESCRIBE, DUSK_WC_REVISION, DUSK_WC_DATE, DUSK_BUILD_TYPE);
    DuskLog.info("Platform: {}", DUSK_PLATFORM_NAME);
}

// =========================================================================
// PC ENTRY POINT
// =========================================================================
int game_main(int argc, char* argv[]) {
    // On iOS, when connected to an external monitor, SDLUIKitSceneDelegate scene:willConnectToSession:
    // can call our main function again. Explicitly guard against this reinitialization.
    if (mainCalled) {
        return 0;
    }
    mainCalled = true;

    dusk::registerSettings();
    dusk::config::FinishRegistration();

    cxxopts::ParseResult parsed_arg_options;

    try {
        cxxopts::Options arg_options("Dusklight", "PC Port of a classic adventure game");

        arg_options.add_options()
            ("l,log-level", "Log level from " + std::to_string(AuroraLogLevel::LOG_DEBUG) + " to " + std::to_string(AuroraLogLevel::LOG_FATAL), cxxopts::value<uint8_t>()->default_value("0"))
            ("h,help", "Print usage")
            ("console", "Show the Windows console window for logs", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
            ("dvd", "Path to DVD image file", cxxopts::value<std::string>())
            ("backend", "Graphics API backend to use (auto, d3d12, d3d11, metal, vulkan, null)", cxxopts::value<std::string>())
            ("cvar", "Override configuration variables without modifying config", cxxopts::value<std::vector<std::string>>());

        arg_options.parse_positional({"dvd"});
        arg_options.positional_help("<dvd-image>");
        arg_options.allow_unrecognised_options();

        parsed_arg_options = arg_options.parse(argc, argv);

        if (parsed_arg_options.count("help"))
        {
            printf("%s", (arg_options.help() + "\n").c_str());
            exit(0);
        }
    }
    catch (const cxxopts::exceptions::exception& e) {
        fprintf(stderr, "Argument Error: %s\n", e.what());
        exit(1);
    }

    const auto startupLogLevel =
        static_cast<AuroraLogLevel>(parsed_arg_options["log-level"].as<uint8_t>());
    const auto dataPaths = dusk::data::initialize_data();
    dusk::ConfigPath = dataPaths.userPath;
    dusk::CachePath = dataPaths.cachePath;
    dusk::InitializeFileLogging(dusk::CachePath, startupLogLevel);

    log_build_info();

    dusk::config::LoadFromUserPreferences();
    dusk::i18n::load(dusk::getSettings().backend.uiLanguage.getValue());
    if (dusk::getSettings().game.speedrunMode) {
        dusk::resetForSpeedrunMode();
    }
    ApplyCVarOverrides(parsed_arg_options["cvar"]);
    // Apply the persisted CPU-boost preference (Switch-only; no-op elsewhere) so the
    // user's saved choice is in effect from the first frame.
    dusk::perf::set_boost(dusk::getSettings().video.cpuBoost.getValue(),
                          dusk::getSettings().video.cpuBoostPlus.getValue());
    dusk::crash_reporting::initialize();
    dusk::crash_handler::install();
    // TODO: How to handle this?
    // PADSetDefaultMapping(&defaultPadMapping, PAD_TYPE_STANDARD);

    {
        // Load mappings from https://github.com/mdqinc/SDL_GameControllerDB
        const auto mappingsPath = asset_path("gamecontrollerdb.txt");
        if (SDL_AddGamepadMappingsFromFile(mappingsPath.c_str()) < 0) {
            DuskLog.warn("Failed to load gamecontrollerdb.txt: {}", SDL_GetError());
        }
    }

    // Set SDL metadata for audio mixers and macOS "About" menu
    SDL_SetAppMetadata("Dusklight", DUSK_VERSION_STRING, "dev.twilitrealm.dusk");

    {
        const auto userPathString = dusk::ConfigPath.u8string();
        const auto cachePathString = dusk::CachePath.u8string();
        AuroraConfig config{};
        config.appName = dusk::AppName;
        config.userPath = reinterpret_cast<const char*>(userPathString.c_str());
        // v13: AuroraConfig.configPath was REMOVED. The SQLite pipeline/blob cache now lives at
        // cachePath (aurora cache_path() = g_config.cachePath / "dawn_cache.db"). Leaving cachePath
        // null -> aurora falls back to "sdmc:/aurora", a dir that doesn't exist -> SQLITE_CANTOPEN(14)
        // -> cache disabled -> every shader recompiled every run (the perf killer). Point it at the
        // real cache dir so the bundled/persisted cache opens.
        config.cachePath = reinterpret_cast<const char*>(cachePathString.c_str());
        config.vsync = dusk::getSettings().video.enableVsync;
        config.startFullscreen = dusk::getSettings().video.enableFullscreen;
        config.windowPosX = -1;
        config.windowPosY = -1;
        config.windowWidth = defaultWindowWidth * 2;
        config.windowHeight = defaultWindowHeight * 2;
        config.desiredBackend = ResolveDesiredBackend(parsed_arg_options);
        config.logCallback = &aurora_log_callback;
#ifdef __SWITCH__
        // PERF: on Switch there are no CLI args, so startupLogLevel stays at its
        // default LOG_DEBUG(0) -> NOTHING is filtered and the engine's per-frame
        // [DEBUG|dusk] spam (fapGm_Execute, fpc*, Loading Resource...) each does a
        // double fflush (stdout + SD file) in aurora_log_callback -> stutters/spikes.
        // The upstream filter at aurora/lib/logging.hpp:22 (g_config.logLevel > level
        // -> return) drops them for free when the level is raised. The per-frame DEBUG
        // spam is the ONLY heavy log volume (the stutter source, HW-confirmed); every
        // level above DEBUG drops it, so LOG_WARNING costs the same as FATAL but keeps
        // the genuinely-useful WARNING/ERROR/FATAL lines (no spam, not blind).
        config.logLevel = LOG_WARNING;
#else
        config.logLevel = startupLogLevel;
#endif
        config.mem1Size = 256 * 1024 * 1024;
        config.mem2Size = 24 * 1024 * 1024;
        config.allowJoystickBackgroundEvents = dusk::getSettings().game.allowBackgroundInput;
        config.pauseOnFocusLost = dusk::getSettings().game.pauseOnFocusLost;
        config.imGuiInitCallback = &aurora_imgui_init_callback;
        // v13: AuroraConfig.allowTextureReplacements was REMOVED. Replacements now auto-load from
        // resourcesPath when present; allowTextureDumps only gates dumping. dusk's
        // game.enableTextureReplacements no longer maps to an aurora config flag.
        config.allowTextureDumps = false;
        auroraInfo = aurora_initialize(argc, argv, &config);
    }

    // Register the user's texture-replacement directory from the saved config. Without this the
    // setting persists as enabled but the pack is only ever loaded by the settings-UI toggle
    // (set_enabled -> reload), so a fresh boot renders vanilla until the user re-toggles it.
    dusk::texture_replacements::reload();

#ifdef DUSK_DISCORD
    if (dusk::getSettings().game.enableDiscordPresence) {
        dusk::discord::initialize();
    }
#endif

    VISetWindowTitle(
        fmt::format("Dusklight {} [{}]", DUSK_WC_DESCRIBE, dusk::backend_name(auroraInfo.backend))
        .c_str());

    if (dusk::getSettings().video.lockAspectRatio) {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
    } else {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
    }
#ifdef __SWITCH__
    {
        char b[160];
        snprintf(b, sizeof b, "[dusk] VISetFrameBufferScale(%d) -- internalResolutionScale\n",
                 dusk::getSettings().game.internalResolutionScale.getValue());
        ::dusk_switch_log(b);
    }
#endif
    VISetFrameBufferScale(dusk::ui::internal_resolution_scale(
        dusk::getSettings().game.internalResolutionScale.getValue()));
    switch (dusk::getSettings().game.resampler.getValue()) {
    case dusk::Resampler::Area:
        aurora_set_resampler(SAMPLER_AREA);
        break;
    case dusk::Resampler::Bilinear:
    default:
        aurora_set_resampler(SAMPLER_BILINEAR);
        break;
    }

    dusk::audio::SetMasterVolume(dusk::audio::MasterVolumeToLinear(dusk::getSettings().audio.masterVolume / 100.0f));
    dusk::audio::SetEnableReverb(dusk::getSettings().audio.enableReverb);
    dusk::audio::EnableHrtf = dusk::getSettings().audio.enableHrtf;

    // Run ImGui UI loop if Aurora couldn't initialize a backend
    if (auroraInfo.backend == BACKEND_NULL) {
        launchUILoop();
        dusk::crash_reporting::shutdown();
        dusk::ShutdownFileLogging();
        fflush(stdout);
        fflush(stderr);
#ifdef DUSK_DISCORD
        dusk::discord::shutdown();
#endif
        dusk::ui::shutdown();
        aurora_shutdown();
        return 0;
    }

    dusk::ui::initialize();
    dusk::ui::push_document(std::make_unique<dusk::ui::Overlay>(), true, true);
    dusk::ui::push_document(std::make_unique<dusk::ui::MenuBar>(), false);

    // Invalidate a bad saved isoPath so that Dusklight can't get blocked from starting up.
    // This is only a metadata check; full hash verification is handled by the prelaunch UI.
    bool forcePreLaunchUI = false;
    bool saveConfigBeforePrelaunch = false;

    const std::string p = dusk::getSettings().backend.isoPath;
    dusk::iso::DiscInfo discInfo{};
    if (!p.empty() &&
        dusk::iso::inspect(p.c_str(), discInfo) != dusk::iso::ValidationError::Success)
    {
        DuskLog.warn("Saved DVD image path failed validation, clearing configured path: {}", p);
        dusk::getSettings().backend.isoPath.setValue("");
        dusk::getSettings().backend.isoVerification.setValue(dusk::DiscVerificationState::Unknown);
        forcePreLaunchUI = true;
        saveConfigBeforePrelaunch = true;
    }

#ifdef __SWITCH__
    // #6: accept any GameCube disc image placed in the data folder, regardless of file name or
    // extension. If no valid disc is configured, scan the data dir and adopt the first file that
    // validates as a supported TP disc. iso::inspect uses nod, so GCM/ISO/RVZ/CISO/GCZ/WBFS/WIA/etc.
    // all work, and the game-id check rejects non-disc files (config.json, *.db, saves). See
    // HayatoG/dusklight#6.
    if (dusk::getSettings().backend.isoPath.getValue().empty()) {
        std::error_code scanEc;
        bool found = false;
        if (std::filesystem::is_directory(dusk::ConfigPath, scanEc)) {
            for (const auto& entry : std::filesystem::directory_iterator(dusk::ConfigPath, scanEc)) {
                if (scanEc) {
                    break;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }
                // Skip files too small to be a disc image (config.json, *.controller, cache .db,
                // saves) so only real images are inspected — iso::inspect enforces the same floor.
                std::error_code sizeEc;
                const auto sz = entry.file_size(sizeEc);
                if (sizeEc || sz < (16ull * 1024 * 1024)) {
                    continue;
                }
                const auto candidateU8 = entry.path().u8string();
                const std::string candidate(reinterpret_cast<const char*>(candidateU8.c_str()),
                                            candidateU8.size());
                dusk::iso::DiscInfo scanInfo{};
                if (dusk::iso::inspect(candidate.c_str(), scanInfo) ==
                    dusk::iso::ValidationError::Success) {
                    DuskLog.warn("Auto-detected disc image in data folder: {}", candidate);
                    dusk::getSettings().backend.isoPath.setValue(candidate);
                    dusk::getSettings().backend.isoVerification.setValue(
                        dusk::DiscVerificationState::Unknown);
                    saveConfigBeforePrelaunch = true;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            DuskLog.warn("No compatible disc image found in the data folder — place your game "
                         "(any name/format: .gcm/.iso/.rvz/...) in sdmc:/TwilitRealm/Dusklight/.");
        }
    }
#endif

    std::string dvd_path;
    bool dvd_opened = false;
    if (parsed_arg_options.count("dvd")) {
        dvd_path = parsed_arg_options["dvd"].as<std::string>();
        if (dusk::iso::inspect(dvd_path.c_str(), discInfo) == dusk::iso::ValidationError::Success) {
            DuskLog.info("Loading DVD image from command line: {}", dvd_path);
            dvd_opened = aurora_dvd_open(dvd_path.c_str());
            if (!dvd_opened) {
                DuskLog.warn("Failed to open DVD image from command line: {}, opening prelaunch UI", dvd_path);
                forcePreLaunchUI = true;
            } else {
                dusk::getSettings().backend.isoPath.setValue(dvd_path);
                dusk::getSettings().backend.isoVerification.setValue(
                    dusk::DiscVerificationState::Unknown);
                dusk::config::Save();
                dusk::IsGameLaunched = true;
            }
        } else {
            DuskLog.warn("DVD image from command line failed validation: {}, opening prelaunch UI", dvd_path);
            forcePreLaunchUI = true;
        }
    }

    dusk::iso::log_verification_state(
        dusk::getSettings().backend.isoPath.getValue(),
        dusk::getSettings().backend.isoVerification.getValue());

    if (!dvd_opened) {
        if (dusk::getSettings().backend.isoPath.getValue().empty()) {
            forcePreLaunchUI = true;
        }
        if (forcePreLaunchUI && dusk::getSettings().backend.skipPreLaunchUI.getValue()) {
            DuskLog.warn("Prelaunch UI was disabled with no usable DVD image, enabling prelaunch UI");
            dusk::getSettings().backend.skipPreLaunchUI.setValue(false);
            saveConfigBeforePrelaunch = true;
        }
        if (saveConfigBeforePrelaunch) {
            dusk::config::Save();
        }

        if (!dusk::getSettings().backend.skipPreLaunchUI) {
            dusk::ui::push_document(std::make_unique<dusk::ui::Prelaunch>(), true);

            // pre game launch ui main loop
#ifdef __SWITCH__
            ::dusk_switch_log("[dusk] main: entering launchUILoop\n");
#endif
            const bool launchUIResult = launchUILoop();
#ifdef __SWITCH__
            {
                char b[160];
                snprintf(b, sizeof b, "[dusk] main: launchUILoop returned %d (IsRunning=%d IsGameLaunched=%d)\n",
                         (int)launchUIResult, (int)dusk::IsRunning, (int)dusk::IsGameLaunched);
                ::dusk_switch_log(b);
            }
            // The Prelaunch document remains in dusk::ui::sDocumentStack even
            // after it auto-flips IsGameLaunched. With any_document_visible()
            // still returning true, dusk::ui::input::sync_input_block() calls
            // PADBlockInput(true) on every frame, zeroing out PADRead() for
            // the GameCube pad and freezing the in-game file-select menu
            // (user reported: animation runs but no button works). Close any
            // launcher documents now that the game is taking over input.
            if (launchUIResult) {
                ::dusk_switch_log("[dusk] main: closing launcher documents (PAD unblock)\n");
                for (auto& doc : dusk::ui::get_document_stack()) {
                    // Close only the VISIBLE launcher documents (Prelaunch) — those are what keep
                    // any_document_visible() true and block the GameCube pad. Skip hidden documents
                    // like the MenuBar (pushed show=false at startup): it must survive into gameplay
                    // so the (-) menu has something to toggle (top_document() only returns active docs;
                    // closing the MenuBar here was why top_document()==null in-game and (-) did nothing).
                    if (doc && !doc->closed() && doc->visible()) {
                        doc->hide(true);
                    }
                }
            }
#endif
            if (!launchUIResult) {
                dusk::crash_reporting::shutdown();
                dusk::ShutdownFileLogging();
                fflush(stdout);
                fflush(stderr);
#ifdef DUSK_DISCORD
                dusk::discord::shutdown();
#endif
                dusk::ui::shutdown();
                aurora_shutdown();
                return 0;
            }
        }

        dvd_path = dusk::getSettings().backend.isoPath;

        if (dvd_path.empty()) {
            DuskLog.fatal("No DVD image specified, unable to boot!");
        }
        if (!dusk::IsGameLaunched &&
            dusk::iso::inspect(dvd_path.c_str(), discInfo) != dusk::iso::ValidationError::Success)
        {
            DuskLog.fatal("DVD image failed validation: {}", dvd_path);
        }
        DuskLog.info("Loading DVD image: {}", dvd_path);
        if (!aurora_dvd_open(dvd_path.c_str())) {
            DuskLog.fatal("Failed to open DVD image: {}", dvd_path);
        }

        dusk::IsGameLaunched = true;
    }

#if DUSK_ENABLE_SENTRY_NATIVE
    if (dusk::crash_reporting::get_consent() == dusk::crash_reporting::Consent::Unknown) {
        dusk::ui::push_document(std::make_unique<dusk::ui::CrashReportWindow>());
    }
#endif

    if (!dusk::getSettings().backend.wasPresetChosen) {
#ifdef __SWITCH__
        // Switch: only the Dusklight preset is supported. Apply it silently
        // and persist `wasPresetChosen=true` so the chooser never opens here.
        dusk::ui::apply_preset_dusk_silently();
        dusk::getSettings().backend.wasPresetChosen.setValue(true);
        dusk::config::Save();
        ::dusk_switch_log("[dusk] main: auto-applied Dusklight preset (Switch)\n");
#else
        dusk::ui::push_document(std::make_unique<dusk::ui::PresetWindow>());
#endif
    }

#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: post-prelaunch -> version::init\n");
#endif
    dusk::version::init();
#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: LanguageInit\n");
#endif
    LanguageInit();
#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: OSInit\n");
#endif

    OSInit();

    mDoMain::sPowerOnTime = OSGetTime();

    // Reset Data
    static mDoRstData sResetData = {0};
    mDoRst::setResetData(&sResetData);
    mDoRst::offReset();
    mDoRst::setLogoScnFlag(0);

#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: dComIfG_ct\n");
#endif
    // Global Context Init
    dComIfG_ct();
#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: dComIfG_ct returned\n");
#endif

    // Development Mode
    // mDoMain::developmentMode = 1;  // Force Dev Mode for Debugging
    mDoDvdThd::SyncWidthSound = false;

    OSReport("Starting main01 (Game Loop)...\n");
#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: -> main01\n");
#endif

    main01();
#ifdef __SWITCH__
    ::dusk_switch_log("[dusk] main: main01 returned\n");
#endif

    dusk::MoviePlayerShutdown();

    dusk::crash_reporting::shutdown();
    dusk::ShutdownFileLogging();
    fflush(stdout);
    fflush(stderr);

    mDoMch_Destroy();

    // Notifies all CVs and causes threads to exit
    OSResetSystem(OS_RESET_SHUTDOWN, 0, 0);

#ifdef DUSK_DISCORD
    dusk::discord::shutdown();
#endif
    dusk::ui::shutdown();
    aurora_shutdown();

    return 0;
}


bool JKRHeap::dump_sort() {
    return true;
}

#ifdef __MWERKS__
template <typename T>
JHIComPortManager<T>* JHIComPortManager<T>::instance = nullptr;

template <>
JHIComPortManager<JHICmnMem>* JHIComPortManager<JHICmnMem>::instance = nullptr;

template<>
Z2WolfHowlMgr* JASGlobalInstance<Z2WolfHowlMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2EnvSeMgr* JASGlobalInstance<Z2EnvSeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2FxLineMgr* JASGlobalInstance<Z2FxLineMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2Audience* JASGlobalInstance<Z2Audience>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundObjMgr* JASGlobalInstance<Z2SoundObjMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundInfo* JASGlobalInstance<Z2SoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundInfo* JASGlobalInstance<JAUSoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundNameTable* JASGlobalInstance<JAUSoundNameTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAUSoundTable* JASGlobalInstance<JAUSoundTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISoundInfo* JASGlobalInstance<JAISoundInfo>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundMgr* JASGlobalInstance<Z2SoundMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAIStreamMgr* JASGlobalInstance<JAIStreamMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISeqMgr* JASGlobalInstance<JAISeqMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISeMgr* JASGlobalInstance<JAISeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SpeechMgr2* JASGlobalInstance<Z2SpeechMgr2>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SoundStarter* JASGlobalInstance<Z2SoundStarter>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JAISoundStarter* JASGlobalInstance<JAISoundStarter>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2StatusMgr* JASGlobalInstance<Z2StatusMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SceneMgr* JASGlobalInstance<Z2SceneMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SeqMgr* JASGlobalInstance<Z2SeqMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
Z2SeMgr* JASGlobalInstance<Z2SeMgr>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JASAudioThread* JASGlobalInstance<JASAudioThread>::sInstance JAS_GLOBAL_INSTANCE_INIT;

template<>
JASDefaultBankTable* JASGlobalInstance<JASDefaultBankTable>::sInstance JAS_GLOBAL_INSTANCE_INIT;
#endif // __MWERKS__
