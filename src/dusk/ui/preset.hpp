#pragma once
#include "component.hpp"
#include "window.hpp"

#include <memory>
#include <vector>

namespace dusk::ui {

// Applies the "Dusklight" preset's settings without opening the chooser modal.
// Used on Switch to skip the chooser (the "Classic" option sets
// `hideTvSettingsScreen=false` which routes file-select into a brightness
// check UI we haven't wired to the Aurora render path → freezes on a black
// screen after the player names Epona). Keeping the chooser hidden also keeps
// the user from accidentally re-introducing the freeze later.
void apply_preset_dusk_silently();

class PresetWindow : public WindowSmall {
public:
    PresetWindow();

    bool focus() override;

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;

private:
    std::vector<std::unique_ptr<Component>> mButtons;
};

}  // namespace dusk::ui
