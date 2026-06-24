#include "prelaunch.hpp"

#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/file_select.hpp"
#include "dusk/i18n.hpp"
#include "dusk/iso_validate.hpp"
#include "dusk/main.h"
#include "dusk/settings.h"
#include "dusk/update_check.hpp"
#include "modal.hpp"
#include "preset.hpp"
#include "settings.hpp"
#include "version.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_misc.h>
#include <aurora/lib/logging.hpp>
#include <aurora/lib/window.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <filesystem>
#include <optional>
#include <thread>

#include "m_Do/m_Do_MemCard.h"

namespace dusk::ui {
namespace {
aurora::Module PrelaunchLog{"dusk::ui::prelaunch"};

const Rml::String kDocumentSource = R"RML(
<rml>
<head>
    <link type="text/rcss" href="res/rml/prelaunch.rcss" />
</head>
<body>
    <div class="gradient" />
    <div class="background" />
    <content id="root" open>
        <menu>
            <hero class="intro-item delay-0">
                <eyebrow><span>Twilit Realm</span> presents</eyebrow>
                <img src="res/logo.png" />
            </hero>
            <div id="menu-list" />
        </menu>
        <disc-info class="intro-item delay-4">
            <div id="disc-status">
                <icon />
                <span id="disc-status-label" />
            </div>
            <span id="disc-version" class="detail" />
        </disc-info>
        <version-info class="intro-item delay-5">
            <div class="version"><span id="version-label"></span> <span id="version-text"></span></div>
            <div id="update-status" class="update">
                <span id="update-message"></span>
                <button id="update-download">
                    <span id="update-download-label"></span>
                    &nbsp;<icon />
                </button>
            </div>
        </version-info>
    </content>
</body>
</rml>
)RML";

constexpr const char* kDiscFilterDiscPattern = "iso;gcm;ciso;gcz;nfs;rvz;wbfs;wia;tgc";
constexpr const char* kDiscFilterAllPattern = "*";

struct DiscVerificationResult {
    std::string path;
    iso::DiscInfo info;
    iso::ValidationError validation = iso::ValidationError::Unknown;
};

struct DiscVerificationTask {
    explicit DiscVerificationTask(std::string discPath) : path(std::move(discPath)) {
        worker = std::thread([this] {
            try {
                validation = iso::validate(path.c_str(), status, info);
            } catch (const std::exception& e) {
                PrelaunchLog.error(
                    "Disc verification failed with exception for '{}': {}", path, e.what());
                validation = iso::ValidationError::Unknown;
            } catch (...) {
                PrelaunchLog.error(
                    "Disc verification failed with unknown exception for '{}'", path);
                validation = iso::ValidationError::Unknown;
            }
            done.store(true, std::memory_order_release);
        });
    }

    ~DiscVerificationTask() {
        status.shouldCancel.store(true, std::memory_order_relaxed);
        join();
    }

    void join() {
        if (worker.joinable()) {
            worker.join();
        }
    }

    [[nodiscard]] bool finished() const { return done.load(std::memory_order_acquire); }

    std::string path;
    iso::DiscInfo info;
    iso::VerificationStatus status;
    iso::ValidationError validation = iso::ValidationError::Unknown;
    std::atomic_bool done = false;
    std::thread worker;
};

std::unique_ptr<DiscVerificationTask> sDiscVerificationTask;
bool sDiscVerificationModalPushed = false;

struct UpdateCheckTask {
    UpdateCheckTask() {
        worker = std::thread([this] {
            try {
                result = update_check::check_latest_github_release("TwilitRealm", "dusklight");
            } catch (const std::exception& e) {
                result = {
                    .status = update_check::Status::Failed,
                    .message = fmt::format("Update check failed with exception: {}", e.what()),
                };
            } catch (...) {
                result = {
                    .status = update_check::Status::Failed,
                    .message = "Update check failed with an unknown exception",
                };
            }
            done.store(true, std::memory_order_release);
        });
    }

    ~UpdateCheckTask() { join(); }

    void join() {
        if (worker.joinable()) {
            worker.join();
        }
    }

    [[nodiscard]] bool finished() const { return done.load(std::memory_order_acquire); }

