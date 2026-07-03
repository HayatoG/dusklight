#include "settings.hpp"

#include "aurora/gfx.h"
#include "bool_button.hpp"
#include "controller_config.hpp"
#include "dusk/app_info.hpp"
#include "dusk/audio/DuskAudioSystem.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/android_frame_rate.hpp"
#include "dusk/config.hpp"
#include "dusk/hotkeys.h"
#include "dusk/data.hpp"
#include "dusk/i18n.hpp"
#include "dusk/file_select.hpp"
#include "dusk/imgui/ImGuiEngine.hpp"
#include "dusk/io.hpp"
#include "dusk/livesplit.h"
#include "dusk/perf.hpp"
#include "dusk/discord_presence.hpp"
#include "graphics_tuner.hpp"
#include "m_Do/m_Do_main.h"
#include "menu_bar.hpp"
#include "modal.hpp"
#include "number_button.hpp"
#include "menu_bar.hpp"
#include "pane.hpp"
#include "prelaunch.hpp"
#include "touch_controls_editor.hpp"
#include "ui.hpp"

#include <aurora/lib/window.hpp>
#include <SDL3/SDL_filesystem.h>
#include <fmt/format.h>

#if DUSK_ENABLE_SENTRY_NATIVE
#include "dusk/crash_reporting.h"
#endif

#include <algorithm>
#include <filesystem>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_ANDROID) || defined(__ANDROID__) || \
    (defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST)
#define TOUCH_CONTROLS_AVAILABLE true
#else
#define TOUCH_CONTROLS_AVAILABLE false
#endif

namespace dusk::ui {
namespace {

constexpr std::array kLanguageNames = {
    "English",
    "German",
    "French",
    "Spanish",
    "Italian",
};

// Launcher/menu UI languages (index = backend.uiLanguage). ASCII display names to avoid any font
// glyph gaps in the selector itself; the translated strings live in res/lang/<code>.json.
constexpr std::array kUiLanguageNames = {
    "English",
    "Portugues",
    "Espanol",
};

constexpr std::array kCardFileTypes = {
    "Card Image",
    "GCI Folder",
};

constexpr std::array kFpsOverlayCornerNames = {
    "Top Left",
    "Top Right",
    "Bottom Left",
    "Bottom Right",
};

constexpr std::array kInterpolationModes = {
    "Off",
    "Capped",
    "Unlimited",
};

constexpr std::array kGyroInputModeLabels = {
    "Sensor",
    "Mouse",
};

constexpr std::array kMenuScalingModeLabels = {
    "GameCube",
    "Wii",
    "Dusklight",
};

constexpr std::array kMagicArmorModes = {
    "Normal",
    "On Damage",
    "Double Defense",
    "Invincible",
    "Cosmetic",
};

bool try_parse_backend(std::string_view backend, AuroraBackend& outBackend) {
    if (backend == "auto") {
        outBackend = BACKEND_AUTO;
        return true;
    }
    if (backend == "d3d11") {
        outBackend = BACKEND_D3D11;
        return true;
    }
    if (backend == "d3d12") {
        outBackend = BACKEND_D3D12;
        return true;
    }
    if (backend == "metal") {
        outBackend = BACKEND_METAL;
        return true;
    }
    if (backend == "vulkan") {
        outBackend = BACKEND_VULKAN;
        return true;
    }
    if (backend == "opengl") {
        outBackend = BACKEND_OPENGL;
        return true;
    }
    if (backend == "opengles") {
        outBackend = BACKEND_OPENGLES;
        return true;
    }
    if (backend == "webgpu") {
        outBackend = BACKEND_WEBGPU;
        return true;
    }
    if (backend == "null") {
        outBackend = BACKEND_NULL;
        return true;
    }

    return false;
}

std::string_view backend_name(AuroraBackend backend) {
    switch (backend) {
    default:
        return "Auto";
    case BACKEND_D3D12:
        return "D3D12";
    case BACKEND_D3D11:
        return "D3D11";
    case BACKEND_METAL:
        return "Metal";
    case BACKEND_VULKAN:
        return "Vulkan";
    case BACKEND_OPENGL:
        return "OpenGL";
    case BACKEND_OPENGLES:
        return "OpenGL ES";
    case BACKEND_WEBGPU:
        return "WebGPU";
    case BACKEND_NULL:
        return "Null";
    }
}

std::string_view backend_id(AuroraBackend backend) {
    switch (backend) {
    default:
        return "auto";
    case BACKEND_D3D12:
        return "d3d12";
    case BACKEND_D3D11:
        return "d3d11";
    case BACKEND_METAL:
        return "metal";
    case BACKEND_VULKAN:
        return "vulkan";
    case BACKEND_OPENGL:
        return "opengl";
    case BACKEND_OPENGLES:
        return "opengles";
    case BACKEND_WEBGPU:
        return "webgpu";
    case BACKEND_NULL:
        return "null";
    }
}

std::vector<AuroraBackend> available_backends() {
    std::vector<AuroraBackend> backends;
    backends.emplace_back(BACKEND_AUTO);
    size_t backendCount = 0;
    const AuroraBackend* raw = aurora_get_available_backends(&backendCount);
    for (size_t i = 0; i < backendCount; ++i) {
        // Do not expose NULL
        if (raw[i] != BACKEND_NULL) {
            backends.emplace_back(raw[i]);
        }
    }
    return backends;
}

AuroraBackend configured_backend() {
    AuroraBackend configuredBackend = BACKEND_AUTO;
    const auto configuredId = getSettings().backend.graphicsBackend.getValue();
    if (!try_parse_backend(configuredId, configuredBackend)) {
        configuredBackend = BACKEND_AUTO;
    }
    return configuredBackend;
}

void reset_for_speedrun_mode() {
    mDoMain::developmentMode = -1;

    getSettings().game.enableTurboKeybind.setSpeedrunValue(false);

    getSettings().game.damageMultiplier.setSpeedrunValue(1);
    getSettings().game.instantDeath.setSpeedrunValue(false);
    getSettings().game.noHeartDrops.setSpeedrunValue(false);
    getSettings().game.autoSave.setSpeedrunValue(false);
    getSettings().game.sunsSong.setSpeedrunValue(false);

    getSettings().game.infiniteHearts.setSpeedrunValue(false);
    getSettings().game.infiniteArrows.setSpeedrunValue(false);
    getSettings().game.infiniteSeeds.setSpeedrunValue(false);
    getSettings().game.infiniteBombs.setSpeedrunValue(false);
    getSettings().game.infiniteOil.setSpeedrunValue(false);
    getSettings().game.infiniteOxygen.setSpeedrunValue(false);
    getSettings().game.infiniteRupees.setSpeedrunValue(false);
    getSettings().game.enableIndefiniteItemDrops.setSpeedrunValue(false);
    getSettings().game.moonJump.setSpeedrunValue(false);
    getSettings().game.superClawshot.setSpeedrunValue(false);
    getSettings().game.alwaysGreatspin.setSpeedrunValue(false);
    getSettings().game.enableFastIronBoots.setSpeedrunValue(false);
    getSettings().game.canTransformAnywhere.setSpeedrunValue(false);
    getSettings().game.fastRoll.setSpeedrunValue(false);
    getSettings().game.fastSpinner.setSpeedrunValue(false);
    getSettings().game.armorRupeeDrain.setSpeedrunValue(MagicArmorMode::NORMAL);
    getSettings().game.invincibleEnemies.setSpeedrunValue(false);

    getSettings().game.pauseOnFocusLost.setSpeedrunValue(false);
    aurora_set_pause_on_focus_lost(false);

    getSettings().backend.enableAdvancedSettings.setSpeedrunValue(false);
    getSettings().game.recordingMode.setSpeedrunValue(false);
    getSettings().game.debugFlyCam.setSpeedrunValue(false);
}

void clear_speedrun_overrides() {
    config::EnumerateRegistered([](config::ConfigVarBase& cvar) {
        cvar.clearSpeedrunOverride();
    });
}

void restore_from_speedrun_mode() {
    clear_speedrun_overrides();
    aurora_set_pause_on_focus_lost(getSettings().game.pauseOnFocusLost.getValue());
}

std::filesystem::path normalized_display_path(const std::filesystem::path& path) {
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return normalized;
    }

