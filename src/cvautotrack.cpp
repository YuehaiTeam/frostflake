#include "cvautotrack.h"
#include "../lib/json11.hpp"
#include "../lib/sodium.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <windows.h>
void ws_broadcast(std::string msg);
bool isBroadcast = false;
std::thread cvThread;
json11::Json cvatErrorToJson() {
    char err[1024];
    cvat_GetLastErrJson(err, 1024);
    std::string errStr = err;
    std::string jerr = "";
    return json11::Json::parse(errStr, jerr);
}
json11::Json cvAutotrackCommand(json11::Json cmd) {
    std::string action = cmd["action"].string_value();
    int version = cmd["version"].is_number() ? cmd["version"].int_value() : 0;
    if (version < 7) {
        return json11::Json::object{
            {"msg", "当前插件版本高于JS版本，请更新"}};
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
        double x, y, a, r;
        int m;
        bool status = track(x, y, a, r, m);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y, a, r, m}}};
    }
    if (action == "load") {
        return json11::Json::object{
            {"msg", cvat_load()}};
    }
    if (action == "init") {
        return json11::Json::object{
            {"status", cvat_init()}};
    }
    if (action == "uninit") {
        return json11::Json::object{
            {"status", cvat_uninit()}};
    }
    if (action == "SetHandle") {
        long long int handle = cmd["data"].array_items()[0].number_value();
        return json11::Json::object{
            {"status", cvat_SetHandle(handle)}};
    }
    if (action == "SetUseDx11CaptureMode") {
        return json11::Json::object{
            {"status", cvat_SetUseDx11CaptureMode()}};
    }
    if (action == "SetUseBitbltCaptureMode") {
        return json11::Json::object{
            {"status", cvat_SetUseBitbltCaptureMode()}};
    }
    if (action == "SetWorldCenter") {
        double x = cmd["data"].array_items()[0].number_value();
        double y = cmd["data"].array_items()[1].number_value();
        return json11::Json::object{
            {"status", cvat_SetWorldCenter(x, y)}};
    }
    if (action == "SetWorldScale") {
        double scale = cmd["data"].array_items()[0].number_value();
        return json11::Json::object{
            {"status", cvat_SetWorldScale(scale)}};
    }
    if (action == "GetTransform") {
        double x, y, a;
        int m;
        bool status = cvat_GetTransformOfMap(x, y, a, m);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y, a, m}}};
    }
    if (action == "GetPosition") {
        double x, y;
        int m;
        bool status = cvat_GetPositionOfMap(x, y, m);
        return json11::Json::object{
            {"status", status},
            {"data", json11::Json::array{x, y, m}}};
    }
    if (action == "GetDirection") {
        double a;
        bool status = cvat_GetDirection(a);
        return json11::Json::object{
            {"status", status},
            {"data", a}};
    }
    if (action == "GetRotation") {
        double a;
        bool status = cvat_GetRotation(a);
        return json11::Json::object{
            {"status", status},
            {"data", a}};
    }
    if (action == "GetLastErr") {
        return json11::Json::object{
            {"data", cvat_GetLastErr()}};
    }
    if (action == "DebugCaptureRes") {
        // get temp file path
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        // check if temp path is ascii
        bool isAscii = true;
        for (int i = 0; i < MAX_PATH; i++) {
            if (tempPath[i] == 0) {
                break;
            }
            if (tempPath[i] > 127) {
                isAscii = false;
                break;
            }
        }
        if (!isAscii) {
            return json11::Json::object{
                {"msg", "当前系统临时文件路径包含中文或特殊字符，无法保存截图"}};
        }
        // get temp file name
        wchar_t tempFileName[MAX_PATH];
        GetTempFileNameW(tempPath, L"cocogoat-cvat-", 0, tempFileName);
        // add suffix .jpg
        std::wstring tempFileNameW = tempFileName + std::wstring(L".jpg");
        std::string tempfn = ToString(tempFileNameW);
        // do debug capture
        cvat_DebugCapturePath(tempfn.c_str(), tempfn.length());
        // check file exists
        if (fileExists(tempFileNameW)) {
            // open file handle
            HANDLE hFile = CreateFileW(tempFileNameW.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile) {
                std::string buffer = "";
                DWORD dwFileSize = GetFileSize(hFile, NULL);
                if (dwFileSize > 0) {
                    buffer.resize(dwFileSize);
                    DWORD dwRead = 0;
                    ReadFile(hFile, &buffer[0], dwFileSize, &dwRead, NULL);
                }
                CloseHandle(hFile);
                // delete file
                DeleteFileW(tempFileNameW.c_str());
                // return base64
                size_t dstlen = sodium_base64_encoded_len(buffer.length(), sodium_base64_VARIANT_ORIGINAL);
                std::string base64;
                base64.resize(dstlen);
                sodium_bin2base64(&base64[0], dstlen, (const unsigned char *)buffer.c_str(), buffer.length(), sodium_base64_VARIANT_ORIGINAL);
                base64 = "data:image/jpeg;base64," + base64;
                return json11::Json::object{
                    {"data", base64}};
            }
        }
        return json11::Json::object{
            {"error", cvatErrorToJson()}};
    }
    return json11::Json::object{
        {"status", false},
        {"msg", "unknown action"}};
}
void threadedTrack() {
    while (isBroadcast) {
        double x, y, a, r;
        int m;
        bool status = track(x, y, a, r, m);
        int err = cvat_GetLastErr();
        std::string errjson;
        json11::Json data = json11::Json::object{
            {"x", x},
            {"y", y},
            {"a", a},
            {"r", r},
            {"m", m},
            {"e", err},
            {"j", cvatErrorToJson()},
            {"s", status}};
        json11::Json obj = json11::Json::object{
            {"action", "cvautotrack"},
            {"data", data}};
        std::string json = obj.dump();
        ws_broadcast(json);
        Sleep(err > 0 ? 800 : 250);
    }
}
bool startTrack() {
    if (isBroadcast)
        return true;
    if (cvat_init()) {
        isBroadcast = true;
        cvThread = std::thread(threadedTrack);
        return true;
    }
    return false;
}
bool stopTrack() {
    if (!isBroadcast)
        return true;
    if (cvat_uninit()) {
        isBroadcast = false;
        TerminateThread(cvThread.native_handle(), 0);
        return true;
    }
    return false;
}
bool track(double &x, double &y, double &a, double &r, int &m) {
    if (!cvat_GetTransformOfMap(x, y, a, m)) {
        return false;
    }
    if (!cvat_GetRotation(r)) {
        return false;
    }
    return true;
}