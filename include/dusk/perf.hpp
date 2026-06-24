#pragma once

// Platform performance controls.
//
// On Switch, set_boost() drives the Tegra X1 clocks directly via clkrst. Two tiers:
//   boost     -> CPU 1785 MHz + GPU 768 MHz (the docked maxima; not an overclock).
//   boostPlus -> GPU 921 MHz + memory (EMC) 1600 MHz on top of the 1785 MHz CPU. An overclock that
//                needs an OC sysmodule (sys-clk-OC); clkrst clamps the GPU to 768 MHz on stock FW.
// The frame loop is bound by the CPU, GPU and memory bandwidth, so pinning the clocks claws back
// framerate at the cost of power/heat. On every other platform this is a no-op (the OS governs clocks).

namespace dusk::perf {

// boost     = pin CPU 1785 MHz + GPU 768 MHz (the Tegra X1 docked maxima).
// boostPlus = stronger tier: GPU 921 MHz + memory (EMC) 1600 MHz, an OVERCLOCK that only applies with
//             an overclock sysmodule (sys-clk-OC) — on stock firmware clkrst clamps the GPU to 768.
//             boostPlus overrides boost. No-op on non-Switch platforms.
#ifdef __SWITCH__
void set_boost(bool boost, bool boostPlus);
#else
inline void set_boost(bool /*boost*/, bool /*boostPlus*/) {}
#endif

}  // namespace dusk::perf
