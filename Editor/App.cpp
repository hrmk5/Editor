#include "stdafx.h"
#include "App.h"

using namespace D2D1;

App::App() :
	hwnd(nullptr),
	direct2d_factory(nullptr),
	render_target(nullptr),
	light_slate_gray_brush(nullptr),
	cornflower_blue_brush(nullptr) {
}

App::~App() {
	safe_release(&direct2d_factory);
	safe_release(&render_target);
	safe_release(&light_slate_gray_brush);
	safe_release(&cornflower_blue_brush);
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
	HRESULT result = S_OK;
	result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &direct2d_factory);

	return result;
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

		if (SUCCEEDED(result)) {
			result = render_target->CreateSolidColorBrush(ColorF(ColorF::LightSlateGray), &light_slate_gray_brush);
		}
		if (SUCCEEDED(result)) {
			result = render_target->CreateSolidColorBrush(ColorF(ColorF::CornflowerBlue), &cornflower_blue_brush);
		}
	}

	return result;
}

void App::discard_device_resources() {
	safe_release(&render_target);
	safe_release(&light_slate_gray_brush);
	safe_release(&cornflower_blue_brush);
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

		auto rt_size = render_target->GetSize();

		auto width = static_cast<int>(rt_size.width);
		auto height = static_cast<int>(rt_size.height);

		for (int x = 0; x < width; x += 10)
		{
			render_target->DrawLine(
				D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
				D2D1::Point2F(static_cast<FLOAT>(x), rt_size.height),
				light_slate_gray_brush,
				0.5f
			);
		}

		for (int y = 0; y < height; y += 10)
		{
			render_target->DrawLine(
				D2D1::Point2F(0.0f, static_cast<FLOAT>(y)),
				D2D1::Point2F(rt_size.width, static_cast<FLOAT>(y)),
				light_slate_gray_brush,
				0.5f
			);
		}

		D2D1_RECT_F rectangle1 = D2D1::RectF(
			rt_size.width / 2 - 50.0f,
			rt_size.height / 2 - 50.0f,
			rt_size.width / 2 + 50.0f,
			rt_size.height / 2 + 50.0f
		);

		D2D1_RECT_F rectangle2 = D2D1::RectF(
			rt_size.width / 2 - 100.0f,
			rt_size.height / 2 - 100.0f,
			rt_size.width / 2 + 100.0f,
			rt_size.height / 2 + 100.0f
		);

		render_target->FillRectangle(&rectangle1, light_slate_gray_brush);
		render_target->DrawRectangle(&rectangle2, cornflower_blue_brush);

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