//
// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license
//

#if (defined(_MSC_VER) && !defined(_DEBUG))
  #pragma comment(linker,"/opt:nowin98")
  #pragma comment(linker,"/ignore:4078")
  #pragma comment(linker,"/merge:.rdata=.text")
#endif

#ifdef UNICODE
# ifndef _UNICODE
#    define _UNICODE
#  endif
#endif

#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <tchar.h>
#include <wininet.h>

#if defined(_DEBUG) || 0
#  define PLUGIN_DEBUG 1
void MYTRACE(LPCTSTR fmt, ...)
{
  va_list argptr;
  va_start(argptr, fmt);
  TCHAR buffer[2048] = { _T('\0') };
  wvsprintf(buffer, fmt, argptr);
  buffer[(sizeof(buffer)/sizeof(*buffer)) - 1] = _T('\0');
  OutputDebugString(buffer);
  va_end(argptr);
}
#else
void MYTRACE(...) { }
#endif
#  define TRACE MYTRACE

#ifndef ASSERT
#  define ASSERT(x)
#endif

#define NSISPIEXPORTFUNC EXTERN_C void __declspec(dllexport) __cdecl

namespace NSIS {

#define NSISCALL __stdcall
typedef struct _xparams_t {
  LPVOID xx1;//exec_flags_type *exec_flags;
  int (NSISCALL *ExecuteCodeSegment)(int, HWND);
  void (NSISCALL *validate_filename)(TCHAR*);
  int (NSISCALL *RegisterPluginCallback)(HMODULE,LPVOID);
} xparams_t;
typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[1];
} stack_t;

} // namespace NSIS

enum NSPIM 
{
  NSPIM_UNLOAD,
  NSPIM_GUIUNLOAD,
};
