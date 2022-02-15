#pragma once

#include <codecvt>

#include "Window.h"
#ifdef _OGLWINDOW
#include "OGLWindow.h"
#endif
#ifndef _MINIMAL
#include "TextView.h"
#include "Button.h"
#endif

#define Invisible (HBRUSH)GetStockObject(HOLLOW_BRUSH)

typedef std::wstring File;
typedef std::vector<File> FileList;

// Convert from string to wstring
std::wstring ToWString(const std::string& str);

// Convert from wstring to string
std::string ToString(const std::wstring& wstr);

// Get COLORREF from r, g, b values
// Example: 255, 255, 255
COLORREF Rgb(BYTE r, BYTE g, BYTE b);

// Get COLORREF from Hex dword
// Example: 0xFFFFFF
COLORREF Hex(DWORD h);

// Get COLORREF from Hex string
// Example: "#FFFFFF"
COLORREF Hex(std::wstring hex);

// Get Solid Brush from Color
HBRUSH Brush(COLORREF ref);

// Load Bitmap from file
HBITMAP Bitmap(std::wstring dir);

// Load Bitmap from resource
HBITMAP Bitmap(WORD id);

// Load Bitmap from resource
HBITMAP Bitmap(HINSTANCE hInstance, WORD id);

// Load Icon from file
HICON Icon(std::wstring dir);

// Load Icon from resource
HICON Icon(WORD id);

// Load Icon from resource
HICON Icon(HINSTANCE hInstance, WORD id);

// Load Menu from resource
HMENU Menu(WORD id);

// Load Menu from resource
HMENU Menu(HINSTANCE hInstance, WORD id);

DWORD Async(LPTHREAD_START_ROUTINE, LPVOID);

int Dialog(std::wstring text, std::wstring title = L"Dialog MessageBox", DWORD flags = MB_SYSTEMMODAL | MB_OK);

File OpenFileDialog(UINT size, const COMDLG_FILTERSPEC* c_rgFileTypes);

FileList OpenMultiFileDialog(UINT size, const COMDLG_FILTERSPEC* c_rgFileTypes);

File SaveFileDialog(UINT size, const COMDLG_FILTERSPEC* c_rgFileTypes, std::wstring fName = L"");

File SelectFolderDialog();