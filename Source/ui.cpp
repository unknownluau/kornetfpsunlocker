#include <Windows.h>
#include <shellapi.h>

#include <cstdio>

#include "ui.h"
#include "resource.h"
#include "settings.h"
#include "KFU.h"

#define KFU_TRAYICON			WM_APP + 1
#define KFU_TRAYMENU_APC		WM_APP + 2
#define KFU_TRAYMENU_CONSOLE	WM_APP + 3
#define KFU_TRAYMENU_EXIT		WM_APP + 4
#define KFU_TRAYMENU_VSYNC		WM_APP + 5
#define KFU_TRAYMENU_LOADSET	WM_APP + 6
#define KFU_TRAYMENU_GITHUB		WM_APP + 7
#define KFU_TRAYMENU_STUDIO		WM_APP + 8
#define KFU_TRAYMENU_KFU		WM_APP + 9
#define KFU_TRAYMENU_ADV_NBE	WM_APP + 10
#define KFU_TRAYMENU_ADV_SE		WM_APP + 11
#define KFU_TRAYMENU_ADV_QS		WM_APP + 12
#define KFU_TRAYMENU_CLIENT		WM_APP + 13

#define KFU_FCS_FIRST			(WM_APP + 20)
#define KFU_FCS_NONE			KFU_FCS_FIRST + 0
#define KFU_FCS_30				KFU_FCS_FIRST + 1
#define KFU_FCS_60				KFU_FCS_FIRST + 2
#define KFU_FCS_75				KFU_FCS_FIRST + 3
#define KFU_FCS_100				KFU_FCS_FIRST + 4
#define KFU_FCS_120				KFU_FCS_FIRST + 5
#define KFU_FCS_144				KFU_FCS_FIRST + 6
#define KFU_FCS_240				KFU_FCS_FIRST + 7
#define KFU_FCS_LAST			(KFU_FCS_240)

HWND UI::Window = NULL;
int UI::AttachedProcessesCount = 0;
bool UI::IsConsoleOnly = false;

HANDLE WatchThread;
NOTIFYICONDATA NotifyIconData;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case KFU_TRAYICON:
	{
		if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN)
		{
			POINT position;
			GetCursorPos(&position);

			HMENU popup = CreatePopupMenu();

			char buffer[64];
			sprintf_s(buffer, "Attached Processes: %d", UI::AttachedProcessesCount);

			AppendMenu(popup, MF_STRING | MF_GRAYED, KFU_TRAYMENU_APC, buffer);
			AppendMenu(popup, MF_SEPARATOR, 0, NULL);

			AppendMenu(popup, MF_STRING | (Settings::UnlockClient ? MF_CHECKED : 0), KFU_TRAYMENU_CLIENT, "Unlock Kornet Player");
			AppendMenu(popup, MF_STRING | (Settings::UnlockStudio ? MF_CHECKED : 0), KFU_TRAYMENU_STUDIO, "Unlock Kornet Studio");
			AppendMenu(popup, MF_STRING | (Settings::CheckForUpdates ? MF_CHECKED : 0), KFU_TRAYMENU_KFU, "Check for Updates");

			HMENU submenu = CreatePopupMenu();
			AppendMenu(submenu, MF_STRING, KFU_FCS_NONE, "None");
			AppendMenu(submenu, MF_STRING, KFU_FCS_30, "30");
			AppendMenu(submenu, MF_STRING, KFU_FCS_60, "60");
			AppendMenu(submenu, MF_STRING, KFU_FCS_75, "75");
			AppendMenu(submenu, MF_STRING, KFU_FCS_100, "100");
			AppendMenu(submenu, MF_STRING, KFU_FCS_120, "120");
			AppendMenu(submenu, MF_STRING, KFU_FCS_144, "144");
			AppendMenu(submenu, MF_STRING, KFU_FCS_240, "240");
			CheckMenuRadioItem(submenu, KFU_FCS_FIRST, KFU_FCS_LAST, KFU_FCS_FIRST + Settings::FPSCapSelection, MF_BYCOMMAND);
			AppendMenu(popup, MF_POPUP, (UINT_PTR)submenu, "FPS Cap");

			HMENU advanced = CreatePopupMenu();
			AppendMenu(advanced, MF_STRING | (Settings::SilentErrors ? MF_CHECKED : 0), KFU_TRAYMENU_ADV_SE, "Silent Errors");
			AppendMenu(advanced, MF_STRING | (Settings::SilentErrors ? MF_GRAYED : 0) | (Settings::NonBlockingErrors ? MF_CHECKED : 0), KFU_TRAYMENU_ADV_NBE, "Use Console Errors");
			AppendMenu(advanced, MF_STRING | (Settings::QuickStart ? MF_CHECKED : 0), KFU_TRAYMENU_ADV_QS, "Quick Start");
			AppendMenu(popup, MF_POPUP, (UINT_PTR)advanced, "Advanced");

			AppendMenu(popup, MF_SEPARATOR, 0, NULL);
			AppendMenu(popup, MF_STRING, KFU_TRAYMENU_LOADSET, "Load Settings");
			AppendMenu(popup, MF_STRING, KFU_TRAYMENU_CONSOLE, "Toggle Console");
			AppendMenu(popup, MF_STRING, KFU_TRAYMENU_GITHUB, "Visit GitHub");
			AppendMenu(popup, MF_STRING, KFU_TRAYMENU_EXIT, "Exit");

			SetForegroundWindow(hwnd);
			BOOL result = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, position.x, position.y, 0, hwnd, NULL);

			if (result != 0)
			{
				switch (result)
				{
				case KFU_TRAYMENU_EXIT:
					SetFPSCapExternal(60);
					Shell_NotifyIcon(NIM_DELETE, &NotifyIconData);
					TerminateThread(WatchThread, 0);
					FreeConsole();
					PostQuitMessage(0);
					break;

				case KFU_TRAYMENU_CONSOLE:
					UI::ToggleConsole();
					break;

				case KFU_TRAYMENU_GITHUB:
					ShellExecuteA(NULL, "open", "https://github.com/" KFU_GITHUB_REPO, NULL, NULL, SW_SHOWNORMAL);
					break;

				case KFU_TRAYMENU_LOADSET:
					Settings::Load();
					Settings::Update();
					break;

				case KFU_TRAYMENU_CLIENT:
					Settings::UnlockClient = !Settings::UnlockClient;
					CheckMenuItem(popup, KFU_TRAYMENU_CLIENT, Settings::UnlockClient ? MF_CHECKED : MF_UNCHECKED);
					break;
				
				case KFU_TRAYMENU_STUDIO:
					Settings::UnlockStudio = !Settings::UnlockStudio;
					CheckMenuItem(popup, KFU_TRAYMENU_STUDIO, Settings::UnlockStudio ? MF_CHECKED : MF_UNCHECKED);
					break;

				case KFU_TRAYMENU_KFU:
					Settings::CheckForUpdates = !Settings::CheckForUpdates;
					CheckMenuItem(popup, KFU_TRAYMENU_KFU, Settings::CheckForUpdates ? MF_CHECKED : MF_UNCHECKED);
					break;

				case KFU_TRAYMENU_ADV_NBE:
					Settings::NonBlockingErrors = !Settings::NonBlockingErrors;
					CheckMenuItem(popup, KFU_TRAYMENU_ADV_NBE, Settings::NonBlockingErrors ? MF_CHECKED : MF_UNCHECKED);
					break;

				case KFU_TRAYMENU_ADV_SE:
					Settings::SilentErrors = !Settings::SilentErrors;
					CheckMenuItem(popup, KFU_TRAYMENU_ADV_SE, Settings::SilentErrors ? MF_CHECKED : MF_UNCHECKED);
					if (Settings::SilentErrors) CheckMenuItem(popup, KFU_TRAYMENU_ADV_NBE, MF_GRAYED);
					break;

				case KFU_TRAYMENU_ADV_QS:
					Settings::QuickStart = !Settings::QuickStart;
					CheckMenuItem(popup, KFU_TRAYMENU_ADV_QS, Settings::QuickStart ? MF_CHECKED : MF_UNCHECKED);
					break;

				default:
					if (result >= KFU_FCS_FIRST
						&& result <= KFU_FCS_LAST)
					{
						static double fcs_map[] = { 0.0, 30.0, 60.0, 75.0, 100.0, 120.0, 144.0, 240.0 };
						Settings::FPSCapSelection = result - KFU_FCS_FIRST;
						Settings::FPSCap = fcs_map[Settings::FPSCapSelection];
					}
				}

				if (result != KFU_TRAYMENU_CONSOLE
					&& result != KFU_TRAYMENU_LOADSET
					&& result != KFU_TRAYMENU_GITHUB
					&& result != KFU_TRAYMENU_EXIT)
				{
					Settings::Update();
					Settings::Save();
				}
			}

			return 1;
		}

		break;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

