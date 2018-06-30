#include "stdafx.h"
#include "Utils.h"

const wchar_t* char_to_wchar(const char* src) {
	icu::UnicodeString ustr(src, strlen(src), "shift_jis");

	std::wstring dest;
	dest.assign(ustr.getBuffer(), ustr.getBuffer() + ustr.length());

	return dest.c_str();
}
