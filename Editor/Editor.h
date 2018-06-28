#pragma once

#include "stdafx.h"

class EditorException : public std::exception {
private:
	std::string message;
public:
	EditorException(const std::string& message) : message(message) {}
	const char* what() const noexcept { return message.c_str(); }
};

using TimerFunc = std::function<void()>;

class Timer {
public:
	UINT id;
	UINT elapse;
	TimerFunc func;

	Timer(UINT id, UINT elapse, const TimerFunc& func);
};

struct Selection {
	int start;
	int end;
};

struct Char {
	float x;
	float y;
	float width;
	wchar_t wchar;
};

struct EditorOptions {
	int cursor_blink_rate_msec; // カーソルの点滅速度 (ミリ秒)
	float cursor_width; // カーソルの幅
	std::wstring font_name; // フォントの名前
	float font_size; // フォントサイズ
};

EditorOptions default_editoroptions();

class Editor {
private:
	static constexpr int ID_CURSOR_BLINK_TIMER = 1;

	EditorOptions options;
	float char_height;
	std::vector<Char> chars;
	Selection selection;

	bool visible_cursor;
	
	HWND hwnd;
	IDWriteFactory* factory;
	IDWriteTextFormat* text_format;

	DWRITE_FONT_METRICS get_font_metrics();

	Char create_char(wchar_t character);

	void toggle_cursor_visible();
public:
	std::vector<Timer> timers;

	Editor(HWND hwnd, IDWriteFactory* factory, const EditorOptions& options = default_editoroptions());
	~Editor();
	void initialize();

	void set_text(const std::wstring& str);
	void append_char(wchar_t wchar);

	void render(ID2D1HwndRenderTarget* rt);
	void render_cursor(ID2D1HwndRenderTarget* rt, float x, float y, ID2D1Brush* brush);
	void on_char(wchar_t character);
};