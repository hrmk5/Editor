#pragma once

#include "stdafx.h"

struct Char {
	wchar_t ch;
};

class Editor {
private:
	std::vector<Char> chars;
public:
	Editor();
	void render(ID2D1HwndRenderTarget* rt);
};