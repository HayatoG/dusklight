// Minimal ImGui forward declarations for the Switch build.
//
// AURORA_ENABLE_IMGUI is OFF on Switch, so <imgui.h> is unavailable. But
// game-core headers (ImGuiConsole.hpp, ImGuiMenuTools.hpp, etc.) reference
// ImGui types as part of class declarations that are transitively pulled
// in by core game code. This shim makes those headers parseable.
//
// Linking succeeds because the actual ImGui*.cpp files are excluded from
// the Switch build (CMakeLists.txt list(FILTER ...) on src/dusk/imgui/).

#pragma once

#ifdef __SWITCH__

#include <cstdint>

struct ImVec2 {
    float x = 0.0f;
    float y = 0.0f;
    ImVec2() = default;
    ImVec2(float _x, float _y) : x(_x), y(_y) {}
};
struct ImVec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    ImVec4() = default;
    ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct ImGuiWindow;        // opaque
struct ImGuiContext;       // opaque
struct ImGuiIO;            // opaque
struct ImFont;             // opaque
struct ImFontAtlas;        // opaque
struct ImDrawList;         // opaque
using ImGuiID = uint32_t;
using ImTextureID = uint64_t;

enum ImGuiKey : int {
    ImGuiKey_None = 0,
};

using ImGuiKeyChord = int;
using ImGuiSliderFlags = int;
using ImGuiTreeNodeFlags = int;
using ImGuiWindowFlags = int;
using ImGuiInputTextFlags = int;

#endif // __SWITCH__