    normalized = std::filesystem::absolute(path, ec);
    if (!ec) {
        return normalized.lexically_normal();
    }

    return path.lexically_normal();
}

std::filesystem::path user_home_path() {
    const char* homePath = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (homePath == nullptr || homePath[0] == '\0') {
        return {};
    }
    return std::filesystem::path{reinterpret_cast<const char8_t*>(homePath)};
}

Rml::String abbreviated_data_path_string() {
    const auto path = data::configured_data_path();
    const auto homePath = user_home_path();
    if (path.empty() || homePath.empty()) {
        return io::fs_path_to_string(path);
    }

    const auto normalizedPath = normalized_display_path(path);
    const auto normalizedHome = normalized_display_path(homePath);
    if (normalizedPath == normalizedHome) {
        return "~";
    }

    const auto relativePath = normalizedPath.lexically_relative(normalizedHome);
    if (!relativePath.empty() && !relativePath.is_absolute()) {
        const auto it = relativePath.begin();
        if (it == relativePath.end() || *it != "..") {
            return io::fs_path_to_string(std::filesystem::path{"~"} / relativePath);
        }
    }

    return io::fs_path_to_string(path);
}

Rml::String configured_data_path_display_name() {
    const auto path = abbreviated_data_path_string();
    if (path.empty()) {
        return dusk::i18n::tr("settings.common.none");
    }

    auto display = display_name_for_path(path);
    if (display.empty()) {
        return path;
    }
    return display;
}

class DataFolderPathText : public Component {
public:
    explicit DataFolderPathText(Rml::Element* parent) : Component(append(parent, "div")) {}

    void update() override {
        const Rml::String rml = "<span class=\"data-folder-current\">" +
                                dusk::i18n::tr("settings.prelaunch.data_folder.current") + "<br/>" +
                                escape(abbreviated_data_path_string()) + "</span>";
        if (rml != mCurrentRml) {
            mRoot->SetInnerRML(rml);
            mCurrentRml = rml;
        }
        Component::update();
    }

private:
    Rml::String mCurrentRml;
};

void show_data_folder_error_modal(std::string_view message) {
    auto dismiss = [](Modal& modal) {
        mDoAud_seStartMenu(kSoundWindowClose);
        modal.pop();
    };
    push_document(std::make_unique<Modal>(Modal::Props{
        .title = dusk::i18n::tr("settings.prelaunch.data_folder.error_title"),
        .bodyRml = escape(message),
        .actions =
            {
                ModalAction{
                    .label = dusk::i18n::tr("settings.common.ok"),
                    .onPressed = dismiss,
                },
            },
        .onDismiss = dismiss,
        .icon = "warning",
    }));
    if (auto* doc = top_document()) {
        doc->focus();
    }
}

void data_folder_dialog_callback(void*, const char* path, const char* error) {
    if (error != nullptr) {
        show_data_folder_error_modal(error);
        return;
    }
    if (path == nullptr) {
        return;
    }

    std::string dataPathError;
    if (data::set_custom_data_path(path, &dataPathError)) {
        mDoAud_seStartMenu(kSoundItemChange);
        return;
    }

    if (dataPathError.empty()) {
        dataPathError = dusk::i18n::tr_fmt("settings.prelaunch.data_folder.use_error", AppName);
    }
    show_data_folder_error_modal(dataPathError);
}

int float_setting_percent(ConfigVar<float>& var) {
    return static_cast<int>(var.getValue() * 100.0f + 0.5f);
}

bool gyro_enabled() {
    return getSettings().game.enableGyroAim || getSettings().game.enableGyroRollgoal;
}

struct ConfigBoolProps {
    Rml::String key;
    Rml::String icon;
    Rml::String helpText;
    std::function<void(bool)> onChange;
    std::function<bool()> isDisabled;
};

SelectButton& config_bool_select(
    Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var, ConfigBoolProps props) {
    auto& button = leftPane.add_child<BoolButton>(BoolButton::Props{
        .key = std::move(props.key),
        .icon = std::move(props.icon),
        .getValue = [&var] { return var.getValue(); },
        .setValue =
            [&var, callback = std::move(props.onChange)](bool value) {
                if (value == var.getValue()) {
                    return;
                }
                var.setValue(value);
                config::Save();
                if (callback) {
                    callback(value);
                }
            },
        .isDisabled = std::move(props.isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
    });
    leftPane.register_control(
        button, rightPane, [helpText = std::move(props.helpText)](Pane& pane) {
            pane.clear();
            pane.add_rml(helpText);
        });
    return button;
}

void add_speedrun_disabled_option(Pane& leftPane, Pane& rightPane, ConfigVar<bool>& var,
    const Rml::String& key, const Rml::String& helpText) {
    config_bool_select(leftPane, rightPane, var, {
        .key = key,
        .helpText = helpText,
        .isDisabled = [] { return getSettings().game.speedrunMode; },
    });
}

SelectButton& config_percent_select(Pane& leftPane, Pane& rightPane, ConfigVar<float>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}) {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return float_setting_percent(var); },
        .setValue =
            [&var, min, max](int value) {
                var.setValue(std::clamp(value, min, max) / 100.0f);
                config::SaveDeferred();
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = "%",
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_rml(helpText);
    });
    return button;
}

SelectButton& config_int_select(Pane& leftPane, Pane& rightPane, ConfigVar<int>& var,
    Rml::String key, Rml::String helpText, int min, int max, int step = 5,
    std::function<bool()> isDisabled = {}, std::function<void(int)> onChange = {},
    std::string suffix = "") {
    auto& button = leftPane.add_child<NumberButton>(NumberButton::Props{
        .key = std::move(key),
        .getValue = [&var] { return var; },
        .setValue =
            [&var, min, max, callback = std::move(onChange)](int value) {
                const int clampedValue = std::clamp(value, min, max);
                var.setValue(clampedValue);
                config::SaveDeferred();
                if (callback) {
                    callback(clampedValue);
                }
            },
        .isDisabled = std::move(isDisabled),
        .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
        .min = min,
        .max = max,
        .step = step,
        .suffix = suffix,
    });
    leftPane.register_control(button, rightPane, [helpText = std::move(helpText)](Pane& pane) {
        pane.clear();
        pane.add_text(helpText);
    });
    return button;
}

template <typename T>
void graphics_tuner_control(Window& window, Pane& leftPane, Pane& rightPane, ConfigVar<T>& var,
    const GraphicsTunerProps& props, bool prelaunch) {
    leftPane.register_control(
        leftPane
            .add_select_button({
                .key = props.title,
                .getValue =
                    [&var, option = props.option] {
                        if constexpr (std::is_same_v<T, float>) {
                            return format_graphics_setting_value(
                                option, float_setting_percent(var));
                        } else {
                            return format_graphics_setting_value(
                                option, static_cast<int>(var.getValue()));
                        }
                    },
                .isModified = [&var] { return var.getValue() != var.getDefaultValue(); },
                .submit = false,
            })
            .on_nav_command([&window, props, prelaunch](Rml::Event&, NavCommand cmd) {
                if (cmd == NavCommand::Confirm || cmd == NavCommand::Left ||
                    cmd == NavCommand::Right) {
                    window.push(std::make_unique<GraphicsTuner>(props, prelaunch));
                    return true;
                }
                return false;
            }),
        rightPane, [helpText = props.helpText](Pane& pane) {
            pane.clear();
            pane.add_text(helpText);
        });
}

}  // namespace

