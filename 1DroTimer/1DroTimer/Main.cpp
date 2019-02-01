#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <limits.h>
#include <shobjidl.h>

#define WINDOW_WIDTH 250
#define WINDOW_HEIGHT 250

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_gdi.h"
LPCWSTR appName = TEXT(L"1DroTimer");

static struct Timer {
	enum {
		STOP, COUNTING, PAUSE,
	};
	int id;
	int state;
	int h, m, s;
	int initTime, restTime;
	bool notify;
	void Start(HWND hWnd) {
		KillTimer(hWnd, id);
		SetTimer(hWnd, id, 1000, 0);
		state = COUNTING;
		initTime = restTime = 3600 * h + 60 * m + s;
		if (initTime == 0) {
			initTime = restTime = 3600; // A hour count will run if the time is set to 0.
			//notify = true;
		}
	}
	void Tick(HWND hWnd) {
		if (state == COUNTING) {
			if (--restTime < 0) {
				state = STOP;
				KillTimer(hWnd, id);
				//Beep(440, 1000);
			//	if (notify) {
					MessageBoxW(hWnd, TEXT(L"Time up!"), appName, MB_OK);
			//		notify = false;
			//	}
			}
		}
	}
	Timer(int id_) : id(id_), notify(0) {}
} timer(1);

static class TBP {
	CLSID clsid;
	IID iid;
	ITaskbarList3 *ppv;
public:
	void Init() {
		CLSIDFromString(OLESTR("{56FDF344-FD6D-11d0-958A-006097C9A090}"), &clsid);
		IIDFromString(OLESTR("{EA1AFB91-9E28-4B86-90E9-9E9F8A5EEFAF}"), &iid);
		CoCreateInstance(clsid, NULL, CLSCTX_ALL, iid, (void **)&ppv);
	}
	void Set(HWND hWnd, int completed, int total) {
		ppv->SetProgressValue(hWnd, completed, total);
	}
} tbp;


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_DESTROY:
		timer.state = Timer::STOP;
		PostQuitMessage(0);
		return 0;
	case WM_TIMER:
		timer.Tick(hWnd);
		break;
	}

	if (nk_gdi_handle_event(hWnd, msg, wp, lp)) return 0;

	return DefWindowProcW(hWnd, msg, wp, lp);
}

int WinMain(HINSTANCE hInst, HINSTANCE, LPSTR psCmdLine, int nCmdShow) {
	WNDCLASSW wc={};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleW(0);
	//wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"WindowClass";
	RegisterClassW(&wc);

	HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, appName,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, wc.hInstance, NULL);

	HDC hDC = GetDC(hWnd);

	GdiFont* font = nk_gdifont_create("Arial", 14);
	GdiFont* f2 = nk_gdifont_create("Arial", 24);
	nk_context *ctx = nk_gdi_init(font, hDC, WINDOW_WIDTH, WINDOW_HEIGHT);

	int needs_refresh = 1;
	for (bool running = true; running; ) {
		/* Input */
		MSG msg;
		nk_input_begin(ctx);
		if (needs_refresh == 0) {
			if (GetMessageW(&msg, NULL, 0, 0) <= 0) running = false;
			else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			needs_refresh = 1;
		}
		else needs_refresh = 0;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) running = false;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			needs_refresh = 1;
		}

		nk_input_end(ctx);
		RECT rc;
		GetClientRect(hWnd, &rc);
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;

		/* GUI */
		if (nk_begin(ctx, "", nk_rect(0, 0, width, height), NK_WINDOW_NO_SCROLLBAR))
		{
			nk_window_set_size(ctx, "", nk_vec2(rc.right - rc.left, rc.bottom - rc.top));

			//
			static const float ratio[] = { 60, 150 };
			{
				//set_style(ctx, THEME_WHITE);
				nk_layout_row(ctx, NK_STATIC, 30, 2, ratio);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Hours: %02d", timer.h);
				nk_progress(ctx, (size_t *)&timer.h, 100, NK_MODIFIABLE);
				//set_style(ctx, THEME_BLUE);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Minutes: %02d", timer.m);
				nk_progress(ctx, (size_t *)&timer.m, 60, NK_MODIFIABLE);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Seconds: %02d", timer.s);
				nk_progress(ctx, (size_t *)&timer.s, 60, NK_MODIFIABLE);
			}

			float r2[] = {150, 60};
			switch (timer.state) {
			case Timer::STOP:
				nk_layout_row_static(ctx, 60, 150+60+4, 1);
				if (nk_button_label(ctx, "Start")) {
					timer.Start(hWnd);
					tbp.Init();
				}
				break;
			case Timer::COUNTING:
				nk_layout_row(ctx, NK_STATIC, 60, 2, r2);
				if (nk_button_label(ctx, "Pause")) timer.state = Timer::PAUSE;
				if (nk_button_label(ctx, "Stop")) timer.state = Timer::STOP;
				//if (nk_button_symbol(ctx, NK_SYMBOL_RECT_SOLID)) timer.state = Timer::TS_STOP;
				break;
			case Timer::PAUSE:
				nk_layout_row(ctx, NK_STATIC, 60, 2, r2);
				if (nk_button_label(ctx, "Continue")) timer.state = Timer::COUNTING;
				if (nk_button_label(ctx, "Stop")) timer.state = Timer::STOP;
				break;
			}

			if (timer.state != Timer::STOP) {
				nk_style_item prev = ctx->style.progress.cursor_normal;
				ctx->style.progress.cursor_normal = nk_style_item_color(nk_rgba(50, 230, 150, 255));	// BAR

				nk_layout_row(ctx, NK_STATIC, 30, 2, ratio);
				nk_labelf(ctx, NK_TEXT_RIGHT, "%02d:%02d'%02d\"", timer.restTime / 3600, (timer.restTime / 60) % 60, timer.restTime % 60);
				nk_progress(ctx, (size_t *)&timer.restTime, timer.initTime, 0);
				tbp.Set(hWnd, timer.restTime, timer.initTime);

				ctx->style.progress.cursor_normal = prev;
			}
		}
		nk_end(ctx);
		nk_gdi_render(nk_rgb(30, 30, 30));
	}

	nk_gdifont_del(font);
	ReleaseDC(hWnd, hDC);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);
	return 0;
}
