// Switch CPU/GPU performance control (clkrst).
//
// The Dusklight frame loop on the Tegra X1 is bound by BOTH the CPU (game logic +
// GX->Vulkan translation) and the GPU (the scene render passes). HW testing proved
// this: appletSetCpuBoostMode(FastLoad) boosts the CPU but THROTTLES the GPU to its
// minimum clock, which tanked the in-game scene from ~30 fps to ~9 fps. The present
// profiler ("fence/GPU=12us") only measures the blit, NOT the scene render — so the
// GPU work was invisible there and the FastLoad throttle was catastrophic.
//
// So instead of FastLoad we drive the clocks directly via clkrst: pin the CPU and
// the GPU to their Tegra X1 docked stock maxima (NOT an overclock — these are the
// same ceilings the OS itself uses). This boosts the CPU WITHOUT starving the GPU.
//
// Requires clkrst service access, which the app only has when launched in full /
// title-takeover mode (not applet mode). clkrst itself is [8.0.0+]. If init fails
// we log the Result and leave the clocks alone.
//
// Opt-in (UserSettings::video.cpuBoost) and persisted: boost raises power draw and
// heat, so the user owns that choice.

#include "dusk/perf.hpp"

#include <cstdio>

#include <switch.h>

extern "C" void dusk_switch_log(const char* msg);

namespace dusk::perf {

namespace {

// Tegra X1 docked clocks (Hz). Boost = the hardware's official maxima; "stock" =
// the normal docked defaults we restore to when the boost is turned off.
constexpr u32 kCpuBoostHz = 1785000000u;  // vs 1020 MHz stock
constexpr u32 kCpuStockHz = 1020000000u;
constexpr u32 kGpuBoostHz = 768000000u;   // vs 384 MHz stock (docked)
constexpr u32 kGpuStockHz = 384000000u;

bool s_clkrstReady = false;
bool s_clkrstTried = false;
bool s_active = false;
bool s_initialized = false;
bool s_everBoosted = false;  // have we ever pinned clocks this session?

bool ensure_clkrst() {
    if (s_clkrstTried) {
        return s_clkrstReady;
    }
    s_clkrstTried = true;
    const Result rc = clkrstInitialize();
    s_clkrstReady = R_SUCCEEDED(rc);
    if (!s_clkrstReady) {
        char b[96];
        // 0x1a8 = service not accessible (e.g. applet mode). Decode tells us why.
        snprintf(b, sizeof b, "[perf] clkrstInitialize failed rc=0x%08x (full/title mode needed)\n",
                 static_cast<unsigned>(rc));
        dusk_switch_log(b);
    }
    return s_clkrstReady;
}

// Open a session for one module, set its rate, read it back, log. Closing the
// session does NOT revert the rate (the sysmodule keeps the override; this mirrors
// how sys-clk drives the clocks).
void set_module(PcvModuleId mod, u32 hz, const char* name) {
    ClkrstSession sess;
    Result rc = clkrstOpenSession(&sess, mod, 3);  // unk=3, as official sysmodules use
    char b[112];
    if (R_FAILED(rc)) {
        snprintf(b, sizeof b, "[perf]   %s open failed rc=0x%08x\n", name, static_cast<unsigned>(rc));
        dusk_switch_log(b);
        return;
    }
    rc = clkrstSetClockRate(&sess, hz);
    u32 got = 0;
    clkrstGetClockRate(&sess, &got);
    snprintf(b, sizeof b, "[perf]   %s set %u MHz -> rc=0x%08x now=%u MHz\n", name,
             hz / 1000000u, static_cast<unsigned>(rc), got / 1000000u);
    dusk_switch_log(b);
    clkrstCloseSession(&sess);
}

}  // namespace

void set_cpu_boost(bool enabled) {
    if (s_initialized && enabled == s_active) {
        return;
    }
    // Default boot state is OFF: if we've never pinned the clocks, leave the OS
    // DVFS governor untouched (pinning stock rates here could cap the GPU's
    // under-load boost and regress the baseline). Only act once a boost has run.
    if (!enabled && !s_everBoosted) {
        s_active = false;
        s_initialized = true;
        return;
    }
    if (!ensure_clkrst()) {
        return;
    }

    char b[80];
    snprintf(b, sizeof b, "[perf] CPU boost %s (clkrst: CPU %u / GPU %u MHz)\n",
             enabled ? "ON" : "OFF", (enabled ? kCpuBoostHz : kCpuStockHz) / 1000000u,
             (enabled ? kGpuBoostHz : kGpuStockHz) / 1000000u);
    dusk_switch_log(b);

    set_module(PcvModuleId_CpuBus, enabled ? kCpuBoostHz : kCpuStockHz, "CpuBus");
    set_module(PcvModuleId_GPU, enabled ? kGpuBoostHz : kGpuStockHz, "GPU");

    if (enabled) {
        s_everBoosted = true;
    }
    s_active = enabled;
    s_initialized = true;
}

}  // namespace dusk::perf
