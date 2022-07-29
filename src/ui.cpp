#include "../lib/MfcClass/Manager.h"
#include "../lib/struct.h"
#include "../version.h"
#include "tray.h"
#include <ctime>
#include <unordered_map>
using namespace std;
Tray tray;
extern Window *windowPtr;
extern WM_AUTHENCATION_DATA authData;
extern time_t lastHotkeyPressed;
extern string localAuth;
extern boolean hideWindow;
extern unordered_map<string, string> args;
extern unordered_map<string, string> knownAppNames;
void ws_broadcast(string action, string msg);
bool verifyToken(unordered_map<string, string> &args);

void ShowContextMenu(HWND hwnd, POINT pt) {
    UINT menuItemId = 0;
    HMENU hMenu = LoadMenuW(Control::hinstance(), MAKEINTRESOURCE(209));
    if (hMenu) {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu) {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
                uFlags |= TPM_RIGHTALIGN;
            } else {
                uFlags |= TPM_LEFTALIGN;
            }
            uFlags |= TPM_RETURNCMD;
            menuItemId = TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
            if (menuItemId == 124) {
                windowPtr->close();
            }
        }
        DestroyMenu(hMenu);
    }
}

void uiOnCustomMessage(Control *control, int message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_AUTHENCATION) {
        windowPtr->show();
        windowPtr->restore();
        windowPtr->setTopMost(true);
        string origin = authData.origin;
        // check origin in knownAppNames
        if (knownAppNames.find(origin) != knownAppNames.end()) {
            origin = knownAppNames[origin];
        }
        HRESULT hr;
        wstring title = ToWString("授权管理 - 椰羊");
        wstring header = ToWString(origin + " 申请控制您的设备");
        wstring body = ToWString("该网页将可以模拟操作你的键盘和鼠标，请确保您信任该网页");
        wstring allowstr = ToWString("授权\n即允许 " + origin + " 控制您的设备");
        wstring denystr = ToWString("拒绝\n如果你不知道发生了什么");

        TASKDIALOGCONFIG tdc = {sizeof(TASKDIALOGCONFIG)};

        TASKDIALOG_BUTTON aCustomButtons[] = {
            {1000, allowstr.c_str()},
            {1001, denystr.c_str()}};

        tdc.hwndParent = windowPtr->hwnd();
        tdc.dwFlags = TDF_USE_COMMAND_LINKS;
        tdc.pButtons = aCustomButtons;
        tdc.cButtons = 2;
        tdc.pszWindowTitle = title.c_str();
        tdc.pszMainIcon = TD_INFORMATION_ICON;
        tdc.pszMainInstruction = header.c_str();
        tdc.pszContent = body.c_str();
        if (authData.requestRemember) {
            tdc.pszVerificationText = L"一周内记住授权状态（若拒绝则无效）";
        }

        int nClickedBtn;
        BOOL nVerificationChecked;
        hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &nVerificationChecked);
        if (SUCCEEDED(hr) && 1000 == nClickedBtn) {
            authData.pass = true;
        }
        if (authData.requestRemember && nVerificationChecked) {
            authData.remember = true;
        }
        windowPtr->setTopMost(false);
        if (hideWindow) {
            windowPtr->hide();
        }
    } else if (message == WM_TRAY_ICON) {
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU: {
            POINT const pt = {LOWORD(wParam), HIWORD(wParam)};
            ShowContextMenu(windowPtr->hwnd(), pt);
            break;
        }
        // click tray icon
        case NIN_KEYSELECT:
        case WM_LBUTTONUP: {
            hideWindow = !hideWindow;
            if (hideWindow) {
                windowPtr->hide();
            } else {
                windowPtr->show();
            }
            break;
        }
        }
    }
}

LRESULT CALLBACK uiKeyboardListener(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *)lParam;
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        if (pKeyBoard->vkCode == VK_LWIN || pKeyBoard->vkCode == VK_RWIN) {
            lastHotkeyPressed = std::time(0);
            ws_broadcast("hotkey", "interrput");
        }
        // alt+z
        if (pKeyBoard->vkCode == 0x5A && GetKeyState(VK_MENU) < 0) {
            ws_broadcast("hotkey", "AltZ");
        }
        // alt+x
        if (pKeyBoard->vkCode == 0x58 && GetKeyState(VK_MENU) < 0) {
            ws_broadcast("hotkey", "AltX");
        }
        // alt+c
        if (pKeyBoard->vkCode == 0x43 && GetKeyState(VK_MENU) < 0) {
            ws_broadcast("hotkey", "AltC");
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void sendNotify(std::wstring msg, std::wstring title) {
    tray.ShowBalloon(msg.c_str(), title.c_str());
}
void sendConnectNotify(std::string origin) {
    if (knownAppNames.find(origin) != knownAppNames.end()) {
        origin = knownAppNames[origin];
    }
    sendNotify(ToWString(origin + " 已连接到椰羊插件"), ToWString("该网站或程序将可以控制您的设备，如未授权，请点击托盘图标的右键菜单退出"));
}

void uiThread() {
    HINSTANCE instance = GetModuleHandle(NULL);
    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)uiKeyboardListener, instance, 0);
    Window window(ToWString("椰羊·霜华插件"), 270, 150);
    windowPtr = &window;
    window.setBackground(Brush(Hex(0xffffff)));
    window.setGlobalIcon(Icon(1));
    window.setOnCustomMessage(uiOnCustomMessage);
    string localText = "\n\n本插件不需要任何交互\n请回到网页端继续操作";
    if (localAuth != "") {
        localText = "";
    }
    TextView lbl(&window, ToWString("椰羊·霜华插件 v" + string(VERSION) + localText));
    lbl.setTextColor(Hex(0x888888));
    lbl.setBackground(Brush(Hex(0xffffff)));
    lbl.setFont(ToWString("Microsoft Yahei"));
    lbl.setTextSize(18);
    lbl.setAlign(Center);
    lbl.setSize(255, 70);
    lbl.setLocation(0, 25);
    window.removeFlag(WS_MAXIMIZEBOX);
    window.removeFlag(WS_THICKFRAME);
    if (!hideWindow) {
        window.show();
    }
    tray.Create(window.hwnd(), Icon(1), L"椰羊·霜华插件", WM_TRAY_ICON, 0);
    if (verifyToken(args)) {
        sendConnectNotify(args["register-origin"]);
    } else {
        sendNotify(ToWString("椰羊·霜华插件已启动"), ToWString("您可以随时点击托盘图标的右键菜单退出"));
    }
    window.join();
    window.destroy();
    tray.Delete();
    UnhookWindowsHookEx(hHook);
}

void endUiThread() {
    // sendmessage to close window
    SendMessageW(windowPtr->hwnd(), WM_CLOSE, 0, 0);
}