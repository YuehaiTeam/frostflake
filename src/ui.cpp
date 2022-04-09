#include "../lib/MfcClass/Manager.h"
#include "../lib/struct.h"
#include "../version.h"
#include <ctime>
using namespace std;
extern Window *windowPtr;
extern WM_AUTHENCATION_DATA authData;
extern time_t lastHotkeyPressed;
extern string localAuth;

void uiOnCustomMessage(Control *control, int message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_AUTHENCATION) {
        windowPtr->restore();
        windowPtr->setTopMost(true);
        string origin = authData.origin;
        if (origin == "null") {
            origin = "本地网页";
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

        int nClickedBtn;
        hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, NULL);
        if (SUCCEEDED(hr) && 1000 == nClickedBtn) {
            authData.pass = true;
        }
        windowPtr->setTopMost(false);
    }
}

LRESULT CALLBACK uiKeyboardListener(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *)lParam;
    if (wParam == WM_KEYDOWN) {
        if (pKeyBoard->vkCode == VK_LWIN || pKeyBoard->vkCode == VK_RWIN) {
            lastHotkeyPressed = std::time(0);
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
    lbl.setFont(ToWString("Microsoft Yahei UI"));
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
