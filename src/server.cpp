#include "../lib/struct.h"
#include "../version.h"

#include <unordered_map>

#include "../lib/dpiAware.h"
#include "../lib/httplib.h"
#include "../lib/json11.hpp"

using namespace std;

extern Window *windowPtr;
extern httplib::Server svr;
extern WM_AUTHENCATION_DATA authData;
extern GetDpiForMonitorProc GetDpiForMonitor_;
extern time_t lastHotkeyPressed;

json11::Json cvAutotrackCommand(json11::Json cmd);

unordered_map<string, string> tokens;

void jsonResponse(httplib::Response &res, json11::Json &obj, int code = 200) {
    res.status = code;
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Max-Age", "600");
    res.set_header("Server", string("cocogoat-control/") + string(VERSION));
    res.set_header("Content-Type", "application/json");
    res.set_content(obj.dump(), "application/json");
}

void emptyResponse(httplib::Response &res) {
    res.status = 204;
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Max-Age", "600");
    res.set_header("Server", string("cocogoat-control/") + string(VERSION));
}

boolean apiPreChck(const httplib::Request &req, httplib::Response &res) {
    if (req.remote_addr != "WS") {
        if (req.headers.find("Origin") == req.headers.end()) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Origin"}};
            jsonResponse(res, result, 400);
            return false;
        }
        auto auth = req.headers.find("Authorization");
        if (auth == req.headers.end()) {
            json11::Json result = json11::Json::object{
                {"msg", "Unauthorized"}};
            jsonResponse(res, result, 401);
            return false;
        }
        string token = auth->second.substr(7);
        if (
            auth->second.find("Bearer ") != 0 ||
            tokens.find(token) == tokens.end() ||
            tokens[token] != req.headers.find("Origin")->second) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Token"}};
            jsonResponse(res, result, 401);
            return false;
        }
    }
    if (std::time(0) - lastHotkeyPressed < 5) {
        // Stop because hotkey pressed in the last 5 seconds
        json11::Json result = json11::Json::object{
            {"msg", "Stopped by user"},
            {"time", (long)lastHotkeyPressed}};
        jsonResponse(res, result, 410);
        return false;
    }
    return true;
}

BOOL IsAltTabWindow(HWND hwnd) {
    HWND hwndTry, hwndWalk = NULL;

    if (!IsWindowVisible(hwnd))
        return FALSE;

    hwndTry = GetAncestor(hwnd, GA_ROOTOWNER);
    while (hwndTry != hwndWalk) {
        hwndWalk = hwndTry;
        hwndTry = GetLastActivePopup(hwndWalk);
        if (IsWindowVisible(hwndTry))
            break;
    }
    if (hwndWalk != hwnd)
        return FALSE;

    return TRUE;
}

static BOOL CALLBACK enumWindowsProc(HWND hWnd, LPARAM lparam) {
    vector<WindowData> *windows = (vector<WindowData> *)lparam;
    if (IsAltTabWindow(hWnd)) {
        WindowData data;
        data.hWnd = hWnd;
        wchar_t title[1024];
        GetWindowTextW(hWnd, title, 1024);
        data.title = ToString(title);
        GetClassName(hWnd, title, 1024);
        data.classname = ToString(title);
        RECT rect;
        GetWindowRect(hWnd, &rect);
        data.x = rect.left;
        data.y = rect.top;
        data.width = rect.right - rect.left;
        data.height = rect.bottom - rect.top;
        if (data.width < 10 || data.height < 10 || data.title == "") {
            return TRUE;
        }
        windows->push_back(data);
    }
    return TRUE;
}

static BOOL CALLBACK enumMonitorProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    vector<MonitorData> *monitors = (vector<MonitorData> *)dwData;
    MONITORINFOEXW info;
    info.cbSize = sizeof(MONITORINFOEXW);
    GetMonitorInfoW(hMonitor, &info);
    MonitorData data;
    data.id = (long)hMonitor;
    data.x = info.rcMonitor.left;
    data.y = info.rcMonitor.top;
    data.width = info.rcMonitor.right - info.rcMonitor.left;
    data.height = info.rcMonitor.bottom - info.rcMonitor.top;
    data.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    unsigned int dpi = 0;
    if (GetDpiForMonitor_) {
        GetDpiForMonitor_(hMonitor, MDT_EFFECTIVE_DPI, &dpi, &dpi);
    }
    data.dpi = dpi;
    monitors->push_back(data);
    return TRUE;
}

