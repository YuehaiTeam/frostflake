#include "cvautotrackimpl.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <windows.h>
HMODULE hModule = NULL;
bool fileExists(std::wstring name);
std::wstring getLocalPath(std::string name);

bool cvat_version(char *versionBuff) {
    if (cvat_imp_verison) {
        return cvat_imp_verison(versionBuff);
    }
    return false;
}
bool cvat_init() {
    if (cvat_imp_init) {
        return cvat_imp_init();
    }
    return false;
}
bool cvat_uninit() {
    if (cvat_imp_uninit) {
        return cvat_imp_uninit();
    }
    return false;
}
bool cvat_startServe() {
    if (cvat_imp_startServe) {
        return cvat_imp_startServe();
    }
    return false;
}
bool cvat_stopServe() {
    if (cvat_imp_stopServe) {
        return cvat_imp_stopServe();
    }
    return false;
}
bool cvat_SetUseBitbltCaptureMode() {
    if (cvat_imp_SetUseBitbltCaptureMode) {
        return cvat_imp_SetUseBitbltCaptureMode();
    }
    return false;
}
bool cvat_SetUseDx11CaptureMode() {
    if (cvat_imp_SetUseDx11CaptureMode) {
        return cvat_imp_SetUseDx11CaptureMode();
    }
    return false;
}
bool cvat_SetHandle(long long int handle) {
    if (cvat_imp_SetHandle) {
        return cvat_imp_SetHandle(handle);
    }
    return false;
}
bool cvat_SetWorldCenter(double x, double y) {
    if (cvat_imp_SetWorldCenter) {
        return cvat_imp_SetWorldCenter(x, y);
    }
    return false;
}
bool cvat_SetWorldScale(double scale) {
    if (cvat_imp_SetWorldScale) {
        return cvat_imp_SetWorldScale(scale);
    }
    return false;
}
bool cvat_GetTransformOfMap(double &x, double &y, double &a, int &mapId) {
    if (cvat_imp_GetTransformOfMap) {
        return cvat_imp_GetTransformOfMap(x, y, a, mapId);
    }
    return false;
}
bool cvat_GetPositionOfMap(double &x, double &y, int &mapId) {
    if (cvat_imp_GetPositionOfMap) {
        return cvat_imp_GetPositionOfMap(x, y, mapId);
    }
    return false;
}
bool cvat_GetDirection(double &a) {
    if (cvat_imp_GetDirection) {
        return cvat_imp_GetDirection(a);
    }
    return false;
}
bool cvat_GetRotation(double &a) {
    if (cvat_imp_GetRotation) {
        return cvat_imp_GetRotation(a);
    }
    return false;
}
bool cvat_GetStar(double &x, double &y, bool &isEnd) {
    if (cvat_imp_GetStar) {
        return cvat_imp_GetStar(x, y, isEnd);
    }
    return false;
}
bool cvat_GetStarJson(char *jsonBuff) {
    if (cvat_imp_GetStarJson) {
        return cvat_imp_GetStarJson(jsonBuff);
    }
    return false;
}
bool cvat_GetUID(int &uid) {
    if (cvat_imp_GetUID) {
        return cvat_imp_GetUID(uid);
    }
    return false;
}
bool cvat_GetInfoLoadPicture(char *path, int &uid, double &x, double &y, double &a) {
    if (cvat_imp_GetInfoLoadPicture) {
        return cvat_imp_GetInfoLoadPicture(path, uid, x, y, a);
    }
    return false;
}
bool cvat_GetInfoLoadVideo(char *path, char *pathOutFile) {
    if (cvat_imp_GetInfoLoadVideo) {
        return cvat_imp_GetInfoLoadVideo(path, pathOutFile);
    }
    return false;
}
bool cvat_DebugCapture() {
    if (cvat_imp_DebugCapture) {
        return cvat_imp_DebugCapture();
    }
    return false;
}
bool cvat_DebugCapturePath(const char *path, int buff_size) {
    if (cvat_imp_DebugCapturePath) {
        return cvat_imp_DebugCapturePath(path, buff_size);
    }
    return false;
}
int cvat_GetLastErr() {
    if (cvat_imp_GetLastErr) {
        return cvat_imp_GetLastErr();
    }
    return 0;
}
int cvat_GetLastErrMsg(char *msg_buff, int buff_size) {
    if (cvat_imp_GetLastErrMsg) {
        return cvat_imp_GetLastErrMsg(msg_buff, buff_size);
    }
    return 0;
}
int cvat_GetLastErrJson(char *json_buff, int buff_size) {
    if (cvat_imp_GetLastErrJson) {
        return cvat_imp_GetLastErrJson(json_buff, buff_size);
    }
    return 0;
}
bool cvat_GetCompileVersion(char *version_buff, int buff_size) {
    if (cvat_imp_GetCompileVersion) {
        return cvat_imp_GetCompileVersion(version_buff, buff_size);
    }
    return false;
}
bool cvat_GetCompileTime(char *time_buff, int buff_size) {
    if (cvat_imp_GetCompileTime) {
        return cvat_imp_GetCompileTime(time_buff, buff_size);
    }
    return false;
}

