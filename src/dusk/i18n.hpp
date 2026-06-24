#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

// Tiny UI-string localization layer for the Dusklight launcher/menus.
// Strings live in per-language JSON tables shipped in romfs (res/lang/<code>.json) and are looked up
// by key with tr(). English is always the fallback. See HayatoG/dusklight#11.
namespace dusk::i18n {

// Load the UI string table for the given language (0=English, 1=Portuguese, 2=Spanish) from
// res/lang/<code>.json. English is always loaded as the fallback. Safe to call repeatedly (e.g. when
// the user changes the language at runtime). Never throws.
void load(int uiLanguage);

// Look up a UI string by key. Returns the active-language value, falling back to English, then to the
// key itself (so a missing key renders visibly instead of blank). The returned reference is stable
// for the lifetime of the table.
const std::string& tr(std::string_view key);

// Indexed lookup for enum-keyed label arrays: tr_index("settings.fps_corner", 2) looks up the key
// "settings.fps_corner.2".
const std::string& tr_index(std::string_view prefix, int index);

// Format-aware lookup for strings with placeholders: the table value is an fmt format string, e.g.
// "{:.0f} FPS" used as tr_fmt("overlay.fps", 60.0). Returns a formatted std::string (by value).
template <typename... Args>
std::string tr_fmt(std::string_view key, Args&&... args) {
    return fmt::format(fmt::runtime(tr(key)), std::forward<Args>(args)...);
}

}  // namespace dusk::i18n
