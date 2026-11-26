#pragma once

#include <windows.h>

#include <string>

namespace ui {

// Shows the shell folder picker (IFileOpenDialog with FOS_PICKFOLDERS).
// Returns false when the user cancels or COM is unavailable.
bool PickFolder(HWND owner, std::wstring& path);

}  // namespace ui