void activeWindow(HWND hWnd) {
    if (hWnd == NULL) {
        return;
    }
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void httpThread() {
    svr.set_keep_alive_timeout(300);
    svr.set_keep_alive_max_count(1000);
    svr.Options("/(.*?)", [](const httplib::Request &req, httplib::Response &res) {
        json11::Json obj = json11::Json::object{};
        jsonResponse(res, obj, 200);
    });

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        json11::Json result = json11::Json::object{
            {"service", "cocogoat-control"},
            {"version", VERSION},
        };
        jsonResponse(res, result);
    });

    svr.Post("/token", [](const httplib::Request &req, httplib::Response &res) {
        if (req.headers.find("Origin") == req.headers.end()) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Origin"}};
            jsonResponse(res, result, 400);
            return;
        }
        HWND activeWindow = GetForegroundWindow();
        string origin = req.headers.find("Origin")->second;
        if (req.headers.find("Authorization") != req.headers.end()) {
            string auth = req.headers.find("Authorization")->second;
            string token = auth.substr(7);
            if (auth.find("Bearer ") == 0 && tokens.find(token) != tokens.end() && tokens[token] == origin) {
                json11::Json result = json11::Json::object{
                    {"token", token},
                    {"origin", origin},
                    {"hwnd", (long)activeWindow}};
                jsonResponse(res, result, 201);
                return;
            }
        }
        authData.origin = origin;
        authData.pass = false;
        SetForegroundWindow(windowPtr->hwnd());
        SendMessageW(windowPtr->hwnd(), WM_AUTHENCATION, 1, 0);
        if (!authData.pass) {
            json11::Json result = json11::Json::object{
                {"msg", "Request denied by user"}};
            jsonResponse(res, result, 401);
            return;
        }
        UUID uuid;
        UuidCreate(&uuid);
        RPC_CSTR szUuid = NULL;
        UuidToStringA(&uuid, &szUuid);
        string token = std::string((char *)szUuid);
        RpcStringFreeA(&szUuid);
        tokens[token] = origin;
        json11::Json result = json11::Json::object{
            {"token", token},
            {"origin", origin},
            {"hwnd", (long)activeWindow}};
        jsonResponse(res, result, 201);
    });

    svr.Delete("/token", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string token = req.headers.find("Authorization")->second.substr(7);
        tokens.erase(token);
        emptyResponse(res);
    });
    svr.Get("/api/windows", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        vector<WindowData> windows;
        EnumWindows(enumWindowsProc, (LPARAM)&windows);
        json11::Json result = json11::Json(windows);
        jsonResponse(res, result);
    });
    svr.Get("/api/windows/(.*?)", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string id = req.matches[1];
        HWND hWnd = (HWND)stol(id);
        WindowData data;
        data.hWnd = hWnd;
        wchar_t title[1024];
        GetWindowTextW(hWnd, title, 1024);
        data.title = ToString(title);
        GetClassName(hWnd, title, 1024);
        data.classname = ToString(title);
        RECT rect;
        GetWindowRect(hWnd, &rect);
        data.x = rect.left;
        data.y = rect.top;
        data.width = rect.right - rect.left;
        data.height = rect.bottom - rect.top;
        json11::Json result = json11::Json(data);
        jsonResponse(res, result);
    });
    svr.Patch("/api/windows/(.*?)", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string id = req.matches[1];
        HWND hWnd = (HWND)stol(id);
        activeWindow(hWnd);
        emptyResponse(res);
    });
    svr.Get("/api/monitors", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        int vdx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vdy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vdw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vdh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        vector<MonitorData> monitors;
        EnumDisplayMonitors(NULL, NULL, enumMonitorProc, (LPARAM)&monitors);

        json11::Json array = json11::Json(monitors);
        json11::Json result = json11::Json::object{
            {"virtual", json11::Json::object{
                            {"x", vdx},
                            {"y", vdy},
                            {"width", vdw},
                            {"height", vdh},
                        }},
            {"monitors", array}};
        jsonResponse(res, result);
    });
    svr.Post("/api/SendMessage", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        if (!req.has_param("hWnd") || !req.has_param("Msg") || !req.has_param("wParam") || !req.has_param("lParam")) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        unsigned int msg = stoi(req.get_param_value("Msg"));
        long lParam = stol(req.get_param_value("lParam"));
        long wParam = stol(req.get_param_value("wParam"));
        HWND hWnd = (HWND)stol(req.get_param_value("hWnd"));
        if (hWnd == 0) {
            hWnd = GetActiveWindow();
        }
        SendMessage(hWnd, msg, wParam, lParam);
        emptyResponse(res);
    });
    svr.Post("/api/mouse_event", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        if (!req.has_param("dwFlags") || !req.has_param("dx") || !req.has_param("dy") || !req.has_param("dwData")) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        int i = 1;
        if (req.has_param("repeat")) {
            i = stoi(req.get_param_value("repeat"));
        }
        DWORD dwFlags = stol(req.get_param_value("dwFlags"));
        long dx = stol(req.get_param_value("dx"));
        long dy = stol(req.get_param_value("dy"));
        DWORD dwData = stol(req.get_param_value("dwData"));
        for (int j = 0; j < i; j++) {
            mouse_event(dwFlags, dx, dy, dwData, 0);
        }
        emptyResponse(res);
    });
    svr.Post("/api/keybd_event", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        if (!req.has_param("bVk") || !req.has_param("bScan") || !req.has_param("dwFlags")) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        BYTE bVk = (BYTE)stoi(req.get_param_value("bVk"));
        BYTE bScan = (BYTE)stoi(req.get_param_value("bScan"));
        DWORD dwFlags = stol(req.get_param_value("dwFlags"));
        DWORD dwExtraInfo = stol(req.get_param_value("dwExtraInfo"));
        keybd_event(bVk, bScan, dwFlags, 0);
        emptyResponse(res);
    });
    svr.Post("/api/SetCursorPos", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        if (!req.has_param("x") || !req.has_param("y")) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        long x = stol(req.get_param_value("x"));
        long y = stol(req.get_param_value("y"));
        SetCursorPos(x, y);
        emptyResponse(res);
    });
    svr.Post("/api/cvautotrack", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string body = req.body;
        string err = "";
        json11::Json json = json11::Json::parse(body, err);
        if (err != "") {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        json11::Json resjson = cvAutotrackCommand(json);
        jsonResponse(res, resjson, 200);
    });
}