#pragma once

#include <unicode/unistr.h>

const wchar_t* char_to_wchar(const char* text);

template <typename ...Args>
inline wchar_t* rswprintf(const wchar_t* format, Args const& ...args) {
	wchar_t buf[1024];
	swprintf_s(buf, format, args...);

	return buf;
}