    update_check::Result result;
    std::atomic_bool done = false;
    std::thread worker;
};

std::unique_ptr<UpdateCheckTask> sUpdateCheckTask;
std::optional<update_check::Result> sUpdateCheckResult;

bool verification_state_allows_launch(iso::ValidationError validation) noexcept {
    return validation == iso::ValidationError::Unknown ||
           validation == iso::ValidationError::Success ||
           validation == iso::ValidationError::HashMismatch;
}

iso::ValidationError verification_from_config(DiscVerificationState value) noexcept {
    switch (value) {
    case DiscVerificationState::Success:
        return iso::ValidationError::Success;
    case DiscVerificationState::HashMismatch:
        return iso::ValidationError::HashMismatch;
    default:
        return iso::ValidationError::Unknown;
    }
}

DiscVerificationState verification_to_config(iso::ValidationError validation) {
    switch (validation) {
    case iso::ValidationError::Success:
        return DiscVerificationState::Success;
    case iso::ValidationError::HashMismatch:
        return DiscVerificationState::HashMismatch;
    default:
        return DiscVerificationState::Unknown;
    }
}

std::string format_bytes(std::size_t bytes) {
    constexpr double KiB = 1024.0;
    constexpr double MiB = KiB * 1024.0;
    constexpr double GiB = MiB * 1024.0;
    if (bytes >= static_cast<std::size_t>(GiB)) {
        return fmt::format("{:.2f} GiB", static_cast<double>(bytes) / GiB);
    }
    if (bytes >= static_cast<std::size_t>(MiB)) {
        return fmt::format("{:.0f} MiB", static_cast<double>(bytes) / MiB);
    }
    if (bytes >= static_cast<std::size_t>(KiB)) {
        return fmt::format("{:.0f} KiB", static_cast<double>(bytes) / KiB);
    }
    return fmt::format("{} B", bytes);
}

void begin_disc_verification(std::string path) noexcept {
    if (path.empty()) {
        return;
    }
    if (sDiscVerificationTask != nullptr) {
        sDiscVerificationTask->status.shouldCancel.store(true, std::memory_order_relaxed);
        sDiscVerificationTask.reset();
    }
    sDiscVerificationTask = std::make_unique<DiscVerificationTask>(std::move(path));
    sDiscVerificationModalPushed = false;
}

std::optional<DiscVerificationResult> take_finished_disc_verification() {
    if (sDiscVerificationTask == nullptr || !sDiscVerificationTask->finished()) {
        return std::nullopt;
    }
    DiscVerificationResult result{
        .path = sDiscVerificationTask->path,
        .info = sDiscVerificationTask->info,
        .validation = sDiscVerificationTask->validation,
    };
    sDiscVerificationTask->join();
    sDiscVerificationTask.reset();
    sDiscVerificationModalPushed = false;
    return result;
}

void begin_update_check() {
    if (!getSettings().backend.checkForUpdates.getValue()) {
        return;
    }
    if (sUpdateCheckTask != nullptr || sUpdateCheckResult.has_value()) {
        return;
    }
    sUpdateCheckTask = std::make_unique<UpdateCheckTask>();
}

std::optional<update_check::Result> take_finished_update_check() {
    if (sUpdateCheckTask == nullptr || !sUpdateCheckTask->finished()) {
        return std::nullopt;
    }

    sUpdateCheckTask->join();
    auto result = std::move(sUpdateCheckTask->result);
    sUpdateCheckTask.reset();
    return result;
}

std::string update_release_label(const update_check::Release& release) {
    std::string_view tagName = release.tagName;
    if (!tagName.empty() && tagName.front() == 'v') {
        tagName.remove_prefix(1);
    }
    return std::string(tagName);
}

void open_update_release() {
    if (!sUpdateCheckResult.has_value() ||
        sUpdateCheckResult->status != update_check::Status::UpdateAvailable)
    {
        return;
    }

    const std::string url = sUpdateCheckResult->latest.htmlUrl;
    if (url.empty()) {
        PrelaunchLog.warn("Update is available, but the release did not include a download URL");
        return;
    }
    if (!SDL_OpenURL(url.c_str())) {
        PrelaunchLog.warn("Failed to open update URL '{}': {}", url, SDL_GetError());
    }
}

std::string get_error_msg(iso::ValidationError error) {
    switch (error) {
    default:
        return dusk::i18n::tr("prelaunch.error.unknown");
    case iso::ValidationError::IOError:
        return dusk::i18n::tr("prelaunch.error.io");
    case iso::ValidationError::InvalidImage:
        return dusk::i18n::tr("prelaunch.error.invalid_image");
    case iso::ValidationError::WrongGame:
        return dusk::i18n::tr("prelaunch.error.wrong_game");
    case iso::ValidationError::WrongVersion:
        return dusk::i18n::tr("prelaunch.error.wrong_version");
    case iso::ValidationError::Canceled:
        return dusk::i18n::tr("prelaunch.error.canceled");
    case iso::ValidationError::HashMismatch:
        return dusk::i18n::tr("prelaunch.error.hash_mismatch");
    case iso::ValidationError::Success:
        return dusk::i18n::tr("prelaunch.error.success");
    }
}

void persist_disc_choice(const std::string& path, iso::ValidationError validation) {
    const auto previousPath = getSettings().backend.isoPath.getValue();
    const auto previousVerification = getSettings().backend.isoVerification.getValue();
    const auto verification = verification_to_config(validation);

    getSettings().backend.isoPath.setValue(path);
    getSettings().backend.isoVerification.setValue(verification);
    config::Save();

    if (previousPath != path || previousVerification != verification) {
        iso::log_verification_state(path, verification);
    }
}

void apply_valid_disc_result(
    const std::string& path, const iso::DiscInfo& info, iso::ValidationError validation) {
    auto& state = prelaunch_state();
    state.configuredDiscPath = path;
    state.configuredDiscCanLaunch = true;
    state.configuredDiscInfo = info;
    state.configuredDiscValidation = validation;
    if (state.activeDiscPath.empty() || path == state.activeDiscPath) {
        state.activeDiscPath = path;
        state.activeDiscInfo = info;
    }
    persist_disc_choice(path, validation);
}

void apply_disc_verification_result(const DiscVerificationResult& result) {
    auto& state = prelaunch_state();

    if (result.validation == iso::ValidationError::HashMismatch ||
        result.validation == iso::ValidationError::Canceled)
    {
        state.pendingDiscPath = result.path;
        state.pendingDiscInfo = result.info;
        state.pendingDiscValidation = result.validation;
        state.errorString = escape(get_error_msg(result.validation));
        return;
    }

    if (result.validation == iso::ValidationError::Success) {
        apply_valid_disc_result(result.path, result.info, result.validation);
        state.errorString.clear();
        state.pendingDiscPath.clear();
        state.pendingDiscInfo = {};
        state.pendingDiscValidation = iso::ValidationError::Unknown;
        return;
    }

    state.pendingDiscPath.clear();
    state.pendingDiscInfo = {};
    state.pendingDiscValidation = iso::ValidationError::Unknown;
    state.errorString = escape(get_error_msg(result.validation));
}

class DiscVerificationModal : public WindowSmall {
public:
    DiscVerificationModal() : WindowSmall("modal", "modal-dialog") {
        auto* header = append(mDialog, "div");
        header->SetClass("modal-header", true);

        auto* title = append(header, "div");
        title->SetClass("modal-title", true);
        title->SetInnerRML(escape(dusk::i18n::tr("prelaunch.verify.title")));

        auto* icon = append(header, "icon");
        icon->SetClass("verifying", true);

        auto* body = append(mDialog, "div");
        body->SetClass("modal-body", true);

        auto* content = append(body, "div");
        content->SetClass("verification-progress", true);

        mFileName = append(content, "div");
        mFileName->SetClass("verification-file", true);

        mProgress = append(content, "progress");
        mProgress->SetClass("progress-ongoing", true);
        mProgress->SetClass("verification-progress-bar", true);
        mProgress->SetAttribute("value", 0.f);

        mDetail = append(content, "div");
        mDetail->SetClass("verification-detail", true);

        auto* actions = append(mDialog, "div");
        actions->SetClass("modal-actions", true);
        mCancelButton = std::make_unique<Button>(actions, dusk::i18n::tr("common.cancel"));
        mCancelButton->root()->SetClass("modal-btn", true);
        mCancelButton->on_pressed([this] { request_cancel(); });

        refresh();
    }

