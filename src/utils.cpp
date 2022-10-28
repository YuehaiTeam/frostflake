#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <windows.h>
std::wstring ToWString(const std::string &str);
std::string ToString(const std::wstring &str);
std::wstring getInstallPath(std::wstring path);
// get exe path
std::wstring getExePath() {
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    return szPath;
}
// relative path to exe
std::wstring getRelativePath(std::string name) {
    // get exe path
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);
    PathAppendW(szPath, ToWString(name).c_str());
    return szPath;
}
// check file exists
bool fileExists(std::wstring name) {
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string errToString(HRESULT err) {
    LPWSTR lpMsgBuf = NULL;
    DWORD dw = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);
    if (dw == 0) {
        lpMsgBuf = L"";
    }
    std::string ret = ToString(std::wstring(lpMsgBuf));
    LocalFree(lpMsgBuf);
    return ret;
}

std::wstring getLocalPath(std::string name) {
    if (fileExists(getRelativePath("cocogoat")) || fileExists(getRelativePath("cocogoat-noupdate")) || fileExists(getRelativePath(name))) {
        return getRelativePath(name);
    }
    return getInstallPath(ToWString(name));
}
std::string deviceEncrypt(std::string str) {
    // CryptProtectData
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DATA_BLOB DataEntropy;
    DataIn.pbData = (BYTE *)str.c_str();
    DataIn.cbData = str.length();
    DataEntropy.pbData = (BYTE *)"cocogoat";
    DataEntropy.cbData = 8;
    if (!CryptProtectData(&DataIn, NULL, &DataEntropy, NULL, NULL, 0, &DataOut)) {
        return "";
    }
    // to hex
    std::string ret;
    for (int i = 0; i < DataOut.cbData; i++) {
        char buf[3];
        sprintf(buf, "%02x", DataOut.pbData[i]);
        ret += buf;
    }
    // free
    LocalFree(DataOut.pbData);
    return ret;
}
std::string deviceDecrypt(std::string hex) {
    // hex to buffer
    int len = hex.length() / 2;
    BYTE *buf = new BYTE[len];
    for (int i = 0; i < len; i++) {
        buf[i] = (BYTE)strtol(hex.substr(i * 2, 2).c_str(), NULL, 16);
    }
    // CryptUnprotectData
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DataIn.pbData = buf;
    DataIn.cbData = len;
    DATA_BLOB DataEntropy;
    DataEntropy.pbData = (BYTE *)"cocogoat";
    DataEntropy.cbData = 8;
    if (!CryptUnprotectData(&DataIn, NULL, &DataEntropy, NULL, NULL, 0, &DataOut)) {
        return "";
    }
    // to string
    std::string ret((char *)DataOut.pbData, DataOut.cbData);
    // free
    LocalFree(DataOut.pbData);
    return ret;
}

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// from ntdef.h
typedef struct _WNF_STATE_NAME {
    ULONG Data[2];
} WNF_STATE_NAME;

typedef struct _WNF_STATE_NAME *PWNF_STATE_NAME;
typedef const struct _WNF_STATE_NAME *PCWNF_STATE_NAME;

typedef struct _WNF_TYPE_ID {
    GUID TypeId;
} WNF_TYPE_ID, *PWNF_TYPE_ID;

typedef const WNF_TYPE_ID *PCWNF_TYPE_ID;

typedef ULONG WNF_CHANGE_STAMP, *PWNF_CHANGE_STAMP;

enum FocusAssistResult {
    not_supported = -2,
    failed = -1,
    off = 0,
    priority_only = 1,
    alarms_only = 2
};

std::unordered_map<int, std::string> result_map = {
    {-2, "Not Supported"},
    {-1, "Failed"},
    {0, "Off"},
    {1, "Priority Only"},
    {2, "Alarm Only"}};

typedef NTSTATUS(NTAPI *PNTQUERYWNFSTATEDATA)(
    _In_ PWNF_STATE_NAME StateName,
    _In_opt_ PWNF_TYPE_ID TypeId,
    _In_opt_ const VOID *ExplicitScope,
    _Out_ PWNF_CHANGE_STAMP ChangeStamp,
    _Out_writes_bytes_to_opt_(*BufferSize, *BufferSize) PVOID Buffer,
    _Inout_ PULONG BufferSize);

