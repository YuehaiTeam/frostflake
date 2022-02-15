#include "../version.h"

#include <string>
#include <thread>
#include <vector>

#include "../MfcClass/Manager.h"
#include "../lib/dpiAware.h"
#include "../lib/httplib.h"
#include "../lib/struct.h"

using namespace std;
httplib::Server svr;
Window *windowPtr = nullptr;
WM_AUTHENCATION_DATA authData;

GetDpiForMonitorProc GetDpiForMonitor_;
SetProcessDpiAwarenessContextProc SetProcessDpiAwarenessContext_;

void httpThread();
void uiThread();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    SetProcessDpiAwarenessContext_ = (SetProcessDpiAwarenessContextProc)GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "SetProcessDpiAwarenessContext");
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        GetDpiForMonitor_ = (GetDpiForMonitorProc)GetProcAddress(hShcore, "GetDpiForMonitor");
    }
    if (NULL != SetProcessDpiAwarenessContext_) {
        SetProcessDpiAwarenessContext_(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    thread tUi(uiThread);
    thread tHttp(httpThread);
    tHttp.detach();
    tUi.join();
    svr.stop();
    cout << "exit" << endl;
    return 0;
}