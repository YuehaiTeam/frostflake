#include "../lib/MfcClass/Manager.h"
#include "../lib/struct.h"
#include "../version.h"
#include <ctime>
#include <unordered_map>
using namespace std;
extern Window *windowPtr;
extern WM_AUTHENCATION_DATA authData;
extern time_t lastHotkeyPressed;
extern string localAuth;
extern unordered_map<string, string> knownAppNames;
void ws_broadcast(string action, string msg);
void uiOnCustomMessage(Control *control, int message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_AUTHENCATION) {
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
        tdc.pszVerificationText = L"一周内记住授权状态（若拒绝则无效）";

        int nClickedBtn;
        BOOL nVerificationChecked;
        hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &nVerificationChecked);
        if (SUCCEEDED(hr) && 1000 == nClickedBtn) {
            authData.pass = true;
        }
        if (nVerificationChecked) {
            authData.remember = true;
        }
        windowPtr->setTopMost(false);
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

void uiThread() {
    HINSTANCE instance = GetModuleHandle(NULL);
    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)uiKeyboardListener, instance, 0);
    Window window(ToWString("椰羊·辅助插件"), 270, 150);
    windowPtr = &window;
    window.setBackground(Brush(Hex(0xffffff)));
    window.setGlobalIcon(Icon(1));
    window.setOnCustomMessage(uiOnCustomMessage);
    string localText = "\n\n本插件不需要任何交互\n请回到网页端继续操作";
    if (localAuth != "") {
        localText = "";
    }
    TextView lbl(&window, ToWString("椰羊·辅助插件 v" + string(VERSION) + localText));
    lbl.setTextColor(Hex(0x888888));
    lbl.setBackground(Brush(Hex(0xffffff)));
    lbl.setFont(ToWString("Microsoft Yahei"));
    lbl.setTextSize(18);
    lbl.setAlign(Center);
    lbl.setSize(255, 70);
    lbl.setLocation(0, 25);
    window.removeFlag(WS_MAXIMIZEBOX);
    window.removeFlag(WS_THICKFRAME);
    window.show();
    window.join();
    window.destroy();
    UnhookWindowsHookEx(hHook);
}
