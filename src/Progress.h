#pragma once

#include <functional>
#include <mutex>
#include <string>

// Snapshot of a long running Defender operation. Defender's WMI provider only
// reports start and finish (MSFT_MpScan has Start, MSFT_MpSignature has Update,
// and MSFT_MpComputerStatus has no in-progress fields), so the counters come
// from the local work this app does around the engine call: walking the scan
// scope, counting services, processes and startup entries, and measuring the
// definition folder while it grows.
enum class ProgressKind { Scan, Update };

enum class ProgressPhase { Preparing, Running, Finishing, Done, Failed };

struct ProgressSnapshot {
    ProgressKind kind = ProgressKind::Scan;
    ProgressPhase phase = ProgressPhase::Preparing;
    std::wstring title;
    std::wstring subtitle;
    std::wstring current;
    std::wstring message;

    long long filesChecked = 0;
    long long filesTotal = 0;  // 0 means "unknown", the bar then runs indeterminate
    long long systemItems = 0;
    long long memoryItems = 0;
    long long startupItems = 0;

    long long bytes = 0;
    double bytesPerSecond = 0.0;
    std::wstring signatures;  // version reported while updating definitions
    long long threats = 0;

    int percent = -1;  // -1 while indeterminate
};

inline bool IsFinished(const ProgressSnapshot& snapshot) {
    return snapshot.phase == ProgressPhase::Done || snapshot.phase == ProgressPhase::Failed;
}

class ProgressTracker {
public:
    void Update(const std::function<void(ProgressSnapshot&)>& mutate) {
        std::lock_guard<std::mutex> lock(mutex_);
        mutate(snapshot_);
    }

    ProgressSnapshot Get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    }

private:
    mutable std::mutex mutex_;
    ProgressSnapshot snapshot_;
};