boolean isFocusAssistEnabled() {
    const auto h_ntdll = GetModuleHandleW(L"ntdll");
    const auto pNtQueryWnfStateData = PNTQUERYWNFSTATEDATA(GetProcAddress(h_ntdll, "NtQueryWnfStateData"));
    if (!pNtQueryWnfStateData) {
        return false;
    }
    // state name for active hours (Focus Assist)
    WNF_STATE_NAME WNF_SHEL_QUIETHOURS_ACTIVE_PROFILE_CHANGED{0xA3BF1C75, 0xD83063E};
    // note: we won't use it but it's required
    WNF_CHANGE_STAMP change_stamp = {0};
    // on output buffer will tell us the status of Focus Assist
    DWORD buffer = 0;
    ULONG buffer_size = sizeof(buffer);
    if (NT_SUCCESS(pNtQueryWnfStateData(&WNF_SHEL_QUIETHOURS_ACTIVE_PROFILE_CHANGED, nullptr, nullptr, &change_stamp,
                                        &buffer, &buffer_size))) {
        // check if the result is one of FocusAssistResult
        if (result_map.count(buffer) == 0) {
            return false;
        } else {
            if (buffer > 0) {
                return true;
            }
            return false;
        }
    } else {
        return false;
    }
}

/******************************************************************************\
*       This is a part of the Microsoft Source Code Samples.
*       Copyright 1995 - 1997 Microsoft Corporation.
*       All rights reserved.
*       This source code is only intended as a supplement to
*       Microsoft Development Tools and/or WinHelp documentation.
*       See these sources for detailed information regarding the
*       Microsoft samples programs.
\******************************************************************************/

/*++
Copyright (c) 1997  Microsoft Corporation
Module Name:
    pipeex.c
Abstract:
    CreatePipe-like function that lets one or both handles be overlapped
Author:
    Dave Hart  Summer 1997
Revision History:
--*/

static volatile long PipeSerialNumber;

BOOL
    APIENTRY
    MyCreatePipeEx(
        OUT LPHANDLE lpReadPipe,
        OUT LPHANDLE lpWritePipe,
        IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
        IN DWORD nSize,
        DWORD dwReadMode,
        DWORD dwWriteMode)

/*++
Routine Description:
    The CreatePipeEx API is used to create an anonymous pipe I/O device.
    Unlike CreatePipe FILE_FLAG_OVERLAPPED may be specified for one or
    both handles.
    Two handles to the device are created.  One handle is opened for
    reading and the other is opened for writing.  These handles may be
    used in subsequent calls to ReadFile and WriteFile to transmit data
    through the pipe.
Arguments:
    lpReadPipe - Returns a handle to the read side of the pipe.  Data
        may be read from the pipe by specifying this handle value in a
        subsequent call to ReadFile.
    lpWritePipe - Returns a handle to the write side of the pipe.  Data
        may be written to the pipe by specifying this handle value in a
        subsequent call to WriteFile.
    lpPipeAttributes - An optional parameter that may be used to specify
        the attributes of the new pipe.  If the parameter is not
        specified, then the pipe is created without a security
        descriptor, and the resulting handles are not inherited on
        process creation.  Otherwise, the optional security attributes
        are used on the pipe, and the inherit handles flag effects both
        pipe handles.
    nSize - Supplies the requested buffer size for the pipe.  This is
        only a suggestion and is used by the operating system to
        calculate an appropriate buffering mechanism.  A value of zero
        indicates that the system is to choose the default buffering
        scheme.
Return Value:
    TRUE - The operation was successful.
    FALSE/NULL - The operation failed. Extended error status is available
        using GetLastError.
--*/

{
    HANDLE ReadPipeHandle, WritePipeHandle;
    DWORD dwError;
    char PipeNameBuffer[MAX_PATH];

    //
    // Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
    //

    if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    //  Set the default timeout to 120 seconds
    //

    if (nSize == 0) {
        nSize = 4096;
    }

    sprintf(PipeNameBuffer,
            "\\\\.\\Pipe\\CocogoatControl.%08x.%08x",
            GetCurrentProcessId(),
            InterlockedIncrement(&PipeSerialNumber));

    ReadPipeHandle = CreateNamedPipeA(
        PipeNameBuffer,
        PIPE_ACCESS_INBOUND | dwReadMode,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,          // Number of pipes
        nSize,      // Out buffer size
        nSize,      // In buffer size
        120 * 1000, // Timeout in ms
        lpPipeAttributes);

    if (!ReadPipeHandle) {
        return FALSE;
    }

    WritePipeHandle = CreateFileA(
        PipeNameBuffer,
        GENERIC_WRITE,
        0, // No sharing
        lpPipeAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | dwWriteMode,
        NULL // Template file
    );

    if (INVALID_HANDLE_VALUE == WritePipeHandle) {
        dwError = GetLastError();
        CloseHandle(ReadPipeHandle);
        SetLastError(dwError);
        return FALSE;
    }

    *lpReadPipe = ReadPipeHandle;
    *lpWritePipe = WritePipeHandle;
    return (TRUE);
}