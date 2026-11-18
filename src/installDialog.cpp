#include "installDialog.h"
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <thread>

namespace {
const COLORREF theme = RGB(64, 158, 255);
const COLORREF themeDark = RGB(0, 122, 204);
constexpr UINT progressMessage = WM_APP + 1;
constexpr UINT finishedMessage = WM_APP + 2;
constexpr UINT_PTR finishTimer = 1;

enum class Page { location, installing, success, failure };

struct InstallDialog {
    std::wstring &directory;
    bool fresh;
    Page page = Page::location;
    std::thread worker;
    std::wstring error;
    ULONGLONG startedAt = 0;
};

std::wstring enteredPath(HWND dialog) {
    HWND edit = GetDlgItem(dialog, IDC_INSTALL_PATH);
    std::wstring text(GetWindowTextLengthW(edit) + 1, L'\0');
    int length = GetWindowTextW(edit, &text[0], static_cast<int>(text.size()));
    text.resize(length);
    return text;
}

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
        picker->SetTitle(L"选择上级文件夹");
        IShellItem *item = nullptr;
        if (SUCCEEDED(picker->Show(owner)) && SUCCEEDED(picker->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                auto destination = installDirectoryForSelection(path);
                SetDlgItemTextW(owner, IDC_INSTALL_PATH, destination.empty() ? path : destination.c_str());
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
        wchar_t name[32]{};
        GetWindowTextW(item.hwndItem, name, _countof(name));
        DrawTextW(dc, name, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, previous);
        DeleteObject(heading);
    } else {
        wchar_t caption[32]{};
        GetWindowTextW(item.hwndItem, caption, _countof(caption));
        DrawTextW(dc, caption, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item.itemState & ODS_FOCUS) {
            InflateRect(&rect, -3, -3);
            DrawFocusRect(dc, &rect);
        }
    }
    RestoreDC(dc, saved);
}

void showResult(HWND dialog, InstallDialog &state) {
    const bool success = state.error.empty();
    state.page = success ? Page::success : Page::failure;
    SetDlgItemTextW(dialog, IDC_INSTALL_LABEL, success ? L"安装完成" : L"安装失败");
    const auto detail = success ? L"安装位置：\r\n" + state.directory : state.error;
    SetDlgItemTextW(dialog, IDC_INSTALL_RESULT, detail.c_str());
    ShowWindow(GetDlgItem(dialog, IDC_INSTALL_PROGRESS), SW_HIDE);
    ShowWindow(GetDlgItem(dialog, IDC_INSTALL_DESTINATION), SW_HIDE);
    ShowWindow(GetDlgItem(dialog, IDC_INSTALL_RESULT), SW_SHOW);
    SetDlgItemTextW(dialog, IDOK, success ? L"完成" : L"关闭");
    EnableMenuItem(GetSystemMenu(dialog, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
    ShowWindow(GetDlgItem(dialog, IDOK), SW_SHOW);
    SetFocus(GetDlgItem(dialog, IDOK));
}

void beginInstall(HWND dialog, InstallDialog &state) {
    if (state.fresh) {
        auto path = installDirectoryForSelection(enteredPath(dialog));
        auto error = validateInstallDirectory(path);
        if (!error.empty()) {
            MessageBoxW(dialog, error.c_str(), L"请选择安装位置", MB_OK | MB_ICONINFORMATION);
            SetFocus(GetDlgItem(dialog, IDC_INSTALL_PATH));
            return;
        }
        state.directory = path;
    }
    state.page = Page::installing;
    state.startedAt = GetTickCount64();
    EnableMenuItem(GetSystemMenu(dialog, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    for (int id : {IDC_INSTALL_PATH, IDC_INSTALL_BROWSE, IDCANCEL, IDOK})
        ShowWindow(GetDlgItem(dialog, id), SW_HIDE);
    SetDlgItemTextW(dialog, IDC_INSTALL_LABEL, L"正在安装");
    SetDlgItemTextW(dialog, IDC_INSTALL_DESTINATION, L"准备安装目录");
    ShowWindow(GetDlgItem(dialog, IDC_INSTALL_PROGRESS), SW_SHOW);
    UpdateWindow(dialog);
    try {
        state.worker = std::thread([dialog, &state] {
            try {
                state.error = installFiles(state.directory, state.fresh, [dialog](int percent, InstallStep step) {
                    SendMessageW(dialog, progressMessage, percent, static_cast<LPARAM>(step));
                });
            } catch (const std::exception &) {
                state.error = L"安装过程中发生错误，请关闭安装程序后重试。";
            }
            PostMessageW(dialog, finishedMessage, 0, 0);
        });
    } catch (const std::exception &) {
        state.error = L"无法启动安装任务，请关闭安装程序后重试。";
        showResult(dialog, state);
    }
}

INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto *state = reinterpret_cast<InstallDialog *>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        state = reinterpret_cast<InstallDialog *>(lParam);
        SetDlgItemTextW(dialog, IDC_INSTALL_PATH, state->directory.c_str());
        if (!state->fresh) {
            SendDlgItemMessageW(dialog, IDC_INSTALL_PATH, EM_SETREADONLY, TRUE, 0);
            EnableWindow(GetDlgItem(dialog, IDC_INSTALL_BROWSE), FALSE);
            SetDlgItemTextW(dialog, IDC_INSTALL_DESTINATION, L"将更新此位置的现有安装。");
        }
        SendDlgItemMessageW(dialog, IDC_INSTALL_PATH, EM_SETLIMITTEXT, MAX_PATH, 0);
        // The themed bar animates behind the reported position on fast file operations.
        HWND bar = GetDlgItem(dialog, IDC_INSTALL_PROGRESS);
        HIGHCONTRASTW contrast{sizeof(contrast)};
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
            !(contrast.dwFlags & HCF_HIGHCONTRASTON)) {
            SetWindowTheme(bar, L"", L"");
            SetWindowLongPtrW(bar, GWL_STYLE, GetWindowLongPtrW(bar, GWL_STYLE) | PBS_SMOOTH);
            SendMessageW(bar, PBM_SETBARCOLOR, 0, themeDark);
            SendMessageW(bar, PBM_SETBKCOLOR, 0, RGB(238, 238, 238));
        }
        SendMessageW(dialog, WM_SETICON, ICON_SMALL,
                     (LPARAM)LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1)));
        SendMessageW(dialog, DM_SETDEFID, IDOK, 0);
        SetFocus(GetDlgItem(dialog, IDOK));
        return FALSE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wParam, RGB(255, 255, 255));
        SetTextColor((HDC)wParam, RGB(51, 51, 51));
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    case WM_DRAWITEM:
        paintControl(dialog, *(const DRAWITEMSTRUCT *)lParam);
        return TRUE;
    case progressMessage: {
        static constexpr const wchar_t *labels[] = {L"准备安装目录", L"检查并复制程序文件", L"注册 Windows 计划任务",
                                                    L"注册网页启动协议", L"保存安装和卸载信息", L"安装步骤已完成"};
        if (!state || state->page != Page::installing || wParam > 100 ||
            static_cast<size_t>(lParam) >= _countof(labels)) return TRUE;
        SendDlgItemMessageW(dialog, IDC_INSTALL_PROGRESS, PBM_SETPOS, wParam, 0);
        const auto step = std::wstring(labels[lParam]) + L"  " + std::to_wstring(wParam) + L"%";
        SetDlgItemTextW(dialog, IDC_INSTALL_DESTINATION, step.c_str());
        UpdateWindow(GetDlgItem(dialog, IDC_INSTALL_PROGRESS));
        UpdateWindow(GetDlgItem(dialog, IDC_INSTALL_DESTINATION));
        return TRUE;
    }
    case finishedMessage: {
        if (!state || state->page != Page::installing || !state->worker.joinable()) return TRUE;
        state->worker.join();
        // Let a fast install's progress remain visible before replacing the page.
        const auto elapsed = GetTickCount64() - state->startedAt;
        if (state->error.empty() && elapsed < 500 &&
            SetTimer(dialog, finishTimer, static_cast<UINT>(500 - elapsed), nullptr)) return TRUE;
        showResult(dialog, *state);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == finishTimer && state && state->page == Page::installing && !state->worker.joinable()) {
            KillTimer(dialog, finishTimer);
            showResult(dialog, *state);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        if (!state || state->page == Page::installing) return TRUE;
        if (state->page == Page::location) EndDialog(dialog, IDCANCEL);
        else EndDialog(dialog, state->page == Page::success ? IDOK : IDABORT);
        return TRUE;
    case WM_COMMAND:
        if (!state || state->page == Page::installing) return TRUE;
        switch (LOWORD(wParam)) {
        case IDC_INSTALL_PATH:
            if (HIWORD(wParam) == EN_CHANGE) {
                const auto text = enteredPath(dialog);
                const auto destination = installDirectoryForSelection(text);
                const auto preview = !destination.empty() && destination != text
                    ? L"将安装到：" + destination : std::wstring{};
                SetDlgItemTextW(dialog, IDC_INSTALL_DESTINATION, preview.c_str());
            }
            break;
        case IDC_INSTALL_BROWSE:
            browse(dialog);
            return TRUE;
        case IDOK:
            if (state->page == Page::location) beginInstall(dialog, *state);
            else SendMessageW(dialog, WM_CLOSE, 0, 0);
            return TRUE;
        case IDCANCEL:
            SendMessageW(dialog, WM_CLOSE, 0, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
}

bool showInstallDialog(std::wstring &directory, bool fresh) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS};
    if (!InitCommonControlsEx(&controls)) {
        MessageBoxW(nullptr, L"无法加载安装进度条。", L"椰羊", MB_ICONERROR);
        SetLastError(ERROR_INSTALL_FAILURE);
        return false;
    }
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    InstallDialog state{directory, fresh};
    INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_INSTALL),
                                     nullptr, procedure, reinterpret_cast<LPARAM>(&state));
    if (state.worker.joinable()) state.worker.join();
    if (SUCCEEDED(com)) CoUninitialize();
    if (result == -1)
        MessageBoxW(nullptr, L"无法打开安装窗口。", L"椰羊", MB_ICONERROR);
    if (result != IDOK) SetLastError(result == IDCANCEL ? ERROR_CANCELLED : ERROR_INSTALL_FAILURE);
    return result == IDOK;
}