    void update() override {
        if (mFinished) {
            return;
        }
        if (auto result = take_finished_disc_verification()) {
            mFinished = true;
            apply_disc_verification_result(*result);
            pop();
            return;
        }
        if (sDiscVerificationTask == nullptr) {
            mFinished = true;
            pop();
            return;
        }
        refresh();
    }

    bool focus() override { return mCancelButton != nullptr && mCancelButton->focus(); }

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override {
        if (cmd == NavCommand::Cancel || cmd == NavCommand::Menu) {
            request_cancel();
            event.StopPropagation();
            return true;
        }
        if (cmd == NavCommand::Left || cmd == NavCommand::Right) {
            return true;
        }
        return false;
    }

private:
    void request_cancel() {
        if (sDiscVerificationTask == nullptr || mCancelRequested) {
            return;
        }

        mCancelRequested = true;
        sDiscVerificationTask->status.shouldCancel.store(true, std::memory_order_relaxed);
        if (mCancelButton != nullptr) {
            mCancelButton->set_text(dusk::i18n::tr("prelaunch.verify.cancelling"));
            mCancelButton->set_disabled(true);
        }
    }

    void refresh() {
        if (sDiscVerificationTask == nullptr) {
            return;
        }

        if (mCancelRequested) {
            return;
        }

        if (mFileName != nullptr) {
            std::string fileName = display_name_for_path(sDiscVerificationTask->path);
            if (fileName.empty()) {
                fileName = sDiscVerificationTask->path;
            }
            mFileName->SetInnerRML(escape(fileName));
        }

        const std::size_t bytesRead =
            sDiscVerificationTask->status.bytesRead.load(std::memory_order_relaxed);
        const std::size_t bytesTotal =
            sDiscVerificationTask->status.bytesTotal.load(std::memory_order_relaxed);

        if (bytesTotal == 0) {
            if (mProgress != nullptr) {
                mProgress->SetAttribute("value", 0.f);
            }
            if (mDetail != nullptr) {
                mDetail->SetInnerRML(escape(dusk::i18n::tr("prelaunch.verify.opening")));
            }
            return;
        }

        const float fraction =
            std::clamp(static_cast<float>(bytesRead) / static_cast<float>(bytesTotal), 0.0f, 1.0f);
        if (mProgress != nullptr) {
            mProgress->SetAttribute("value", fraction);
        }
        if (mDetail != nullptr) {
            mDetail->SetInnerRML(escape(dusk::i18n::tr_fmt("prelaunch.verify.progress",
                format_bytes(bytesRead), format_bytes(bytesTotal), fraction * 100.0f)));
        }
    }

