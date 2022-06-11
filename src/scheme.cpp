#include <string>
#include <windows.h>

std::wstring ToWString(const std::string &str);

boolean registerUrlScheme(std::wstring path) {
    // register protocol using registry
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\cocogoat-control", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)L"URL: cocogoat-control Protocol", sizeof(L"URL: cocogoat-control Protocol")) != ERROR_SUCCESS) {
        return false;
    }
    if (RegSetValueExW(hKey, L"URL Protocol", 0, REG_SZ, (BYTE *)"", sizeof("")) != ERROR_SUCCESS) {
        return false;
    }
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        return false;
    }
    // register open command
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\cocogoat-control\\shell\\open\\command", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    WCHAR cmd[MAX_PATH];
    wsprintfW(cmd, L"\"%s\" \"%%1\"", path.c_str());
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)cmd, (wcslen(cmd) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        return false;
    }
    // icon
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\cocogoat-control\\DefaultIcon", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    wsprintfW(cmd, L"\"%s\",0", path.c_str());
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE *)cmd, (wcslen(cmd) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        return false;
    }
    // FriendlyAppName
    std::wstring friendlyAppName = ToWString("椰羊·霜华插件");
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\cocogoat-control\\shell\\open", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    if (RegSetValueExW(hKey, L"FriendlyAppName", 0, REG_SZ, (BYTE *)friendlyAppName.c_str(), (wcslen(friendlyAppName.c_str()) + 1) * sizeof(WCHAR)) != ERROR_SUCCESS) {
        return false;
    }
    if (RegCloseKey(hKey) != ERROR_SUCCESS) {
        return false;
    }

    return true;
}

boolean removeScheme() {
    // regdeletetree
    if (RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\cocogoat-control") != ERROR_SUCCESS) {
        return false;
    }
    return true;
}