SettingsWindow::SettingsWindow(bool prelaunch) : mPrelaunch(prelaunch) {
    if (prelaunch) {
        mSuppressNavFallback = true;
        add_tab(dusk::i18n::tr("settings.prelaunch.tab"), [this](Rml::Element* content) {
            auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
            auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

            leftPane.register_control(
                leftPane
                    .add_select_button({
                        .key = dusk::i18n::tr("settings.prelaunch.disc_image.key"),
                        .getValue =
                            [] {
                                const auto& path = prelaunch_state().configuredDiscPath;
                                std::string display;
                                if (path.empty()) {
                                    display = dusk::i18n::tr("settings.common.none");
                                } else {
                                    display = display_name_for_path(path);
                                    if (display.empty()) {
                                        display = path;
                                    }
                                }
                                return display;
                            },
                        .isModified =
                            [] {
                                const auto& state = prelaunch_state();
                                const auto& active = state.activeDiscPath;
                                return !active.empty() && state.configuredDiscPath != active;
                            },
                    })
                    .on_pressed([] { open_iso_picker(); }),
                rightPane, [](Pane& pane) {
                    pane.add_rml(dusk::i18n::tr("settings.prelaunch.disc_image.help"));
                });
#if DUSK_CAN_CHANGE_DATA_FOLDER
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = dusk::i18n::tr("settings.prelaunch.data_folder.key"),
                    .getValue = [] { return configured_data_path_display_name(); },
                    .isModified = [] { return data::is_data_path_restart_pending(); },
                }),
                rightPane, [](Pane& pane) {
                    pane.add_text(dusk::i18n::tr("settings.prelaunch.data_folder.help"));
                    pane.add_child<DataFolderPathText>();
#if DUSK_CAN_OPEN_DATA_FOLDER
                    pane.add_button(dusk::i18n::tr("settings.prelaunch.data_folder.open")).on_pressed([] {
                        if (data::open_data_path()) {
                            mDoAud_seStartMenu(kSoundClick);
                        }
                    });
#endif
                    pane.add_button(dusk::i18n::tr("settings.prelaunch.data_folder.change")).on_pressed([] {
                        const auto defaultLocation =
                            io::fs_path_to_string(data::configured_data_path());
                        ShowFolderSelect(&data_folder_dialog_callback, nullptr,
                            aurora::window::get_sdl_window(),
                            defaultLocation.empty() ? nullptr : defaultLocation.c_str());
                    });
#if defined(_WIN32)
                    pane.add_button(dusk::i18n::tr("settings.prelaunch.data_folder.portable")).on_pressed([] {
                        if (data::set_portable_data_path()) {
                            mDoAud_seStartMenu(kSoundItemChange);
                        }
                    });
#endif
                    pane.add_button({
                        .text = dusk::i18n::tr("settings.prelaunch.data_folder.reset"),
                        .isDisabled = [] { return data::is_default_data_path(); },
                    }).on_pressed([] {
                        if (data::reset_data_path()) {
                            mDoAud_seStartMenu(kSoundItemChange);
                        }
                    });
                    pane.add_rml(dusk::i18n::tr("settings.prelaunch.data_folder.migrate_note"));
                });
#endif
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = dusk::i18n::tr("settings.prelaunch.language.key"),
                    .getValue =
                        [] {
                            const auto& state = prelaunch_state();
                            if (!state.configuredDiscCanLaunch || !state.configuredDiscInfo.isPal) {
                                return dusk::i18n::tr_index("settings.language", 0);
                            }
                            const u8 idx = static_cast<u8>(getSettings().game.language.getValue());
                            return dusk::i18n::tr_index("settings.language", idx);
                        },
                    .isDisabled =
                        [] {
                            const auto& state = prelaunch_state();
                            return !state.configuredDiscCanLaunch ||
                                   !state.configuredDiscInfo.isPal;
                        },
                    .isModified =
                        [] {
                            return getSettings().game.language.getValue() !=
                                   prelaunch_state().initialLanguage;
                        },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < kLanguageNames.size(); i++) {
                        pane.add_button({
                                            .text = dusk::i18n::tr_index("settings.language", i),
                                            .isSelected =
                                                [i] {
                                                    return getSettings().game.language.getValue() ==
                                                           static_cast<GameLanguage>(i);
                                                },
                                        })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().game.language.setValue(static_cast<GameLanguage>(i));
                                config::Save();
                            });
                    }
                    pane.add_rml(dusk::i18n::tr("settings.prelaunch.language.restart_note"));
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = dusk::i18n::tr("settings.ui_language"),
                    .getValue =
                        [] {
                            return Rml::String{
                                kUiLanguageNames[getSettings().backend.uiLanguage.getValue()]};
                        },
                    .isModified = [] { return getSettings().backend.uiLanguage.getValue() != 0; },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < static_cast<int>(kUiLanguageNames.size()); i++) {
                        pane.add_button({
                                            .text = Rml::String{kUiLanguageNames[i]},
                                            .isSelected =
                                                [i] {
                                                    return getSettings().backend.uiLanguage.getValue() ==
                                                           i;
                                                },
                                        })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.uiLanguage.setValue(i);
                                config::Save();
                                dusk::i18n::load(i);
                            });
                    }
                    pane.add_rml(dusk::i18n::tr("settings.ui_language.note"));
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = dusk::i18n::tr("settings.prelaunch.graphics_backend.key"),
                    .getValue = [] { return Rml::String{backend_name(configured_backend())}; },
                    .isModified =
                        [] {
                            return getSettings().backend.graphicsBackend.getValue() !=
                                   prelaunch_state().initialGraphicsBackend;
                        },
                }),
                rightPane, [](Pane& pane) {
                    const auto availableBackends = available_backends();
                    for (const auto backend : availableBackends) {
                        pane
                            .add_button({
                                .text = Rml::String{backend_name(backend)},
                                .isSelected = [backend] { return configured_backend() == backend; },
                            })
                            .on_pressed([backend] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.graphicsBackend.setValue(
                                    std::string{backend_id(backend)});
                                config::Save();
                            });
                    }
                    pane.add_rml(dusk::i18n::tr("settings.prelaunch.graphics_backend.restart_note"));
                });
            leftPane.register_control(
                leftPane.add_select_button({
                    .key = dusk::i18n::tr("settings.prelaunch.save_file_type.key"),
                    .getValue =
                        [] {
                            return dusk::i18n::tr_index("settings.card_file_type",
                                getSettings().backend.cardFileType.getValue());
                        },
                    .isModified =
                        [] {
                            return getSettings().backend.cardFileType.getValue() !=
                                   prelaunch_state().initialCardFileType;
                        },
                }),
                rightPane, [](Pane& pane) {
                    for (int i = 0; i < kCardFileTypes.size(); i++) {
                        pane
                            .add_button({
                                .text = dusk::i18n::tr_index("settings.card_file_type", i),
                                .isSelected =
                                    [i] {
                                        return getSettings().backend.cardFileType.getValue() == i;
                                    },
                            })
                            .on_pressed([i] {
                                mDoAud_seStartMenu(kSoundItemChange);
                                getSettings().backend.cardFileType.setValue(i);
                                config::Save();
                            });
                    }
                });
        });
    }

    add_tab(dusk::i18n::tr("settings.video.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(dusk::i18n::tr("settings.section.display"));

        leftPane.register_control(leftPane.add_button(dusk::i18n::tr("settings.video.toggle_fullscreen")).on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(!getSettings().video.enableFullscreen);
            VISetWindowFullscreen(getSettings().video.enableFullscreen);
            config::Save();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        leftPane.register_control(leftPane.add_button(dusk::i18n::tr("settings.video.restore_window_size")).on_pressed([] {
            mDoAud_seStartMenu(kSoundItemChange);
            getSettings().video.enableFullscreen.setValue(false);
            VISetWindowFullscreen(false);
            VISetWindowSize(FB_WIDTH * 2, FB_HEIGHT * 2);
            VICenterWindow();
        }),
            rightPane, [](Pane& pane) { pane.clear(); });
        config_bool_select(leftPane, rightPane, getSettings().video.enableVsync,
            {
                .key = dusk::i18n::tr("settings.video.vsync.key"),
                .helpText = dusk::i18n::tr("settings.video.vsync.help"),
                .onChange = [](bool value) { aurora_enable_vsync(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().video.lockAspectRatio,
            {
                .key = dusk::i18n::tr("settings.video.lock_aspect.key"),
                .helpText = dusk::i18n::tr("settings.video.lock_aspect.help"),
                .onChange =
                    [](bool value) {
                        AuroraSetViewportPolicy(
                            value ? AURORA_VIEWPORT_FIT : AURORA_VIEWPORT_STRETCH);
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.pauseOnFocusLost,
            {
                .key = dusk::i18n::tr("settings.video.pause_focus_lost.key"),
                .helpText = dusk::i18n::tr("settings.video.pause_focus_lost.help"),
                .onChange = [](bool value) { aurora_set_pause_on_focus_lost(value); },
                .isDisabled = [] { return IsMobile || getSettings().game.speedrunMode; },
            });
        leftPane.register_control(
            leftPane.add_select_button({
                .key = dusk::i18n::tr("settings.video.fps_counter.key"),
                .getValue =
                    [] {
                        if (!getSettings().video.enableFpsOverlay.getValue()) {
                            return dusk::i18n::tr("settings.common.off");
                        }
                        const int idx = getSettings().video.fpsOverlayCorner.getValue();
                        return dusk::i18n::tr_index("settings.fps_corner", idx);
                    },
                .isModified =
                    [] {
                        const auto& enable = getSettings().video.enableFpsOverlay;
                        const auto& corner = getSettings().video.fpsOverlayCorner;
                        return enable.getValue() != enable.getDefaultValue() ||
                               (enable.getValue() && corner.getValue() != corner.getDefaultValue());
                    },
            }),
            rightPane, [](Pane& pane) {
                pane.add_button(
                        {
                            .text = dusk::i18n::tr("settings.common.off"),
                            .isSelected =
                                [] { return !getSettings().video.enableFpsOverlay.getValue(); },
                        })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        getSettings().video.enableFpsOverlay.setValue(false);
                        config::Save();
                    });
                for (int i = 0; i < static_cast<int>(kFpsOverlayCornerNames.size()); ++i) {
                    pane.add_button(
                            {
                                .text = dusk::i18n::tr_index("settings.fps_corner", i),
                                .isSelected =
                                    [i] {
                                        return getSettings().video.enableFpsOverlay.getValue() &&
                                               getSettings().video.fpsOverlayCorner.getValue() == i;
                                    },
                            })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().video.enableFpsOverlay.setValue(true);
                            getSettings().video.fpsOverlayCorner.setValue(i);
                            config::Save();
                        });
                }
                pane.add_rml(dusk::i18n::tr("settings.video.fps_counter.help"));
            });
#ifdef __SWITCH__
        config_bool_select(leftPane, rightPane, getSettings().video.cpuBoost,
            {
                .key = dusk::i18n::tr("settings.video.cpu_boost.key"),
                .helpText = dusk::i18n::tr("settings.video.cpu_boost.help"),
                .onChange =
                    [](bool value) {
                        // Mutually exclusive with Boost+: enabling this drops the Boost+ tier so the
                        // two toggles act like one setting (turning a toggle off always lowers the
                        // clocks instead of the other tier silently keeping them pinned).
                        if (value) {
                            getSettings().video.cpuBoostPlus.setValue(false);
                        }
                        dusk::perf::set_boost(value, getSettings().video.cpuBoostPlus.getValue());
                        config::Save();
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().video.cpuBoostPlus,
            {
                .key = dusk::i18n::tr("settings.video.cpu_boost_plus.key"),
                .helpText = dusk::i18n::tr("settings.video.cpu_boost_plus.help"),
                .onChange =
                    [](bool value) {
                        // Mutually exclusive with CPU + GPU Boost (Boost+ is the stronger tier).
                        if (value) {
                            getSettings().video.cpuBoost.setValue(false);
                        }
                        dusk::perf::set_boost(getSettings().video.cpuBoost.getValue(), value);
                        config::Save();
                    },
            });
#endif
        config_bool_select(leftPane, rightPane, getSettings().video.rememberWindowSize,
            {
                .key = dusk::i18n::tr("settings.video.remember_window_size.key"),
                .helpText = dusk::i18n::tr("settings.video.remember_window_size.help"),
                .onChange =
                    [](bool value) {
                        if (value && !dusk::getSettings().video.enableFullscreen) {
                            const auto windowSize = aurora::window::get_window_size();
                            dusk::getSettings().video.lastWindowWidth.setValue(windowSize.width);
                            dusk::getSettings().video.lastWindowHeight.setValue(windowSize.height);
                            dusk::config::Save();
                        }
                    },
                .isDisabled = [] { return IsMobile; },
            });
        leftPane.add_section(dusk::i18n::tr("settings.section.resolution"));
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.internalResolutionScale,
            GraphicsTunerProps{
                .option = GraphicsOption::InternalResolution,
                .title = dusk::i18n::tr("settings.video.internal_resolution.key"),
                .helpText = dusk::i18n::tr("settings.video.internal_resolution.help"),
                .valueMin = 0,
                // 0=Auto 1=360p 2=480p 3=540p 4=720p 5=768p 6=810p 7=900p 8=1080p
                // (keep in sync with kPresets in graphics_tuner.cpp)
                .valueMax = 8,
                .defaultValue = 0,
            }, mPrelaunch);
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.shadowResolutionMultiplier,
            GraphicsTunerProps{
                .option = GraphicsOption::ShadowResolution,
                .title = dusk::i18n::tr("settings.video.shadow_resolution.key"),
                .helpText = dusk::i18n::tr("settings.video.shadow_resolution.help"),
                .valueMin = 1,
                .valueMax = 8,
                .defaultValue = 1,
            }, mPrelaunch);
        config_bool_select(leftPane, rightPane, getSettings().game.disableShadows,
            {
                .key = dusk::i18n::tr("settings.video.disable_shadows.key"),
                .helpText = dusk::i18n::tr("settings.video.disable_shadows.help"),
            });
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.resampler,
            GraphicsTunerProps{
                .option = GraphicsOption::Resampler,
                .title = dusk::i18n::tr("settings.video.output_resampling.key"),
                .helpText = dusk::i18n::tr("settings.video.output_resampling.help"),
                .valueMin = static_cast<int>(Resampler::Bilinear),
                .valueMax = static_cast<int>(Resampler::Area),
                .defaultValue = static_cast<int>(Resampler::Bilinear),
            }, mPrelaunch);

        leftPane.add_section(dusk::i18n::tr("settings.section.post_processing"));
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.bloomMode,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMode,
                .title = dusk::i18n::tr("settings.video.bloom.key"),
                .helpText = dusk::i18n::tr("settings.video.bloom.help"),
                .valueMin = static_cast<int>(BloomMode::Off),
                .valueMax = static_cast<int>(BloomMode::Dusk),
                .defaultValue = static_cast<int>(BloomMode::Classic),
            }, mPrelaunch);
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.bloomMultiplier,
            GraphicsTunerProps{
                .option = GraphicsOption::BloomMultiplier,
                .title = dusk::i18n::tr("settings.video.bloom_brightness.key"),
                .helpText = dusk::i18n::tr("settings.video.bloom_brightness.help"),
                .valueMin = 0,
                .valueMax = 100,
                .defaultValue = 100,
                .step = 10,
            },
            mPrelaunch);
        graphics_tuner_control(*this, leftPane, rightPane, getSettings().game.depthOfFieldMode,
            GraphicsTunerProps{
                .option = GraphicsOption::DepthOfFieldMode,
                .title = dusk::i18n::tr("settings.video.depth_of_field.key"),
                .helpText = dusk::i18n::tr("settings.video.depth_of_field.help"),
                .valueMin = static_cast<int>(DepthOfFieldMode::Off),
                .valueMax = static_cast<int>(DepthOfFieldMode::Dusk),
                .defaultValue = static_cast<int>(DepthOfFieldMode::Classic),
            },
            mPrelaunch);

        leftPane.add_section(dusk::i18n::tr("settings.section.rendering"));
        graphics_tuner_control(*this, leftPane, rightPane,
            getSettings().game.enableTextureReplacements,
            GraphicsTunerProps{
                .option = GraphicsOption::TextureReplacements,
                .title = dusk::i18n::tr("settings.video.texture_replacements.key"),
                .helpText = dusk::i18n::tr("settings.video.texture_replacements.help"),
                .valueMin = static_cast<int>(false),
                .valueMax = static_cast<int>(true),
                .defaultValue = static_cast<int>(false),
            },
            mPrelaunch);
        leftPane.register_control(
            leftPane.add_select_button({
                .key = dusk::i18n::tr("settings.video.unlock_framerate.key"),
                .getValue =
                    [] {
                        return dusk::i18n::tr_index("settings.interpolation_mode",
                            static_cast<u8>(getSettings().game.enableFrameInterpolation.getValue()));
                    },
                .isModified =
                    [] {
                        return getSettings().game.enableFrameInterpolation.getValue() !=
                               getSettings().game.enableFrameInterpolation.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < kInterpolationModes.size(); i++) {
                    pane.add_button({
                            .text = dusk::i18n::tr_index("settings.interpolation_mode", i),
                            .isSelected =
                                [i] {
                                    return getSettings().game.enableFrameInterpolation.getValue() == static_cast<FrameInterpMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.enableFrameInterpolation.setValue(static_cast<FrameInterpMode>(i));
                            android::update_surface_frame_rate();
                            config::Save();
                        });
                }
                pane.add_rml(dusk::i18n::tr("settings.video.unlock_framerate.help"));
            });
        config_int_select(leftPane, rightPane, getSettings().video.maxFrameRate,
            dusk::i18n::tr("settings.video.framerate_cap.key"),
            dusk::i18n::tr("settings.video.framerate_cap.help"), 30, 540, 1,
            [] { return getSettings().game.enableFrameInterpolation.getValue() != FrameInterpMode::Capped; },
            [](int) { android::update_surface_frame_rate(); });
        config_bool_select(leftPane, rightPane, getSettings().game.enableMapBackground,
            {
                .key = dusk::i18n::tr("settings.video.minimap_shadows.key"),
                .helpText = dusk::i18n::tr("settings.video.minimap_shadows.help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().game.disableCutscenePillarboxing,
            {
                .key = dusk::i18n::tr("settings.video.disable_pillarboxing.key"),
                .helpText = dusk::i18n::tr("settings.video.disable_pillarboxing.help"),
            });
    });

    add_tab(dusk::i18n::tr("settings.input.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText, std::function<bool()> isDisabled = {}) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                    .isDisabled = std::move(isDisabled),
                });
        };

        leftPane.add_section(dusk::i18n::tr("settings.section.inputs"));
        leftPane.register_control(leftPane.add_button(dusk::i18n::tr("settings.input.configure_inputs.key")).on_pressed([this] {
            push(std::make_unique<ControllerConfigWindow>(mPrelaunch));
        }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(dusk::i18n::tr("settings.input.configure_inputs.help"));
            });
        config_bool_select(leftPane, rightPane, getSettings().game.allowBackgroundInput,
            {
                .key = dusk::i18n::tr("settings.input.background_inputs.key"),
                .helpText = dusk::i18n::tr("settings.input.background_inputs.help"),
                .onChange = [](bool value) { aurora_set_background_input(value); },
            });

#if TOUCH_CONTROLS_AVAILABLE
        leftPane.add_section(dusk::i18n::tr("settings.section.touch"));
        addOption(dusk::i18n::tr("settings.input.touch_controls.key"), getSettings().game.enableTouchControls,
            dusk::i18n::tr("settings.input.touch_controls.help"));
        auto& customizeTouchLayout = leftPane.add_button(ControlledButton::Props{
            .text = dusk::i18n::tr("settings.input.customize_layout.key"),
            .isDisabled = [] { return !getSettings().game.enableTouchControls; },
        });
        leftPane.register_control(customizeTouchLayout.on_pressed(
                                      [this] { push(std::make_unique<TouchControlsEditor>()); }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(dusk::i18n::tr("settings.input.customize_layout.help"));
            });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraXSensitivity,
            dusk::i18n::tr("settings.input.touch_camera_x.key"),
            dusk::i18n::tr("settings.input.touch_camera_x.help"),
            25, 400, 5, [] { return !getSettings().game.enableTouchControls; });
        config_percent_select(leftPane, rightPane, getSettings().game.touchCameraYSensitivity,
            dusk::i18n::tr("settings.input.touch_camera_y.key"),
            dusk::i18n::tr("settings.input.touch_camera_y.help"), 25,
            400, 5, [] { return !getSettings().game.enableTouchControls; });
