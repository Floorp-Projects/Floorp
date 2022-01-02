// Unicode support by Jim Park -- 08/23/2007

#include <windows.h>
#include "exdllutil.h"

// utility functions (not required but often useful)
static int __stdcall popstring(TCHAR *str)
{
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th=(*g_stacktop);
  lstrcpy(str,th->text);
  *g_stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

static void __stdcall pushstring(const TCHAR *str)
{
  stack_t *th;
  if (!g_stacktop) return;
  th=(stack_t*)GlobalAlloc(GPTR,(sizeof(stack_t)+(g_stringsize)*sizeof(TCHAR)));
  lstrcpyn(th->text,str,g_stringsize);
  th->next=*g_stacktop;
  *g_stacktop=th;
}

static TCHAR * __stdcall getuservariable(const int varnum)
{
  if (varnum < 0 || varnum >= __INST_LAST) return NULL;
  return g_variables+varnum*g_stringsize;
}

static void __stdcall setuservariable(const int varnum, const TCHAR *var)
{
	if (var != NULL && varnum >= 0 && varnum < __INST_LAST) 
		lstrcpy(g_variables + varnum*g_stringsize, var);
}

#ifdef _UNICODE
static int __stdcall PopStringA(char* ansiStr)
{
   wchar_t* wideStr = (wchar_t*) GlobalAlloc(GPTR, g_stringsize*sizeof(wchar_t));
   int rval = popstring(wideStr);
   WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, g_stringsize, NULL, NULL);
   GlobalFree((HGLOBAL)wideStr);
   return rval;
}

static void __stdcall PushStringA(const char* ansiStr)
{
   wchar_t* wideStr = (wchar_t*) GlobalAlloc(GPTR, g_stringsize*sizeof(wchar_t));
   MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, wideStr, g_stringsize);
   pushtring(wideStr);
   GlobalFree((HGLOBAL)wideStr);
   return;
}

static void __stdcall GetUserVariableW(const int varnum, wchar_t* wideStr)
{
   lstrcpyW(wideStr, getuservariable(varnum));
}

static void __stdcall GetUserVariableA(const int varnum, char* ansiStr)
{
   wchar_t* wideStr = getuservariable(varnum);
   WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, g_stringsize, NULL, NULL);
}

static void __stdcall SetUserVariableA(const int varnum, const char* ansiStr)
{
   if (ansiStr != NULL && varnum >= 0 && varnum < __INST_LAST)
   {
      wchar_t* wideStr = g_variables + varnum * g_stringsize;
      MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, wideStr, g_stringsize);
   }
}

#else
// ANSI defs
static int __stdcall PopStringW(wchar_t* wideStr)
{
   char* ansiStr = (char*) GlobalAlloc(GPTR, g_stringsize);
   int rval = popstring(ansiStr);
   MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, wideStr, g_stringsize);
   GlobalFree((HGLOBAL)ansiStr);
   return rval;
}

static void __stdcall PushStringW(wchar_t* wideStr)
{
   char* ansiStr = (char*) GlobalAlloc(GPTR, g_stringsize);
   WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, g_stringsize, NULL, NULL);
   pushstring(ansiStr);
   GlobalFree((HGLOBAL)ansiStr);
}

static void __stdcall GetUserVariableW(const int varnum, wchar_t* wideStr)
{
   char* ansiStr = getuservariable(varnum);
   MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, wideStr, g_stringsize);
}

static void __stdcall GetUserVariableA(const int varnum, char* ansiStr)
{
   lstrcpyA(ansiStr, getuservariable(varnum));
}

static void __stdcall SetUserVariableW(const int varnum, const wchar_t* wideStr)
{
   if (wideStr != NULL && varnum >= 0 && varnum < __INST_LAST)
   {
      char* ansiStr = g_variables + varnum * g_stringsize;
      WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, g_stringsize, NULL, NULL);
   }
}
#endif

static BOOL __stdcall IsUnicode(void)
{
#ifdef _UNICODE
   return TRUE;
#else
   return FALSE;
#endif
}

static TCHAR* __stdcall AllocString()
{
   return (TCHAR*) GlobalAlloc(GPTR, g_stringsize*sizeof(TCHAR));
}
