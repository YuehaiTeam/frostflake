#include <iterator>
#include <string>
#include <vector>

#include <windows.h>

#include "../version.h"
#include <imagehlp.h>
#include <shlobj_core.h>

bool fileExists(std::wstring name);
std::wstring getExePath();
boolean removeScheme();
boolean registerUrlScheme(std::wstring path);
std::wstring ToWString(const std::string &str);

std::wstring getInstallPath(std::wstring path) {
    // Program Files
    WCHAR programFilesPath[MAX_PATH];
    SHGetSpecialFolderPathW(NULL, programFilesPath, CSIDL_PROGRAM_FILES, FALSE);
    // cocogoat-control
    std::wstring installPath = programFilesPath + std::wstring(L"\\cocogoat-control") + (path == L"" ? L"" : (L"\\" + path));
    return installPath;
}

boolean checkIfElevated() {
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            CloseHandle(hToken);
            return !!Elevation.TokenIsElevated;
        }
    }
    return false;
}

boolean elevate(PSTR lpCmdLine, std::wstring runPath = L"") {
    if (runPath == L"") {
        runPath = getExePath();
    }
    std::wstring cmd = ToWString(lpCmdLine);
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = runPath.c_str();
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_SHOW;
    sei.fMask = SEE_MASK_NO_CONSOLE;
    if (ShellExecuteExW(&sei)) {
        return true;
    }
    return false;
}

boolean registerScheduleTask(std::wstring path) {
    // gen cmd
    std::wstring cmd = ToWString("schtasks /create /tn \"椰羊插件（以管理员身份运行）\" /RL HIGHEST /sc ONEVENT /EC Application /MO *[System/EventID=32333] /f /TR \"");
    cmd += path;
    cmd += ToWString("\"");
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, (wchar_t *)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // get return value
        DWORD ret;
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &ret);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return ret == 0;
    }
    return false;
}

boolean runScheduleTask(PSTR lpCmdLine) {
    // get tmp folder
    WCHAR tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    std::wstring tmpPathW = tmpPath;
    tmpPathW += L"cocogoat-control.tmplaunch.txt";
    // write file winapi
    HANDLE hFile = CreateFileW(tmpPathW.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written;
    WriteFile(hFile, lpCmdLine, strlen(lpCmdLine), &written, NULL);
    CloseHandle(hFile);

    std::wstring cmd = ToWString("schtasks /run /tn \"椰羊插件（以管理员身份运行）\"");
    // run and get return value
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, (wchar_t *)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // get return value
        DWORD ret;
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &ret);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (ret == 0) {
            return true;
        }
    }
    return false;
}

std::string readCmdFromPipe() {
    // get tmp folder
    WCHAR tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    std::wstring tmpPathW = tmpPath;
    tmpPathW += L"cocogoat-control.tmplaunch.txt";
    // read file
    HANDLE hFile = CreateFileW(tmpPathW.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }
    DWORD read;
    char buf[1024] = {0};
    std::string ret;
    while (ReadFile(hFile, buf, 1024, &read, NULL)) {
        if (read == 0) {
            break;
        }
        ret += buf;
    }
    CloseHandle(hFile);
    // delete file
    DeleteFileW(tmpPathW.c_str());
    return ret;
}

std::wstring RunCmd(std::wstring pszCmd) {
    // 创建匿名管道
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return TEXT(" ");
    }

    // 设置命令行进程启动信息(以隐藏方式启动命令并定位其输出到hWrite
    STARTUPINFO si = {sizeof(STARTUPINFO)};
    GetStartupInfo(&si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;

    // 启动命令行
    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, (LPWSTR)pszCmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return TEXT("");
    }

    // 立即关闭hWrite
    CloseHandle(hWrite);

    // 读取命令行返回值
    std::wstring strRetTmp = L"";
    char buff[1024] = {0};
    DWORD dwRead = 0;
    while (ReadFile(hRead, buff, 1024, &dwRead, NULL)) {
        int count = MultiByteToWideChar(CP_ACP, 0, buff, dwRead, NULL, 0);
        std::wstring wstr(count, 0);
        MultiByteToWideChar(CP_ACP, 0, buff, dwRead, &wstr[0], count);
        strRetTmp += wstr;
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ret;
    GetExitCodeProcess(pi.hProcess, &ret);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (ret != 0) {
        return L"";
    }
    return strRetTmp;
}

boolean queryScheduleTask() {
    std::wstring cmd = ToWString("schtasks /query /tn \"椰羊插件（以管理员身份运行）\" /V /FO list");
    std::wstring res = RunCmd(cmd);
    if (res == L"") {
        return false;
    }
    std::wstring exePathW = getExePath();
    // check exe path in res
    if (res.find(exePathW) == std::wstring::npos) {
        return false;
    }
    return true;
}

boolean isInProgramFiles() {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring pathW = path;
    std::wstring programFiles = getInstallPath(L"");
    if (pathW.find(programFiles) != std::wstring::npos) {
        return true;
    }
    return false;
}

