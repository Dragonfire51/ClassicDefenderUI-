#pragma once

#include <windows.h>

#include <thread>
#include <utility>

// Runs `work` on a detached worker and posts its result back as a heap object.
// Workers never capture the window object, so a closed window cannot dangle.
template <typename Result, typename Work>
void PostAsync(HWND target, UINT message, WPARAM wParam, Work work) {
    std::thread([target, message, wParam, work = std::move(work)]() mutable {
        Result* payload = new Result(std::move(work()));
        if (!PostMessageW(target, message, wParam, reinterpret_cast<LPARAM>(payload))) delete payload;
    }).detach();
}
