#include "dusk/i18n.hpp"

#include "dusk/logging.h"

#include <cstdio>
#include <unordered_map>

#include <SDL3/SDL_filesystem.h>

#include "nlohmann/json.hpp"

namespace dusk::i18n {
namespace {

aurora::Module Log{"dusk::i18n"};

std::unordered_map<std::string, std::string> g_active;
std::unordered_map<std::string, std::string> g_english;
bool g_englishLoaded = false;

const char* code_for(int lang) {
    switch (lang) {
    case 1:
        return "pt";
    case 2:
        return "es";
    default:
        return "en";
    }
}

// Read res/lang/<code>.json into `out`. Returns false (leaving `out` untouched) on any error — the
// caller falls back to English. romfs is the resources root on Switch (SDL_GetBasePath() == "romfs:/").
bool read_table(const char* code, std::unordered_map<std::string, std::string>& out) {
    const char* base = SDL_GetBasePath();
    const std::string path = std::string(base != nullptr ? base : "") + "res/lang/" + code + ".json";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        Log.warn("cannot open language file {}", path);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string buf;
    if (size > 0) {
        buf.resize(static_cast<size_t>(size));
        std::fread(buf.data(), 1, static_cast<size_t>(size), f);
    }
    std::fclose(f);
    try {
        const auto j = nlohmann::json::parse(buf);
        std::unordered_map<std::string, std::string> parsed;
        for (const auto& item : j.items()) {
            if (item.value().is_string()) {
                parsed.emplace(item.key(), item.value().get<std::string>());
            }
        }
        out = std::move(parsed);
        return true;
    } catch (const std::exception& e) {
        Log.warn("failed to parse {}: {}", path, e.what());
        return false;
    }
}

}  // namespace

void load(int uiLanguage) {
    if (!g_englishLoaded) {
        read_table("en", g_english);
        g_englishLoaded = true;
    }
    const char* code = code_for(uiLanguage);
    if (std::string_view(code) == "en") {
        g_active = g_english;
        return;
    }
    if (!read_table(code, g_active)) {
        g_active = g_english;  // fall back to English on any failure
    }
}

const std::string& tr(std::string_view key) {
    const std::string k(key);
    if (const auto it = g_active.find(k); it != g_active.end()) {
        return it->second;
    }
    if (const auto it = g_english.find(k); it != g_english.end()) {
        return it->second;
    }
    // Missing in both tables: cache the key as its own value so the returned reference stays valid
    // (and the untranslated key shows up in the UI, which makes gaps obvious during QA).
    return g_active.emplace(k, k).first->second;
}

const std::string& tr_index(std::string_view prefix, int index) {
    std::string key(prefix);
    key += '.';
    key += std::to_string(index);
    return tr(key);
}

}  // namespace dusk::i18n
