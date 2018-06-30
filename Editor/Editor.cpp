#include "stdafx.h"
#include "Editor.h"
#include "Utils.h"

using namespace D2D1;

Timer::Timer(UINT id, UINT elapse, const TimerFunc & func) :
	id(id),
	elapse(elapse),
	func(func),
	enabled(true) {
}

EditorOptions DefaultEditorOptions() {
	EditorOptions options;
	options.cursorBlinkRateMsec = 500;
	options.cursorWidth = 2;
	options.fontName = L"Yu Gothic";
	options.fontSize = 17.0f;

	return options;
}

Editor::Editor(HWND hwnd, IDWriteFactory* factory, const EditorOptions& options) :
	hwnd(hwnd),
	factory(factory),
	textFormat(nullptr),
	options(options),
	visibleCursor(true),
	compositionStringLength(-1),
	// カーソルを点滅させるタイマー
	cursorBlinkTimer(ID_CURSOR_BLINK_TIMER, options.cursorBlinkRateMsec, std::bind(&Editor::ToggleCursorVisible, this)) {
}

Editor::~Editor() {
	textFormat->Release();
}

void Editor::Initialize() {
	// TextFormat を作成
	HRESULT hr = factory->CreateTextFormat(
		options.fontName.c_str(),
		nullptr,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		options.fontSize,
		L"ja-JP",
		&textFormat);

	if (FAILED(hr)) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::string fontName = converter.to_bytes(options.fontName);
		throw EditorException("Unable to create text format. font: '"
			+ fontName + "', font size: " + std::to_string(options.fontSize));
	}

	textFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, options.fontSize / 0.8f, options.fontSize);

	// 文字の高さを測定
	IDWriteTextLayout* layout;
	// IDWriteTextLayout を作成
	auto testText = L"abcdefghijklnmopqrstuvwxyzABCDEFGHIJKLNMOPQRSTUVWXYZあいうえお漢字汉字繁體字";
	auto result = factory->CreateTextLayout(testText, static_cast<UINT32>(wcslen(testText)), textFormat, 100000000, 1000, &layout);
	if (SUCCEEDED(result)) {
		DWRITE_TEXT_METRICS metrics;
		layout->GetMetrics(&metrics);
		charHeight = metrics.height;

		// IDWriteTextLayout を破棄
		layout->Release();
	}
	
	// タイマーの設定
	timers.push_back(&cursorBlinkTimer);
}

void Editor::SetText(const std::wstring& str) {
	chars.clear();

	for (auto& ch : str) {
		auto character = CreateChar(ch);
		chars.push_back(character);
	}
}

void Editor::AppendChar(wchar_t wchar) {
	auto character = CreateChar(wchar);
	chars.push_back(character);

	selection.start = static_cast<int>(chars.size());
	selection.end = static_cast<int>(chars.size());
}

Char Editor::CreateChar(wchar_t character) {
	Char ch;
	ch.wchar = character;
	
	// 空白文字でなかったら文字の幅を取得
	IDWriteTextLayout* layout;
	// IDWriteTextLayout を作成
	auto result = factory->CreateTextLayout(&character, 1, textFormat, 100, 100, &layout);
	if (SUCCEEDED(result)) {
		DWRITE_TEXT_METRICS metrics;
		layout->GetMetrics(&metrics);
		// widthIncludingTrailingWhitespace は空白文字の幅も返す
		ch.width = metrics.widthIncludingTrailingWhitespace;

		// IDWriteTextLayout を破棄
		layout->Release();
	}

	return ch;
}

void Editor::ToggleCursorVisible() {
	visibleCursor = !visibleCursor;
}

