#include "stdafx.h"
#include "App.h"

using namespace D2D1;

App::App() :
	hwnd(nullptr),
	direct2d_factory(nullptr),
	render_target(nullptr),
	dwrite_factory(nullptr),
	text_format(nullptr),
	editor(nullptr) {
}

App::~App() {
	safe_release(&direct2d_factory);
	safe_release(&dwrite_factory);
	safe_release(&text_format);
	safe_release(&render_target);
}

void App::run_message_loop() {
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

HRESULT App::initialize() {
	HRESULT result;
	result = create_device_independent_resources();

	if (SUCCEEDED(result)) {
		WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = App::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = sizeof(LONG_PTR);
		wcex.hInstance = HINST_THISCOMPONENT;
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDI_APPLICATION);
		wcex.lpszClassName = L"App";

		RegisterClassEx(&wcex);

		FLOAT dpi_x, dpi_y;
		direct2d_factory->GetDesktopDpi(&dpi_x, &dpi_y);

		// ウィンドウを作成
		hwnd = CreateWindow(
			L"App",
			L"This is app",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<UINT>(ceil(640.f * dpi_x / 96.f)),
			static_cast<UINT>(ceil(480.f * dpi_y / 96.f)),
			nullptr,
			nullptr,
			HINST_THISCOMPONENT,
			this);

		result = hwnd ? S_OK : E_FAIL;
		if (SUCCEEDED(result)) {
			ShowWindow(hwnd, SW_SHOWNORMAL);
			UpdateWindow(hwnd);
		}
	}

	return result;
}

HRESULT App::create_device_independent_resources() {
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &direct2d_factory);

	if (SUCCEEDED(hr)) {
		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED, 
			__uuidof(IDWriteFactory), 
			reinterpret_cast<IUnknown**>(&dwrite_factory));
	}

	if (SUCCEEDED(hr)) {
		hr = dwrite_factory->CreateTextFormat(
			L"Yu Gothic",
			nullptr,
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			17.0f,
			L"ja-JP",
			&text_format);
	}

	if (SUCCEEDED(hr)) {
		editor = std::make_unique<Editor>(dwrite_factory, text_format);
		editor->set_text(L"Hello world!");
	}

	return hr;
}

HRESULT App::create_device_resources() {
	HRESULT result = S_OK;

	if (!render_target) {
		RECT rc;
		GetClientRect(hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

		result = direct2d_factory->CreateHwndRenderTarget(
			RenderTargetProperties(),
			HwndRenderTargetProperties(hwnd, size),
			&render_target);
	}

	return result;
}

void App::discard_device_resources() {
	safe_release(&render_target);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;

	if (message == WM_CREATE) {
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lparam;
		App* app = (App*)pcs->lpCreateParams;

		SetWindowLongPtrW(hwnd, GWLP_USERDATA, PtrToUlong(app));

		result = 1;
	}
	else {
		App* app = reinterpret_cast<App*>(static_cast<LONG_PTR>(
			GetWindowLongPtrW(hwnd, GWLP_USERDATA)));

		bool was_handled = false;

		if (app) {
			switch (message) {
			case WM_SIZE:
			{
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				app->on_resize(width, height);
			}

				result = 0;
				was_handled = true;
				break;
			case WM_DISPLAYCHANGE:
				InvalidateRect(hwnd, nullptr, false);
				result = 0;
				was_handled = true;
				break;
			case WM_PAINT:
				app->on_render();
				ValidateRect(hwnd, nullptr);
				result = 0;
				was_handled = true;
				break;
			case WM_DESTROY:
				PostQuitMessage(0);
				result = 1;
				was_handled = true;
				break;
			}
		}

		if (!was_handled) {
			result = DefWindowProc(hwnd, message, wparam, lparam);
		}
	}

	return result;
}

HRESULT App::on_render() {
	HRESULT result = S_OK;

	result = create_device_resources();
	if (SUCCEEDED(result)) {
		render_target->BeginDraw();
		render_target->SetTransform(Matrix3x2F::Identity());
		render_target->Clear(ColorF(ColorF::White));

		editor->render(render_target);

		result = render_target->EndDraw();
		if (result == D2DERR_RECREATE_TARGET) {
			result = S_OK;
			discard_device_resources();
		}
	}

	return result;
}

void App::on_resize(UINT width, UINT height) {
	if (render_target) {
		render_target->Resize(SizeU(width, height));
	}
}