boolean checkInstall() {
    std::wstring installPath = getInstallPath(L"cocogoat-control.exe");
    if (installPath == L"") {
        return false;
    }
    DWORD temp = 1;
    DWORD selfChksum = 0;
    DWORD targChksum = 0;
    if (MapFileAndCheckSum(installPath.c_str(), &temp, &targChksum) != CHECKSUM_SUCCESS) {
        return true;
    }
    std::wstring exePathW = getExePath();
    if (MapFileAndCheckSum(exePathW.c_str(), &temp, &selfChksum) != CHECKSUM_SUCCESS) {
        return true;
    }
    if (selfChksum != targChksum) {
        return false;
    }
    return true;
}

boolean registerUninstallItem(std::wstring installPath) {
    // write uninstall info to registry
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\cocogoat-control", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    std::wstring friendlyAppName = ToWString("椰羊·霜华插件");
    if (RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE *)friendlyAppName.c_str(), (wcslen(friendlyAppName.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    std::wstring uninstallString = L"\"" + installPath + L"\" --uninstall";
    if (RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE *)uninstallString.c_str(), (wcslen(uninstallString.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    if (RegSetValueExW(hKey, L"NoModify", 0, REG_SZ, (BYTE *)L"1", (wcslen(L"1") + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    if (RegSetValueExW(hKey, L"NoRepair", 0, REG_SZ, (BYTE *)L"1", (wcslen(L"1") + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    std::wstring version = ToWString(VERSION);
    if (RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (BYTE *)version.c_str(), (wcslen(version.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    // DisplayIcon
    if (RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE *)installPath.c_str(), (wcslen(installPath.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    // publisher
    std::wstring publisher = L"月海亭YuehaiTeam";
    if (RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (BYTE *)publisher.c_str(), (wcslen(publisher.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    // installLocation
    std::wstring installFolder = getInstallPath(L"");
    if (RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (BYTE *)installFolder.c_str(), (wcslen(installFolder.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    return true;
}

boolean removeUninstallItem() {
    if (RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\cocogoat-control") != ERROR_SUCCESS) {
        return false;
    }
    return true;
}

boolean runInstaller() {
    CreateDirectoryW(getInstallPath(L"").c_str(), NULL);
    // copy self to install path
    std::wstring exePathW = getExePath();
    std::wstring installPath = getInstallPath(L"cocogoat-control.exe");
    if (!CopyFileW(exePathW.c_str(), installPath.c_str(), FALSE)) {
        DWORD err = GetLastError();
        // FormatMessage
        LPWSTR lpMsgBuf = NULL;
        DWORD dw = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);
        if (dw == 0) {
            lpMsgBuf = L"";
        }
        // to wstring
        std::wstring errMsg = lpMsgBuf;
        LocalFree(lpMsgBuf);
        MessageBoxW(NULL, (L"复制文件到 " + installPath + L" 失败 (" + std::to_wstring(err) + L")：" + errMsg).c_str(), L"椰羊：安装失败", MB_OK | MB_ICONERROR);
        exit(301);
        return false;
    }
    if (!registerScheduleTask(installPath)) {
        return false;
    }
    if (!registerUrlScheme(installPath)) {
        return false;
    }
    if (!registerUninstallItem(installPath)) {
        return false;
    }
    return true;
}

int deleteDirectory(const std::wstring &path) {
    std::vector<std::wstring::value_type> doubleNullTerminatedPath;
    std::copy(path.begin(), path.end(), std::back_inserter(doubleNullTerminatedPath));
    doubleNullTerminatedPath.push_back(L'\0');
    doubleNullTerminatedPath.push_back(L'\0');

    SHFILEOPSTRUCTW fileOperation;
    fileOperation.wFunc = FO_DELETE;
    fileOperation.pFrom = &doubleNullTerminatedPath[0];
    fileOperation.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;

    return SHFileOperationW(&fileOperation);
}

boolean runUninstall() {
    // delete folder
    std::wstring installDir = getInstallPath(L"");
    int ret = deleteDirectory(installDir);
    if (ret != 0) {
        MessageBoxW(NULL, ToWString("卸载失败：[D]" + std::to_string(ret)).c_str(), L"错误", MB_OK | MB_ICONERROR);
        return false;
    }
    // delete schtask
    std::wstring cmd = ToWString("schtasks /delete /tn \"椰羊插件（以管理员身份运行）\" /F");
    RunCmd(cmd);
    // remove scheme
    if (!removeScheme()) {
        DWORD err = GetLastError();
        MessageBoxW(NULL, (ToWString("卸载失败：[S]") + std::to_wstring(err)).c_str(), L"错误", MB_OK | MB_ICONERROR);
        return false;
    }
    // remove uninstall item
    if (!removeUninstallItem()) {
        DWORD err = GetLastError();
        MessageBoxW(NULL, (ToWString("卸载失败：[U]") + std::to_wstring(err)).c_str(), L"错误", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}
void askUninstall() {
    std::wstring title = L"卸载 - 椰羊";
    std::wstring header = L"您确定要卸载椰羊·霜华插件，并删除相关的所有组件吗？";
    std::wstring allowstr = L"卸载";
    std::wstring denystr = L"取消";

    TASKDIALOGCONFIG tdc = {sizeof(TASKDIALOGCONFIG)};

    TASKDIALOG_BUTTON aCustomButtons[] = {
        {1000, allowstr.c_str()},
        {1001, denystr.c_str()}};

    tdc.hwndParent = NULL;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = 2;
    tdc.pszWindowTitle = title.c_str();
    tdc.pszMainIcon = TD_WARNING_ICON;
    tdc.pszMainInstruction = header.c_str();

    int nClickedBtn;
    BOOL nVerificationChecked;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &nVerificationChecked);
    if (SUCCEEDED(hr) && 1000 == nClickedBtn) {
        if (runUninstall()) {
            MessageBoxW(NULL, L"卸载成功！", L"卸载 - 椰羊", MB_OK | MB_ICONINFORMATION);
        }
    }
}

boolean deleteSelf() {
    WCHAR path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    std::wstring pathW = path;
    std::wstring batPath = pathW + L"cocogoat-control-uninstaller.bat";
    std::wstring exePathW = getExePath();
    std::wstring batContent = L":Repeat\r\n"
                              L"del \"" +
                              exePathW + L"\"\r\n"
                                         L"if exist \"" +
                              exePathW + L"\" goto Repeat\r\n"
                                         L"del \"" +
                              exePathW + L"\"\r\n" +
                              L"del \"" + batPath + L"\"\r\n";
    // convert to active code page
    int count = WideCharToMultiByte(CP_ACP, 0, batContent.c_str(), batContent.length(), NULL, 0, NULL, NULL);
    std::string str(count, 0);
    WideCharToMultiByte(CP_ACP, 0, batContent.c_str(), -1, &str[0], count, NULL, NULL);
    // write to file winapi
    HANDLE hFile = CreateFileW(batPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        MessageBoxW(NULL, (ToWString("卸载失败：") + std::to_wstring(err)).c_str(), ToWString("错误").c_str(), MB_OK | MB_ICONERROR);
        return false;
    }
    DWORD written;
    WriteFile(hFile, str.c_str(), str.length(), &written, NULL);
    CloseHandle(hFile);
    ShellExecute(NULL, L"open", batPath.c_str(), NULL, NULL, SW_HIDE);
    return true;
}

void preUninstall() {
    // get TEMP path
    WCHAR path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    std::wstring pathW = path;
    // get exe path
    std::wstring exePathW = getExePath();
    // if exe path is in TEMP path
    if (exePathW.find(pathW) != std::wstring::npos) {
        askUninstall();
        deleteSelf();
    } else {
        // copy exe to TEMP path
        std::wstring copyPath = pathW + L"cocogoat-control-uninstaller.exe";
        if (!CopyFileW(exePathW.c_str(), copyPath.c_str(), FALSE)) {
            DWORD err = GetLastError();
            MessageBoxW(NULL, (ToWString("卸载程序启动失败：") + std::to_wstring(err)).c_str(), ToWString("错误").c_str(), MB_OK | MB_ICONERROR);
            return;
        }
        // run copyed exe
        elevate("--uninstall", copyPath);
    }
}

// true for exit
boolean autoElevate(PSTR lpCmdLine) {
    std::string cmd = lpCmdLine;
    boolean noInstall = cmd.find("--stay") != std::string::npos && cmd.find("://") == std::string::npos;
    boolean onlyInstall = cmd.find("--install") != std::string::npos && cmd.find("://") == std::string::npos;
    // only install
    if (onlyInstall) {
        if (checkIfElevated()) {
            runInstaller();
        } else {
            elevate(lpCmdLine);
        }
        return true;
    }
    // only uninstall
    if (cmd.find("--uninstall") != std::string::npos && cmd.find("://") == std::string::npos) {
        if (checkIfElevated()) {
            if (noInstall) {
                askUninstall();
            } else {
                preUninstall();
            }
            return true;
        } else {
            return elevate(lpCmdLine);
        }
    }
    if (checkIfElevated()) {
        if (noInstall) {
            return false;
        } else if (isInProgramFiles()) {
            return false;
        } else if (!runInstaller()) {
            return false;
        }
    }
    if ((isInProgramFiles() ? queryScheduleTask() : checkInstall()) && runScheduleTask(lpCmdLine)) {
        return true;
    }
    if (checkIfElevated()) {
        return false;
    }
    if (!elevate(lpCmdLine)) {
        MessageBoxW(NULL, L"由于需要进行版本更新，请以管理员权限运行本程序。", L"错误", MB_OK | MB_ICONERROR);
    }
    return true;
}