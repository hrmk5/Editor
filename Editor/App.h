#pragma once

#include "stdafx.h"
#include "Editor.h"

template<class Interface>
inline void safe_release(Interface **interfaceToRelease) {
	if (*interfaceToRelease != nullptr) {
		(*interfaceToRelease)->Release();
		(*interfaceToRelease) = nullptr;
	}
}

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

class App {
private:
	HWND hwnd;
	ID2D1Factory* direct2d_factory;
	ID2D1HwndRenderTarget* render_target;
	IDWriteFactory* dwrite_factory;
	IDWriteTextFormat* text_format;
	ID2D1SolidColorBrush* black_brush;
	std::unique_ptr<Editor> editor;

	HRESULT create_device_independent_resources();
	HRESULT create_device_resources();
	void discard_device_resources();
	HRESULT on_render();
	void on_resize(UINT width, UINT height);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
public:
	App();
	~App();

	HRESULT initialize();
	void run_message_loop();
};