bool IsConsoleVisible = false;

void UI::SetConsoleVisible(bool visible)
{
	IsConsoleVisible = visible;
	ShowWindow(GetConsoleWindow(), visible ? SW_SHOWNORMAL : SW_HIDE);
}

void UI::CreateHiddenConsole()
{
	AllocConsole();

	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);

	if (!UI::IsConsoleOnly)
	{
		HMENU menu = GetSystemMenu(GetConsoleWindow(), FALSE);
		EnableMenuItem(menu, SC_CLOSE, MF_GRAYED);
	}

#ifdef _WIN64
	SetConsoleTitleA("Kornet FPS Unlocker " KFU_VERSION " Console");
#endif

	SetConsoleVisible(false);
}

bool UI::ToggleConsole()
{
	if (!GetConsoleWindow())
		UI::CreateHiddenConsole();

	SetConsoleVisible(!IsConsoleVisible);

	return IsConsoleVisible;
}

int UI::Start(HINSTANCE instance, LPTHREAD_START_ROUTINE watchthread)
{	
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(wcex);
	wcex.style = 0;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "KFUClass";
	wcex.hIconSm = NULL;

	RegisterClassEx(&wcex);

	UI::Window = CreateWindow("KFUClass", "Kornet FPS Unlocker", 0, 0, 0, 0, 0, NULL, NULL, instance, NULL);
	if (!UI::Window)
		return 0;

	NotifyIconData.cbSize = sizeof(NotifyIconData);
	NotifyIconData.hWnd = UI::Window;
	NotifyIconData.uID = IDI_ICON1;
	NotifyIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	NotifyIconData.uCallbackMessage = KFU_TRAYICON;
	NotifyIconData.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1));
	strcpy_s(NotifyIconData.szTip, "Kornet FPS Unlocker");

	Shell_NotifyIcon(NIM_ADD, &NotifyIconData);

	WatchThread = CreateThread(NULL, 0, watchthread, NULL, NULL, NULL);

	BOOL ret;
	MSG msg;

	while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (ret != -1)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}
