#include "stdafx.h"
#include "App.h"

using namespace D2D1;

App::App() :
	hwnd(nullptr),
	direct2d_factory(nullptr),
	render_target(nullptr),
	dwrite_factory(nullptr),
	editor(nullptr) {
}

App::~App() {
	safe_release(&direct2d_factory);
	safe_release(&dwrite_factory);
	safe_release(&render_target);
}

void App::run_message_loop() {
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		on_render();
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
		editor = std::make_unique<Editor>(hwnd, dwrite_factory);
		try {
			editor->initialize();
		} catch (const EditorException& e) {
			MessageBox(hwnd, char_to_wchar(e.what()), L"エディタの初期化に失敗しました", MB_OK | MB_ICONERROR);
			exit(1);
		}
		editor->set_text(L"H");
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
	if (message == WM_CREATE) {
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lparam;
		App* app = (App*)pcs->lpCreateParams;

		SetWindowLongPtrW(hwnd, GWLP_USERDATA, PtrToUlong(app));

		// タイマーを設定
		for (auto& timer : app->editor->timers) {
			SetTimer(hwnd, timer.id, timer.elapse, nullptr);
		}

		return 1;
	}
	else {
		App* app = reinterpret_cast<App*>(static_cast<LONG_PTR>(
			GetWindowLongPtrW(hwnd, GWLP_USERDATA)));

		if (app) {
			switch (message) {
			case WM_SETCURSOR:
				if (LOWORD(lparam) == HTCLIENT) {
					SetCursor(LoadCursor(nullptr, IDC_IBEAM));
					return 0;
				}
				break;
			case WM_CHAR:
				app->editor->on_char((wchar_t)wparam);
				return 0;
			case WM_SYSCHAR:
				app->editor->on_char((wchar_t)wparam);
			/*case WM_PAINT:
				app->on_render();
				ValidateRect(hwnd, nullptr);
				result = 0;
				was_handled = true;
				break;*/
			case WM_SIZE:
			{
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				app->on_resize(width, height);
			}
				return 0;
			case WM_TIMER:
				// タイマーの実行
				for (auto& timer : app->editor->timers) {
					if (wparam == timer.id) {
						timer.func();
					}
				}
				return 0;
			case WM_DISPLAYCHANGE:
				InvalidateRect(hwnd, nullptr, false);
				return 0;
			case WM_DESTROY:
				PostQuitMessage(0);
				return 1;
			}
		}

		return DefWindowProc(hwnd, message, wparam, lparam);
	}
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