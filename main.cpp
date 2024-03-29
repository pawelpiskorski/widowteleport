/**
Copyright (c) 2010-2016, Paweł Piskorski
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include <memory>
#include <vector>

#define WM_TRAY_MESSAGE (WM_USER + 1)
#define LOG_MESSAGE_BUFFER_SIZE 100
#define DOUBLE_HOTKEY_TICKS 300
#define PRIMARY_NUMPAD_ID 1
#define SECONDARY_NUMPAD_ID 6
#define TRAY_ICON_ID 1234324

DWORD LastTick;
INT   HotkeyId;
INT   FirstHotkeyId;

const wchar_t Usage[] = 
	L"            Window Teleport v1.35\r\n" 
	L" Copyright 2010-2016 Paweł Piskorski\r\n"
	L"Use CTRL + NUMPAD4, NUMPAD5, NUMPAD1, or NUMPAD2 to place active\r\n"
	L"window on a 2x2 grid.\r\n" 
	L"Use CTRL + ALT + NUMPAD1 up to NUMPAD9 to place active window on a 3x3 grid.\r\n"
	L"Use a combination of any two of the above shortcuts: to point any two window\r\n"
	L"corners.\r\n" 
	L"Use CTRL + UP ARROW to move windows between screen (not active with single monitor)\r\n"
	L"monitors.\r\n"
	L"=====================================\r\n\r\n";

const short ScreenThirds[9][2] = {{0,2},{1,2},{2,2},{0,1},{1,1},{2,1},{0,0},{1,0},{2,0}};
const short ScreenHalfs[5][2] = {{0,1},{1,1},{1,1},{0,0},{1,0}};


class MonitorInfo
{
public:
	MonitorInfo() : timerId_(0)
	{}

	~MonitorInfo()
	{
		if (timerId_)
			KillTimer(NULL, timerId_);
		timerId_ = 0;
	}

	struct MONITOR_INFORMATION
	{
		HMONITOR Handle;
		MONITORINFO Info;
	};

	std::vector<MONITOR_INFORMATION> monitors_;

	void GetMonitorInformation();
	unsigned GetMonitorIdx(HMONITOR monitorHandle) const
	{
		for (unsigned monitorIdx = 0; monitorIdx < monitors_.size(); monitorIdx++)
		{
			if (monitorHandle == monitors_[monitorIdx].Handle)
			{
				return monitorIdx;
			}
		}

		return static_cast<int>(monitors_.size());
	}

	RECT& operator[](int index)
	{
		return monitors_[index].Info.rcWork;
	}

	static BOOL CALLBACK EnumMonitorCallback(
			__in  HMONITOR hMonitor,
			__in  HDC hdcMonitor,
			__in  LPRECT lprcMonitor,
			__in  LPARAM dwData);
	static void CALLBACK DelayedQueryProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
	void TryDelayedQuery();

	bool IsMultiMonitor() const { return Count() > 1; }
	int Count() const { return monitors_.size(); }
private:
	UINT_PTR timerId_;
};

MonitorInfo Monitors;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void RegisterHotKeys(bool MonitorJumping);
void DeregisterHotKeys(bool MonitorJumping);
void HandleHotkey(PMSG msg);


class Exception
{
	public:
		Exception(const wchar_t *message, DWORD systemErrorCode)
		{
			const wchar_t errorCaption[] = L"Error";
			MessageBox(NULL,message,errorCaption,MB_OK | MB_ICONEXCLAMATION);
		}
};

class ApplicationGui
{
	private :
		wchar_t Buffer[LOG_MESSAGE_BUFFER_SIZE];
		NOTIFYICONDATA Tnd;

	public:
		HWND Hwnd;
		HWND EditHwnd;

		ApplicationGui(HINSTANCE hInstance, int nCmdShow);
		~ApplicationGui();
		void LogMessage(LPCWSTR Message,...);

};

std::unique_ptr<ApplicationGui> Gui;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	try {
		Gui.reset(new ApplicationGui(hInstance, nCmdShow));
		if (Gui == nullptr)
		{
			return 2;
		}

		Monitors.GetMonitorInformation();
		RegisterHotKeys(Monitors.IsMultiMonitor());

		// Run the message loop.
		BOOL bRet;

		MSG msg = { };
		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
		{
			if (-1 == bRet)
			{
				return 1;
			}
			switch (msg.message)
			{
				case WM_HOTKEY:
					HandleHotkey(&msg);
					continue;
				case WM_DISPLAYCHANGE:
					DeregisterHotKeys(Monitors.IsMultiMonitor());
					Monitors.GetMonitorInformation();
					RegisterHotKeys(Monitors.IsMultiMonitor());
					Gui->LogMessage(L"got displaychange and it was posted! Tell the author it worked :)");
					break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		DeregisterHotKeys(Monitors.IsMultiMonitor());
	}
	catch (Exception &)
	{
		return 1;
	}

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_TRAY_MESSAGE :
		
		UINT uID;
		UINT uMouseMsg;
		POINT pt;
		uID=(UINT) wParam;
		uMouseMsg=(UINT) lParam;
		if (uMouseMsg==WM_LBUTTONDOWN && uID == TRAY_ICON_ID)
		{
			GetCursorPos(&pt);// Get mouse's current position
			ShowWindow(Gui->Hwnd, SW_SHOWNORMAL);
		}
		return 0;
	case WM_SIZE:
		if (NULL != Gui)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				ShowWindow(Gui->Hwnd, SW_HIDE);
				break;
			}
			MoveWindow(
				Gui->EditHwnd,
				0, 0,                  // starting x- and y-coordinates 
				LOWORD(lParam),        // width of client area 
				HIWORD(lParam),        // height of client area 
				TRUE);                 // repaint window 
		}
		return 0; 

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			FillRect(hdc, &ps.rcPaint, HBRUSH(COLOR_WINDOW+1));

			EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_DISPLAYCHANGE:
		Monitors.GetMonitorInformation();
		return 0;

	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


void RegisterHotKeys(bool MonitorJumping)
{
	HWND hWnd = NULL;
	UINT modifiers = MOD_CONTROL;
	int id;
	HotkeyId = 1;
	BOOL success = FALSE;
	do 
	{
		if (MonitorJumping)
		{
			if (FALSE == RegisterHotKey(hWnd, HotkeyId, modifiers, VK_UP))
				break;
		}
		HotkeyId++;
	
		/*if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_RIGHT))
			break;
		*/
		
		if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD1))
			break;
		if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD2))
			break;
		if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD3))
			break;
		if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD4))
			break;
		if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD5))
			break;


		modifiers |= MOD_ALT;

		for (id = 0; id < 9; id++)
		{
			if (FALSE == RegisterHotKey(hWnd, HotkeyId++, modifiers, VK_NUMPAD1 + id))
				break;
		}
		success = TRUE;
	}
	while (0);

	if (FALSE == success)
	{
		throw Exception(L"Could not create all hotkeys",GetLastError());
	}
}


