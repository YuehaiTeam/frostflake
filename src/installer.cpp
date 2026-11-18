#include <cstring>
#include <fstream>
#include <string>

#include <windows.h>

#include "../version.h"
#include <shlobj_core.h>
#include <shlwapi.h>
#include "installDialog.h"

std::wstring getExePath();
boolean removeScheme();
boolean registerUrlScheme(std::wstring path);
std::wstring ToWString(const std::string &str);

namespace {
const wchar_t uninstallKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\cocogoat-control";
const wchar_t programName[] = L"cocogoat-control.exe";
const wchar_t directoryName[] = L"cocogoat-control";

std::wstring joinPath(const std::wstring &root, const std::wstring &leaf) {
    return root.empty() || leaf.empty() ? root : root + L"\\" + leaf;
}

std::wstring defaultDirectory() {
    wchar_t folder[MAX_PATH]{};
    if (!SHGetSpecialFolderPathW(nullptr, folder, CSIDL_PROGRAM_FILES, FALSE)) return {};
    return installDirectoryForSelection(folder);
}

bool samePath(const std::wstring &a, const std::wstring &b) {
    return !a.empty() && CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

// Paths later passed to legacy Win32 APIs must be absolute, local and fit MAX_PATH.
bool normalizeDirectory(std::wstring &path) {
    for (auto &c : path) if (c == L'/') c = L'\\';
    while (path.size() > 3 && path.back() == L'\\') path.pop_back();
    if (path.size() < 4 || path.substr(1, 2) != L":\\" ||
        path.find_first_of(L"<>:\"|?*", 2) != path.npos ||
        path.size() + 1 + wcslen(programName) >= MAX_PATH) return false;
    if (GetDriveTypeW(path.substr(0, 3).c_str()) != DRIVE_FIXED) return false;
    for (size_t start = 3; start < path.size();) {
        size_t end = path.find(L'\\', start);
        std::wstring part = path.substr(start, end - start);
        if (part.empty() || part.back() == L'.' || part.back() == L' ' ||
            part.find_first_of(L"\r\n\t") != part.npos) return false;
        DWORD attrs = GetFileAttributesW(path.substr(0, end).c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES &&
            ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) || !(attrs & FILE_ATTRIBUTE_DIRECTORY))) return false;
        if (end == path.npos) break;
        start = end + 1;
    }
    wchar_t full[MAX_PATH];
    DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr);
    return length && length < MAX_PATH && samePath(path, full);
}

std::wstring registeredDirectory(bool *hasRecord = nullptr) {
    wchar_t path[MAX_PATH]{};
    DWORD size = sizeof(path);
    LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, uninstallKey, L"InstallLocation",
                                  RRF_RT_REG_SZ, nullptr, path, &size);
    if (hasRecord) *hasRecord = status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND;
    if (status != ERROR_SUCCESS) return {};
    std::wstring result(path);
    return normalizeDirectory(result) ? result : L"";
}
}

std::wstring installDirectoryForSelection(std::wstring path) {
    for (auto &c : path) if (c == L'/') c = L'\\';
    if (path.size() < 3 || path.substr(1, 2) != L":\\") return {};
    while (path.size() > 3 && path.back() == L'\\') path.pop_back();
    if (samePath(path.substr(path.find_last_of(L'\\') + 1), directoryName)) return path;
    if (path.back() != L'\\') path += L'\\';
    return path + directoryName;
}

std::wstring validateInstallDirectory(std::wstring &path) {
    if (!normalizeDirectory(path) || !samePath(path.substr(path.find_last_of(L'\\') + 1), directoryName))
        return L"请选择有效的本地安装位置。安装目标必须是 cocogoat-control 文件夹，且路径不能过长或经过链接目录。";
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && !PathIsDirectoryEmptyW(path.c_str()))
        return L"目标 cocogoat-control 文件夹已有其他文件，请选择另一个上级文件夹。";
    return {};
}

std::wstring getInstallPath(std::wstring path) {
    bool recorded;
    std::wstring directory = registeredDirectory(&recorded);
    auto result = joinPath(recorded ? directory : defaultDirectory(), path);
    return result.size() < MAX_PATH ? result : L"";
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
    std::wstring cmd = ToWString("schtasks /create /tn \"椰羊插件（以管理员身份运行）\" /RL HIGHEST /sc ONEVENT /EC Application /MO *[System/EventID=32333] /f /TR \"'");
    cmd += path;
    cmd += ToWString("\"'");
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
    if (res.find(L"'" + exePathW) == std::wstring::npos && res.find(L"\"" + exePathW) == std::wstring::npos) {
        return false;
    }
    return true;
}

bool sameInstalledBuild(const std::wstring &installPath) {
    std::ifstream self(getExePath().c_str(), std::ios::binary | std::ios::ate);
    std::ifstream installed(installPath.c_str(), std::ios::binary | std::ios::ate);
    if (!self || !installed || self.tellg() != installed.tellg()) return false;
    self.seekg(0);
    installed.seekg(0);
    char a[8192], b[8192];
    do {
        self.read(a, sizeof(a));
        installed.read(b, sizeof(b));
        if (self.gcount() != installed.gcount() || std::memcmp(a, b, self.gcount()) != 0) return false;
    } while (self && installed);
    return self.eof() && installed.eof() && !self.bad() && !installed.bad();
}

