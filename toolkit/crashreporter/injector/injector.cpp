/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "client/windows/handler/exception_handler.h"

using google_breakpad::ExceptionHandler;
using std::wstring;

BOOL WINAPI DummyEntryPoint(HINSTANCE instance,
                            DWORD reason,
                            void* reserved)
{
  __debugbreak();

  return FALSE; // We're being loaded remotely, this shouldn't happen!
}

// support.microsoft.com/kb/94248
extern "C" BOOL WINAPI _CRT_INIT(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved); 

extern "C"
__declspec(dllexport) DWORD Start(void* context)
{
  // Because the remote DLL injector does not call DllMain, we have to
  // initialize the CRT manually
  _CRT_INIT(NULL, DLL_PROCESS_ATTACH, NULL);

  HANDLE hCrashPipe = reinterpret_cast<HANDLE>(context);

  ExceptionHandler* e = new (std::nothrow)
    ExceptionHandler(wstring(), NULL, NULL, NULL,
                     ExceptionHandler::HANDLER_ALL,
                     MiniDumpNormal, hCrashPipe, NULL);
  if (e)
    e->set_handle_debug_exceptions(true);
  return 1;
}
