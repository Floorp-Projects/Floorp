#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsCNativeApp.h"
#include "nsIEventLoop.h"
#include <windows.h>

static NS_DEFINE_CID(kNativeAppCID, NS_NATIVE_APP_CID);

LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ErrorBox(LPSTR text)
{
	MessageBox(NULL, text, "XP Event Loop", MB_OK | MB_ICONSTOP);
}

void InfoBox(LPSTR text)
{
	MessageBox(NULL, text, "XP Event Loop", MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prevInstance, LPSTR lpszCmdLine,
	int nShowCmd)
{
	char* lpszAppName = "HelloWorld";
	HWND wnd;
	WNDCLASSEX wndclass;

	nsCOMPtr<nsIServiceManager> servMgr;
	NS_InitXPCOM(getter_AddRefs(servMgr));
	nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);

	nsresult rv;
	NS_WITH_SERVICE(nsINativeApp, nativeAppService, kNativeAppCID, &rv);

	if(NS_FAILED(rv))
		{
		ErrorBox("Failed to get nativeAppService");
		return -1;
		}
	wndclass.cbSize        = sizeof(wndclass);
   wndclass.style         = CS_HREDRAW | CS_VREDRAW;
   wndclass.lpfnWndProc   = WndProc;
   wndclass.cbClsExtra    = 0;
   wndclass.cbWndExtra    = 0;
   wndclass.hInstance     = inst;
   wndclass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
   wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
   wndclass.lpszMenuName  = NULL;
   wndclass.lpszClassName = lpszAppName;
   wndclass.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

   RegisterClassEx(&wndclass) ;

   wnd = CreateWindow(lpszAppName, "The Hello World",
                    WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    NULL, NULL, inst, NULL);		     

	ShowWindow(wnd, nShowCmd);
	UpdateWindow(wnd);

	nsCOMPtr<nsIEventLoop> eventLoop;

	if(NS_FAILED(nativeAppService->CreateEventLoop(L"_MainLoop", 
		nsEventLoopTypes::MainAppLoop, getter_AddRefs(eventLoop))))
		{
		ErrorBox("Failed to create event Loop");
		return 0;
		} 


	int retCode;
	eventLoop->Run(nsnull, nsnull, nsnull, &retCode);

	InfoBox("Hello World app is out of loop");

	NS_ShutdownXPCOM(servMgr);
	InfoBox("Hello World app is exiting");
	return retCode;
}

LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC         hdc;
	PAINTSTRUCT ps;
	RECT        rect;

	switch(msg)
		{
		case WM_PAINT:
			hdc = BeginPaint(wnd, &ps);

			GetClientRect(wnd, &rect);

			DrawText(hdc, "Hello, XP Event Loop!", -1, &rect,
					 DT_SINGLELINE | DT_CENTER | DT_VCENTER);

			EndPaint(wnd, &ps);
			return 0;

		case WM_DESTROY:
			{
			nsresult rv;
			NS_WITH_SERVICE(nsINativeApp, nativeAppService, kNativeAppCID, &rv);
			if(NS_FAILED(rv))
				{
				ErrorBox("Could not get NativeAppService");
				return 0;
				}
			nsCOMPtr<nsIEventLoop> eventLoop;

			if(NS_FAILED(nativeAppService->FindEventLoop(L"_MainLoop", 
				getter_AddRefs(eventLoop))))
				{
				ErrorBox("Failed to find event Loop");
				return 0;
				}
			eventLoop->Exit(0);
			}
			return 0;
		}

	return DefWindowProc(wnd, msg, wParam, lParam);
}
