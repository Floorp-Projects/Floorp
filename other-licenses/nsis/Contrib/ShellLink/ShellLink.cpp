/*
Module : ShellLink.cpp
Purpose: NSIS Plug-in for retriving shell link information
Created: 12/16/2003
Last Update: 01/14/2004
                          
Copyright (c) 2004 Angelo Mandato.  
See ShellLink.html for more information


Modified: 21/09/2005
Author:   Shengalts Aleksander aka Instructor (Shengalts@mail.ru)
Changes:  -code has been rewritten
          -added functions to change shell link information
          -reduced dll size 44Kb -> 4Kb
*/

#define UNICODE
#define _UNICODE

//  Uncomment for debugging message boxes
//#define SHELLLINK_DEBUG

#include <windows.h>
#include <shlobj.h>

#define xatoi
#include "ConvFunc.h"

#define MAX_STRLEN 1024
#define SHELLLINKTYPE_GETARGS 1
#define SHELLLINKTYPE_GETDESC 2
#define SHELLLINKTYPE_GETHOTKEY 3
#define SHELLLINKTYPE_GETICONLOC 4
#define SHELLLINKTYPE_GETICONINDEX 5
#define SHELLLINKTYPE_GETPATH 6
#define SHELLLINKTYPE_GETSHOWMODE 7
#define SHELLLINKTYPE_GETWORKINGDIR 8
#define SHELLLINKTYPE_SETARGS 9
#define SHELLLINKTYPE_SETDESC 10
#define SHELLLINKTYPE_SETHOTKEY 11
#define SHELLLINKTYPE_SETICONLOC 12
#define SHELLLINKTYPE_SETICONINDEX 13
#define SHELLLINKTYPE_SETPATH 14
#define SHELLLINKTYPE_SETSHOWMODE 15
#define SHELLLINKTYPE_SETWORKINGDIR 16

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_STRLEN];
} stack_t;

stack_t **g_stacktop;
unsigned int g_stringsize;
TCHAR *g_variables;
TCHAR szBuf[MAX_STRLEN]=TEXT("");
TCHAR szBuf2[MAX_STRLEN]=TEXT("");
int nBuf;
WORD wHotkey;

void ShortCutData(int nType, HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop);
int popstring(TCHAR *str);
void pushstring(TCHAR *str);