    Rml::Element* mFileName = nullptr;
    Rml::Element* mProgress = nullptr;
    Rml::Element* mDetail = nullptr;
    std::unique_ptr<Button> mCancelButton;
    bool mCancelRequested = false;
    bool mFinished = false;
};

void file_dialog_callback(void*, const char* path, const char* error) {
    if (path == nullptr || error != nullptr) {
        return;
    }

    begin_disc_verification(path);
}

PrelaunchState sPrelaunchState;

}  // namespace

PrelaunchState& prelaunch_state() noexcept {
    return sPrelaunchState;
}

void refresh_configured_disc_state() noexcept {
    auto& state = prelaunch_state();
    if (state.configuredDiscPath.empty()) {
        state.configuredDiscCanLaunch = false;
        state.configuredDiscInfo = {};
        state.configuredDiscValidation = iso::ValidationError::Unknown;
        return;
    }

    iso::DiscInfo info{};
    const auto metadataValidation = iso::inspect(state.configuredDiscPath.c_str(), info);
    if (metadataValidation != iso::ValidationError::Success) {
        state.configuredDiscCanLaunch = false;
        state.configuredDiscInfo = {};
        state.configuredDiscValidation = metadataValidation;
        if (state.configuredDiscPath == state.activeDiscPath) {
            state.activeDiscInfo = {};
        }
        return;
    }

    auto verification = iso::ValidationError::Unknown;
    if (state.configuredDiscPath == getSettings().backend.isoPath.getValue()) {
        verification = verification_from_config(getSettings().backend.isoVerification.getValue());
    }

    if (verification_state_allows_launch(verification)) {
        state.configuredDiscCanLaunch = true;
        state.configuredDiscInfo = info;
        state.configuredDiscValidation = verification;
        if (state.configuredDiscPath == state.activeDiscPath) {
            state.activeDiscInfo = info;
        }
        return;
    }

    state.configuredDiscCanLaunch = false;
    state.configuredDiscInfo = {};
    state.configuredDiscValidation = iso::ValidationError::Unknown;
    if (state.configuredDiscPath == state.activeDiscPath) {
        state.activeDiscInfo = {};
    }
}

