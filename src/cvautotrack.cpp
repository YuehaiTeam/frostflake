#include "cvautotrack.h"
#include "../lib/json11.hpp"
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <windows.h>
HMODULE hModule = NULL;
void ws_broadcast(std::string msg);
void downloadInNewThread(std::wstring infourl, std::wstring path, unsigned long &downloaded, unsigned long &total, std::string &status);
unsigned long downloaded = 0;
unsigned long total = 0;
bool isBroadcast = false;
std::string updatestatus = "";
std::thread cvThread;
json11::Json cvAutotrackCommand(json11::Json cmd) {
    std::string action = cmd["action"].string_value();
    if (action == "version") {
        return json11::Json::object{
            {"msg", getDLLUpdateTime()}};
    }
    if (action == "update") {
        return json11::Json::object{
            {"msg", updateDLL()}};
    }
    if (action == "progress") {
        return json11::Json::object{
            {"msg", updatestatus},
            {"data", json11::Json::array{(long)downloaded, (long)total}}};
    }
    if (action == "sub") {
        return json11::Json::object{
            {"status", startTrack()}};
    }
    if (action == "unsub") {
        return json11::Json::object{
            {"status", stopTrack()}};
    }
    if (action == "track") {
        double x, y, a, b;
        bool status = track(x, y, a, b);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y, a, b}}};
    }
    if (action == "load") {
        return json11::Json::object{
            {"msg", load()}};
    }
    if (action == "init") {
        return json11::Json::object{
            {"status", init()}};
    }
    if (action == "uninit") {
        return json11::Json::object{
            {"status", uninit()}};
    }
    if (action == "SetHandle") {
        long long int handle = cmd["data"].array_items()[0].number_value();
        return json11::Json::object{
            {"status", SetHandle(handle)}};
    }
    if (action == "SetWorldCenter") {
        double x = cmd["data"].array_items()[0].number_value();
        double y = cmd["data"].array_items()[1].number_value();
        return json11::Json::object{
            {"status", SetWorldCenter(x, y)}};
    }
    if (action == "SetWorldScale") {
        double scale = cmd["data"].array_items()[0].number_value();
        return json11::Json::object{
            {"status", SetWorldScale(scale)}};
    }
    if (action == "GetTransform") {
        float x, y, a;
        bool status = GetTransform(x, y, a);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y, a}}};
    }
    if (action == "GetPosition") {
        double x, y;
        bool status = GetPosition(x, y);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y}}};
    }
    if (action == "GetDirection") {
        double a;
        bool status = GetDirection(a);
        return json11::Json::object{
            {"status", status},
            {"data", a}};
    }
    if (action == "GetRotation") {
        double a;
        bool status = GetRotation(a);
        return json11::Json::object{
            {"status", status},
            {"data", a}};
    }
    if (action == "GetLastErr") {
        return json11::Json::object{
            {"data", GetLastErr()}};
    }
    return json11::Json::object{
        {"status", false},
        {"msg", "unknown action"}};
}
std::string updateDLL() {
    std::wstring updateUrl = L"https://cocogoat-1251105598.file.myqcloud.com/77/cvautotrack.txt";
    WCHAR szPath[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) < 0) {
        return "faild: getfolderpath";
    }
    PathAppendW(szPath, L"\\cocogoat-control\\");
    CreateDirectoryW(szPath, NULL);
    PathAppendW(szPath, L"cvautotrack.dll");
    downloadInNewThread(updateUrl, szPath, downloaded, total, updatestatus);
    return "started";
}
std::string getDLLUpdateTime() {
    WCHAR szPath[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) < 0) {
        return "faild";
    }
    PathAppendW(szPath, L"\\cocogoat-control\\");
    CreateDirectoryW(szPath, NULL);
    PathAppendW(szPath, L"cvautotrack.dll");
    HANDLE hFile = CreateFileW(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "nofile";
    }
    FILETIME ft;
    GetFileTime(hFile, NULL, NULL, &ft);
    CloseHandle(hFile);
    // iso time gmt
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    char buf[100];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}
std::string load() {
    if (cvInit) {
        return "loaded";
    }
    WCHAR szPath[MAX_PATH];
    // Get path for each computer, non-user specific and non-roaming data.
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) >= 0) {
        hModule = LoadLibraryExW(L"cvautotrack.dll", NULL, NULL);
        long errcode1 = GetLastError();
        if (!hModule) {
            // Append product-specific path
            PathAppendW(szPath, L"\\cocogoat-control\\");
            CreateDirectoryW(szPath, NULL);
            PathAppendW(szPath, L"cvautotrack.dll");
            if (INVALID_FILE_ATTRIBUTES == GetFileAttributesW(szPath)) {
                return "nofile";
            }
            hModule = LoadLibraryExW(szPath, NULL, NULL);
            if (!hModule) {
                long errcode = GetLastError();
                return "faild: loadlibrary " + std::to_string(errcode) + "/" + std::to_string(errcode1);
            }
        }
        cvInit = (cvInitProc)GetProcAddress(hModule, "init");
        cvUnInit = (cvUnInitProc)GetProcAddress(hModule, "uninit");
        cvSetHandle = (cvSetHandleProc)GetProcAddress(hModule, "SetHandle");
        cvSetWorldCenter = (cvSetWorldCenterProc)GetProcAddress(hModule, "SetWorldCenter");
        cvSetWorldScale = (cvSetWorldScaleProc)GetProcAddress(hModule, "SetWorldScale");
        cvGetTransform = (cvGetTransformProc)GetProcAddress(hModule, "GetTransform");
        cvGetPosition = (cvGetPositionProc)GetProcAddress(hModule, "GetPosition");
        cvGetDirection = (cvGetDirectionProc)GetProcAddress(hModule, "GetDirection");
        cvGetRotation = (cvGetRotationProc)GetProcAddress(hModule, "GetRotation");
        cvGetLastErr = (cvGetLastErrProc)GetProcAddress(hModule, "GetLastErr");
        if (!cvInit || !cvUnInit || !cvSetHandle || !cvSetWorldCenter || !cvSetWorldScale || !cvGetTransform || !cvGetPosition || !cvGetDirection || !cvGetRotation || !cvGetLastErr) {
            long errcode = GetLastError();
            return "faild: getprocaddr " + std::to_string(errcode);
        }
        return "loaded";
    } else {
        return "faild: getfolderpath";
    }
}
bool init() {
    if (cvInit) {
        return cvInit();
    }
    return false;
}
bool uninit() {
    if (cvUnInit) {
        return cvUnInit();
    }
    return false;
}
bool SetHandle(long long int handle) {
    if (cvSetHandle) {
        return cvSetHandle(handle);
    }
    return false;
}
bool SetWorldCenter(double x, double y) {
    if (cvSetWorldCenter) {
        return cvSetWorldCenter(x, y);
    }
    return false;
}
bool SetWorldScale(double scale) {
    if (cvSetWorldScale) {
        return cvSetWorldScale(scale);
    }
    return false;
}
void threadedTrack() {
    while (isBroadcast) {
        double x, y, a, b;
        bool status = track(x, y, a, b);
        int s = status ? 1 : 0;
        json11::Json obj = json11::Json::object{
            {"action", "cvautotrack"},
            {"data", json11::Json::array{x, y, a, b, s}}};
        std::string json = obj.dump();
        ws_broadcast(json);
        Sleep(100);
    }
}
bool startTrack() {
    if (isBroadcast)
        return true;
    if (cvInit) {
        isBroadcast = true;
        cvThread = std::thread(threadedTrack);
        return true;
    }
    return false;
}
bool stopTrack() {
    if (!isBroadcast)
        return true;
    if (cvUnInit) {
        isBroadcast = false;
        TerminateThread(cvThread.native_handle(), 0);
        return true;
    }
    return false;
}
bool track(double &x, double &y, double &a, double &b) {
    bool rtn = true;
    rtn = rtn && GetPosition(x, y);
    rtn = rtn && GetRotation(a);
    rtn = rtn && GetDirection(b);
    return rtn;
}
bool GetTransform(float &x, float &y, float &a) {
    if (cvGetTransform) {
        return cvGetTransform(x, y, a);
    }
    return false;
}
bool GetPosition(double &x, double &y) {
    if (cvGetPosition) {
        return cvGetPosition(x, y);
    }
    return false;
}
bool GetDirection(double &a) {
    if (cvGetDirection) {
        return cvGetDirection(a);
    }
    return false;
}
bool GetRotation(double &a) {
    if (cvGetRotation) {
        return cvGetRotation(a);
    }
    return false;
}
int GetLastErr() {
    if (cvGetLastErr) {
        return cvGetLastErr();
    }
    return -999;
}