void Editor::Render(ID2D1HwndRenderTarget* rt) {
	ID2D1SolidColorBrush* brush;
	HRESULT hr = rt->CreateSolidColorBrush(ColorF(ColorF::Black), &brush);

	if (SUCCEEDED(hr)) {
		float x = 0;
		float y = 0;
		std::size_t i = 0;
		for (auto& character : chars) {
			// 文字を描画
			rt->DrawText(
				&character.wchar,
				1,
				textFormat,
				&RectF(x, y, x + character.width, y + charHeight),
				brush);

			character.x = x;
			character.y = y;
			x += character.width;

			// 改行だったら y 座標を更新する
			if (character.wchar == '\n') {
				x = 0;
				y += charHeight;
			}
		}

		// カーソルを描画
		if (visibleCursor) {
			if (chars.empty()) {
				// 文字を描画していない場合は左上に描画
				RenderCursor(rt, 0, 0, brush);
			} else if (selection.end >= static_cast<signed int>(chars.size())) {
				// 末尾を選択している場合
				RenderCursor(rt, x, y, brush);
			} else {
				auto character = chars[selection.end];
				RenderCursor(rt, character.x, character.y, brush);
			}
		}
	}
}

void Editor::RenderCursor(ID2D1HwndRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
	rt->FillRectangle(
		RectF(x, y, x + options.cursorWidth, y + charHeight),
		brush);
}

void Editor::OnChar(wchar_t character) {
	if (character == '\b') {
		// バックスペースキーが押された場合はカーソルの前の文字を削除する
		if (selection.end > 0) {
			chars.erase(chars.begin() + selection.end - 1);
			selection.end--;
		}
	} else if (character == '\r') {
		// エンターキーを押すと \r が入力されるので \n に置き換えて追加
		//AppendChar('\n');
		chars.insert(chars.begin() + selection.end, CreateChar('\n'));
		selection.end++;
	} else {
		//AppendChar(character);
		chars.insert(chars.begin() + selection.end, CreateChar(character));
		selection.end++;
	}
}

void Editor::OnIMEComposition() {
	auto imc = ImmGetContext(hwnd);
	if (!imc) {
		std::wcout << L"Unable to get imm context" << std::endl;
		return;
	}

	// 未確定文字列を取得
	auto bytes = ImmGetCompositionString(imc, GCS_COMPSTR, NULL, 0);
	auto size = bytes / sizeof(wchar_t);

	wchar_t* buf = new wchar_t[size];
	auto result = ImmGetCompositionString(imc, GCS_COMPSTR, buf, bytes);
	if (result == IMM_ERROR_NODATA) {
		MessageBox(hwnd, rswprintf(L"エラーが発生しました: IMM_ERROR_NODATA (%d)", result).c_str(), L"エラー", MB_OK | MB_ICONERROR);
		return;
	} else if (result == IMM_ERROR_GENERAL) {
		MessageBox(hwnd, rswprintf(L"エラーが発生しました: IMM_ERROR_GENERAL (%d)", result).c_str(), L"エラー", MB_OK | MB_ICONERROR);
		return;
	}

	std::wcout << rswprintf(L"ajioefjjあは%d", 6) << std::endl;

	// 以前挿入した未確定文字列を削除
	if (compositionStringLength != -1) {
		chars.erase(chars.begin() + selection.end, chars.begin() + selection.end  + compositionStringLength);
	}

	// 未確定文字列を挿入
	auto str = std::wstring(buf, size);

	int i = 0;
	for (auto& c : str) {
		auto ch = CreateChar(c);
		chars.insert(chars.begin() + selection.end + i, ch);
		i++;
	}

	// 未確定文字列の長さを保存
	compositionStringLength = static_cast<int>(str.length());

	ImmReleaseContext(hwnd, imc);
	delete[] buf;
}

void Editor::OnIMEStartComposition() {
}

void Editor::OnIMEEndComposition() {
	compositionStringLength = -1;
}

void Editor::OnKeyDown(int keyCode) {
	// カーソルを表示させて点滅を停止する
	cursorBlinkTimer.enabled = false;
	visibleCursor = true;

	switch (keyCode) {
	case VK_LEFT:
		if (selection.end > 0) {
			selection.end -= 1;
			selection.start = selection.end;
		}
		break;
	case VK_RIGHT:
		if (selection.end < chars.size()) {
			selection.end += 1;
			selection.start = selection.end;
		}
		break;
	}
}

void Editor::OnKeyUp(int keyCode) {
	cursorBlinkTimer.enabled = true;
}
