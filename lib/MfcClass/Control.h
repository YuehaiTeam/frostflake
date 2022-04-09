#pragma once
#pragma warning(disable : 26495)

#ifndef _LEGACY
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define _AFXDLL
#include <SDKDDKVer.h>
#include <afxwin.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Control;
class Window;
#ifdef _OGLWINDOW
class OGLWindow;
#endif
class TextView;
class PictureBox;
class EditText;
class MultiEditText;
class TabControl;
class GroupBox;
class Button;
class ImageButton;
class CheckBox;
class RadioButton;
class ComboBox;
class FixedComboBox;
class TrackBar;
class ProgressBar;
class ListBox;
class TreeView;

enum MouseKeys {
    LeftButton = WM_LBUTTONDOWN,
    RightButton = WM_RBUTTONDOWN,
    CenterButton = WM_MBUTTONDOWN,
    OtherButton = WM_XBUTTONDOWN,
};

using f_onRender = void (*)(long long);
using f_onHover = void (*)(Control *, bool);
using f_onFocus = void (*)(Control *, bool);
using f_onClick = void (*)(Control *, MouseKeys);
using f_onDoubleClick = void (*)(Control *, MouseKeys);
using f_onDisplayMenu = HMENU (*)(Control *);
using f_onMenuClick = void (*)(Control *, int);
using f_onSelectIndex = void (*)(Control *, int);
using f_onSelectItem = void (*)(Control *, LPARAM);
using f_onCheckChange = void (*)(Control *, bool);
using f_onCustomMessage = void (*)(Control *, int, WPARAM, LPARAM);

enum Align {
    Undefined,
    Left,
    Center,
    Right
};

enum Orientation {
    Vertical,
    Horizontal
};

class Control {
public:
    ~Control();
    Control *child(int);
    bool create();
    void destroy();
    virtual void show();
    void hide();
    void enable();
    void disable();
    void redraw();
    void setGlobalSleepTime(int);
    void setOnRender(f_onRender);
    void setOnHover(f_onHover);
    void setOnDisplayMenu(f_onDisplayMenu);
    void setOnMenuClick(f_onMenuClick);
    void setVisibile(bool);
    void setEnabled(bool);
    void setLocation(int, int);
    void setGlobalIcon(HICON);
    void setContextMenu(HMENU);
    void setTextSize(int);
    void setItalic(bool);
    void setUnderline(bool);
    void setStrikeout(bool);
    void setFont(std::wstring);
    virtual void setTextColor(COLORREF);
    void setBackground(HBRUSH);
    void setOldStyle(bool);
    POINT getCursorPos();
    POINT getCursorScreenPos();
    HWND hwnd();
    HICON getGlobalIcon();
    int getX();
    int getY();
    int getWidth();
    int getHeight();
    int getClientWidth();
    int getClientHeight();
    RECT getRect();
    RECT getClientRect();
    bool hasFlag(DWORD);
    void appendFlag(DWORD);
    void removeFlag(DWORD);
    bool hasFlagEx(DWORD);
    void appendFlagEx(DWORD);
    void removeFlagEx(DWORD);
    virtual void setText(std::wstring);
    virtual void setSize(int, int);
    virtual void setRect(RECT);
    virtual void setRectRaw(RECT);
    virtual std::wstring getText();
    bool isVisible();
    void join();
    void ljoin();
    static HINSTANCE hinstance();
    void preMessage(void (*fun)(MSG &));

protected:
    friend class Window;
    virtual LRESULT execute(UINT, WPARAM, LPARAM);
    virtual LRESULT cnotify(UINT, WPARAM, LPARAM);
    virtual LRESULT drawctl(UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK SubWndProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static thread_local std::wstring mClassName;
    static thread_local WNDCLASSEX mWndClass;
    static thread_local bool mLoopAlive;
    static thread_local std::unordered_map<HWND, Control *> mControls;
    static thread_local int mLapse;
    static thread_local HICON mIcon;
    static thread_local f_onRender mOnRender;
    f_onHover mOnHover = nullptr;
    f_onDisplayMenu mOnDisplayMenu = nullptr;
    f_onMenuClick mOnMenuClick = nullptr;
    f_onCustomMessage mOnCustomMessage = nullptr;
    bool mCreated = false;
    bool mEnabled = true;
    bool mOldStyle = false;
    std::wstring mType;
    std::wstring mText;
    DWORD mStyle = NULL;
    DWORD mExStyle = NULL;
    HCURSOR mCursor;
    COLORREF mFtColor = 0x0;
    HBRUSH mBkBrush = nullptr;
    int mX = CW_USEDEFAULT, mY = 0;
    int mWidth, mHeight;
    RECT mClientRect{0};
    HWND mHwnd;
    LOGFONT mLogFont;
    HFONT mFont = nullptr;
    HMENU mId = NULL;
    HMENU mContextMenu = nullptr;
    Control *mParent = nullptr;
    std::vector<Control *> mChildrens;
    unsigned int mDpi=0;

    Control();
    Control(Control *, std::wstring, int, int);
    Control(Control *, DWORD, std::wstring, int, int);
    bool globalClassInit(DWORD = 0);
    bool cmnControlInit(DWORD);
    void eraseWithChilds(Control *);
    static bool ctlExists(DWORD);
    void updateClientRect();
    void updateWindowRect();
    void updateFont();
    void showContextMenu(HWND);
    void handleDpiChange(unsigned long newDpi,RECT* suggestedRect);
    std::wstring pullWindowText();
};