#pragma once

#define IDI_COCOGOAT 1
#define IDD_INSTALL 300
#define IDI_INSTALL_LOGO 301
#define IDC_INSTALL_HEADER 302
#define IDC_INSTALL_PATH 303
#define IDC_INSTALL_BROWSE 304
#define IDC_INSTALL_DESTINATION 305
#define IDC_INSTALL_LABEL 306
#define IDC_INSTALL_PROGRESS 307
#define IDC_INSTALL_RESULT 308

#ifndef RC_INVOKED
#include <functional>
#include <string>

enum class InstallStep { directory, files, scheduledTask, protocol, registration, done };
using InstallProgress = std::function<void(int percent, InstallStep step)>;
bool showInstallDialog(std::wstring &directory, bool fresh);
std::wstring installFiles(const std::wstring &directory, bool fresh, const InstallProgress &progress);
std::wstring installDirectoryForSelection(std::wstring path);
std::wstring validateInstallDirectory(std::wstring &directory);
#endif