std::string cvat_load() {
    if (cvat_imp_init) {
        return "loaded";
    }
    std::wstring dllPath = getLocalPath("cvautotrack.dll");
    if (!fileExists(dllPath)) {
        return "nofile";
    }
    hModule = LoadLibraryExW(dllPath.c_str(), NULL, NULL);
    if (!hModule) {
        long errcode = GetLastError();
        return "Error: LoadLibrary " + std::to_string(errcode) + ": " + errToString(errcode);
    }
    cvat_imp_init = (cvautotrack_init)GetProcAddress(hModule, "init");
    cvat_imp_uninit = (cvautotrack_uninit)GetProcAddress(hModule, "uninit");
    cvat_imp_startServe = (cvautotrack_startServe)GetProcAddress(hModule, "startServe");
    cvat_imp_stopServe = (cvautotrack_stopServe)GetProcAddress(hModule, "stopServe");
    cvat_imp_SetUseBitbltCaptureMode = (cvautotrack_SetUseBitbltCaptureMode)GetProcAddress(hModule, "SetUseBitbltCaptureMode");
    cvat_imp_SetUseDx11CaptureMode = (cvautotrack_SetUseDx11CaptureMode)GetProcAddress(hModule, "SetUseDx11CaptureMode");
    cvat_imp_SetHandle = (cvautotrack_SetHandle)GetProcAddress(hModule, "SetHandle");
    cvat_imp_SetWorldCenter = (cvautotrack_SetWorldCenter)GetProcAddress(hModule, "SetWorldCenter");
    cvat_imp_SetWorldScale = (cvautotrack_SetWorldScale)GetProcAddress(hModule, "SetWorldScale");
    cvat_imp_GetTransformOfMap = (cvautotrack_GetTransformOfMap)GetProcAddress(hModule, "GetTransformOfMap");
    cvat_imp_GetPositionOfMap = (cvautotrack_GetPositionOfMap)GetProcAddress(hModule, "GetPositionOfMap");
    cvat_imp_GetDirection = (cvautotrack_GetDirection)GetProcAddress(hModule, "GetDirection");
    cvat_imp_GetRotation = (cvautotrack_GetRotation)GetProcAddress(hModule, "GetRotation");
    cvat_imp_GetStar = (cvautotrack_GetStar)GetProcAddress(hModule, "GetStar");
    cvat_imp_GetStarJson = (cvautotrack_GetStarJson)GetProcAddress(hModule, "GetStarJson");
    cvat_imp_GetUID = (cvautotrack_GetUID)GetProcAddress(hModule, "GetUID");
    cvat_imp_GetInfoLoadPicture = (cvautotrack_GetInfoLoadPicture)GetProcAddress(hModule, "GetInfoLoadPicture");
    cvat_imp_GetInfoLoadVideo = (cvautotrack_GetInfoLoadVideo)GetProcAddress(hModule, "GetInfoLoadVideo");
    cvat_imp_DebugCapture = (cvautotrack_DebugCapture)GetProcAddress(hModule, "DebugCapture");
    cvat_imp_DebugCapturePath = (cvautotrack_DebugCapturePath)GetProcAddress(hModule, "DebugCapturePath");
    cvat_imp_GetLastErr = (cvautotrack_GetLastErr)GetProcAddress(hModule, "GetLastErr");
    cvat_imp_GetLastErrMsg = (cvautotrack_GetLastErrMsg)GetProcAddress(hModule, "GetLastErrMsg");
    cvat_imp_GetLastErrJson = (cvautotrack_GetLastErrJson)GetProcAddress(hModule, "GetLastErrJson");
    cvat_imp_GetCompileVersion = (cvautotrack_GetCompileVersion)GetProcAddress(hModule, "GetCompileVersion");
    cvat_imp_GetCompileTime = (cvautotrack_GetCompileTime)GetProcAddress(hModule, "GetCompileTime");

    if (
        !cvat_imp_init ||
        !cvat_imp_uninit ||
        !cvat_imp_startServe ||
        !cvat_imp_stopServe ||
        !cvat_imp_SetUseBitbltCaptureMode ||
        !cvat_imp_SetUseDx11CaptureMode ||
        !cvat_imp_SetHandle ||
        !cvat_imp_SetWorldCenter ||
        !cvat_imp_SetWorldScale ||
        !cvat_imp_GetTransformOfMap ||
        !cvat_imp_GetPositionOfMap ||
        !cvat_imp_GetDirection ||
        !cvat_imp_GetRotation ||
        !cvat_imp_GetStar ||
        !cvat_imp_GetStarJson ||
        !cvat_imp_GetUID ||
        !cvat_imp_GetInfoLoadPicture ||
        !cvat_imp_GetInfoLoadVideo ||
        !cvat_imp_DebugCapture ||
        !cvat_imp_GetLastErr ||
        !cvat_imp_GetLastErrMsg ||
        !cvat_imp_GetCompileVersion ||
        !cvat_imp_GetCompileTime) {
        long errcode = GetLastError();
        return "Error: GetProcAddress " + std::to_string(errcode) + ": " + errToString(errcode);
    }
    return "loaded";
}