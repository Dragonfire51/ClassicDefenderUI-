#include "FolderPicker.h"

#include <shobjidl.h>

namespace ui {

bool PickFolder(HWND owner, std::wstring& path) {
    IFileOpenDialog* dialog = nullptr;
    const HRESULT created = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
                                             reinterpret_cast<void**>(&dialog));
    if (FAILED(created) || !dialog) return false;

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Select a folder");

    bool picked = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item) {
            PWSTR text = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &text)) && text) {
                path.assign(text);
                picked = true;
            }
            CoTaskMemFree(text);
            item->Release();
        }
    }

    dialog->Release();
    return picked;
}

}  // namespace ui
