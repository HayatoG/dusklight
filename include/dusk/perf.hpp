#pragma once

// Platform performance controls.
//
// On Switch, set_cpu_boost() requests the Tegra X1's 1785 MHz CPU boost clock
// (ApmCpuBoostMode_FastLoad) to claw back headroom on the CPU-bound frame loop.
// The frame loop is CPU-bound (HW profiling: GPU ~12 us, present ~132 us out of a
// ~34 ms frame), so trading the (idle) GPU's max clock for a faster CPU is a net win.
//
// On every other platform this is a no-op (the OS governs clocks), so callers can
// invoke it unconditionally without #ifdef.

namespace dusk::perf {

#ifdef __SWITCH__
void set_cpu_boost(bool enabled);
#else
inline void set_cpu_boost(bool /*enabled*/) {}
#endif

}  // namespace dusk::perf