boolean registerUninstallItem(const std::wstring &installFolder) {
    const std::wstring installPath = joinPath(installFolder, programName);
    // write uninstall info to registry
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, uninstallKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    struct CloseKey { HKEY key; ~CloseKey() { RegCloseKey(key); } } closeKey{hKey};
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
    if (RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (BYTE *)installFolder.c_str(), (wcslen(installFolder.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    return true;
}

boolean removeUninstallItem() {
    LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, uninstallKey);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

static bool installError(const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"椰羊 · 霜华插件", MB_OK | MB_ICONERROR);
    SetLastError(ERROR_INSTALL_FAILURE);
    return false;
}

static bool regularFile(const std::wstring &path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

static std::wstring existingInstallation() {
    bool recorded;
    std::wstring directory = registeredDirectory(&recorded);
    if (!recorded) {
        directory = defaultDirectory();
        if (!normalizeDirectory(directory) || !regularFile(joinPath(directory, programName))) return {};
    }
    return directory;
}

// Hold the handle through the copy and all registrations, including rollback.
struct InstallationLock {
    HANDLE handle = CreateMutexW(nullptr, FALSE, L"Global\\cocogoat-control-installation");
    DWORD wait = handle ? WaitForSingleObject(handle, 0) : WAIT_FAILED;
    bool acquired = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
    ~InstallationLock() {
        DWORD error = GetLastError();
        if (acquired) ReleaseMutex(handle);
        if (handle) CloseHandle(handle);
        SetLastError(error);
    }
};

// Stage the new executable beside the old one, so a failed copy cannot destroy a working install.
bool runInstaller(std::wstring &directory) {
    InstallationLock lock;
    if (!lock.acquired) return installError(L"另一个安装或卸载程序正在运行。");
    directory = existingInstallation();
    bool fresh = directory.empty();
    if (fresh) {
        bool recorded;
        registeredDirectory(&recorded);
        if (recorded) return installError(L"已登记的安装位置不可用，请检查磁盘和文件夹。");
        directory = defaultDirectory();
        if (!chooseInstallDirectory(directory)) return false;
        std::wstring error = validateInstallDirectory(directory);
        if (!error.empty()) return installError(error);
    }
    bool newFolder = GetFileAttributesW(directory.c_str()) == INVALID_FILE_ATTRIBUTES;
    int created = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS)
        return installError(L"无法创建安装文件夹：\n" + directory);

    const std::wstring target = joinPath(directory, programName);
    bool hadExe = regularFile(target);
    bool replace = !samePath(getExePath(), target) && !(hadExe && sameInstalledBuild(target));
    wchar_t staged[MAX_PATH]{}, backup[MAX_PATH]{};
    bool copied = false;
    bool useTask = samePath(directory, defaultDirectory());
    bool taskCreated = false;
    bool schemeAttempted = false;
    bool registryAttempted = false;
    std::wstring failure;
    if (replace) {
        if (GetTempFileNameW(directory.c_str(), L"cgs", 0, staged) &&
            CopyFileW(getExePath().c_str(), staged, FALSE)) {
            if (!hadExe)
                copied = MoveFileW(staged, target.c_str()) != FALSE;
            else if (GetTempFileNameW(directory.c_str(), L"cgb", 0, backup) &&
                     CopyFileW(target.c_str(), backup, FALSE))
                copied = MoveFileExW(staged, target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        }
        if (!copied) failure = L"无法复制程序。请先退出正在运行的插件，并检查文件夹权限。";
    }
    if (failure.empty() && useTask) {
        taskCreated = registerScheduleTask(target);
        if (!taskCreated) failure = L"无法注册 Windows 计划任务。";
    }
    if (failure.empty()) {
        schemeAttempted = true;
        if (!registerUrlScheme(target)) failure = L"无法注册椰羊网页唤起协议。";
    }
    if (failure.empty()) {
        registryAttempted = true;
        if (!registerUninstallItem(directory)) failure = L"无法保存安装位置。";
    }
    if (staged[0]) DeleteFileW(staged);
    if (!failure.empty()) {
        if (fresh) {
            if (registryAttempted) removeUninstallItem();
            if (schemeAttempted) removeScheme();
            if (taskCreated) RunCmd(L"schtasks /delete /tn \"椰羊插件（以管理员身份运行）\" /F");
        }
        if (copied && hadExe && !MoveFileExW(backup, target.c_str(), MOVEFILE_REPLACE_EXISTING))
            return installError(failure + L"\n上一版本保留在：\n" + backup);
        if (copied && !hadExe) DeleteFileW(target.c_str());
        if (backup[0]) DeleteFileW(backup);
        if (newFolder) RemoveDirectoryW(directory.c_str());
        return installError(failure + L"\n" + directory);
    }
    if (backup[0]) DeleteFileW(backup);
    return true;
}

static bool componentName(const std::wstring &name) {
    return !name.empty() && name.size() < MAX_PATH && name != L"." && name != L".." &&
           name.back() != L'.' && name.back() != L' ' &&
           name.find_first_of(L"\\/:*?\"<>|") == name.npos;
}

void recordInstalledComponent(const std::wstring &path) {
    std::wstring directory = registeredDirectory();
    std::wstring name = path.substr(path.find_last_of(L'\\') + 1);
    if (directory.empty() || !componentName(name) || !samePath(path, joinPath(directory, name))) return;
    HKEY key;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, (std::wstring(uninstallKey) + L"\\Components").c_str(),
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        DWORD installed = 1;
        RegSetValueExW(key, name.c_str(), 0, REG_DWORD, (BYTE *)&installed, sizeof(installed));
        RegCloseKey(key);
    }
}

static bool removeInstalledFile(const std::wstring &path) {
    if (DeleteFileW(path.c_str())) return true;
    return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
}

boolean runUninstall() {
    InstallationLock lock;
    if (!lock.acquired) return installError(L"另一个安装或卸载程序正在运行。");
    std::wstring installDir = existingInstallation();
    if (installDir.empty()) return installError(L"找不到已登记的安装位置。");
    HKEY key;
    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, (std::wstring(uninstallKey) + L"\\Components").c_str(),
                                  0, KEY_QUERY_VALUE, &key);
    if (status == ERROR_SUCCESS) {
        for (DWORD i = 0;; ++i) {
            wchar_t name[MAX_PATH];
            DWORD length = MAX_PATH;
            status = RegEnumValueW(key, i, name, &length, nullptr, nullptr, nullptr, nullptr);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS || !componentName(name) ||
                !removeInstalledFile(joinPath(installDir, name))) {
                RegCloseKey(key);
                return installError(L"无法清理已安装的组件，请退出插件后重试。");
            }
        }
        RegCloseKey(key);
    } else if (status != ERROR_FILE_NOT_FOUND) return installError(L"无法读取组件记录。");
    if (!removeInstalledFile(joinPath(installDir, programName)))
        return installError(L"无法删除插件，请先退出运行中的插件。");
    // RemoveDirectory only removes empty directories. User files and untracked legacy files survive.
    RemoveDirectoryW(installDir.c_str());
    if (samePath(installDir, defaultDirectory()))
        RunCmd(L"schtasks /delete /tn \"椰羊插件（以管理员身份运行）\" /F");
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
    const std::wstring copyPath = pathW + L"cocogoat-control-uninstaller.exe";
    if (samePath(exePathW, copyPath)) {
        askUninstall();
        deleteSelf();
    } else {
        // copy exe to TEMP path
        if (!CopyFileW(exePathW.c_str(), copyPath.c_str(), FALSE)) {
            DWORD err = GetLastError();
            MessageBoxW(NULL, (ToWString("卸载程序启动失败：") + std::to_wstring(err)).c_str(), ToWString("错误").c_str(), MB_OK | MB_ICONERROR);
            return;
        }
        // run copyed exe
        elevate("--uninstall", copyPath);
    }
}

