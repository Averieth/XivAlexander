#include "pch.h"
#include "App_Window_ProgressPopupWindow.h"

#include "DllMain.h"
#include "resource.h"

constexpr auto margin = 10.;
constexpr auto elementHeight = 30.;
constexpr auto buttonWidth = 80.;

static WNDCLASSEXW WindowClass() {
	const auto hIcon = Utils::Win32::Icon(LoadIconW(Dll::Module(), MAKEINTRESOURCEW(IDI_TRAY_ICON)),
		nullptr,
		"LoadIconW");
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = Dll::Module();
	wcex.hIcon = hIcon;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
	wcex.lpszClassName = L"XivAlexander::Window::ProgressPopupWindow";
	wcex.hIconSm = hIcon;
	return wcex;
}

App::Window::ProgressPopupWindow::ProgressPopupWindow(HWND hParentWindow)
	: BaseWindow(WindowClass(), nullptr, WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr)
	, m_hParentWindow(hParentWindow)
	, m_hProgressBar(CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_MARQUEE, 0, 0, 0, 0, m_hWnd, nullptr, Dll::Module(), nullptr))
	, m_hMessage(CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 0, 0, m_hWnd, nullptr, Dll::Module(), nullptr))
	, m_hCancelButton(CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(IDCANCEL), Dll::Module(), nullptr))
	, m_hCancelEvent(Utils::Win32::Event::Create()) {
	SetWindowSubclass(m_hProgressBar, [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) -> LRESULT {
		if (uMsg == WM_NCHITTEST)
			return HTTRANSPARENT;
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}, 0, 0);

	NONCLIENTMETRICS ncm = {.cbSize = sizeof(NONCLIENTMETRICS)};
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
	ncm.lfMessageFont.lfHeight = static_cast<LONG>(ncm.lfMessageFont.lfHeight * GetZoom());
	m_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
	SendMessageW(m_hMessage, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
	SendMessageW(m_hCancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
}

App::Window::ProgressPopupWindow::~ProgressPopupWindow() {
	Destroy();
}

void App::Window::ProgressPopupWindow::UpdateProgress(uint64_t progress, uint64_t max) {
	const auto prevStyle = GetWindowLongPtrW(m_hProgressBar, GWL_STYLE);
	if (!max) {
		if (!(prevStyle & PBS_MARQUEE))
			SetWindowLongPtrW(m_hProgressBar, GWL_STYLE, prevStyle | PBS_MARQUEE);
	} else {
		if (prevStyle & PBS_MARQUEE) {
			SetWindowLongPtrW(m_hProgressBar, GWL_STYLE, GetWindowLongPtrW(m_hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
			SendMessageW(m_hProgressBar, PBM_SETRANGE32, 0, static_cast<LPARAM>(10000000));
		}
		SendMessageW(m_hProgressBar, PBM_SETPOS, static_cast<LPARAM>(10000000.0 * static_cast<double>(progress) / static_cast<double>(max)), 0);
	}
}

void App::Window::ProgressPopupWindow::UpdateMessage(const std::string& message) {
	SendMessageW(m_hMessage, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(Utils::FromUtf8(message).c_str()));
}

void App::Window::ProgressPopupWindow::Cancel() {
	SendMessageW(m_hWnd, WM_CLOSE, 0, 1);
}

const Utils::Win32::Event& App::Window::ProgressPopupWindow::GetCancelEvent() const {
	return m_hCancelEvent;
}

DWORD App::Window::ProgressPopupWindow::DoModalLoop(int ms, std::vector<HANDLE> events) {
	events.insert(events.begin(), m_hCancelEvent);

	MSG msg;
	const auto until = static_cast<int64_t>(GetTickCount64()) + ms;
	while (true) {
		const auto remaining = until - static_cast<int64_t>(GetTickCount64());
		if (remaining < 0)
			return WAIT_TIMEOUT;
		const auto which = MsgWaitForMultipleObjectsEx(static_cast<DWORD>(events.size()), &events[0], static_cast<DWORD>(remaining), QS_ALLEVENTS, 0);

		if (which == WAIT_IO_COMPLETION || which == WAIT_FAILED
			|| (WAIT_OBJECT_0 <= which && which < WAIT_OBJECT_0 + events.size())
			|| (WAIT_ABANDONED_0 <= which && which < WAIT_ABANDONED_0 + events.size())) {
			return which;

		} else if (which == WAIT_OBJECT_0 + events.size()) {
			if (GetMessageW(&msg, nullptr, 0, 0)) {
				if (msg.hwnd == m_hWnd || msg.hwnd == m_hProgressBar || msg.hwnd == m_hMessage || msg.hwnd == m_hCancelButton) {
					if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
						msg.hwnd = m_hCancelButton;
						msg.message = WM_COMMAND;
						msg.wParam = MAKEWPARAM(IDCANCEL, BN_CLICKED);
						msg.lParam = reinterpret_cast<LPARAM>(m_hCancelButton);
					}
				}
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			} else {
				m_hCancelEvent.Set();
				PostQuitMessage(static_cast<int>(msg.wParam));
				return 0;
			}
		}
	}
}

void App::Window::ProgressPopupWindow::Show() {
	if (IsWindowVisible(m_hWnd))
		return;

	RECT parentRect;
	if (m_hParentWindow)
		GetWindowRect(m_hParentWindow, &parentRect);
	else
		parentRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};

	RECT targetRect = {0, 0, static_cast<int>(GetZoom() * 640), static_cast<int>(GetZoom() * (margin * 3 + elementHeight * 2))};
	AdjustWindowRectEx(&targetRect, GetWindowLongW(m_hWnd, GWL_STYLE), FALSE, 0);
	SetWindowPos(m_hWnd, nullptr,
		parentRect.left + (parentRect.right - parentRect.left - targetRect.right - targetRect.left) / 2,
		parentRect.top + (parentRect.bottom - parentRect.top - targetRect.bottom - targetRect.top) / 2,
		targetRect.right - targetRect.left,
		targetRect.bottom - targetRect.top,
		SWP_NOACTIVATE | SWP_NOREPOSITION | SWP_NOZORDER);
	ShowWindow(m_hWnd, SW_SHOW);
	SetFocus(m_hCancelButton);
}

void App::Window::ProgressPopupWindow::OnLayout(double zoom, double width, double height, int resizeType) {
	if (m_hParentWindow) {
		if (resizeType != SIZE_MINIMIZED && IsIconic(m_hParentWindow))
			ShowWindowAsync(m_hParentWindow, SW_RESTORE);
		else if (resizeType == SIZE_MINIMIZED && !IsIconic(m_hParentWindow))
			ShowWindowAsync(m_hParentWindow, SW_MINIMIZE);
	}

	if (resizeType != SIZE_MINIMIZED) {
		struct RECTLF {
			double left;
			double top;
			double right;
			double bottom;
		};

		const auto targets = std::map<HWND, RECTLF>{
			{m_hProgressBar, {margin, margin, width - margin, margin + elementHeight}},
			{m_hCancelButton, {width - margin - buttonWidth, margin + elementHeight + margin, width - margin, margin + elementHeight + margin + elementHeight}},
			{m_hMessage, {margin, margin + elementHeight + margin, width - margin - buttonWidth - margin, margin + elementHeight + margin + elementHeight}},
		};

		auto hdwp = BeginDeferWindowPos(static_cast<int>(targets.size()));
		for (const auto& [hwnd, rt] : targets)
			hdwp = DeferWindowPos(hdwp, hwnd, nullptr,
				static_cast<int>(rt.left * zoom),
				static_cast<int>(rt.top * zoom),
				static_cast<int>((rt.right - rt.left) * zoom),
				static_cast<int>((rt.bottom - rt.top) * zoom),
				0);
		EndDeferWindowPos(hdwp);
	}
}

void App::Window::ProgressPopupWindow::OnDestroy() {
	m_hCancelEvent.Set();
	BaseWindow::OnDestroy();
	if (m_hFont)
		DeleteFont(m_hFont);
}

LRESULT App::Window::ProgressPopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CLOSE: {
			if (lParam == 0) {
				if (Utils::Win32::MessageBoxF(hwnd, MB_YESNO | MB_DEFBUTTON2, L"XA", L"Cancel?") == IDNO)
					return 0;
			}
			break;
		}

		case WM_COMMAND: {
			if (wParam == IDCANCEL || wParam == IDOK)
				SendMessageW(hwnd, WM_CLOSE, 0, 0);
			break;
		}

		case WM_DPICHANGED: {
			if (m_hFont)
				DeleteFont(m_hFont);
			NONCLIENTMETRICS ncm = {.cbSize = sizeof(NONCLIENTMETRICS)};
			SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
			ncm.lfMessageFont.lfHeight = static_cast<LONG>(ncm.lfMessageFont.lfHeight * GetZoom());
			m_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
			SendMessageW(m_hMessage, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
			SendMessageW(m_hCancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
			break;
		}

		case WM_CTLCOLORSTATIC: {
			if (reinterpret_cast<HWND>(lParam) == m_hMessage) {
				const auto hdcStatic = reinterpret_cast<HDC>(wParam);
				SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
				return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
			}
		}

		case WM_NCHITTEST: {
			return HTCAPTION;
		}
	}
	return BaseWindow::WndProc(hwnd, msg, wParam, lParam);
}