void DeregisterHotKeys(bool MonitorJumping)
{
	HWND hWnd = NULL;
	
	for (--HotkeyId; HotkeyId >1; HotkeyId--)
	{
		if (FALSE == UnregisterHotKey(hWnd, HotkeyId))
		{
			throw Exception(L"Could not delete all hotkeys",GetLastError());
		}
	}
	if (MonitorJumping)
	{
		if (FALSE == UnregisterHotKey(hWnd, HotkeyId))
		{
			throw Exception(L"Could not delete all hotkeys",GetLastError());
		}
		--HotkeyId;
	}
}


void HandleHotkey(PMSG msg)
{
	DWORD Tick = GetTickCount();
	HWND activeWnd = GetForegroundWindow();
	RECT activeWndRect;
	GetWindowRect(activeWnd,&activeWndRect);
	HMONITOR activeWndMonitor = MonitorFromRect(&activeWndRect,MONITOR_DEFAULTTOPRIMARY);
	INT monitorIdx = Monitors.GetMonitorIdx(activeWndMonitor);

	RECT monitor = Monitors[monitorIdx];
	UINT monitorHeight = (monitor.bottom - monitor.top);
	UINT monitorWidth = (monitor.right - monitor.left);

	Gui->LogMessage(L"Hotkey %d. Wr=[%d; %d; %d; %d], monitor %d",
		msg->wParam, activeWndRect.top, activeWndRect.right, activeWndRect.bottom, activeWndRect.left, monitorIdx);

	if (msg->wParam <= PRIMARY_NUMPAD_ID) //left
	{
		if (msg->wParam == 1 && --monitorIdx<0) 
			monitorIdx = Monitors.Count()-1;
		else if (msg->wParam == 2 && ++monitorIdx>=Monitors.Count())
			monitorIdx = 0;
		monitor = Monitors[monitorIdx];

		if (activeWndRect.right-activeWndRect.left>monitor.right-monitor.left)
			activeWndRect.right = monitor.right-monitor.left;
		else
			activeWndRect.right -= activeWndRect.left;
		if (activeWndRect.bottom-activeWndRect.top>monitor.bottom-monitor.top)
			activeWndRect.bottom = monitor.bottom-monitor.top;
		else
			activeWndRect.bottom -= activeWndRect.top;
		BOOL WasZoomed = IsZoomed(activeWnd);
		ShowWindow(activeWnd,SW_RESTORE);
		MoveWindow(activeWnd,monitor.left,monitor.top,activeWndRect.right,activeWndRect.bottom,TRUE);
		if (WasZoomed)
		{
			ShowWindow(activeWnd,SW_SHOWMAXIMIZED);
		}
	}
	else if (Tick - LastTick < DOUBLE_HOTKEY_TICKS)
	{
		int Top=monitor.bottom-monitor.top,
			Left = monitor.right-monitor.left,
			Bottom = 0,
			Right = 0,
			x,y;
		const short *cell;
		int key = FirstHotkeyId;
		for (int i=0; i<4; ++i)
		{
			if (key <= SECONDARY_NUMPAD_ID)
			{
				cell = ScreenHalfs[key - PRIMARY_NUMPAD_ID-1]; 
				x = (cell[0] + (i & 1))*monitorWidth/2;
				y = (cell[1] + (i & 1))*monitorHeight/2;
			}
			else
			{
				cell = ScreenThirds[key - SECONDARY_NUMPAD_ID-1]; 
				x = (cell[0] + (i & 1))*monitorWidth/3;
				y = (cell[1] + (i & 1))*monitorHeight/3;
			}
			if (x<Left)
				Left = x;
			if (x>Right)
				Right = x;
			if (y<Top)
				Top = y;
			if (y>Bottom)
				Bottom = y;
			if (i==1)
				key = msg->wParam;
		}

		Left += monitor.left;
		Right += monitor.left;
		Top += monitor.top;
		Bottom += monitor.top;

		if (Left == monitor.left && Top == monitor.top && Right == monitor.right && Bottom == monitor.bottom)
		{
			ShowWindow(activeWnd, SW_SHOWMAXIMIZED);
		}
		else
		{
			if (IsZoomed(activeWnd))
			{
				ShowWindow(activeWnd,SW_SHOWNORMAL);
			}
			MoveWindow(activeWnd,Left, Top, Right-Left, Bottom-Top,TRUE);
		}
	
		LastTick=0;
	}
	else
	{
		FirstHotkeyId = msg->wParam;
	}
	LastTick = Tick;
}


