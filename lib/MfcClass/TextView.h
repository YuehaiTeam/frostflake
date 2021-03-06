#pragma once
#pragma warning (disable : 26495)

#include "Control.h"

class TextView : public Control
{
public:
	TextView();
	TextView(Control*, std::wstring, RECT);
	TextView(Control*, std::wstring, int = 0, int = 0);

	void setOnClick(f_onClick);
	void setOnDoubleClick(f_onDoubleClick);
	void setEllipsis(bool);
	bool isEllipsisOn();
	void setAutosize(bool);
	void setAlign(Align);
	virtual void setText(std::wstring);
	virtual void setSize(int, int);
	virtual void setRect(RECT);
	Align getAlign();
protected:
	f_onClick mOnClick = nullptr;
	f_onDoubleClick mOnDoubleClick = nullptr;
	bool mAutoSize = false;

	void autosize();
	virtual LRESULT execute(UINT, WPARAM, LPARAM);
	virtual LRESULT drawctl(UINT, WPARAM, LPARAM);
};