#endif

        leftPane.add_section(dusk::i18n::tr("settings.section.camera"));
        addOption(dusk::i18n::tr("settings.input.free_camera.key"), getSettings().game.freeCamera,
            dusk::i18n::tr("settings.input.free_camera.help"));
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraXSensitivity,
            dusk::i18n::tr("settings.input.free_camera_x.key"),
            dusk::i18n::tr("settings.input.free_camera_x.help"),
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        config_percent_select(leftPane, rightPane, getSettings().game.freeCameraYSensitivity,
            dusk::i18n::tr("settings.input.free_camera_y.key"),
            dusk::i18n::tr("settings.input.free_camera_y.help"),
            50, 200, 5, [] { return !getSettings().game.freeCamera; });
        addOption(dusk::i18n::tr("settings.input.invert_camera_x.key"), getSettings().game.invertCameraXAxis,
            dusk::i18n::tr("settings.input.invert_camera_x.help"));
        addOption(dusk::i18n::tr("settings.input.invert_camera_y.key"), getSettings().game.invertCameraYAxis,
            dusk::i18n::tr("settings.input.invert_camera_y.help"),
            [] { return !getSettings().game.freeCamera; });
        addOption(dusk::i18n::tr("settings.input.invert_fp_x.key"), getSettings().game.invertFirstPersonXAxis,
            dusk::i18n::tr("settings.input.invert_fp_x.help"));
        addOption(dusk::i18n::tr("settings.input.invert_fp_y.key"), getSettings().game.invertFirstPersonYAxis,
            dusk::i18n::tr("settings.input.invert_fp_y.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.gyro"));
        addOption(dusk::i18n::tr("settings.input.gyro_aim.key"), getSettings().game.enableGyroAim,
            dusk::i18n::tr("settings.input.gyro_aim.help"));
        addOption(dusk::i18n::tr("settings.input.gyro_rollgoal.key"), getSettings().game.enableGyroRollgoal,
            dusk::i18n::tr("settings.input.gyro_rollgoal.help"));
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityY,
            dusk::i18n::tr("settings.input.gyro_pitch.key"), dusk::i18n::tr("settings.input.gyro_pitch.help"), 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityX,
            dusk::i18n::tr("settings.input.gyro_yaw.key"), dusk::i18n::tr("settings.input.gyro_yaw.help"), 25, 400, 5,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSensitivityRollgoal,
            dusk::i18n::tr("settings.input.rollgoal_sensitivity.key"), dusk::i18n::tr("settings.input.rollgoal_sensitivity.help"),
            25, 400, 5,
            [] { return !getSettings().game.enableGyroRollgoal; });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroDeadband, dusk::i18n::tr("settings.input.gyro_deadband.key"),
            dusk::i18n::tr("settings.input.gyro_deadband.help"), 0, 50, 1,
            [] { return !gyro_enabled(); });
        config_percent_select(leftPane, rightPane, getSettings().game.gyroSmoothing,
            dusk::i18n::tr("settings.input.gyro_smoothing.key"), dusk::i18n::tr("settings.input.gyro_smoothing.help"), 0, 100, 1,
            [] { return !gyro_enabled(); });
        addOption(dusk::i18n::tr("settings.input.invert_gyro_pitch.key"), getSettings().game.gyroInvertPitch,
            dusk::i18n::tr("settings.input.invert_gyro_pitch.help"), [] { return !gyro_enabled(); });
        addOption(dusk::i18n::tr("settings.input.invert_gyro_yaw.key"), getSettings().game.gyroInvertYaw,
            dusk::i18n::tr("settings.input.invert_gyro_yaw.help"), [] { return !gyro_enabled(); });

        leftPane.add_section(dusk::i18n::tr("settings.section.mouse"));
        addOption(dusk::i18n::tr("settings.input.mouse_aim.key"), getSettings().game.enableMouseAim,
            dusk::i18n::tr("settings.input.mouse_aim.help"));
        addOption(dusk::i18n::tr("settings.input.mouse_camera.key"), getSettings().game.enableMouseCamera,
            dusk::i18n::tr("settings.input.mouse_camera.help"));
        config_percent_select(leftPane, rightPane, getSettings().game.mouseAimSensitivity,
            dusk::i18n::tr("settings.input.mouse_aim_sensitivity.key"), dusk::i18n::tr("settings.input.mouse_aim_sensitivity.help"), 25, 400, 5,
            [] { return !getSettings().game.enableMouseAim; });
        config_percent_select(leftPane, rightPane, getSettings().game.mouseCameraSensitivity,
            dusk::i18n::tr("settings.input.mouse_camera_sensitivity.key"), dusk::i18n::tr("settings.input.mouse_camera_sensitivity.help"), 25, 400, 5,
            [] { return !getSettings().game.enableMouseCamera; });
        addOption(dusk::i18n::tr("settings.input.invert_mouse_y.key"), getSettings().game.invertMouseY,
            dusk::i18n::tr("settings.input.invert_mouse_y.help"),
            [] { return !getSettings().game.enableMouseAim || !getSettings().game.enableMouseCamera; });

        leftPane.add_section(dusk::i18n::tr("settings.section.gameplay"));
        addOption(dusk::i18n::tr("settings.input.menu_pointer.key"), getSettings().game.enableMenuPointer,
            dusk::i18n::tr("settings.input.menu_pointer.help"));
        addOption(dusk::i18n::tr("settings.input.invert_airswim_x.key"), getSettings().game.invertAirSwimX,
            dusk::i18n::tr("settings.input.invert_airswim_x.help"));
        addOption(dusk::i18n::tr("settings.input.invert_airswim_y.key"), getSettings().game.invertAirSwimY,
            dusk::i18n::tr("settings.input.invert_airswim_y.help"));
        addOption(dusk::i18n::tr("settings.input.swap_direct_select.key"), getSettings().game.swapDirectSelect,
            dusk::i18n::tr("settings.input.swap_direct_select.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.tools"));
        addOption(dusk::i18n::tr("settings.input.turbo_key.key"), getSettings().game.enableTurboKeybind,
            dusk::i18n::tr("settings.input.turbo_key.help"),
            [] { return getSettings().game.speedrunMode; });
        addOption(dusk::i18n::tr_fmt("settings.input.reset_key.key", Rml::String{hotkeys::DO_RESET}),
            getSettings().game.enableResetKeybind,
            dusk::i18n::tr_fmt("settings.input.reset_key.help", Rml::String{hotkeys::DO_RESET}));
    });

    add_tab(dusk::i18n::tr("settings.audio.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        // TODO: Individual sliders for Main Music, Sub Music, Sound Effects, and Fanfare.
        leftPane.add_section(dusk::i18n::tr("settings.section.volume"));
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = dusk::i18n::tr("settings.audio.master_volume.key"),
                .getValue = [] { return getSettings().audio.masterVolume.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().audio.masterVolume.setValue(value);
                        config::Save();
                        audio::SetMasterVolume(audio::MasterVolumeToLinear(value / 100.0f));
                    },
                .isModified =
                    [] {
                        return getSettings().audio.masterVolume.getValue() !=
                               getSettings().audio.masterVolume.getDefaultValue();
                    },
                .max = 100,
                .suffix = "%",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(dusk::i18n::tr("settings.audio.master_volume.help"));
            });

        leftPane.add_section(dusk::i18n::tr("settings.section.effects"));
        config_bool_select(leftPane, rightPane, getSettings().audio.enableReverb,
            {
                .key = dusk::i18n::tr("settings.audio.reverb.key"),
                .helpText = dusk::i18n::tr("settings.audio.reverb.help"),
                .onChange = [](bool value) { audio::SetEnableReverb(value); },
            });
        config_bool_select(leftPane, rightPane, getSettings().audio.enableHrtf,
            {
                .key = dusk::i18n::tr("settings.audio.spatial_sound.key"),
                .helpText = dusk::i18n::tr("settings.audio.spatial_sound.help"),
                .onChange = [](bool value) { audio::EnableHrtf = value; },
            });
        config_bool_select(leftPane, rightPane, getSettings().audio.menuSounds,
            {
                .key = dusk::i18n::tr("settings.audio.menu_sounds.key"),
                .helpText = dusk::i18n::tr("settings.audio.menu_sounds.help"),
            });

        leftPane.add_section(dusk::i18n::tr("settings.section.tweaks"));
        config_bool_select(leftPane, rightPane, getSettings().game.noLowHpSound,
            {
                .key = dusk::i18n::tr("settings.audio.no_low_hp.key"),
                .helpText = dusk::i18n::tr("settings.audio.no_low_hp.help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().game.midnasLamentNonStop,
            {
                .key = dusk::i18n::tr("settings.audio.midna_lament.key"),
                .helpText = dusk::i18n::tr("settings.audio.midna_lament.help"),
            });
    });

    add_tab(dusk::i18n::tr("settings.gameplay.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                             const Rml::String& helpText) {
            config_bool_select(leftPane, rightPane, value,
                {
                    .key = key,
                    .helpText = helpText,
                });
        };
        auto addSpeedrunDisabledOption = [&](const Rml::String& key, ConfigVar<bool>& value,
                                             const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section(dusk::i18n::tr("settings.section.general"));
        addOption(dusk::i18n::tr("settings.gameplay.mirror_mode.key"), getSettings().game.enableMirrorMode,
            dusk::i18n::tr("settings.gameplay.mirror_mode.help"));
        addOption(dusk::i18n::tr("settings.gameplay.minimal_hud.key"), getSettings().game.minimalHUD,
            dusk::i18n::tr("settings.gameplay.minimal_hud.help"));
        config_percent_select(leftPane, rightPane, getSettings().game.hudScale,
            dusk::i18n::tr("settings.gameplay.hud_scale.key"),
            dusk::i18n::tr("settings.gameplay.hud_scale.help"),
            50, 200, 5,
            [] { return getSettings().game.minimalHUD.getValue(); });
        addOption(dusk::i18n::tr("settings.gameplay.restore_wii_glitches.key"), getSettings().game.restoreWiiGlitches,
            dusk::i18n::tr("settings.gameplay.restore_wii_glitches.help"));
        addOption(dusk::i18n::tr("settings.gameplay.link_doll_rotation.key"), getSettings().game.enableLinkDollRotation,
            dusk::i18n::tr("settings.gameplay.link_doll_rotation.help"));
        addOption(dusk::i18n::tr("settings.gameplay.hide_owl_markers.key"), getSettings().game.removeQuestMapMarkers,
            dusk::i18n::tr("settings.gameplay.hide_owl_markers.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.difficulty"));
        leftPane.register_control(
            leftPane.add_child<NumberButton>(NumberButton::Props{
                .key = dusk::i18n::tr("settings.gameplay.damage_multiplier.key"),
                .getValue = [] { return getSettings().game.damageMultiplier.getValue(); },
                .setValue =
                    [](int value) {
                        getSettings().game.damageMultiplier.setValue(value);
                        config::Save();
                    },
                .isDisabled = [] { return getSettings().game.speedrunMode; },
                .isModified =
                    [] {
                        return getSettings().game.damageMultiplier.getValue() !=
                               getSettings().game.damageMultiplier.getDefaultValue();
                    },
                .min = 1,
                .max = 8,
                .suffix = "×",
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_text(dusk::i18n::tr("settings.gameplay.damage_multiplier.help"));
            });
        addSpeedrunDisabledOption(
            dusk::i18n::tr("settings.gameplay.instant_death.key"), getSettings().game.instantDeath,
            dusk::i18n::tr("settings.gameplay.instant_death.help"));
        addSpeedrunDisabledOption(dusk::i18n::tr("settings.gameplay.no_heart_drops.key"), getSettings().game.noHeartDrops,
            dusk::i18n::tr("settings.gameplay.no_heart_drops.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.quality_of_life"));
        addOption(dusk::i18n::tr("settings.gameplay.bigger_wallets.key"), getSettings().game.biggerWallets,
            dusk::i18n::tr("settings.gameplay.bigger_wallets.help"));
        addOption(dusk::i18n::tr("settings.gameplay.disable_rupee_cutscenes.key"), getSettings().game.disableRupeeCutscenes,
            dusk::i18n::tr("settings.gameplay.disable_rupee_cutscenes.help"));
        addOption(dusk::i18n::tr("settings.gameplay.faster_climbing.key"), getSettings().game.fastClimbing,
            dusk::i18n::tr("settings.gameplay.faster_climbing.help"));
        addOption(dusk::i18n::tr("settings.gameplay.faster_tears.key"), getSettings().game.fastTears,
            dusk::i18n::tr("settings.gameplay.faster_tears.help"));
        addSpeedrunDisabledOption(dusk::i18n::tr("settings.gameplay.autosave.key"), getSettings().game.autoSave,
            dusk::i18n::tr("settings.gameplay.autosave.help"));
        addOption(dusk::i18n::tr("settings.gameplay.instant_saves.key"), getSettings().game.instantSaves,
            dusk::i18n::tr("settings.gameplay.instant_saves.help"));
        addOption(dusk::i18n::tr("settings.gameplay.instant_text.key"), getSettings().game.instantText,
            dusk::i18n::tr("settings.gameplay.instant_text.help"));
        addOption(dusk::i18n::tr("settings.gameplay.no_climb_miss.key"), getSettings().game.noMissClimbing,
            dusk::i18n::tr("settings.gameplay.no_climb_miss.help"));
        addOption(dusk::i18n::tr("settings.gameplay.no_rupee_returns.key"), getSettings().game.noReturnRupees,
            dusk::i18n::tr("settings.gameplay.no_rupee_returns.help"));
        addOption(dusk::i18n::tr("settings.gameplay.no_sword_recoil.key"), getSettings().game.noSwordRecoil,
            dusk::i18n::tr("settings.gameplay.no_sword_recoil.help"));
        addOption(dusk::i18n::tr("settings.gameplay.no_2nd_fish.key"), getSettings().game.no2ndFishForCat,
            dusk::i18n::tr("settings.gameplay.no_2nd_fish.help"));
        addOption(dusk::i18n::tr("settings.gameplay.button_fishing.key"), getSettings().game.buttonFishing,
            dusk::i18n::tr("settings.gameplay.button_fishing.help"));
        addOption(dusk::i18n::tr("settings.gameplay.poe_count_map.key"), getSettings().game.enhancedMapMenus,
            dusk::i18n::tr("settings.gameplay.poe_count_map.help"));
        addSpeedrunDisabledOption(dusk::i18n::tr("settings.gameplay.suns_song.key"), getSettings().game.sunsSong,
            dusk::i18n::tr("settings.gameplay.suns_song.help"));
        addOption(dusk::i18n::tr("settings.gameplay.quick_transform.key"), getSettings().game.enableQuickTransform,
            dusk::i18n::tr("settings.gameplay.quick_transform.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.speedrunning"));
        config_bool_select(leftPane, rightPane, getSettings().game.speedrunMode,
            {
                .key = dusk::i18n::tr("settings.gameplay.speedrun_mode.key"),
                .helpText = dusk::i18n::tr("settings.gameplay.speedrun_mode.help"),
                .onChange =
                    [](bool enabled) {
                        if (enabled) {
                            reset_for_speedrun_mode();
                        } else {
                            restore_from_speedrun_mode();
                            if (getSettings().game.liveSplitEnabled) {
                                speedrun::disconnectLiveSplit();
                            }
                        }
                        for (auto& doc : get_document_stack()) {
                            if (dynamic_cast<MenuBar*>(doc.get())) {
                                doc = std::make_unique<MenuBar>();
                                break;
                            }
                        }
                    },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.liveSplitEnabled,
            {
                .key = dusk::i18n::tr("settings.gameplay.livesplit.key"),
                .helpText = dusk::i18n::tr("settings.gameplay.livesplit.help"),
                .onChange =
                    [](bool enabled) {
                        if (enabled) {
                            speedrun::connectLiveSplit();
                        } else {
                            speedrun::disconnectLiveSplit();
                        }
                    },
                .isDisabled = [] { return IsMobile || !getSettings().game.speedrunMode; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showSpeedrunRTATimer,
            {
                .key = dusk::i18n::tr("settings.gameplay.show_rta.key"),
                .helpText = dusk::i18n::tr("settings.gameplay.show_rta.help"),
                .isDisabled = [] { return !getSettings().game.speedrunMode; },
            });
    });

    add_tab(dusk::i18n::tr("settings.cheats.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        auto addCheat = [&](const Rml::String& key, ConfigVar<bool>& value,
                            const Rml::String& helpText) {
            add_speedrun_disabled_option(leftPane, rightPane, value, key, helpText);
        };

        leftPane.add_section(dusk::i18n::tr("settings.section.resources"));
        addCheat(dusk::i18n::tr("settings.cheats.infinite_hearts.key"), getSettings().game.infiniteHearts, dusk::i18n::tr("settings.cheats.infinite_hearts.help"));
        addCheat(
            dusk::i18n::tr("settings.cheats.infinite_arrows.key"), getSettings().game.infiniteArrows, dusk::i18n::tr("settings.cheats.infinite_arrows.help"));
        addCheat(dusk::i18n::tr("settings.cheats.infinite_seeds.key"), getSettings().game.infiniteSeeds, dusk::i18n::tr("settings.cheats.infinite_seeds.help"));
        addCheat(dusk::i18n::tr("settings.cheats.infinite_bombs.key"), getSettings().game.infiniteBombs, dusk::i18n::tr("settings.cheats.infinite_bombs.help"));
        addCheat(dusk::i18n::tr("settings.cheats.infinite_oil.key"), getSettings().game.infiniteOil, dusk::i18n::tr("settings.cheats.infinite_oil.help"));
        addCheat(dusk::i18n::tr("settings.cheats.infinite_oxygen.key"), getSettings().game.infiniteOxygen,
            dusk::i18n::tr("settings.cheats.infinite_oxygen.help"));
        addCheat(
            dusk::i18n::tr("settings.cheats.infinite_rupees.key"), getSettings().game.infiniteRupees, dusk::i18n::tr("settings.cheats.infinite_rupees.help"));
        addCheat(dusk::i18n::tr("settings.cheats.no_item_timer.key"), getSettings().game.enableIndefiniteItemDrops,
            dusk::i18n::tr("settings.cheats.no_item_timer.help"));

        leftPane.add_section(dusk::i18n::tr("settings.section.abilities"));
        addCheat(
            dusk::i18n::tr("settings.cheats.moon_jump.key"), getSettings().game.moonJump, dusk::i18n::tr("settings.cheats.moon_jump.help"));
        addCheat(dusk::i18n::tr("settings.cheats.super_clawshot.key"), getSettings().game.superClawshot,
            dusk::i18n::tr("settings.cheats.super_clawshot.help"));
        addCheat(dusk::i18n::tr("settings.cheats.always_greatspin.key"), getSettings().game.alwaysGreatspin,
            dusk::i18n::tr("settings.cheats.always_greatspin.help"));
        addCheat(dusk::i18n::tr("settings.cheats.fast_iron_boots.key"), getSettings().game.enableFastIronBoots,
            dusk::i18n::tr("settings.cheats.fast_iron_boots.help"));
        addCheat(dusk::i18n::tr("settings.cheats.transform_anywhere.key"), getSettings().game.canTransformAnywhere,
            dusk::i18n::tr("settings.cheats.transform_anywhere.help"));
        addCheat(dusk::i18n::tr("settings.cheats.fast_roll.key"), getSettings().game.fastRoll,
            dusk::i18n::tr("settings.cheats.fast_roll.help"));
        addCheat(dusk::i18n::tr("settings.cheats.fast_spinner.key"), getSettings().game.fastSpinner,
            dusk::i18n::tr("settings.cheats.fast_spinner.help"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = dusk::i18n::tr("settings.cheats.magic_armor.key"),
                .getValue =
                    [] {
                        return dusk::i18n::tr_index("settings.magic_armor_mode",
                            static_cast<u8>(getSettings().game.armorRupeeDrain.getValue()));
                    },
                .isDisabled = [] { return getSettings().game.speedrunMode; },
                .isModified =
                    [] {
                        return getSettings().game.armorRupeeDrain.getValue() !=
                               getSettings().game.armorRupeeDrain.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < kMagicArmorModes.size(); i++) {
                    pane.add_button({
                            .text = dusk::i18n::tr_index("settings.magic_armor_mode", i),
                            .isSelected =
                                [i] {
                                    return getSettings().game.armorRupeeDrain.getValue() == static_cast<MagicArmorMode>(i);
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.armorRupeeDrain.setValue(static_cast<MagicArmorMode>(i));
                            config::Save();
                        });
                }
                pane.add_rml(dusk::i18n::tr("settings.cheats.magic_armor.help"));
            });
        addCheat(dusk::i18n::tr("settings.cheats.invincible_enemies.key"), getSettings().game.invincibleEnemies,
            dusk::i18n::tr("settings.cheats.invincible_enemies.help"));
    });

    add_tab(dusk::i18n::tr("settings.interface.tab"), [this](Rml::Element* content) {
        auto& leftPane = add_child<Pane>(content, Pane::Type::Controlled);
        auto& rightPane = add_child<Pane>(content, Pane::Type::Uncontrolled);

        leftPane.add_section(dusk::i18n::tr("settings.section.dusklight"));
#if DUSK_CAN_OPEN_DATA_FOLDER
        leftPane.register_control(
            leftPane.add_button(dusk::i18n::tr("settings.interface.open_data_folder.key")).on_pressed([] {
                mDoAud_seStartMenu(kSoundClick);
                data::open_data_path();
            }),
            rightPane, [](Pane& pane) {
                pane.add_text(dusk::i18n::tr("settings.interface.open_data_folder.help"));
            });
#endif
        leftPane.register_control(
            leftPane.add_select_button({
                .key = dusk::i18n::tr("settings.interface.notifications.key"),
                .getValue = [] {
                    const bool ach = getSettings().game.enableAchievementToasts.getValue();
                    const bool ctl = getSettings().game.enableControllerToasts.getValue();
                    if (!ach && !ctl) {
                        return dusk::i18n::tr("settings.common.off");
                    }
                    if (ach && ctl) {
                        return dusk::i18n::tr("settings.interface.notifications.all");
                    }
                    return dusk::i18n::tr("settings.interface.notifications.some");
                },
                .isModified = [] {
                    const auto& ach = getSettings().game.enableAchievementToasts;
                    const auto& ctl = getSettings().game.enableControllerToasts;
                    return ach.getValue() != ach.getDefaultValue() || ctl.getValue() != ctl.getDefaultValue();
                },
            }),
            rightPane, [](Pane& pane) {
                pane.clear();
                pane.add_button(dusk::i18n::tr("settings.interface.notifications.select_all")).on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(true);
                    getSettings().game.enableControllerToasts.setValue(true);
                    config::Save();
                });
                pane.add_button(dusk::i18n::tr("settings.interface.notifications.select_none")).on_pressed([] {
                    mDoAud_seStartMenu(kSoundItemChange);
                    getSettings().game.enableAchievementToasts.setValue(false);
                    getSettings().game.enableControllerToasts.setValue(false);
                    config::Save();
                });

                pane.add_section(dusk::i18n::tr("settings.section.types"));
                pane.add_button(
                    {
                        .text = dusk::i18n::tr("settings.interface.notifications.achievements"),
                        .isSelected =
                        [] {
                            return getSettings().game.enableAchievementToasts.getValue();
                        },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableAchievementToasts;
                        v.setValue(!v.getValue());
                        config::Save();
                    });
                pane.add_button(
                    {
                        .text = dusk::i18n::tr("settings.interface.notifications.missing_device"),
                        .isSelected =
                            [] { return getSettings().game.enableControllerToasts.getValue(); },
                    })
                    .on_pressed([] {
                        mDoAud_seStartMenu(kSoundItemChange);
                        auto& v = getSettings().game.enableControllerToasts;
                        v.setValue(!v.getValue());
                        config::Save();
                    });
                pane.add_rml(dusk::i18n::tr("settings.interface.notifications.help"));
            });
#if DUSK_ENABLE_SENTRY_NATIVE
        auto& crashReporting = leftPane.add_child<BoolButton>(BoolButton::Props{
            .key = dusk::i18n::tr("settings.interface.crash_reporting.key"),
            .getValue =
                [] { return crash_reporting::get_consent() == crash_reporting::Consent::Given; },
            .setValue = [](bool enabled) { crash_reporting::set_consent(enabled); },
            .isDisabled =
                [] {
                    return crash_reporting::get_consent() == crash_reporting::Consent::Unavailable;
                },
            .isModified = [] { return false; },
        });
        leftPane.register_control(crashReporting, rightPane, [](Pane& pane) {
            pane.clear();
            pane.add_rml(dusk::i18n::tr("settings.interface.crash_reporting.help"));
        });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.skipPreLaunchUI,
            {
                .key = dusk::i18n::tr("settings.interface.skip_main_menu.key"),
                .helpText = dusk::i18n::tr("settings.interface.skip_main_menu.help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().backend.showPipelineCompilation,
            {
                .key = dusk::i18n::tr("settings.interface.show_pipeline_compilation.key"),
                .helpText = dusk::i18n::tr("settings.interface.show_pipeline_compilation.help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().backend.checkForUpdates,
            {
                .key = dusk::i18n::tr("settings.interface.check_updates.key"),
                .helpText = dusk::i18n::tr("settings.interface.check_updates.help"),
            });
#ifdef DUSK_DISCORD
        config_bool_select(leftPane, rightPane, getSettings().game.enableDiscordPresence,
            {
                .key = dusk::i18n::tr("settings.interface.discord_presence.key"),
                .helpText = dusk::i18n::tr("settings.interface.discord_presence.help"),
                .onChange = [](bool enabled) {
                    if (enabled) {
                        dusk::discord::initialize();
                    } else {
                        dusk::discord::shutdown();
                    }
                },
            });
#endif
        config_bool_select(leftPane, rightPane, getSettings().backend.enableAdvancedSettings,
            {
                .key = dusk::i18n::tr("settings.interface.advanced_settings.key"),
                .icon = "warning",
                .helpText = dusk::i18n::tr("settings.interface.advanced_settings.help"),
                .onChange =
                    [](bool) {
                        for (auto& doc : get_document_stack()) {
                            if (dynamic_cast<MenuBar*>(doc.get())) {
                                doc = std::make_unique<MenuBar>();
                                break;
                            }
                        }
                    },
                .isDisabled = [] { return getSettings().game.speedrunMode; },
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewer,
            {
                .key = dusk::i18n::tr("settings.interface.input_viewer.key"),
                .helpText = dusk::i18n::tr("settings.interface.input_viewer.help"),
            });
        config_bool_select(leftPane, rightPane, getSettings().game.showInputViewerGyro,
            {
                .key = dusk::i18n::tr("settings.interface.gyro_input_viewer.key"),
                .helpText = dusk::i18n::tr("settings.interface.gyro_input_viewer.help"),
                .isDisabled = [] { return !getSettings().game.showInputViewer; },
            });
        leftPane.add_section(dusk::i18n::tr("settings.section.game"));
        leftPane.register_control(
            leftPane.add_select_button({
                .key = dusk::i18n::tr("settings.interface.menu_scaling.key"),
                .getValue =
                    [] {
                        return Rml::String{kMenuScalingModeLabels[static_cast<u8>(
                            getSettings().game.menuScalingMode.getValue())]};
                    },
                .isModified =
                    [] {
                        const auto& mode = getSettings().game.menuScalingMode;
                        return mode.getValue() != mode.getDefaultValue();
                    },
            }),
            rightPane, [](Pane& pane) {
                for (int i = 0; i < static_cast<int>(kMenuScalingModeLabels.size()); ++i) {
                    pane
                        .add_button({
                            .text = kMenuScalingModeLabels[i],
                            .isSelected =
                                [i] {
                                    return getSettings().game.menuScalingMode.getValue() ==
                                           static_cast<MenuScaling>(i);
                                    ;
                                },
                        })
                        .on_pressed([i] {
                            mDoAud_seStartMenu(kSoundItemChange);
                            getSettings().game.menuScalingMode.setValue(
                                static_cast<MenuScaling>(i));
                            ;
                            config::Save();
                        });
                }
                pane.add_rml(dusk::i18n::tr("settings.interface.menu_scaling.help"));
            });
        config_bool_select(leftPane, rightPane, getSettings().game.hideTvSettingsScreen,
            {
                .key = dusk::i18n::tr("settings.interface.skip_tv_settings.key"),
                .helpText = dusk::i18n::tr("settings.interface.skip_tv_settings.help"),
            });
        add_speedrun_disabled_option(leftPane, rightPane, getSettings().game.recordingMode,
            dusk::i18n::tr("settings.interface.recording_mode.key"),
            dusk::i18n::tr("settings.interface.recording_mode.help"));
    });
}

void SettingsWindow::update() {
    if (mPrelaunch && top_document() == this) {
        try_push_verification_modal(*this);
    }

    Window::update();
}

void SettingsWindow::hide(bool close) {
    config::Save();
    Window::hide(close);
}

}  // namespace dusk::ui
