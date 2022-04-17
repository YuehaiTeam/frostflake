#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string>
#include <windows.h>
std::wstring ToWString(const std::string &str);
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
std::wstring getLocalPath(std::string name) {
    if (fileExists(getRelativePath("cocogoat")) || fileExists(getRelativePath("cocogoat-noupdate")) || fileExists(getRelativePath(name))) {
        return getRelativePath(name);
    }
    WCHAR szPath[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) < 0) {
        return L"";
    }
    PathAppendW(szPath, L"\\cocogoat-control\\");
    CreateDirectoryW(szPath, NULL);
    PathAppendW(szPath, ToWString(name).c_str());
    return szPath;
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