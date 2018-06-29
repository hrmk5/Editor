#pragma once

#include "stdafx.h"
#include "Editor.h"
#include "Utils.h"

template<class Interface>
inline void SafeRelease(Interface **interfaceToRelease) {
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
	ID2D1Factory* direct2dFactory;
	ID2D1HwndRenderTarget* renderTarget;
	IDWriteFactory* dwriteFactory;
	ID2D1SolidColorBrush* blackBrush;
	std::unique_ptr<Editor> editor;

	HRESULT CreateDeviceIndependentResources();
	HRESULT CreateDeviceResources();
	void DiscardDeviceResources();
	HRESULT OnRender();
	void OnResize(UINT width, UINT height);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
public:
	App();
	~App();

	HRESULT Initialize();
	void RunMessageLoop();
};