void try_push_verification_modal(Document& host) {
    auto& state = prelaunch_state();
    if (sDiscVerificationTask != nullptr && !sDiscVerificationModalPushed) {
        sDiscVerificationModalPushed = true;
        host.push(std::make_unique<DiscVerificationModal>());
        return;
    }

    if (state.errorString.empty()) {
        return;
    }

    auto dismiss = [](Modal& modal) {
        auto& state = prelaunch_state();
        state.errorString.clear();
        state.pendingDiscPath.clear();
        state.pendingDiscInfo = {};
        state.pendingDiscValidation = iso::ValidationError::Unknown;
        modal.pop();
    };

    if (!state.pendingDiscPath.empty()) {
        const Rml::String bodyRml =
            state.errorString + "<br/><br/>" + dusk::i18n::tr("prelaunch.verify.proceed_at_risk");
        auto acceptHashMismatch = [](Modal& modal) {
            auto& st = prelaunch_state();
            std::string path = std::move(st.pendingDiscPath);
            const auto info = st.pendingDiscInfo;
            const auto validation = st.pendingDiscValidation;
            st.pendingDiscPath.clear();
            st.pendingDiscInfo = {};
            st.pendingDiscValidation = iso::ValidationError::Unknown;
            st.errorString.clear();
            apply_valid_disc_result(path, info, validation);
            refresh_configured_disc_state();
            modal.pop();
        };
        host.push(std::make_unique<Modal>(Modal::Props{
            .title = dusk::i18n::tr("prelaunch.verify.warning_title"),
            .bodyRml = bodyRml,
            .actions =
                {
                    ModalAction{
                        .label = dusk::i18n::tr("common.cancel"),
                        .onPressed = dismiss,
                    },
                    ModalAction{
                        .label = dusk::i18n::tr("prelaunch.verify.continue_anyway"),
                        .onPressed = acceptHashMismatch,
                    },
                },
            .onDismiss = dismiss,
            .variant = "danger",
            .icon = "warning",
        }));
        return;
    }

    host.push(std::make_unique<Modal>(Modal::Props{
        .title = dusk::i18n::tr("prelaunch.verify.error_title"),
        .bodyRml = state.errorString,
        .actions =
            {
                ModalAction{
                    .label = dusk::i18n::tr("common.ok"),
                    .onPressed = dismiss,
                },
            },
        .onDismiss = dismiss,
        .icon = "error",
    }));
}

void ensure_initialized() noexcept {
    auto& state = prelaunch_state();
    if (state.initialized) {
        return;
    }

    state.configuredDiscPath = getSettings().backend.isoPath;
    state.activeDiscPath = state.configuredDiscPath;
    state.configuredDiscValidation =
        verification_from_config(getSettings().backend.isoVerification.getValue());
    state.initialLanguage = getSettings().game.language;
    state.initialGraphicsBackend = getSettings().backend.graphicsBackend;
    state.initialCardFileType = getSettings().backend.cardFileType;
    state.errorString.clear();
    state.initialized = true;
    refresh_configured_disc_state();
}

