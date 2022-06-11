#include "../lib/struct.h"
#include "../version.h"

#include "versionhelpers.h"

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
extern std::string localAuth;
extern unordered_map<string, string> tokens;

typedef struct IupdateInfo {
    string url;
    string name;
    string md5;
    string status;
    long downloaded;
    long total;
} IupdateInfo;

void runYasInNewThread(std::string &argv);
std::string getYasJsonContent();
json11::Json cvAutotrackCommand(json11::Json cmd);
void sendConnectNotify(std::string origin);
void checkAndDownloadInNewThread(std::wstring infourl, IupdateInfo *info);

unordered_map<string, IupdateInfo *> updateMap;

void textResponse(httplib::Response &res, std::string obj, std::string content_type, int code = 200) {
    res.status = code;
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Max-Age", "600");
    res.set_header("Access-Control-Allow-Private-Network", "true");
    res.set_header("Server", string("cocogoat-control/") + string(VERSION));
    // make it const
    res.set_content(obj, content_type.c_str());
}

void jsonResponse(httplib::Response &res, json11::Json &obj, int code = 200) {
    textResponse(res, obj.dump(), "application/json", code);
}

void emptyResponse(httplib::Response &res, int status = 204) {
    res.status = status;
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
        if (token == localAuth) {
            return true;
        }
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
    if (GetDpiForMonitor_ != nullptr) {
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
VERSIONHELPERAPI _IsWindows11OrGreater() {
    OSVERSIONINFOEXW osvi = {sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0};
    DWORDLONG const dwlConditionMask = VerSetConditionMask(
        VerSetConditionMask(
            VerSetConditionMask(
                0, VER_MAJORVERSION, VER_GREATER_EQUAL),
            VER_MINORVERSION, VER_GREATER_EQUAL),
        VER_BUILDNUMBER, VER_GREATER_EQUAL);

    osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN10);
    osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN10);
    osvi.dwBuildNumber = 22000;

    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, dwlConditionMask) != FALSE;
}
int winver() {
    // return 7, 8, 10 or 11
    int ver = 0;
    if (_IsWindows11OrGreater()) {
        ver = 11;
    } else if (IsWindows10OrGreater()) {
        ver = 10;
    } else if (IsWindows8OrGreater()) {
        ver = 8;
    } else if (IsWindows7OrGreater()) {
        ver = 7;
    }
    return ver;
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
        authData.origin = origin;
        authData.pass = false;
        authData.remember = false;
        authData.requestRemember = req.get_param_value_count("remember") > 0;
        UUID uuid;
        UuidCreate(&uuid);
        RPC_CSTR szUuid = NULL;
        UuidToStringA(&uuid, &szUuid);
        string utoken = std::string((char *)szUuid);
        RpcStringFreeA(&szUuid);
        if (req.headers.find("Authorization") != req.headers.end()) {
            string auth = req.headers.find("Authorization")->second;
            string token = auth.substr(7);
            // get windows version
            if (auth.find("Bearer ") == 0) {
                vector<string> tokenvec;
                if (token.find(" ") != string::npos) {
                    tokenvec.push_back(token.substr(0, token.find(" ")));
                    tokenvec.push_back(token.substr(token.find(" ") + 1));
                } else {
                    tokenvec.push_back(token);
                }
                for (auto &t : tokenvec) {
                    if (t.find("-") == string::npos) {
                        string dec = deviceDecrypt(t);
                        if (dec != "") {
                            string err = "";
                            json11::Json parsed = json11::Json::parse(dec, err);
                            if (err == "") {
                                if (parsed["_"].string_value() == "remember" && parsed["o"].string_value() == origin) {
                                    long now = (long)time(NULL);
                                    int created = parsed["t"].int_value();
                                    // expire in 7 days
                                    if (now - created < 60 * 60 * 24 * 7) {
                                        authData.pass = true;
                                        tokens[t] = utoken;
                                        sendConnectNotify(origin);
                                    }
                                }
                            }
                        }
                    } else if (tokens.find(t) != tokens.end() && tokens[t] == origin) {
                        json11::Json result = json11::Json::object{
                            {"token", t},
                            {"origin", origin},
                            {"winver", winver()},
                            {"hwnd", (long)activeWindow}};
                        sendConnectNotify(origin);
                        jsonResponse(res, result, 201);
                        return;
                    }
                }
            }
        }
        if (!authData.pass) {
            SetForegroundWindow(windowPtr->hwnd());
            SendMessageW(windowPtr->hwnd(), WM_AUTHENCATION, 1, 0);
        }
        if (!authData.pass) {
            json11::Json result = json11::Json::object{
                {"msg", "Request denied by user"}};
            jsonResponse(res, result, 401);
            return;
        }
        tokens[utoken] = origin;
        string remember = "";
        if (authData.remember) {
            // int timestamp
            long timestamp = (long)time(NULL);
            json11::Json remb = json11::Json::object{
                {"_", "remember"},
                {"o", origin},
                {"t", timestamp}};
            remember = deviceEncrypt(remb.dump());
        }
        json11::Json result = json11::Json::object{
            {"token", utoken},
            {"remember", remember},
            {"origin", origin},
            {"winver", winver()},
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

        std::string action = json["action"].string_value();
        if (action == "update") {
            string infoUrl = string("https://cocogoat-1251105598.file.myqcloud.com/77/upgrade/cvautotrack.json");
            IupdateInfo *info = NULL;
            // check if exists
            auto it = updateMap.find("cvautotrack");
            if (it == updateMap.end()) {
                info = new IupdateInfo();
            } else {
                info = it->second;
            }
            info->md5 = "";
            info->url = "";
            info->name = "";
            info->status = "waiting";
            info->downloaded = 0;
            info->total = 0;
            checkAndDownloadInNewThread(ToWString(infoUrl), info);
            updateMap.insert_or_assign("cvautotrack", info);
            json11::Json result = json11::Json::object{
                {"msg", "Created"}};
            jsonResponse(res, result, 201);
            return;
        }
        if (action == "progress") {
            auto it = updateMap.find("cvautotrack");
            if (it == updateMap.end()) {
                json11::Json result = json11::Json::object{
                    {"msg", "Job Not Found"}};
                jsonResponse(res, result, 404);
                return;
            }
            json11::Json result = json11::Json::object{
                {"msg", it->second->status},
                {"data", json11::Json::array{(long)it->second->downloaded, (long)it->second->total}}};
            jsonResponse(res, result, 200);
            return;
        }
        json11::Json resjson = cvAutotrackCommand(json);
        jsonResponse(res, resjson, 200);
    });
    svr.Post("/api/yas", [](const httplib::Request &req, httplib::Response &res) {
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
        // argv string from json
        string argv = json["argv"].string_value();
        // run yas
        runYasInNewThread(argv);
        emptyResponse(res, 201);
    });
    svr.Get("/api/yas", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string content = getYasJsonContent();
        textResponse(res, content, "application/json", 200);
    });
    svr.Post("/api/upgrade/(.*?)", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string key = string(req.matches[1]);
        // key should be only 0-9a-zA-Z
        if (key.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") != string::npos) {
            json11::Json result = json11::Json::object{
                {"msg", "Bad Request"}};
            jsonResponse(res, result, 400);
            return;
        }
        string infoUrl = string("https://cocogoat-1251105598.file.myqcloud.com/77/upgrade/") + key + string(".json");
        IupdateInfo *info = NULL;
        // check if exists
        auto it = updateMap.find(key);
        if (it == updateMap.end()) {
            info = new IupdateInfo();
        } else {
            info = it->second;
        }
        info->md5 = "";
        info->url = "";
        info->name = "";
        info->status = "waiting";
        info->downloaded = 0;
        info->total = 0;
        checkAndDownloadInNewThread(ToWString(infoUrl), info);
        updateMap.insert_or_assign(key, info);
        emptyResponse(res, 201);
    });
    svr.Get("/api/upgrade/(.*?)", [](const httplib::Request &req, httplib::Response &res) {
        if (!apiPreChck(req, res))
            return;
        string key = string(req.matches[1]);
        // get from info
        auto it = updateMap.find(key);
        if (it == updateMap.end()) {
            json11::Json result = json11::Json::object{
                {"msg", "Job Not Found"}};
            jsonResponse(res, result, 404);
            return;
        }
        json11::Json result = json11::Json::object{
            {"msg", it->second->status},
            {"downloaded", (long)it->second->downloaded},
            {"total", (long)it->second->total},
            {"md5", it->second->md5},
            {"url", it->second->url},
            {"name", it->second->name}};
        jsonResponse(res, result, 200);
    });
}