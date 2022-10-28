#include "cvautotrack.h"

cvautotrack_verison cvat_imp_verison = nullptr;
cvautotrack_init cvat_imp_init = nullptr;
cvautotrack_uninit cvat_imp_uninit = nullptr;
cvautotrack_startServe cvat_imp_startServe = nullptr;
cvautotrack_stopServe cvat_imp_stopServe = nullptr;
cvautotrack_SetUseBitbltCaptureMode cvat_imp_SetUseBitbltCaptureMode = nullptr;
cvautotrack_SetUseDx11CaptureMode cvat_imp_SetUseDx11CaptureMode = nullptr;
cvautotrack_SetHandle cvat_imp_SetHandle = nullptr;
cvautotrack_SetWorldCenter cvat_imp_SetWorldCenter = nullptr;
cvautotrack_SetWorldScale cvat_imp_SetWorldScale = nullptr;
cvautotrack_GetTransformOfMap cvat_imp_GetTransformOfMap = nullptr;
cvautotrack_GetPositionOfMap cvat_imp_GetPositionOfMap = nullptr;
cvautotrack_GetDirection cvat_imp_GetDirection = nullptr;
cvautotrack_GetRotation cvat_imp_GetRotation = nullptr;
cvautotrack_GetStar cvat_imp_GetStar = nullptr;
cvautotrack_GetStarJson cvat_imp_GetStarJson = nullptr;
cvautotrack_GetUID cvat_imp_GetUID = nullptr;
cvautotrack_GetInfoLoadPicture cvat_imp_GetInfoLoadPicture = nullptr;
cvautotrack_GetInfoLoadVideo cvat_imp_GetInfoLoadVideo = nullptr;
cvautotrack_DebugCapture cvat_imp_DebugCapture = nullptr;
cvautotrack_DebugCapturePath cvat_imp_DebugCapturePath = nullptr;
cvautotrack_GetLastErr cvat_imp_GetLastErr = nullptr;
cvautotrack_GetLastErrMsg cvat_imp_GetLastErrMsg = nullptr;
cvautotrack_GetLastErrJson cvat_imp_GetLastErrJson = nullptr;
cvautotrack_GetCompileVersion cvat_imp_GetCompileVersion = nullptr;
cvautotrack_GetCompileTime cvat_imp_GetCompileTime = nullptr;