void open_iso_picker() noexcept {
    ensure_initialized();
    // tr() returns references stable for the table lifetime, so these c_str() pointers
    // remain valid for the duration of the (synchronous-dispatch) file dialog call.
    const std::array<SDL_DialogFileFilter, 2> discFileFilters{{
        {dusk::i18n::tr("prelaunch.filter.disc").c_str(), kDiscFilterDiscPattern},
        {dusk::i18n::tr("prelaunch.filter.all").c_str(), kDiscFilterAllPattern},
    }};
    ShowFileSelect(&file_dialog_callback, nullptr, aurora::window::get_sdl_window(),
        discFileFilters.data(), discFileFilters.size(), nullptr, false);
}

bool is_restart_pending() noexcept {
    const auto& state = prelaunch_state();
    if (!state.activeDiscPath.empty() && state.configuredDiscPath != state.activeDiscPath) {
        return true;
    }
    if (data::is_data_path_restart_pending()) {
        return true;
    }
    if (getSettings().backend.graphicsBackend.getValue() != state.initialGraphicsBackend) {
        return true;
    }
    if (getSettings().game.language.getValue() != state.initialLanguage) {
        return true;
    }
    return false;
}

void apply_intro_animation(Rml::Element* element, const char* delay_class) {
    if (element == nullptr || delay_class == nullptr) {
        return;
    }
    element->SetClass("intro-item", true);
    element->SetClass(delay_class, true);
}

void try_apply_mirrored_layout(Rml::Element* body) {
    if (body == nullptr) {
        return;
    }
    body->SetClass("mirrored", getSettings().game.enableMirrorMode.getValue());
}

Prelaunch::Prelaunch() : Document(kDocumentSource), mRoot(mDocument->GetElementById("root")) {
    ensure_initialized();
    begin_update_check();

    if (auto* menuList = mDocument->GetElementById("menu-list")) {
        auto& state = prelaunch_state();
        const bool activeDiscLoaded = !state.activeDiscPath.empty();
        mMenuButtons.push_back(
            std::make_unique<Button>(menuList, activeDiscLoaded ? dusk::i18n::tr("prelaunch.play")
                                                                : dusk::i18n::tr("prelaunch.select_disc")));
        mMenuButtons.back()->on_pressed([this] {
            if (prelaunch_state().activeDiscPath.empty()) {
                open_iso_picker();
                return;
            }

            mDoAud_seStartMenu(kSoundPlay);
            show_menu_notification();

            if (getSettings().audio.menuSounds) {
                JAISoundHandle* handle = g_mEnvSeMgr.field_0x144.getHandle();
                if (*handle) {
                    (*handle)->stop(60);
                    (*handle)->releaseHandle();
                }
            }

            if (g_mDoMemCd_control.mCardCommand == mDoMemCd_Ctrl_c::Command_e::COMM_NONE_e) {
                mDoMemCd_ThdInit();
            }

            IsGameLaunched = true;
            pop(false);
        });
        apply_intro_animation(mMenuButtons.back()->root(), "delay-1");

        mMenuButtons.push_back(std::make_unique<Button>(menuList, dusk::i18n::tr("common.settings")));
        mMenuButtons.back()->on_pressed([this] {
            mRestartSuppressed = false;
            push(std::make_unique<SettingsWindow>(true));
        });
        apply_intro_animation(mMenuButtons.back()->root(), "delay-2");

        mMenuButtons.push_back(std::make_unique<Button>(menuList, dusk::i18n::tr("common.quit")));
        mMenuButtons.back()->on_pressed([] { IsRunning = false; });
        apply_intro_animation(mMenuButtons.back()->root(), "delay-3");
    }

    mDiscStatus = mDocument->GetElementById("disc-status");
    mDiscDetail = mDocument->GetElementById("disc-version");
    if (auto* versionLabel = mDocument->GetElementById("version-label")) {
        versionLabel->SetInnerRML(escape(dusk::i18n::tr("prelaunch.version")));
    }
    mVersion = mDocument->GetElementById("version-text");
    mUpdateStatus = mDocument->GetElementById("update-status");
    mUpdateMessage = mDocument->GetElementById("update-message");
    mUpdateDownload = mDocument->GetElementById("update-download");
    mUpdateDownloadLabel = mDocument->GetElementById("update-download-label");

    if (mUpdateDownload != nullptr) {
        listen(mUpdateDownload, Rml::EventId::Click, [](Rml::Event& event) {
            open_update_release();
            event.StopPropagation();
        });
        listen(mUpdateDownload, Rml::EventId::Keydown, [](Rml::Event& event) {
            if (map_nav_event(event) == NavCommand::Confirm) {
                open_update_release();
                event.StopPropagation();
            }
        });
    }

    try_apply_mirrored_layout(mDocument);

    listen(mDocument, Rml::EventId::Transitionend, [this](Rml::Event& event) {
        auto* target = event.GetTargetElement();
        if (target == nullptr) {
            return;
        }
        if (target == mDocument && !mDocument->HasAttribute("open")) {
            Document::hide(true);
        } else if (target->GetTagName() == "button" && !target->IsClassSet("anim-done")) {
            target->SetClass("anim-done", true);
        }
    });
}

