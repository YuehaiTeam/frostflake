#include "tray.h"
#include <windows.h>
Tray::Tray(void) {
    ZeroMemory(&m_NotifyIconData, sizeof(m_NotifyIconData));
    m_hIcon = NULL;
}

Tray::~Tray(void) {
    Delete();
}

bool Tray::Create(HWND hOwner,
                  HICON hIcon,
                  LPCWSTR lpToolTip,
                  UINT uCallbackMessage,
                  UINT uId,
                  UINT uFlags) {
    m_NotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    m_NotifyIconData.hWnd = hOwner;
    m_NotifyIconData.uID = uId;
    m_NotifyIconData.uFlags = uFlags; // NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_NotifyIconData.uCallbackMessage = uCallbackMessage;
    m_NotifyIconData.hIcon = hIcon;
    m_hIcon = hIcon;
    wcscpy_s(m_NotifyIconData.szTip, 128, lpToolTip);

    if (!Shell_NotifyIconW(NIM_ADD, &m_NotifyIconData)) {
        return false;
    }

    m_NotifyIconData.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &m_NotifyIconData);
}

bool Tray::Update() {
    m_NotifyIconData.hIcon = m_hIcon;
    BOOL bRet = Shell_NotifyIconW(NIM_MODIFY, &m_NotifyIconData);
    if (FALSE == bRet) {
        bRet = Shell_NotifyIconW(NIM_ADD, &m_NotifyIconData);
    }

    return TRUE == bRet;
}

bool Tray::Delete() {
    return (TRUE == Shell_NotifyIconW(NIM_DELETE, &m_NotifyIconData));
}

void Tray::Modify(LPCWSTR lpToolTip) {
    wcscpy_s(m_NotifyIconData.szTip, 128, lpToolTip);
    m_NotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    m_NotifyIconData.uFlags |= NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_NotifyIconData);
}

void Tray::Modify(HICON hIcon) {
    m_NotifyIconData.hIcon = hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &m_NotifyIconData);
}

void Tray::Modify(LPCWSTR lpToolTip, HICON hIcon) {
    wcscpy_s(m_NotifyIconData.szTip, 128, lpToolTip);
    m_NotifyIconData.hIcon = hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &m_NotifyIconData);
}

bool Tray::ShowBalloon(LPCWSTR lpBalloonTitle, LPCWSTR lpBalloonMsg, DWORD dwIcon, UINT nTimeOut) {
    m_NotifyIconData.dwInfoFlags = dwIcon;
    m_NotifyIconData.uFlags |= NIF_INFO;
    m_NotifyIconData.uTimeout = nTimeOut;
    // Set the balloon title
    wcscpy_s(m_NotifyIconData.szInfoTitle, 64, lpBalloonTitle);

    // Set balloon message
    wcscpy_s(m_NotifyIconData.szInfo, 256, lpBalloonMsg);

    // Show balloon....
    return TRUE == Shell_NotifyIconW(NIM_MODIFY, &m_NotifyIconData);
}