/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ConsoleHost.h"
#include "ConsoleHostImpl.h"

#if __has_include("InputHook.h")
#include "InputHook.h"
#endif

#include <thread>
#include <condition_variable>

#include <CoreConsole.h>

#include <LaunchMode.h>

static std::thread g_consoleThread;
static std::once_flag g_consoleInitialized;
bool g_consoleFlag;
extern int g_scrollTop;
extern int g_bufferHeight;

void ProcessWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& pass, LRESULT& lresult)
{
	if (g_consoleFlag)
	{
		static bool g_consoleClosing = false;

		// should the console be closed?
		if (wParam == VK_F8)
		{
			if (msg == WM_KEYDOWN)
			{
				g_consoleClosing = true;
				return;
			}

			if (g_consoleClosing)
			{
				if (msg == WM_KEYUP)
				{
					g_consoleClosing = false;
					g_consoleFlag = false;
					InputHook::SetGameMouseFocus(true);

					return;
				}
			}
		}
	}
}

static InitFunction initFunction([] ()
{
	console::CoreAddPrintListener([](ConsoleChannel channel, const char* msg)
	{
		ConHost::Print(channel, msg);
	});

	static ConsoleCommand quitCommand("quit", []()
	{
		// NetHook will replace ExitProcess with a version that terminates NetLibrary as well as calling TerminateProcess
		if (!CfxIsSinglePlayer())
		{
			ExitProcess(-1);
		}
		else
		{
			TerminateProcess(GetCurrentProcess(), -1);
		}
	});

#if __has_include("InputHook.h")
	InputHook::QueryInputTarget.Connect([](std::vector<InputTarget*>& targets)
	{
		if (g_consoleFlag)
		{
			static bool g_consoleClosing = false;

			static struct : InputTarget
			{
				virtual void KeyDown(UINT vKey, UINT scanCode) override
				{
					if (vKey == VK_F8)
					{
						g_consoleClosing = true;
					}
				}

				virtual void KeyUp(UINT vKey, UINT scanCode) override
				{
					if (vKey == VK_F8 && g_consoleClosing)
					{
						g_consoleClosing = false;
						g_consoleFlag = false;
					}
				}
			} closeTarget;

			targets.push_back(&closeTarget);
		}
		else
		{
			static struct : InputTarget
			{
				virtual void KeyUp(UINT vKey, UINT scanCode) override
				{
					if (vKey == VK_F8)
					{
						g_consoleFlag = true;
					}
				}
			} openTarget;

			targets.push_back(&openTarget);
		}

		return true;
	}, -100);

	static bool hadConsoleFlag = false;

	InputHook::DeprecatedOnWndProc.Connect([](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& pass, LRESULT& lresult)
	{
		hadConsoleFlag = g_consoleFlag;

		ProcessWndProc(hWnd, msg, wParam, lParam, pass, lresult);
	}, -10);

	// check this last so we don't keep the event from the game due to g_consoleFlag
	InputHook::DeprecatedOnWndProc.Connect([](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& pass, LRESULT& lresult)
	{
		// check a *different* flag so it doesn't get toggled back on when we just disabled the console
		if (!hadConsoleFlag)
		{
			// check if the console should be opened
			if (msg == WM_KEYUP && wParam == VK_F8)
			{
				g_consoleFlag = true;
				InputHook::SetGameMouseFocus(false);
			}
		}
	}, 10);
#endif
});