void Prelaunch::show() {
    Document::show();
    mDocument->SetAttribute("open", "");
    mRoot->SetAttribute("open", "");

    if (is_restart_pending() && !mRestartSuppressed) {
        const auto dismiss = [this](Modal& modal) {
            mRestartSuppressed = true;
            modal.pop();
        };
        std::vector<ModalAction> actions;
        if constexpr (dusk::SupportsProcessRestart) {
            actions.push_back(ModalAction{
                .label = dusk::i18n::tr("prelaunch.restart.later"),
                .onPressed = dismiss,
            });
            actions.push_back(ModalAction{
                .label = dusk::i18n::tr("prelaunch.restart.now"),
                .onPressed = [](Modal&) { dusk::RequestRestart(); },
            });
        } else {
            actions.push_back(ModalAction{
                .label = dusk::i18n::tr("common.ok"),
                .onPressed = dismiss,
            });
        }
        push(std::make_unique<Modal>(Modal::Props{
            .title = dusk::i18n::tr("prelaunch.restart.title"),
            .bodyRml =
                dusk::SupportsProcessRestart ?
                    dusk::i18n::tr("prelaunch.restart.body_supported") :
                    dusk::i18n::tr("prelaunch.restart.body_unsupported"),
            .actions = std::move(actions),
            .onDismiss = dismiss,
        }));
    }
}

void Prelaunch::hide(bool close) {
    if (close) {
        if (!mEntranceAnimationStarted) {
            // Close document immediately
            Document::hide(true);
        } else {
            mPendingClose = true;
        }
        mDocument->RemoveAttribute("open");
    } else {
        mRoot->RemoveAttribute("open");
    }
}

