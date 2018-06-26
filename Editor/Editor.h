#pragma once

#include "stdafx.h"

struct Char {
	float x;
	float y;
	float width;
	wchar_t wchar;
};

struct EditorOptions {
};

EditorOptions default_editoroptions();

class Editor {
private:
	EditorOptions options;
	float char_height;
	std::vector<Char> chars;
	
	IDWriteFactory* factory;
	IDWriteTextFormat* text_format;

	Char create_char(wchar_t character);
public:
	Editor(IDWriteFactory* factory, IDWriteTextFormat* text_format, const EditorOptions& options = default_editoroptions());
	void initialize();

	void set_text(const std::wstring& str);

	void render(ID2D1HwndRenderTarget* rt);
};