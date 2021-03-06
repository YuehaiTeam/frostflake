#pragma once
#pragma warning (disable : 26495)

#include "Control.h"

class Button : public Control
{
public:
	Button();
	Button(Control*, std::wstring, RECT);
	Button(Control*, std::wstring, int = 0, int = 0);

	void setOnClick(f_onClick);
	void setOnDoubleClick(f_onDoubleClick);
	void setAlign(Align);
	void setFlat(bool);
	Align getAlign();
protected:
	f_onClick mOnClick = nullptr;
	f_onDoubleClick mOnDoubleClick = nullptr;

	Button(std::wstring, int = 0, int = 0);
	virtual LRESULT execute(UINT, WPARAM, LPARAM);
};