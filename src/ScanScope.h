#pragma once

#include <functional>
#include <string>
#include <vector>

// Real scope work done next to the Defender engine call: the same categories
// Norton's progress window reported (files, system, memory, startup entries).
namespace scope {

struct EnvironmentCounts {
    long long services = 0;   // HKLM\SYSTEM\CurrentControlSet\Services subkeys: drivers + services
    long long startup = 0;    // Run/RunOnce values plus the two Startup folders
    long long processes = 0;  // live process list, re-read while the scan runs
};

EnvironmentCounts CountEnvironment();
long long CountProcesses();
long long CountStartupEntries();
long long CountServices();

// 1 = quick, 2 = full, 3 = custom (uses customPath)
std::vector<std::wstring> Roots(int scanType, const std::wstring& customPath);

struct WalkResult {
    long long files = 0;
    long long directories = 0;
    long long denied = 0;
    bool stopped = false;  // true when the callback or the engine asked to stop
};

// Recurses through roots without following reparse points. report() is called
// every file with the current path and counters; returning false stops the walk.
// stopRequested() is polled per directory so the walk can end as soon as the
// engine finishes.
WalkResult Walk(const std::vector<std::wstring>& roots,
                const std::function<bool(const std::wstring& current, const WalkResult&)>& report,
                const std::function<bool()>& stopRequested);

long long CountFiles(const std::wstring& root, const std::function<bool()>& stopRequested);

// Total bytes currently on disk under the Defender definition and platform
// folders; used to derive the download rate while Update runs.
long long DefinitionBytes();

}  // namespace scope
