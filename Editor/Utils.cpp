#include "stdafx.h"
#include "Utils.h"

wchar_t* char_to_wchar(const char* text) {
	size_t size = strlen(text) + 1;
	wchar_t* wchar = new wchar_t[size];
	size_t ret;
	mbstowcs_s(&ret, wchar, size, text, _TRUNCATE);
	return wchar;
}
