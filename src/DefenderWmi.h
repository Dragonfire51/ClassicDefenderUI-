#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

// Snapshot of MSFT_MpComputerStatus, the class Windows Security itself reads.
struct DefenderStatus {
    bool wmiConnected = false;
    bool antivirusEnabled = false;
    bool realTimeProtection = false;
    bool behaviorMonitoring = false;
    bool cloudProtection = false;
    bool tamperProtection = false;
    std::wstring signatureVersion;
    std::wstring engineVersion;
    std::wstring serviceVersion;
    std::wstring lastQuickScan;
    std::wstring lastFullScan;
    std::wstring lastSignatureUpdate;
};

// One row of MSFT_MpThreatDetection, joined with the MSFT_MpThreat catalogue.
struct ThreatRecord {
    long long threatId = 0;
    std::wstring name;
    int severityId = 0;
    int statusId = 0;
    std::wstring detected;
    std::wstring resources;
    bool actionSucceeded = false;
};

// Initialises COM for the calling thread. WMI must never be driven from the UI
// thread, so every worker owns one of these before touching DefenderClient.
class ComApartment {
public:
    ComApartment();
    ~ComApartment();
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    bool usable() const { return usable_; }

private:
    long result_;
    bool usable_;
};

std::wstring SeverityName(int severityId);
std::wstring ThreatStatusName(int statusId);

// Thin COM/WMI wrapper around ROOT\Microsoft\Windows\Defender.
class DefenderClient {
public:
    DefenderClient();
    ~DefenderClient();
    DefenderClient(const DefenderClient&) = delete;
    DefenderClient& operator=(const DefenderClient&) = delete;

    bool QueryStatus(DefenderStatus& status);
    bool QueryThreats(std::vector<ThreatRecord>& threats);
    bool QueryExclusions(std::vector<std::wstring>& paths);

    // scanType: 1 = quick, 2 = full, 3 = custom (path required).
    bool StartScan(long scanType, const std::wstring& path = std::wstring());
    bool UpdateSignatures();

    // Quarantine actions. The provider exposes no method for them, so these
    // drive the engine's own MpCmdRun.exe (-Restore / -Remove).
    bool RestoreThreat(const std::wstring& threatName);
    bool RemoveThreat(const std::wstring& threatName);

    bool AddExclusion(const std::wstring& path);
    bool RemoveExclusion(const std::wstring& path);

    bool SetRealTimeProtection(bool enabled);
    bool SetCloudProtection(bool enabled);

    // Human readable reason for the most recent failure (localised system text
    // plus the WBEM HRESULT, e.g. access denied under Tamper Protection).
    const std::wstring& LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
