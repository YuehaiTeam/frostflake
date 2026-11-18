#include "installDialog.h"
#include <windows.h>
#include <shobjidl.h>

namespace {
const COLORREF theme = RGB(64, 158, 255);
const COLORREF themeDark = RGB(0, 122, 204);

void browse(HWND owner) {
    IFileOpenDialog *picker = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&picker)))) {
        MessageBoxW(owner, L"无法打开文件夹选择器，请直接输入安装路径。", L"椰羊", MB_OK);
        return;
    }
    FILEOPENDIALOGOPTIONS options;
    if (SUCCEEDED(picker->GetOptions(&options)) &&
        SUCCEEDED(picker->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM))) {
        picker->SetTitle(L"选择安装霜华插件的空文件夹");
        IShellItem *item = nullptr;
        if (SUCCEEDED(picker->Show(owner)) && SUCCEEDED(picker->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                SetDlgItemTextW(owner, IDC_INSTALL_PATH, path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    picker->Release();
}

void paintControl(HWND dialog, const DRAWITEMSTRUCT &item) {
    const bool header = item.CtlID == IDC_INSTALL_HEADER;
    HDC dc = item.hDC;
    int saved = SaveDC(dc);
    RECT rect = item.rcItem;
    SetDCBrushColor(dc, header || !(item.itemState & ODS_SELECTED) ? theme : themeDark);
    FillRect(dc, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    HFONT font = (HFONT)SendMessageW(dialog, WM_GETFONT, 0, 0);
    SelectObject(dc, font);
    if (header) {
        int height = rect.bottom - rect.top;
        int left = height / 6;
        HICON logo = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_INSTALL_LOGO),
                                       IMAGE_ICON, height, height, LR_SHARED);
        DrawIconEx(dc, left, 0, logo, height, height, 0, nullptr, DI_NORMAL);
        LOGFONTW title{};
        GetObjectW(font, sizeof(title), &title);
        title.lfHeight = -height / 3;
        title.lfWeight = FW_SEMIBOLD;
        HFONT heading = CreateFontIndirectW(&title);
        HGDIOBJ previous = SelectObject(dc, heading);
        rect.left = left + height;
        DrawTextW(dc, L"椰羊 · 霜华插件", -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, previous);
        DeleteObject(heading);
    } else {
        DrawTextW(dc, L"安装", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item.itemState & ODS_FOCUS) {
            InflateRect(&rect, -3, -3);
            DrawFocusRect(dc, &rect);
        }
    }
    RestoreDC(dc, saved);
}

INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        SetDlgItemTextW(dialog, IDC_INSTALL_PATH, ((std::wstring *)lParam)->c_str());
        SendDlgItemMessageW(dialog, IDC_INSTALL_PATH, EM_SETLIMITTEXT, MAX_PATH, 0);
        SendMessageW(dialog, WM_SETICON, ICON_SMALL,
                     (LPARAM)LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1)));
        SendMessageW(dialog, DM_SETDEFID, IDOK, 0);
        SetFocus(GetDlgItem(dialog, IDOK));
        return FALSE;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wParam, RGB(255, 255, 255));
        SetTextColor((HDC)wParam, RGB(51, 51, 51));
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    case WM_DRAWITEM:
        paintControl(dialog, *(const DRAWITEMSTRUCT *)lParam);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_INSTALL_BROWSE:
            browse(dialog);
            return TRUE;
        case IDOK: {
            wchar_t text[MAX_PATH + 1]{};
            GetDlgItemTextW(dialog, IDC_INSTALL_PATH, text, MAX_PATH + 1);
            std::wstring path = text;
            std::wstring error = validateInstallDirectory(path);
            if (!error.empty()) {
                MessageBoxW(dialog, error.c_str(), L"请选择安装位置", MB_OK | MB_ICONINFORMATION);
                SetFocus(GetDlgItem(dialog, IDC_INSTALL_PATH));
                return TRUE;
            }
            *(std::wstring *)GetWindowLongPtrW(dialog, DWLP_USER) = path;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
}

bool chooseInstallDirectory(std::wstring &directory) {
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_INSTALL),
                                     nullptr, procedure, (LPARAM)&directory);
    if (SUCCEEDED(com)) CoUninitialize();
    if (result == -1)
        MessageBoxW(nullptr, L"无法打开安装窗口。", L"椰羊", MB_ICONERROR);
    if (result != IDOK) SetLastError(result == IDCANCEL ? ERROR_CANCELLED : ERROR_INSTALL_FAILURE);
    return result == IDOK;
}