void MonitorInfo::GetMonitorInformation()
{
	int MonitorCount = GetSystemMetrics(SM_CMONITORS);
	Gui->LogMessage(
		L"GetMonitorInformation says there %s %d monitor%s",
		MonitorCount>1 ? L"are" : L"is", 
		MonitorCount, 
		MonitorCount>1 ? L"s" : L"");

	monitors_.clear();

	if (FALSE == EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback, reinterpret_cast<LPARAM>(this)))
	{
		Gui->LogMessage(L"Could not enumerate monitors",GetLastError());
		timerId_ = SetTimer(
			NULL,
			timerId_,
			5000,
			&MonitorInfo::DelayedQueryProc);
	}
}

BOOL CALLBACK MonitorInfo::EnumMonitorCallback(
  __in  HMONITOR hMonitor,
  __in  HDC hdcMonitor,
  __in  LPRECT lprcMonitor,
  __in  LPARAM dwData
)
{
	MonitorInfo *this_ = reinterpret_cast<MonitorInfo*>(dwData);
	MONITOR_INFORMATION mi;
	mi.Info.cbSize = sizeof(mi.Info);
	mi.Handle = hMonitor;
	GetMonitorInfo(mi.Handle,&mi.Info);
	Gui->LogMessage(L"monitor[%d]: l:%d, t:%d, r:%d, b:%d",
		this_->Count(), mi.Info.rcWork.left, mi.Info.rcWork.top, mi.Info.rcWork.right, mi.Info.rcWork.bottom);
	this_->monitors_.push_back(mi);
	return TRUE;
}