//Get
extern "C" void __declspec(dllexport) GetShortCutArgs(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETARGS, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutDescription(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETDESC, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutHotkey(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETHOTKEY, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutIconLocation(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETICONLOC, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutIconIndex(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETICONINDEX, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutTarget(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETPATH, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutShowMode(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETSHOWMODE, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) GetShortCutWorkingDirectory(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_GETWORKINGDIR, hwndParent, string_size, variables, stacktop);
}

//Set
extern "C" void __declspec(dllexport) SetShortCutArgs(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETARGS, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutDescription(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETDESC, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutHotkey(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETHOTKEY, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutIconLocation(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETICONLOC, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutIconIndex(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETICONINDEX, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutTarget(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETPATH, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutShowMode(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{

	ShortCutData(SHELLLINKTYPE_SETSHOWMODE, hwndParent, string_size, variables, stacktop);
}

extern "C" void __declspec(dllexport) SetShortCutWorkingDirectory(HWND hwndParent, int string_size, 
                                      TCHAR *variables, stack_t **stacktop)
{
	ShortCutData(SHELLLINKTYPE_SETWORKINGDIR, hwndParent, string_size, variables, stacktop);
}

void ShortCutData(int nType, HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  g_stringsize=string_size;
  g_stacktop=stacktop;
  g_variables=variables;
  {
	HRESULT hRes;
	IShellLink* m_psl;
	IPersistFile* m_ppf;

	popstring(szBuf);
	if (nType > SHELLLINKTYPE_GETWORKINGDIR) popstring(szBuf2);

	hRes=CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*) &m_psl);
	if (hRes == S_OK)
	{
		hRes=m_psl->QueryInterface(IID_IPersistFile, (LPVOID*) &m_ppf);
		if (hRes == S_OK)
		{
#ifdef UNICODE
			hRes=m_ppf->Load(szBuf, STGM_READWRITE);
#else
			WCHAR wszPath[MAX_PATH];
			MultiByteToWideChar(CP_ACP, 0, szBuf, -1, wszPath, MAX_PATH);

			hRes=m_ppf->Load(wszPath, STGM_READWRITE);
#endif
			if (hRes == S_OK)
			{
				if (nType <= SHELLLINKTYPE_GETWORKINGDIR)
				{
					//Get
					switch(nType)
					{
						case SHELLLINKTYPE_GETARGS:
						{
							hRes=m_psl->GetArguments(szBuf, MAX_PATH);
							if (hRes != S_OK) szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETDESC: 
						{
							hRes=m_psl->GetDescription(szBuf, MAX_PATH);
							if (hRes != S_OK) szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETHOTKEY: 
						{
							hRes=m_psl->GetHotkey(&wHotkey);
							if (hRes == S_OK) wsprintf(szBuf, TEXT("%d"), wHotkey);
							else szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETICONLOC: 
						{
							hRes=m_psl->GetIconLocation(szBuf, MAX_PATH, &nBuf);
							if (hRes != S_OK) szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETICONINDEX: 
						{
							hRes=m_psl->GetIconLocation(szBuf, MAX_PATH, &nBuf);
							if (hRes == S_OK) wsprintf(szBuf, TEXT("%d"), nBuf);
							else szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETPATH: 
						{
							WIN32_FIND_DATA fd;

							hRes=m_psl->GetPath(szBuf, MAX_PATH, &fd, SLGP_UNCPRIORITY);
							if (hRes != S_OK) szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETSHOWMODE: 
						{
							hRes=m_psl->GetShowCmd(&nBuf);
							if (hRes == S_OK) wsprintf(szBuf, TEXT("%d"), nBuf);
							else szBuf[0]=TEXT('\0');
						}; break;
						case SHELLLINKTYPE_GETWORKINGDIR:
						{ 
							hRes=m_psl->GetWorkingDirectory(szBuf, MAX_PATH);
							if (hRes != S_OK) szBuf[0]=TEXT('\0');
						}; break;
					}
				}
				else
				{
					//Set
					switch(nType)
					{
						case SHELLLINKTYPE_SETARGS:
						{
							hRes=m_psl->SetArguments(szBuf2);
						}; break;
						case SHELLLINKTYPE_SETDESC: 
						{
							hRes=m_psl->SetDescription(szBuf2);
						}; break;
						case SHELLLINKTYPE_SETHOTKEY:
						{
							wHotkey=(unsigned short)xatoi(szBuf2);
							hRes=m_psl->SetHotkey(wHotkey);
						}; break;
						case SHELLLINKTYPE_SETICONLOC:
						{
							hRes=m_psl->GetIconLocation(szBuf, MAX_PATH, &nBuf);
							if (hRes == S_OK)
								hRes=m_psl->SetIconLocation(szBuf2, nBuf);
						}; break;
						case SHELLLINKTYPE_SETICONINDEX: 
						{
							int nBuf2;
							nBuf=xatoi(szBuf2);
							hRes=m_psl->GetIconLocation(szBuf, MAX_PATH, &nBuf2);
							if (hRes == S_OK)
								hRes=m_psl->SetIconLocation(szBuf, nBuf);
						}; break;
						case SHELLLINKTYPE_SETPATH: 
						{
							hRes=m_psl->SetPath(szBuf2);
						}; break;
						case SHELLLINKTYPE_SETSHOWMODE: 
						{
							nBuf=xatoi(szBuf2);
							hRes=m_psl->SetShowCmd(nBuf);
						}; break;
						case SHELLLINKTYPE_SETWORKINGDIR:
						{ 
							hRes=m_psl->SetWorkingDirectory(szBuf2);
						}; break;
					}
					if (hRes == S_OK) hRes=m_ppf->Save(NULL, FALSE);
					#ifdef SHELLLINK_DEBUG
					else MessageBox(hwndParent, "ERROR: Save()", "ShellLink", MB_OK);
					#endif
				}
			}
			#ifdef SHELLLINK_DEBUG
			else MessageBox(hwndParent, "ERROR: Load()", "ShellLink", MB_OK);
			#endif
		}
		#ifdef SHELLLINK_DEBUG
		else MessageBox(hwndParent, "CShellLink::Initialise, Failed in call to QueryInterface for IPersistFile, HRESULT was %x\n", "ShellLink", MB_OK);
		#endif

		// Cleanup:
		if (m_ppf) m_ppf->Release();
		if (m_psl) m_psl->Release();
	}
	#ifdef SHELLLINK_DEBUG
	else MessageBox(hwndParent, "ERROR: CoCreateInstance()", "ShellLink", MB_OK);
	#endif

	if (hRes == S_OK)
	{
		if (nType <= SHELLLINKTYPE_GETWORKINGDIR) pushstring(szBuf);
		else pushstring(TEXT("0"));
	}
	else
	{
		if (nType <= SHELLLINKTYPE_GETWORKINGDIR) pushstring(TEXT(""));
		else pushstring(TEXT("-1"));
	}
  }
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

int popstring(TCHAR *str)
{
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th=(*g_stacktop);
  lstrcpy(str,th->text);
  *g_stacktop=th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

void pushstring(TCHAR *str)
{
  stack_t *th;
  if (!g_stacktop) return;
  th=(stack_t*)GlobalAlloc(GPTR,sizeof(stack_t)+g_stringsize);
  lstrcpyn(th->text,str,g_stringsize);
  th->next=*g_stacktop;
  *g_stacktop=th;
}
