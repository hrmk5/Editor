#include "stdafx.h"
#include "Editor.h"

using namespace D2D1;

Timer::Timer(UINT id, UINT elapse, const TimerFunc & func) :
	id(id),
	elapse(elapse),
	func(func) {
}

EditorOptions default_editoroptions() {
	EditorOptions options;
	options.cursor_blink_rate_msec = 500;
	options.cursor_width = 2;
	options.font_name = L"Yu Gothic";
	options.font_size = 17.0f;

	return options;
}

Editor::Editor(HWND window, IDWriteFactory* factory, const EditorOptions& options) :
	hwnd(hwnd),
	factory(factory),
	text_format(nullptr),
	options(options),
	visible_cursor(true){
}

Editor::~Editor() {
	text_format->Release();
}

void Editor::initialize() {
	// TextFormat を作成
	HRESULT hr = factory->CreateTextFormat(
		options.font_name.c_str(),
		nullptr,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		options.font_size,
		L"ja-JP",
		&text_format);

	if (FAILED(hr)) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::string font_name = converter.to_bytes(options.font_name);
		throw EditorException("Unable to create text format. font: '"
			+ font_name + "', font size: " + std::to_string(options.font_size));
	}

	text_format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, options.font_size / 0.8, options.font_size);

	// 文字の高さを測定
	IDWriteTextLayout* layout;
	// IDWriteTextLayout を作成
	auto test_text = L"abcdefghijklnmopqrstuvwxyzABCDEFGHIJKLNMOPQRSTUVWXYZあいうえお漢字汉字繁體字";
	auto result = factory->CreateTextLayout(test_text, wcslen(test_text), text_format, 100000000, 1000, &layout);
	if (SUCCEEDED(result)) {
		DWRITE_TEXT_METRICS metrics;
		layout->GetMetrics(&metrics);
		char_height = metrics.height;

		// IDWriteTextLayout を破棄
		layout->Release();
	}
	
	// カーソルを点滅させるタイマーの設定
	timers.push_back(Timer(ID_CURSOR_BLINK_TIMER, options.cursor_blink_rate_msec, std::bind(&Editor::toggle_cursor_visible, this)));
}

void Editor::set_text(const std::wstring& str) {
	chars.clear();

	for (auto& ch : str) {
		auto character = create_char(ch);
		chars.push_back(character);
	}
}

void Editor::append_char(wchar_t wchar) {
	auto character = create_char(wchar);
	chars.push_back(character);

	selection.start = chars.size() + 1;
	selection.end = chars.size() + 1;
}

DWRITE_FONT_METRICS Editor::get_font_metrics() {
	// TODO: エラーチェック

	// フォントファミリーの名前と IDWriteFontCollection を取得
	IDWriteFontCollection* collection;
	WCHAR family_name[64];
	text_format->GetFontFamilyName(family_name, 64);
	text_format->GetFontCollection(&collection);

	// 名前からフォントファミリーを探す
	UINT32 index;
	BOOL exists;
	collection->FindFamilyName(family_name, &index, &exists);

	IDWriteFontFamily* family;
	collection->GetFontFamily(index, &family);

	IDWriteFont* font;
	family->GetFirstMatchingFont(text_format->GetFontWeight(), text_format->GetFontStretch(), text_format->GetFontStyle(), &font);
	
	// DWRITE_FONT_METRICS の取得
	DWRITE_FONT_METRICS metrics;
	font->GetMetrics(&metrics);

	font->Release();
	family->Release();
	collection->Release();

	return metrics;
}

Char Editor::create_char(wchar_t character) {
	Char ch;
	ch.wchar = character;
	
	// 空白文字でなかったら文字の幅を取得
	IDWriteTextLayout* layout;
	// IDWriteTextLayout を作成
	auto result = factory->CreateTextLayout(&character, wcslen(&character), text_format, 100, 100, &layout);
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

void Editor::toggle_cursor_visible() {
	visible_cursor = !visible_cursor;
}

void Editor::render(ID2D1HwndRenderTarget* rt) {
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
				text_format,
				&RectF(x, y, x + character.width, y + char_height),
				brush);

			character.x = x;
			character.y = y;
			x += character.width;
		}

		// カーソルを描画
		if (visible_cursor) {
			if (chars.empty()) {
				// 文字を描画していない場合は左上に描画
				render_cursor(rt, 0, 0, brush);
			} else if (selection.end > (signed int)chars.size()) {
				// 末尾を選択している場合は 末尾の文字の x 座標 + 末尾の文字の幅 に描画する
				auto last_char = chars[chars.size() - 1];
				render_cursor(rt, last_char.x + last_char.width, last_char.y, brush);
			} else {
				auto character = chars[selection.end];
				render_cursor(rt, character.x, character.y, brush);
			}
		}
	}
}

void Editor::render_cursor(ID2D1HwndRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
	rt->FillRectangle(
		RectF(x, y, x + options.cursor_width, y + char_height),
		brush);
}

void Editor::on_char(wchar_t character) {
	if (character == '\b') {
		// バックスペースキーが押された場合		
		if (chars.size() > 1) {
			// 文字列のサイズが 2 以上の場合は末尾の文字を削除する
			chars.pop_back();
		} else {
			// 文字列のサイズが 2 未満の場合はすべての文字を削除
			chars.clear();
		}
	} else {
		append_char(character);
	}
}
