// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _EXDLL_H_
#define _EXDLL_H_

#include <windows.h>
#include <tchar.h>

#define PLUGINFUNCTION(name)                                   \
  extern "C" void __declspec(dllexport)                        \
      name(HWND hWndParent, int string_size, TCHAR* variables, \
           stack_t** stacktop, extra_parameters* extra)

#define EXDLL_INIT()                                  \
  {                                                   \
    g_stringsize = string_size;                       \
    g_stacktop = stacktop;                            \
    g_executeCodeSegment = extra->ExecuteCodeSegment; \
    g_hwndParent = hWndParent;                        \
  }

#define WM_NOTIFY_OUTER_NEXT (WM_USER + 0x8)
#define WM_NOTIFY_CUSTOM_READY (WM_USER + 0xd)

typedef struct _stack_t {
  struct _stack_t* next;
  TCHAR text[1];  // the real length of this buffer should be string_size
} stack_t;

extern unsigned int g_stringsize;
extern stack_t** g_stacktop;
extern int(__stdcall* g_executeCodeSegment)(int pos, HWND hwndProgress);
extern HWND g_hwndParent;
extern HINSTANCE gHInst;

typedef struct {
  int autoclose;
  int all_user_var;
  int exec_error;
  int abort;
  int exec_reboot;
  int reboot_called;
  int XXX_cur_insttype;      // deprecated
  int XXX_insttype_changed;  // deprecated
  int silent;
  int instdir_error;
  int rtl;
  int errlvl;
} exec_flags;

// NSIS Plug-In Callback Messages
enum NSPIM {
  NSPIM_UNLOAD,     // This is the last message a plugin gets, do final cleanup
  NSPIM_GUIUNLOAD,  // Called after .onGUIEnd
};

typedef UINT_PTR (*NSISPLUGINCALLBACK)(enum NSPIM);

typedef struct {
  exec_flags* exec_flags;
  int(__stdcall* ExecuteCodeSegment)(int pos, HWND hwndProgress);
  void(__stdcall* validate_filename)(LPWSTR);
  int(__stdcall* RegisterPluginCallback)(
      HMODULE,
      NSISPLUGINCALLBACK);  // returns 0 on success, 1 if already
                            // registered and < 0 on errors
} extra_parameters;

int popstring(TCHAR* str);
void pushstring(const TCHAR* str);

UINT_PTR __cdecl NSISPluginCallback(NSPIM msg);

#endif  //_EXDLL_H_
