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
	bool enabled;

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
	int cursorBlinkRateMsec; // �J�[�\���̓_�ő��x (�~���b)
	float cursorWidth; // �J�[�\���̕�
	std::wstring fontName; // �t�H���g�̖��O
	float fontSize; // �t�H���g�T�C�Y
};

EditorOptions DefaultEditorOptions();

class Editor {
private:
	static constexpr int ID_CURSOR_BLINK_TIMER = 1;

	Timer cursorBlinkTimer;

	EditorOptions options;
	float charHeight;
	std::vector<Char> chars;
	Selection selection;
	int compositionStringLength;

	bool visibleCursor;
	
	HWND hwnd;
	IDWriteFactory* factory;
	IDWriteTextFormat* textFormat;

	Char CreateChar(wchar_t character);

	void ToggleCursorVisible();
public:
	std::vector<Timer*> timers;

	Editor(HWND hwnd, IDWriteFactory* factory, const EditorOptions& options = DefaultEditorOptions());
	~Editor();
	void Initialize();

	void SetText(const std::wstring& str);
	void AppendChar(wchar_t wchar);
	int findIndexByPosition(float x, float y);

	void Render(ID2D1HwndRenderTarget* rt);
	void RenderCursor(ID2D1HwndRenderTarget* rt, float x, float y, ID2D1Brush* brush);
	void OnChar(wchar_t character);
	void OnIMEComposition();
	void OnIMEStartComposition();
	void OnIMEEndComposition();
	void OnKeyDown(int keyCode);
	void OnKeyUp(int keyCode);
	void OnLButtonDown(float x, float y);
};