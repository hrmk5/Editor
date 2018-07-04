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
	compositionStringLength(-1),
	compositionTextPos(-1),
	selectionStart(-1),
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

void Editor::DeleteSelection() {
	chars.erase(
		chars.begin() + (selection.start < selection.end ? selection.start : selection.end),
		chars.begin() + (selection.start < selection.end ? selection.end : selection.start));

	MoveCaret((selection.start < selection.end ? selection.end : selection.start) - abs(selection.end - selection.start));
}

int Editor::findIndexByPosition(float x, float y) {
	int index = 0;
	for (auto&& character : chars) {
		// 同じ行かどうか
		if (y >= character.y && y <= character.y + charHeight) {
			if (x >= character.x && x <= character.x + character.width / 2) {
				// 左半分だった場合
				return index;
			} else if (x >= character.x + character.width / 2 && x <= character.x + character.width) {
				// 右半分だった場合
				return index + 1;
			}
		}

		index++;
	}

	return -1;
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
	caret.visible = !caret.visible;
}

void Editor::MoveCaret(int index, bool isSelectRange) {
	caret.index = index;

	if (isSelectRange) {
		selection.end = index;
	} else {
		selection.start = index;
		selection.end = index;
	}
}

void Editor::Render(ID2D1HwndRenderTarget* rt) {
	if (dragged) {
		// カーソルの座標を取得
		POINT pos;
		GetCursorPos(&pos);

		// スクリーン座標なのでクライアント座標に変換
		ScreenToClient(hwnd, &pos);

		// カーソルの位置の文字のインデックスを検索
		int index = findIndexByPosition(pos.x, pos.y);
		if (index != -1) {
			if (index > selection.start) {
				selection.start = selectionStartWhileDrag;
				selection.end = index;
			} else if (index < selection.start) {
				selection.end = selectionStartWhileDrag;
				selection.start = index;
			}
		}
	}

	ID2D1SolidColorBrush* brush;
	ID2D1SolidColorBrush* compositionCharBrush = nullptr;
	ID2D1SolidColorBrush* selectionBrush = nullptr;
	HRESULT hr = rt->CreateSolidColorBrush(ColorF(ColorF::Black), &brush);

	if (SUCCEEDED(hr)) {
		hr = rt->CreateSolidColorBrush(ColorF(ColorF::LightGray), &compositionCharBrush);
	}

	if (SUCCEEDED(hr)) {
		hr = rt->CreateSolidColorBrush(ColorF(ColorF::CornflowerBlue), &selectionBrush);
	}

	if (SUCCEEDED(hr)) {
		float x = 0;
		float y = 0;
		std::size_t i = 0;
		for (auto& character : chars) {
			// 未確定文字列を描画
			if (compositionTextPos != -1 && i == compositionTextPos) {
				RenderCompositionText(rt, brush, compositionCharBrush, &x, &y);
			}

			// 選択範囲を描画
			if ((selection.start < selection.end && i >= selection.start && i < selection.end) ||
				(selection.start > selection.end && i < selection.start && i >= selection.end)) {
				rt->FillRectangle(
					RectF(x, y, x + character.width + 1, y + charHeight),
					selectionBrush);
			}

			// 文字を描画
			RenderChar(rt, &character, &x, &y, brush);

			i++;
		}

		// 未確定文字列が末尾にあった場合
		if (compositionTextPos == chars.size()) {
			RenderCompositionText(rt, brush, compositionCharBrush, &x, &y);
		}

		// キャレットを描画
		if (caret.visible) {
			if (chars.empty()) {
				// 文字を描画していない場合は左上に描画
				RenderCursor(rt, 0, 0, brush);
				caret.x = 0;
				caret.y = 0;
			} else if (caret.index >= static_cast<signed int>(chars.size())) {
				// 末尾を選択している場合
				RenderCursor(rt, x, y, brush);
				caret.x = x;
				caret.y = y;
			} else {
				auto character = chars[caret.index];
				RenderCursor(rt, character.x, character.y, brush);
				caret.x = character.x;
				caret.y = character.y;
			}
		}

		brush->Release();
		compositionCharBrush->Release();
		selectionBrush->Release();
	}
}

void Editor::RenderChar(ID2D1HwndRenderTarget* rt, Char* const character, float* const x, float* const y, ID2D1Brush* brush) {
	// 文字を描画
	rt->DrawText(
		&character->wchar,
		1,
		textFormat,
		&RectF(*x, *y, *x + character->width, *y + charHeight),
		brush);

	character->x = *x;
	character->y = *y;
	*x += character->width;

	// 改行だったら y 座標を更新する
	if (character->wchar == '\n') {
		*x = 0;
		*y += charHeight;
	}
}

void Editor::RenderCompositionText(ID2D1HwndRenderTarget* rt, ID2D1Brush* brush, ID2D1Brush* backgroundBrush, float* const x, float* const y) {
	for (auto& compositionChar : compositionChars) {
		rt->FillRectangle(
			RectF(*x, *y, *x + compositionChar.width + 1, *y + charHeight),
			backgroundBrush);

		RenderChar(rt, &compositionChar, x, y, brush);
	}
}

void Editor::RenderCursor(ID2D1HwndRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
	rt->FillRectangle(
		RectF(x, y, x + options.cursorWidth, y + charHeight),
		brush);
}

