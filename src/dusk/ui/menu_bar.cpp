#include "menu_bar.hpp"

#include <RmlUi/Core.h>

#include "Z2AudioLib/Z2SeMgr.h"
#include "m_Do/m_Do_audio.h"

#include "achievements.hpp"
#include "aurora/rmlui.hpp"
#include "dusk/speedrun.h"
#include "dusk/livesplit.h"
#include "dusk/i18n.hpp"
#include "dusk/main.h"
#include "dusk/settings.h"
#include "editor.hpp"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "imgui.h"
#include "modal.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "warp.hpp"
#include "window.hpp"

#include <chrono>
#include <cmath>

namespace dusk::ui {
namespace {

const Rml::String kDocumentSource = R"RML(
<rml>
<head>
    <link type="text/rcss" href="res/rml/tabbing.rcss" />
    <link type="text/rcss" href="res/rml/popup.rcss" />
</head>
<body>
    <popup id="popup" />
</body>
</rml>
)RML";

}

MenuBar::MenuBar() : Document(kDocumentSource), mRoot(mDocument->GetElementById("popup")) {
    mTabBar = std::make_unique<TabBar>(mRoot, TabBar::Props{
                                                  .onClose =
                                                      [this] {
                                                          mDoAud_seStartMenu(kSoundMenuClose);
                                                          hide(false);
                                                      },
                                                  .autoSelect = false,
                                              });
    mTabBar->add_tab(dusk::i18n::tr("common.settings"),
                     [this] { push(std::make_unique<SettingsWindow>()); });

    if (getSettings().backend.enableAdvancedSettings) {
        mTabBar->add_tab(dusk::i18n::tr("menu.warp"), [this] { push(std::make_unique<WarpWindow>()); });
        mTabBar->add_tab(dusk::i18n::tr("menu.editor"),
                         [this] { push(std::make_unique<EditorWindow>()); });
    }

    mTabBar->add_tab(dusk::i18n::tr("menu.achievements"),
                     [this] { push(std::make_unique<AchievementsWindow>()); });


    mTabBar->add_tab(dusk::i18n::tr("menu.reset"), [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = dusk::i18n::tr("reset.title"),
            .bodyRml = dusk::i18n::tr("reset.body"),
            .actions =
                {
                    ModalAction{
                        .label = dusk::i18n::tr("common.cancel"),
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundWindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = dusk::i18n::tr("common.reset"),
                        .onPressed =
                            [this, dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundClick);
                                if (fpcM_SearchByName(fpcNm_LOGO_SCENE_e)) {
                                    dismiss(modal);
                                    return;
                                }
                                JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
                                dismiss(modal);
                                hide(false);
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });
    mTabBar->add_tab(dusk::i18n::tr("common.quit"), [this] {
        mTabBar->set_active_tab(-1);
        const auto dismiss = [](Modal& modal) { modal.pop(); };
        push(std::make_unique<Modal>(Modal::Props{
            .title = dusk::i18n::tr("menu.quit.title"),
            .bodyRml = dusk::i18n::tr("menu.quit.body"),
            .actions =
                {
                    ModalAction{
                        .label = dusk::i18n::tr("common.cancel"),
                        .onPressed =
                            [dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundWindowClose);
                                dismiss(modal);
                            },
                    },
                    ModalAction{
                        .label = dusk::i18n::tr("common.quit"),
                        .onPressed =
                            [dismiss](Modal& modal) {
                                mDoAud_seStartMenu(kSoundClick);
                                dismiss(modal);
                                IsRunning = false;
                            },
                    },
                },
            .onDismiss = dismiss,
            .icon = "question-mark",
        }));
    });

    if (getSettings().game.speedrunMode) {
        mTabBar->add_tab(dusk::i18n::tr("menu.reset_timer"), [this] {
            mTabBar->set_active_tab(-1);
            mDoAud_seStartMenu(kSoundClick);
            m_speedrunInfo.reset();
            if (getSettings().game.liveSplitEnabled) {
                dusk::speedrun::reset();
            }
            hide(false);
        });
    }

    // Hide document after transition completion
    listen(mRoot, Rml::EventId::Transitionend, [this](Rml::Event& event) {
        if (event.GetTargetElement() == mRoot && !mRoot->HasAttribute("open") &&
            Document::visible())
        {
            Document::hide(mPendingClose);
        }
    });
}

void MenuBar::show() {
    Document::show();
    mRoot->SetAttribute("open", "");
    mTabBar->set_active_tab(-1);
    if (!mTabBar->focus_tab(mFocusedTabIndex)) {
        mTabBar->focus();
    }
}

void MenuBar::hide(bool close) {
    mFocusedTabIndex = mTabBar->focused_tab_index();
    mRoot->RemoveAttribute("open");
    if (close) {
        mPendingClose = true;
    }
}

void MenuBar::update() {
    update_safe_area();
    Document::update();
}

void MenuBar::update_safe_area() noexcept {
    if (mDocument == nullptr || mTabBar == nullptr) {
        return;
    }

    // Avoid ImGui menu bar if shown
    if (const auto* viewport = ImGui::GetMainViewport();
        viewport != nullptr && mTopMargin != viewport->WorkPos.y)
    {
        mTopMargin = viewport->WorkPos.y;
        mRoot->SetProperty(Rml::PropertyId::MarginTop, Rml::Property(mTopMargin, Rml::Unit::DP));
    }

    Rml::Context* context = mDocument->GetContext();
    Insets safeInsets = safe_area_insets(context);
    safeInsets = {
        0.0f,
        std::round(safeInsets.right),
        0.0f,
        std::round(safeInsets.left),
    };
    if (safeInsets == mTabBarPadding) {
        return;
    }

    mTabBarPadding = safeInsets;
    auto* tabBar = mTabBar->root();
    tabBar->SetProperty(
        Rml::PropertyId::PaddingRight, Rml::Property(safeInsets.right, Rml::Unit::PX));
    tabBar->SetProperty(
        Rml::PropertyId::PaddingLeft, Rml::Property(safeInsets.left, Rml::Unit::PX));
    if (auto* close = tabBar->QuerySelector("close")) {
        close->SetProperty(Rml::PropertyId::Right,
            Rml::Property(safeInsets.right + 8.0f * context->GetDensityIndependentPixelRatio(),
                Rml::Unit::PX));
    }
}

bool MenuBar::visible() const {
    return mRoot->HasAttribute("open");
}

bool MenuBar::handle_nav_command(Rml::Event& event, NavCommand cmd) {
    if (!getSettings().backend.wasPresetChosen) {
        return true;
    }
    if (cmd == NavCommand::Cancel && visible()) {
        mDoAud_seStartMenu(kSoundMenuClose);
        hide(false);
        return true;
    }
    return Document::handle_nav_command(event, cmd);
}

bool MenuBar::focus() {
    return mTabBar->focus();
}

}  // namespace dusk::ui