VOID CALLBACK MonitorInfo::DelayedQueryProc(
	_In_ HWND     hwnd,
	_In_ UINT     uMsg,
	_In_ UINT_PTR idEvent,
	_In_ DWORD    dwTime
)
{
	//typedef VOID(CALLBACK* TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
	Monitors.GetMonitorInformation();
}


void MonitorInfo::TryDelayedQuery()
{
	
}


ApplicationGui::ApplicationGui(HINSTANCE hInstance, int nCmdShow)
{
	// Register the window class.
	const wchar_t CLASS_NAME[]  = L"Window teleport";
	UINT 
		nClientWidth = 600,
		nClientHeight = 400;

	WNDCLASS wc = { };

	wc.lpfnWndProc   = WindowProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	// Create the window.
	Hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Window Teleport",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, nClientWidth, nClientHeight,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (Hwnd == NULL)
	{
		throw Exception(L"Could not create window",GetLastError());
	}

	EditHwnd = CreateWindow(
		L"edit", 
		L"",
		WS_VISIBLE|WS_CHILD|WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOHSCROLL|ES_AUTOVSCROLL,
		0, 0, nClientWidth, nClientHeight, 
		Hwnd, NULL	, hInstance, NULL);

	ShowWindow(Hwnd, SW_HIDE);
	ShowWindow(EditHwnd,SW_MAXIMIZE);
	SendMessage(EditHwnd, EM_REPLACESEL, 0, (LPARAM) Usage); 

	Tnd.cbSize = sizeof(NOTIFYICONDATA);
	Tnd.hWnd = Hwnd;
	Tnd.uCallbackMessage = WM_TRAY_MESSAGE;
	Tnd.uID = TRAY_ICON_ID;
	Tnd.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
	Tnd.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON1));
	wcscpy_s(Tnd.szTip,L"Window Teleport");
	Shell_NotifyIcon(NIM_ADD,&Tnd);
}

ApplicationGui::~ApplicationGui()
{
	Shell_NotifyIcon(NIM_DELETE,&Tnd);
}

void ApplicationGui::LogMessage(LPCWSTR Message,...)
{
	va_list argptr;
	va_start( argptr, Message );
	int bytes = vswprintf_s(
		Buffer,
		LOG_MESSAGE_BUFFER_SIZE-3,
		Message,
		argptr);
	Buffer[bytes]='\r';
	Buffer[++bytes] = '\n';
	Buffer[++bytes] = 0;
	SendMessage(EditHwnd, EM_REPLACESEL, 0, (LPARAM) Buffer);
}