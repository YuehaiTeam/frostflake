#pragma once

#define IDD_INSTALL 300
#define IDI_INSTALL_LOGO 301
#define IDC_INSTALL_HEADER 302
#define IDC_INSTALL_PATH 303
#define IDC_INSTALL_BROWSE 304
#define IDC_INSTALL_DESTINATION 305

#ifndef RC_INVOKED
#include <string>

bool chooseInstallDirectory(std::wstring &directory);
std::wstring installDirectoryForSelection(std::wstring path);
std::wstring validateInstallDirectory(std::wstring &directory);
#endif
