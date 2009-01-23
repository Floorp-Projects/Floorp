// Unicode support by Jim Park -- 08/23/2007
// Jim Park: Should probably turn this into a nice class for C++ programs.

#pragma once
#include <windows.h>
#include <tchar.h>
// only include this file from one place in your DLL.
// (it is all static, if you use it in two places it will fail)

#define EXDLL_INIT()           {  \
        g_stringsize=string_size; \
        g_stacktop=stacktop;      \
        g_variables=variables; }

// For page showing plug-ins
#define WM_NOTIFY_OUTER_NEXT (WM_USER+0x8)
#define WM_NOTIFY_CUSTOM_READY (WM_USER+0xd)

/* Jim Park: This char is compared as an int value and therefore
   it's fine as an ASCII.  Do not need to change to wchar_t since
   it will get the same integer value. */
#define NOTIFY_BYE_BYE _T('x')

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[1]; // this should be the length of string_size
} stack_t;

static unsigned int g_stringsize;
static stack_t **g_stacktop;
static TCHAR *g_variables;


enum
{
INST_0,         // $0
INST_1,         // $1
INST_2,         // $2
INST_3,         // $3
INST_4,         // $4
INST_5,         // $5
INST_6,         // $6
INST_7,         // $7
INST_8,         // $8
INST_9,         // $9
INST_R0,        // $R0
INST_R1,        // $R1
INST_R2,        // $R2
INST_R3,        // $R3
INST_R4,        // $R4
INST_R5,        // $R5
INST_R6,        // $R6
INST_R7,        // $R7
INST_R8,        // $R8
INST_R9,        // $R9
INST_CMDLINE,   // $CMDLINE
INST_INSTDIR,   // $INSTDIR
INST_OUTDIR,    // $OUTDIR
INST_EXEDIR,    // $EXEDIR
INST_LANG,      // $LANGUAGE
__INST_LAST
};

typedef struct {
  int autoclose;
  int all_user_var;
  int exec_error;
  int abort;
  int exec_reboot;
  int reboot_called;
  int XXX_cur_insttype; // deprecated
  int XXX_insttype_changed; // deprecated
  int silent;
  int instdir_error;
  int rtl;
  int errlvl;
  int alter_reg_view;
} exec_flags_type;

typedef struct {
  exec_flags_type *exec_flags;
  int (__stdcall *ExecuteCodeSegment)(int, HWND);
  void (__stdcall *validate_filename)(TCHAR *);
} extra_parameters;

static int    __stdcall popstring(TCHAR *str); // 0 on success, 1 on empty stack
static void   __stdcall pushstring(const TCHAR *str);
static char * __stdcall getuservariable(const int varnum);
static void   __stdcall setuservariable(const int varnum, const TCHAR *var);

#ifdef _UNICODE
#define PopStringW(x) popstring(x)
#define PushStringW(x) pushstring(x)
#define SetUserVariableW(x,y) setuservariable(x,y)

static int  __stdcall PopStringA(char* ansiStr);
static void __stdcall PushStringA(const char* ansiStr);
static void __stdcall GetUserVariableW(const int varnum, wchar_t* wideStr);
static void __stdcall GetUserVariableA(const int varnum, char* ansiStr);
static void __stdcall SetUserVariableA(const int varnum, const char* ansiStr);

#else
// ANSI defs

#define PopStringA(x) popstring(x)
#define PushStringA(x) pushstring(x)
#define SetUserVariableA(x,y) setuservariable(x,y)

static int  __stdcall PopStringW(wchar_t* wideStr);
static void __stdcall PushStringW(wchar_t* wideStr);
static void __stdcall GetUserVariableW(const int varnum, wchar_t* wideStr);
static void __stdcall GetUserVariableA(const int varnum, char* ansiStr);
static void __stdcall SetUserVariableW(const int varnum, const wchar_t* wideStr);

#endif

static BOOL __stdcall IsUnicode(void)
static TCHAR* __stdcall AllocString();

