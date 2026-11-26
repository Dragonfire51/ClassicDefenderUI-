// Console harness for the scan scope layer. It runs on Windows (or Wine) and
// builds a throwaway tree under %TEMP% to check the walk and the counters.
//   scope_test.exe

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "ScanScope.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* label) {
    std::printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++g_failures;
}

std::wstring TempRoot() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetTempPathW(MAX_PATH, buffer);
    if (length == 0) return std::wstring();
    return std::wstring(buffer, length) + L"cdui_scope_test";
}

void MakeFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
}

bool MakeDir(const std::wstring& path) { return CreateDirectoryW(path.c_str(), nullptr) != FALSE; }

}  // namespace

int main() {
    const std::wstring root = TempRoot();
    if (root.empty()) {
        std::printf("FAIL  %%TEMP%% is not available\n");
        return 1;
    }

    MakeDir(root);
    const std::wstring first = root + L"\\first";
    const std::wstring second = root + L"\\second";
    const std::wstring nested = first + L"\\nested";
    MakeDir(first);
    MakeDir(second);
    MakeDir(nested);

    for (int index = 0; index < 3; ++index) MakeFile(root + L"\\file" + std::to_wstring(index) + L".bin");
    for (int index = 0; index < 5; ++index) MakeFile(first + L"\\file" + std::to_wstring(index) + L".bin");
    for (int index = 0; index < 2; ++index) MakeFile(nested + L"\\file" + std::to_wstring(index) + L".bin");
    for (int index = 0; index < 2; ++index) MakeFile(second + L"\\file" + std::to_wstring(index) + L".bin");

    const std::vector<std::wstring> roots = scope::Roots(3, root);
    Check(roots.size() == 1, "a custom scope resolves to the chosen folder");
    Check(scope::Roots(3, L"Z:\\no\\such\\folder").empty(), "a missing custom folder yields an empty scope");

    const std::function<bool()> never = [] { return false; };
    Check(scope::CountFiles(root, never) == 12, "counting a folder walks all 12 files");

    const scope::WalkResult walk = scope::Walk(roots, nullptr, never);
    Check(walk.files == 12, "walk reports every file");
    Check(walk.directories >= 4, "walk visits the nested directories");
    Check(!walk.stopped, "walk completes when nothing asks it to stop");

    long long reported = 0;
    const scope::WalkResult streamed =
        scope::Walk(roots, [&](const std::wstring&, const scope::WalkResult& result) {
            reported = result.files;
            return true;
        }, never);
    Check(reported == 12 && streamed.files == 12, "the progress callback sees the running count");

    const scope::WalkResult cancelled = scope::Walk(roots, nullptr, [] { return true; });
    Check(cancelled.stopped, "a stop request ends the walk early");

    Check(scope::CountProcesses() > 0, "the process list is readable");
    Check(scope::CountServices() > 0, "the services key is readable");
    Check(scope::CountStartupEntries() >= 0, "the startup locations are readable");

    const scope::EnvironmentCounts environment = scope::CountEnvironment();
    Check(environment.processes > 0, "environment counts include the running processes");

    Check(!scope::Roots(1, std::wstring()).empty(), "the quick scope resolves real system folders");

    DeleteFileW((root + L"\\file0.bin").c_str());
    DeleteFileW((root + L"\\file1.bin").c_str());
    DeleteFileW((root + L"\\file2.bin").c_str());
    for (int index = 0; index < 5; ++index) DeleteFileW((first + L"\\file" + std::to_wstring(index) + L".bin").c_str());
    for (int index = 0; index < 2; ++index) DeleteFileW((nested + L"\\file" + std::to_wstring(index) + L".bin").c_str());
    for (int index = 0; index < 2; ++index) DeleteFileW((second + L"\\file" + std::to_wstring(index) + L".bin").c_str());
    RemoveDirectoryW(nested.c_str());
    RemoveDirectoryW(first.c_str());
    RemoveDirectoryW(second.c_str());
    RemoveDirectoryW(root.c_str());

    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
