#include "../version.h"

#include <string>
#include <thread>
#include <vector>

#include "../MfcClass/Manager.h"
#include "../lib/dpiAware.h"
#include "../lib/httplib.h"
#include "../lib/struct.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;
server wss;

using namespace std;
httplib::Server svr;
Window *windowPtr = nullptr;
WM_AUTHENCATION_DATA authData;
time_t lastHotkeyPressed = 0;

GetDpiForMonitorProc GetDpiForMonitor_ = nullptr;
GetDpiForWindowProc GetDpiForWindow_ = nullptr;

SetProcessDpiAwarenessContextProc SetProcessDpiAwarenessContext_;
string localAuth = "";

void httpThread();
void uiThread();
void ws_server();
void ws_stopall();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    SetProcessDpiAwarenessContext_ = (SetProcessDpiAwarenessContextProc)GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "SetProcessDpiAwarenessContext");
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        GetDpiForMonitor_ = (GetDpiForMonitorProc)GetProcAddress(hShcore, "GetDpiForMonitor");
        GetDpiForWindow_ = (GetDpiForWindowProc)GetProcAddress(hShcore, "GetDpiForWindow");
    }
    if (NULL != SetProcessDpiAwarenessContext_) {
        SetProcessDpiAwarenessContext_(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    std::string argv = string(lpCmdLine) + string(" ");
    // argv includes '--local-auth'
    if (argv.find("--local-auth=") != std::string::npos) {
        localAuth = argv.substr(argv.find("=") + 1, argv.find(" ") - argv.find("=") - 1);
    }

    thread tUi(uiThread);
    thread tHttp(ws_server);
    tUi.join();
    ws_stopall();
    tHttp.join();
    cout << "exit: " << wss.stopped() << endl;
    return 0;
}