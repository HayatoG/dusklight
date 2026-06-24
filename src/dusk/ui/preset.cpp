#include "preset.hpp"

#include "button.hpp"
#include "dusk/config.hpp"
#include "dusk/i18n.hpp"
#include "dusk/settings.h"
#include "ui.hpp"

#include <dolphin/gx/GXAurora.h>

namespace dusk::ui {
namespace {

void applyPresetClassic() {
    auto& s = getSettings();
    s.video.lockAspectRatio.setValue(true);
    s.game.bloomMode.setValue(BloomMode::Classic);
    s.game.depthOfFieldMode.setValue(DepthOfFieldMode::Classic);
    s.game.enableAchievementToasts.setValue(false);
    s.game.enableControllerToasts.setValue(false);
    s.game.internalResolutionScale.setValue(1);
    s.game.shadowResolutionMultiplier.setValue(1);
    s.game.hideTvSettingsScreen.setValue(false);
    s.game.menuScalingMode.setValue(MenuScaling::GameCube);
    s.game.enableMenuPointer.setValue(false);
    AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
}

void applyPresetDusk() {
    auto& s = getSettings();
    s.game.hideTvSettingsScreen.setValue(true);
    s.game.noReturnRupees.setValue(true);
    s.game.disableRupeeCutscenes.setValue(true);
    s.game.noSwordRecoil.setValue(true);
    s.game.fastClimbing.setValue(true);
    s.game.noMissClimbing.setValue(true);
    s.game.fastTears.setValue(true);
    s.game.biggerWallets.setValue(true);
    s.game.invertCameraXAxis.setValue(true);
    s.game.invertFirstPersonYAxis.setValue(true);
    s.game.no2ndFishForCat.setValue(true);
    s.game.buttonFishing.setValue(true);
    s.game.enableAchievementToasts.setValue(true);
    s.game.enableControllerToasts.setValue(true);
    s.game.enableQuickTransform.setValue(true);
    s.game.instantSaves.setValue(true);
    s.game.midnasLamentNonStop.setValue(true);
    s.game.enableFrameInterpolation.setValue(FrameInterpMode::Unlimited);
    s.game.sunsSong.setValue(true);
    s.game.bloomMode.setValue(BloomMode::Dusk);
    s.game.depthOfFieldMode.setValue(DepthOfFieldMode::Dusk);
    s.game.internalResolutionScale.setValue(0);
    s.game.shadowResolutionMultiplier.setValue(4);
    s.game.enableGyroAim.setValue(true);
    s.game.autoSave.setValue(true);
    s.game.menuScalingMode.setValue(MenuScaling::Dusklight);
    s.game.enhancedMapMenus.setValue(true);
    s.game.enableMenuPointer.setValue(true);
}

}  // namespace

void apply_preset_dusk_silently() {
    applyPresetDusk();
    // Switch is GPU fill-rate/bandwidth-bound on the Tegra X1 (GM20B): HW-validated that
    // the in-game framerate is dominated by resolution + per-frame GPU effects, not CPU.
    // Default the recommended preset to the optimized graphics config (~40-50fps @720p vs
    // ~30 with full effects). Everything here stays changeable from Settings -> Graphics.
    auto& s = getSettings();
    s.game.bloomMode.setValue(BloomMode::Off);
    s.game.depthOfFieldMode.setValue(DepthOfFieldMode::Off);
    s.game.enableTextureReplacements.setValue(false);
    s.game.resampler.setValue(Resampler::Bilinear);
    s.game.internalResolutionScale.setValue(4);   // 720p (kPresets index)
    s.game.shadowResolutionMultiplier.setValue(1);
}

PresetWindow::PresetWindow() : WindowSmall("modal", "modal-dialog") {
    mDialog->SetClass("modal-dialog", true);

    auto* header = append(mDialog, "div");
    header->SetClass("modal-header", true);

    auto* title = append(header, "div");
    title->SetClass("modal-title", true);
    title->SetInnerRML(dusk::i18n::tr("preset.title"));

    auto* headIcon = append(header, "icon");
    headIcon->SetClass("celebration", true);

    auto* intro = append(mDialog, "div");
    intro->SetClass("modal-body", true);
    intro->SetInnerRML(dusk::i18n::tr("preset.body"));

    auto* grid = append(mDialog, "div");
    grid->SetClass("preset-grid", true);

    struct PresetInfo {
        const char* nameKey;
        const char* descKey;
        void (*apply)();
    };

    static constexpr PresetInfo kPresets[] = {
        {"preset.classic_name", "preset.classic_desc", applyPresetClassic},
        {"preset.dusklight_name", "preset.dusklight_desc", applyPresetDusk},
    };

    for (const auto& preset : kPresets) {
        auto* col = append(grid, "div");
        col->SetClass("preset-col", true);

        auto btn = std::make_unique<Button>(col, dusk::i18n::tr(preset.nameKey));
        btn->on_nav_command([this, apply = preset.apply](Rml::Event&, NavCommand cmd) {
            if (cmd == NavCommand::Confirm) {
                apply();
                getSettings().backend.wasPresetChosen.setValue(true);
                config::Save();
                hide(true);
                return true;
            }
            return false;
        });
        mButtons.push_back(std::move(btn));

        auto* desc = append(col, "div");
        desc->SetClass("preset-desc", true);
        desc->SetInnerRML(dusk::i18n::tr(preset.descKey));
    }
}

bool PresetWindow::focus() {
    if (!mButtons.empty()) {
        return mButtons.back()->focus();
    }
    return false;
}

bool PresetWindow::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (cmd == NavCommand::Cancel || cmd == NavCommand::Menu) {
        return true;
    }
    int direction = 0;
    if (cmd == NavCommand::Left) {
        direction = -1;
    } else if (cmd == NavCommand::Right) {
        direction = 1;
    } else {
        return false;
    }
    auto* target = event.GetTargetElement();
    for (int i = 0; i < static_cast<int>(mButtons.size()); ++i) {
        if (mButtons[i]->contains(target)) {
            const int next = i + direction;
            if (next >= 0 && next < static_cast<int>(mButtons.size())) {
                if (mButtons[next]->focus()) {
                    mDoAud_seStartMenu(kSoundItemFocus);
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

}  // namespace dusk::ui
