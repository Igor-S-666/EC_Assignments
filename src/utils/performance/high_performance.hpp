/**
 * @file high_performance.hpp
 * @brief Keeps time-limited experiments from being slowed down by Windows' power management.
 *
 * On hybrid CPUs (e.g. i7-12700: 8 performance + 4 efficiency cores), Windows 11 may treat a console process
 * without a focused window as background work. It then applies EcoQoS ("power throttling"), which moves the process
 * to the efficiency cores at a low clock. That made runs of the lab 6/7 experiments up to ~10x slower, at random
 * moments. A time-limited method then does far fewer iterations, so results depend on luck.
 *
 * request_high_performance():
 *  - opts the process out of execution-speed power throttling (EcoQoS),
 *  - sets HIGH_PRIORITY_CLASS,
 *  - restricts the process to the most performant cores (highest EfficiencyClass), if the CPU has several classes,
 *  - asks the system not to go to sleep while the experiment runs.
 * It returns a short description of what was applied. On other systems it does nothing.
 */

#pragma once

#include <string>

#ifdef _WIN32
// the APIs below are Windows 10 ones; MinGW may default to an older target and hide their declarations
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>

inline std::string request_high_performance() {
    std::string report;
    HANDLE process = GetCurrentProcess();

    PROCESS_POWER_THROTTLING_STATE throttling{};
    throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    throttling.StateMask = 0;  // controlled + not set = never throttle
    report += SetProcessInformation(process, ProcessPowerThrottling, &throttling, sizeof(throttling))
                  ? "power throttling off" : "power throttling: FAILED";

    report += SetPriorityClass(process, HIGH_PRIORITY_CLASS) ? ", high priority" : ", priority: FAILED";

    // performance cores = CPU sets with the highest EfficiencyClass. MinGW's headers have the types but not these
    // two functions, so they're loaded from kernel32 at runtime.
    using get_info_t = BOOL(WINAPI*)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);
    using set_sets_t = BOOL(WINAPI*)(HANDLE, const ULONG*, ULONG);
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto GetSystemCpuSetInformation =
        reinterpret_cast<get_info_t>(reinterpret_cast<void*>(GetProcAddress(kernel32, "GetSystemCpuSetInformation")));
    const auto SetProcessDefaultCpuSets =
        reinterpret_cast<set_sets_t>(reinterpret_cast<void*>(GetProcAddress(kernel32, "SetProcessDefaultCpuSets")));
    ULONG length = 0;
    if (GetSystemCpuSetInformation && SetProcessDefaultCpuSets) {
        GetSystemCpuSetInformation(nullptr, 0, &length, process, 0);
    } else {
        report += ", core pinning: not available";
    }
    std::vector<char> buffer(length);
    auto* info = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(buffer.data());
    if (length > 0 && GetSystemCpuSetInformation(info, length, &length, process, 0)) {
        std::vector<ULONG> ids;
        std::vector<BYTE> classes;
        for (ULONG offset = 0; offset < length;) {
            const auto* entry = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buffer.data() + offset);
            if (entry->Type == CpuSetInformation) {
                ids.push_back(entry->CpuSet.Id);
                classes.push_back(entry->CpuSet.EfficiencyClass);
            }
            offset += entry->Size;
        }
        BYTE best = 0, worst = 255;
        for (const BYTE c : classes) best = c > best ? c : best, worst = c < worst ? c : worst;
        if (best != worst) {
            std::vector<ULONG> performance;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (classes[i] == best) performance.push_back(ids[i]);
            }
            report += SetProcessDefaultCpuSets(process, performance.data(), static_cast<ULONG>(performance.size()))
                          ? ", pinned to " + std::to_string(performance.size()) + "/" + std::to_string(ids.size()) +
                                " logical cores (performance cores)"
                          : ", core pinning: FAILED";
        } else {
            report += ", all cores equal (no pinning)";
        }
    }

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    return report;
}
#else
inline std::string request_high_performance() { return "not Windows: nothing to do"; }
#endif
