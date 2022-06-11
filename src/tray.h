#pragma once
#include <windows.h>
class Tray {
public:
    Tray(void);
    ~Tray(void);

public:
    bool Create(HWND hWnd,
                HICON hIcon,
                LPCWSTR lpNotifyText,
                UINT uCallbackMessage,
                UINT uId,
                UINT uFlags = (NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP));

    bool Update();
    bool Delete();
    void Modify(HICON hIcon);
    void Modify(LPCWSTR lpToolTip);
    void Modify(LPCWSTR lpToolTip, HICON hIcon);
    bool ShowBalloon(LPCWSTR lpBalloonTitle, LPCWSTR lpBalloonMsg, DWORD dwIcon = NIIF_NONE, UINT nTimeOut = 10);

protected:
    NOTIFYICONDATAW m_NotifyIconData;
    HICON m_hIcon;
};