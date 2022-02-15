#include "../MfcClass/Manager.h"
#include "../lib/struct.h"
#include "../version.h"
using namespace std;
extern Window *windowPtr;
extern WM_AUTHENCATION_DATA authData;
void uiOnCustomMessage(Control *control, int message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_AUTHENCATION) {
        windowPtr->restore();
        windowPtr->setTopMost(true);
        HRESULT hr;
        wstring title = ToWString("授权管理 - 椰羊");
        wstring header = ToWString(string(authData.origin) + " 申请控制您的设备");
        wstring body = ToWString("该网页将可以模拟操作你的键盘和鼠标，请确保您信任该网页");
        wstring allowstr = ToWString("授权\n即允许 " + authData.origin + " 控制您的设备");
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

void uiThread() {
    Window window(ToWString("椰羊·自动操作"), 300, 150);
    windowPtr = &window;
    window.setBackground(Brush(Hex(0xffffff)));
    window.setGlobalIcon(Icon(1));
    window.setOnCustomMessage(uiOnCustomMessage);
    TextView lbl(&window, ToWString("椰羊·自动操作 v" + string(VERSION)));
    lbl.setTextColor(Hex(0x2b3399));
    lbl.setBackground(Brush(Hex(0xffffff)));
    lbl.setFont(ToWString("Microsoft Yahei UI"));
    lbl.setTextSize(20);
    lbl.setAlign(Center);
    lbl.setSize(285, 50);
    lbl.setLocation(0, 35);
    window.removeFlag(WS_MAXIMIZEBOX);
    window.removeFlag(WS_THICKFRAME);
    window.show();
    window.join();
    window.destroy();
}