void Editor::OnChar(wchar_t character) {
	// エンターキーを押すと character は \r になるので \n に置き換え
	if (character == '\r')
		character = '\n';

	if (character == '\b') {
		// 選択範囲を削除
		if (selection.start != selection.end) {
			DeleteSelection();
			return;
		}

		// バックスペースキーが押された場合はカーソルの前の文字を削除する
		if (selection.end > 0) {
			chars.erase(chars.begin() + selection.end - 1);
			MoveCaret(caret.index - 1);
		}
	} else {
		// 選択範囲を削除
		if (selection.start != selection.end) {
			DeleteSelection();
		}

		chars.insert(chars.begin() + selection.end, CreateChar(character));
		compositionTextPos = selection.end;

		// キャレットを動かす
		MoveCaret(caret.index + 1);
	}
}

void Editor::OnOpenCandidate() {
	// 候補ウィンドウが開いた時に呼ばれる

	auto imc = ImmGetContext(hwnd);

	// 候補ウィンドウを位置を指定して設定
	CANDIDATEFORM form;
	form.dwIndex = 0;
	form.dwStyle = CFS_FORCE_POSITION;
	form.ptCurrentPos.x = static_cast<LONG>(caret.x);
	form.ptCurrentPos.y = static_cast<LONG>(caret.y + charHeight);

	ImmSetCandidateWindow(imc, &form);

	ImmReleaseContext(hwnd, imc);
}

void Editor::OnQueryCharPosition(IMECHARPOSITION* pos) {
	// 文字の位置を設定する

	// 文字が描画される範囲をスクリーン座標で指定する
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rcClient.left));
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rcClient.right));
	pos->rcDocument = rcClient;

	// 行の高さ
	pos->cLineHeight = static_cast<UINT>(charHeight);

	// 文字の位置をスクリーン座標で指定する
	pos->pt.x = static_cast<LONG>(caret.x);
	pos->pt.y = static_cast<LONG>(caret.y);
	ClientToScreen(hwnd, &pos->pt);
}


void Editor::OnIMEComposition(LPARAM lparam) {
	auto imc = ImmGetContext(hwnd);
	if (!imc) {
		std::wcout << L"Unable to get imm context" << std::endl;
		return;
	}

	if (lparam & GCS_COMPSTR || lparam & GCS_RESULTSTR) {
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

		compositionChars.clear();
		compositionTextPos = selection.end;

		// 未確定文字列を挿入
		auto str = std::wstring(buf, size);

		int i = 0;
		for (auto& c : str) {
			auto ch = CreateChar(c);
			compositionChars.push_back(ch);
			i++;
		}

		delete[] buf;
	}

	ImmReleaseContext(hwnd, imc);
}

void Editor::OnIMEStartComposition() {
	caret.visible = false;
	cursorBlinkTimer.enabled = false;
}

void Editor::OnIMEEndComposition() {
	compositionStringLength = -1;
	compositionTextPos = -1;
}

void Editor::OnKeyDown(int keyCode) {
	// カーソルを表示させて点滅を停止する
	cursorBlinkTimer.enabled = false;
	caret.visible = true;

	// シフトキーを押しているかどうか
	bool shiftKey = GetKeyState(VK_SHIFT) < 0;

	switch (keyCode) {
	case VK_LEFT:
		if (selection.end > 0) {
			MoveCaret(caret.index - 1, shiftKey);
		}
		break;
	case VK_RIGHT:
		if (selection.end < chars.size()) {
			MoveCaret(caret.index + 1, shiftKey);
		}
		break;
	case VK_UP:
	{
		auto index = findIndexByPosition(caret.x, caret.y - charHeight + 1);
		if (index != -1) {
			MoveCaret(index, shiftKey);
		}
	}
		break;
	case VK_DOWN:
	{
		auto index = findIndexByPosition(caret.x, caret.y + charHeight + 1);
		if (index != -1) {
			MoveCaret(index, shiftKey);
		}
	}
		break;
	case VK_HOME:
	{
		if (caret.index == 0) {
			MoveCaret(0, shiftKey);
			break;
		}

		bool found = false;
		for (auto itr = chars.begin() + selection.end - 1; itr != chars.begin(); itr--) {
			auto&& character = *itr;
			if (character.wchar == '\n') {
				auto index = std::distance(chars.begin(), itr);
				MoveCaret(index + 1, shiftKey);
				found = true;
				break;
			}
		}

		if (!found) {
			MoveCaret(0, shiftKey);
		}
	}

		break;
	case VK_END:
	{
		bool found = false;
		for (auto itr = chars.begin() + selection.end; itr != chars.end(); itr++) {
			auto&& character = *itr;
			if (character.wchar == '\n') {
				auto index = std::distance(chars.begin(), itr);
				MoveCaret(index, shiftKey);
				found = true;
				break;
			}
		}

		if (!found) {
			MoveCaret(static_cast<int>(chars.size()), shiftKey);
		}
	}
	
		break;
	case VK_DELETE:
		// 選択範囲を削除
		if (selection.start != selection.end) {
			DeleteSelection();
			break;
		}

		// カーソルの後の文字を削除する
		if (caret.index < chars.size()) {
			chars.erase(chars.begin() + caret.index);
		}
		break;
	}
}

void Editor::OnKeyUp(int keyCode) {
	cursorBlinkTimer.enabled = true;
}

void Editor::OnLButtonDown(float x, float y) {
	// クリックされた位置から文字のインデックスを探す
	int index = findIndexByPosition(x, y);
	// 文字が見つかったらカーソルを動かす
	if (index != -1) {
		selectionStartWhileDrag = index;
		selection.start = index;
		selection.end = index;
	}

	dragged = true;
}

void Editor::OnLButtonUp(float x, float y) {
	dragged = false;
}