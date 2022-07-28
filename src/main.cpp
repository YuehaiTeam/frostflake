#include "../version.h"

#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../MfcClass/Manager.h"
#include "../lib/dpiAware.h"
#include "../lib/httplib.h"
#include "../lib/sodium.h"
#include "../lib/struct.h"

#include <curl/curl.h>
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
boolean hideWindow = true;

unordered_map<string, string> args;
unordered_map<string, string> tokens;
extern unordered_map<string, string> knownAppNames;

void httpThread();
void uiThread();
void ws_server();
void ws_stopall();
void tmr_run();
void tmr_stop();
boolean isFocusAssistEnabled();
boolean autoElevate(PSTR lpCmdLine);
string readCmdFromPipe();

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

    if (autoElevate(lpCmdLine)) {
        return 0;
    }

    string strCmdLine = lpCmdLine;
    if (strCmdLine == "") {
        strCmdLine = readCmdFromPipe();
    }

    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, L"cocogoat-control-singleinstance");
    if (hMutex) {
        CloseHandle(hMutex);
        return 0;
    }
    hMutex = CreateMutex(NULL, 0, L"cocogoat-control-singleinstance");

    std::string argv = strCmdLine + string("&");
    boolean launchFromUrl = argv.find("://") != string::npos;
    for (int i = 0; i < argv.length(); i++) {
        if (argv[i] == ' ' || argv[i] == '"') {
            argv[i] = '&';
        }
    }
    // remove more than 2 spaces
    for (int i = 0; i < argv.length(); i++) {
        if (argv[i] == '&' && argv[i + 1] == '&') {
            argv.erase(i, 1);
        }
        if (argv[i] == '-' && argv[i + 1] == '-') {
            argv.erase(i, 2);
        }
    }
    // parse querystring into map
    string querystring = argv.find("?") != string::npos ? argv.substr(argv.find("?") + 1) : argv;
    string key, value;
    for (int i = 0; i < querystring.length(); i++) {
        if (querystring[i] == '=') {
            key = value;
            value = "";
        } else if (querystring[i] == '&') {
            args.insert_or_assign(key, value == "" ? "true" : value);
            key = value = "";
        } else {
            value += querystring[i];
        }
    }
    hideWindow = !isFocusAssistEnabled();
    if (!launchFromUrl) {
        localAuth = args.find("local-auth") != args.end() ? args["local-auth"] : "";
        hideWindow = args.find("show-window") != args.end() ? false : hideWindow;
        hideWindow = args.find("hide-window") != args.end() ? true : hideWindow;
    }
    if (args.find("register-token") != args.end() && args.find("register-origin") != args.end() && knownAppNames.find(args["register-origin"]) != knownAppNames.end()) {
        tokens.insert_or_assign(args["register-token"], args["register-origin"]);
    }
    curl_global_init(CURL_GLOBAL_ALL);
    sodium_init();
    thread tUi(uiThread);
    thread tHttp(ws_server);
    thread tIdleTimer(tmr_run);
    tUi.join();
    ws_stopall();
    tmr_stop();
    tHttp.join();
    tIdleTimer.join();
    curl_global_cleanup();
    ReleaseMutex(hMutex);
    cout << "exit: " << wss.stopped() << endl;
    return 0;
}