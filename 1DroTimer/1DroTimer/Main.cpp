#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <limits.h>
#include <shobjidl.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm")

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

static class TBP {
	ITaskbarList3 *ppv;
public:
	void Init() {
		CLSID clsid;
		IID iid;
		if (!SUCCEEDED(CLSIDFromString(OLESTR("{56FDF344-FD6D-11d0-958A-006097C9A090}"), &clsid))) return;
		if (!SUCCEEDED(IIDFromString(OLESTR("{EA1AFB91-9E28-4B86-90E9-9E9F8A5EEFAF}"), &iid))) return;
		if (!SUCCEEDED(CoCreateInstance(clsid, NULL, CLSCTX_ALL, iid, (void **)&ppv))) return;
	}
	void Set(HWND hWnd, int completed, int total) {
		if (!ppv) return;
		ppv->SetProgressValue(hWnd, completed, total);
		double r = (double)completed / total;
		if (r < 0.5) ppv->SetProgressState(hWnd, TBPF_PAUSED);
		if (r < 0.2) ppv->SetProgressState(hWnd, TBPF_ERROR);
	}
	void Notify(HWND hWnd) {
		if (!ppv) return;
		ppv->SetProgressState(hWnd, TBPF_ERROR);
		ppv->SetProgressValue(hWnd, 1, 1);
	}
	void Reset(HWND hWnd) {
		if (!ppv) return;
		ppv->SetProgressValue(hWnd, 0, 0);
		ppv->SetProgressState(hWnd, TBPF_NORMAL);
	}
} tbp;

#define ALARM_FREQ 440*4
static struct Timer {
	enum {
		STOP, COUNTING, PAUSE,
	};
	int id;
	int state;
	int h, m, s;
	int initTime, restTime;

	void Start(HWND hWnd) {
		KillTimer(hWnd, id);
		SetTimer(hWnd, id, 1000, 0);
		state = COUNTING;
		initTime = restTime = 3600 * h + 60 * m + s;
		if (initTime == 0) {
			initTime = restTime = 3600; // A hour count will run if the time is set to 0.
		}
	}
	void Tick(HWND hWnd) {
		if (state == COUNTING) {
			if (--restTime < 0) {
				state = STOP;
				KillTimer(hWnd, id);
				tbp.Notify(hWnd);
				if (!PlaySound("./alarm.wav", NULL, SND_FILENAME | SND_SYNC | SND_NODEFAULT)) {
					Beep(ALARM_FREQ, 100);
					Beep(ALARM_FREQ, 100);
					Beep(ALARM_FREQ, 100);
					Beep(ALARM_FREQ, 100);
				}
				MessageBoxW(hWnd, TEXT(L"Time up!"), appName, MB_OK );
				tbp.Reset(hWnd);
			}
		}
	}
	Timer(int id_) : id(id_) {}
} timer(196884);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_DESTROY:
		timer.state = Timer::STOP;
		PostQuitMessage(0);
		return 0;
	case WM_TIMER:
		timer.Tick(hWnd);
		break;
	case WM_SIZE:
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
	wc.lpszClassName = L"1DRO_TIMER";
	RegisterClassW(&wc);
	WS_OVERLAPPEDWINDOW;
	HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, appName,
		WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, wc.hInstance, NULL);

	HDC hDC = GetDC(hWnd);

	GdiFont* font = nk_gdifont_create("Arial", 14);
	//GdiFont* f2 = nk_gdifont_create("Arial", 24);
	nk_context *ctx = nk_gdi_init(font, hDC, WINDOW_WIDTH, WINDOW_HEIGHT);

	tbp.Init();

	for (bool running = true, refresh = true; running; ) {
		/* Input */
		MSG msg;
		nk_input_begin(ctx);
		if (!refresh) {
			if (GetMessageW(&msg, NULL, 0, 0) <= 0) running = false;
			else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			refresh = true;
		}
		else refresh = false;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) running = false;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			refresh = true;
		}
		nk_input_end(ctx);

		RECT rc;
		GetClientRect(hWnd, &rc);
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;

		/* GUI */
		if (nk_begin(ctx, "", nk_rect(0, 0, width, height), NK_WINDOW_NO_SCROLLBAR)) {
			nk_window_set_size(ctx, "", nk_vec2(width, height));

			static const float ratio[] = { 60, 150 };
			{
				nk_layout_row(ctx, NK_STATIC, 30, 2, ratio);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Hours: %02d", timer.h);
				nk_progress(ctx, (size_t *)&timer.h, 100, NK_MODIFIABLE);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Minutes: %02d", timer.m);
				nk_progress(ctx, (size_t *)&timer.m, 60, NK_MODIFIABLE);
				nk_labelf(ctx, NK_TEXT_RIGHT, "Seconds: %02d", timer.s);
				nk_progress(ctx, (size_t *)&timer.s, 60, NK_MODIFIABLE);
			}

			static const float r2[] = { 150, 60 };
			if (timer.state == Timer::STOP) {
				nk_layout_row_static(ctx, 60 + 30 + 4, r2[0] + r2[1] + 4, 1);
				if (nk_button_label(ctx, "Start")) timer.Start(hWnd);
			} else {
				nk_style_item prev = ctx->style.progress.cursor_normal;
				ctx->style.progress.cursor_normal = nk_style_item_color(nk_rgba(50, 230, 150, 255));	// Green BAR
					nk_layout_row(ctx, NK_STATIC, 30, 2, ratio);
					nk_labelf(ctx, NK_TEXT_RIGHT, "%02d:%02d'%02d\"", timer.restTime / 3600, (timer.restTime / 60) % 60, timer.restTime % 60);
					nk_progress(ctx, (size_t *)&timer.restTime, timer.initTime, 0);
					tbp.Set(hWnd, timer.restTime, timer.initTime);
				ctx->style.progress.cursor_normal = prev;

				nk_layout_row(ctx, NK_STATIC, 60, 2, r2);
				if(timer.state == Timer::COUNTING)
					if (nk_button_label(ctx, "Pause")) timer.state = Timer::PAUSE;
				if (timer.state == Timer::PAUSE)
					if (nk_button_label(ctx, "Continue")) timer.state = Timer::COUNTING;
				if (nk_button_label(ctx, "Stop")) {
					timer.state = Timer::STOP;
					tbp.Reset(hWnd);
				}
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