// Negative means continue to the desktop plugin; otherwise return the installer exit code.
int autoElevate(PSTR lpCmdLine) {
    std::string cmd = lpCmdLine;
    bool fromUrl = cmd.find("://") != cmd.npos;
    bool stay = !fromUrl && cmd.find("--stay") != cmd.npos;
    bool installOnly = !fromUrl && cmd.find("--install") != cmd.npos;
    bool uninstall = !fromUrl && cmd.find("--uninstall") != cmd.npos;
    bool elevated = checkIfElevated();
    if (uninstall) {
        if (!elevated) return elevate(lpCmdLine) ? 0 : ERROR_CANCELLED;
        if (stay) askUninstall();
        else preUninstall();
        return 0;
    }
    if (stay && !installOnly) return elevated ? -1 : (elevate(lpCmdLine) ? 0 : ERROR_CANCELLED);

    std::wstring directory = existingInstallation();
    std::wstring target = joinPath(directory, programName);
    bool installedCopy = samePath(getExePath(), target);
    if (!installOnly && installedCopy && elevated) return -1;
    if (!elevated) {
        if (!installOnly && !target.empty() && (installedCopy || sameInstalledBuild(target))) {
            // Only the Program Files installation gets an unattended highest-privilege task.
            // Custom roots may be writable by an ordinary user; launch them with the normal UAC prompt.
            if (samePath(directory, defaultDirectory()) && (!installedCopy || queryScheduleTask()) &&
                runScheduleTask(lpCmdLine)) return 0;
            return elevate(lpCmdLine, target) ? 0 : ERROR_CANCELLED;
        }
        return elevate(lpCmdLine) ? 0 : ERROR_CANCELLED;
    }
    if (!runInstaller(directory)) return GetLastError() == ERROR_CANCELLED ? ERROR_INSTALL_USEREXIT : ERROR_INSTALL_FAILURE;
    if (installOnly) return 0;
    return elevate(lpCmdLine, joinPath(directory, programName)) ? 0 : ERROR_INSTALL_FAILURE;
}