void Prelaunch::update() {
    ensure_initialized();
    try_apply_mirrored_layout(mDocument);

    if (top_document() == this) {
        try_push_verification_modal(*this);
    }

    const auto& state = prelaunch_state();

    const bool canLaunchConfiguredDisc = state.configuredDiscCanLaunch;
    const bool activeDiscLoaded = !state.activeDiscPath.empty();
    const bool discRestartPending =
        activeDiscLoaded && state.configuredDiscPath != state.activeDiscPath;
    mDocument->SetClass("disc-ready", IsGameLaunched);
#ifndef __SWITCH__
    if (canLaunchConfiguredDisc) {
        IsGameLaunched = true;
    }
#else
    // On Switch the auto-launch fires every frame because our saved config has
    // a valid isoPath, and that turned the launcher invisible to the user.
    // Keep the prelaunch UI up so the user can navigate it; the Play button
    // (line 719) still flips IsGameLaunched when explicitly pressed.
#endif

    if (!mEntranceAnimationStarted && mDocument != nullptr) {
        mDocument->SetClass("animate-in", true);
        mEntranceAnimationStarted = true;
    }

    if (!mMenuButtons.empty()) {
        mMenuButtons[0]->set_text(activeDiscLoaded ? dusk::i18n::tr("prelaunch.play")
                                                   : dusk::i18n::tr("prelaunch.select_disc"));
    }

    const auto discStatusLabel = mDiscStatus->GetElementById("disc-status-label");

    if (mDiscStatus != nullptr && discStatusLabel != nullptr) {
        if (!activeDiscLoaded) {
            mDiscStatus->RemoveAttribute("status");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.none"));
        } else if (discRestartPending) {
            mDiscStatus->SetAttribute("status", "pending");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.pending"));
        } else if (state.configuredDiscValidation == iso::ValidationError::Success) {
            mDiscStatus->SetAttribute("status", "good");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.ready"));
        } else if (state.configuredDiscValidation == iso::ValidationError::HashMismatch) {
            mDiscStatus->SetAttribute("status", "mismatch");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.mismatch"));
        } else if (canLaunchConfiguredDisc) {
            mDiscStatus->SetAttribute("status", "unknown");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.unverified"));
        } else {
            mDiscStatus->SetAttribute("status", "bad");
            discStatusLabel->SetInnerRML(dusk::i18n::tr("prelaunch.disc.unavailable"));
        }
    }
    if (mDiscDetail != nullptr) {
        if (activeDiscLoaded) {
            mDiscDetail->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Block);
            Rml::String innerRML = "GameCube • ";
            innerRML += state.activeDiscInfo.isPal ? "EUR" : "USA";
            mDiscDetail->SetInnerRML(innerRML);
        } else {
            mDiscDetail->SetProperty(Rml::PropertyId::Display, Rml::Style::Display::None);
        }
    }
    if (mVersion != nullptr) {
        std::string_view versionStr(DUSK_WC_DESCRIBE);
        if (versionStr[0] == 'v') {
            versionStr = versionStr.substr(1);
        }
        mVersion->SetInnerRML(escape(versionStr));
    }
    if (mUpdateStatus != nullptr && mUpdateMessage != nullptr) {
        if (auto result = take_finished_update_check()) {
            if (result->status == update_check::Status::Failed) {
                PrelaunchLog.error("Failed to check for updates: {}", result->message);
            }
            sUpdateCheckResult = std::move(*result);
        }

        if (sUpdateCheckTask != nullptr) {
            mUpdateStatus->SetAttribute("state", "checking");
            mUpdateMessage->SetInnerRML(escape(dusk::i18n::tr("prelaunch.update.checking")));
        } else if (!sUpdateCheckResult.has_value() ||
                   sUpdateCheckResult->status == update_check::Status::UpToDate)
        {
            mUpdateStatus->RemoveAttribute("state");
            mUpdateMessage->SetInnerRML("");
        } else if (sUpdateCheckResult->status == update_check::Status::UpdateAvailable) {
            mUpdateStatus->SetAttribute("state", "available");
            mUpdateMessage->SetInnerRML(escape(dusk::i18n::tr("prelaunch.update.available")));
            if (mUpdateDownloadLabel != nullptr) {
                mUpdateDownloadLabel->SetInnerRML(escape(dusk::i18n::tr_fmt(
                    "prelaunch.update.download", update_release_label(sUpdateCheckResult->latest))));
            }
        } else {
            mUpdateStatus->SetAttribute("state", "failed");
            mUpdateMessage->SetInnerRML(escape(dusk::i18n::tr("prelaunch.update.failed")));
        }
    }

    Document::update();
}

bool Prelaunch::focus() {
    if (mMenuButtons.empty()) {
        return false;
    }
    return mMenuButtons.front()->focus();
}

bool Prelaunch::visible() const {
    return mDocument->HasAttribute("open") && mRoot->HasAttribute("open");
}

bool Prelaunch::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    int direction = 0;
    if (cmd == NavCommand::Down) {
        direction = 1;
    } else if (cmd == NavCommand::Up) {
        direction = -1;
    } else {
        return false;
    }
    auto* target = event.GetTargetElement();
    int focusedButton = -1;
    for (int i = 0; i < mMenuButtons.size(); ++i) {
        if (mMenuButtons[i]->contains(target)) {
            focusedButton = i;
            break;
        }
    }
    const auto n = static_cast<int>(mMenuButtons.size());
    int i = ((focusedButton + direction) % n + n) % n;
    while (i >= 0 && i < mMenuButtons.size()) {
        if (mMenuButtons[i]->focus()) {
            mDoAud_seStartMenu(kSoundItemFocus);
            event.StopPropagation();
            return true;
        }
        i += direction;
    }
    return false;
}

}  // namespace dusk::ui
