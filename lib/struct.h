#ifndef STRUCT_H
#define STRUCT_H
#include "../MfcClass/Manager.h"
#include "json11.hpp"
#include <string>
#define WM_AUTHENCATION (WM_USER + 0x0001)
typedef struct WM_AUTHENCATION_DATA {
    std::string origin;
    bool pass;
    bool remember;
} WM_AUTHENCATION_DATA;

class WindowData {
public:
    HWND hWnd;
    std::string title;
    std::string classname;
    int width;
    int height;
    int x;
    int y;
    json11::Json to_json() const {
        return json11::Json::object{{
            {"hWnd", (long)hWnd},
            {"title", title},
            {"classname", classname},
            {"width", width},
            {"height", height},
            {"x", x},
            {"y", y},
        }};
    };
};

class MonitorData {
public:
    long id;
    int width;
    int height;
    int x;
    int y;
    boolean primary;
    int dpi;
    json11::Json to_json() const {
        return json11::Json::object{{{"primary", primary},
                                     {"dpi", dpi},
                                     {"width", width},
                                     {"height", height},
                                     {"x", x},
                                     {"y", y},
                                     {"id", id}}};
    };
};

std::wstring getLocalPath(std::string name);
std::string deviceEncrypt(std::string str);
std::string deviceDecrypt(std::string str);
#endif