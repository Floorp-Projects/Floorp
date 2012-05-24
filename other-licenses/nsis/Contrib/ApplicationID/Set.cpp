/*
 * Module : Set.cpp
 * Purpose: NSIS Plug-in for setting shortcut ApplicationID property
 * Created: 27/12/2009
 * Original code Copyright (c) 2009 Mike Anchor.  
 */

/*
 * Additional Mozilla contributions:
 *  Unicode support
 *  Jump list deletion on uninstall
 *  Pinned item removal on uninstall
 *  contrib: <jmathies@mozilla.com>
 */

#include <windows.h>
#include <shlobj.h>
#include <propvarutil.h>
#include <propkey.h>

#pragma comment (lib, "shlwapi.lib")

#define MAX_STRLEN 1024

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[MAX_PATH];
} stack_t;

stack_t **g_stacktop;
unsigned int g_stringsize;
TCHAR *g_variables;

int popstring(TCHAR *str, int len);
void pushstring(const TCHAR *str, int len);

extern "C" void __declspec(dllexport) Set(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  g_stringsize = string_size;
  g_stacktop   = stacktop;
  g_variables  = variables;

  {
    IPropertyStore *m_pps = NULL;
    WCHAR wszPath[MAX_PATH];
    WCHAR wszAppID[MAX_PATH];
    TCHAR szPath[MAX_PATH];
    TCHAR szAppID[MAX_PATH];
    bool success = false;

    ZeroMemory(wszPath, sizeof(wszPath));
    ZeroMemory(wszAppID, sizeof(wszAppID));
    ZeroMemory(szPath, sizeof(szPath));
    ZeroMemory(szAppID, sizeof(szAppID));

    popstring(szPath, MAX_PATH);
    popstring(szAppID, MAX_PATH);

#if !defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, szPath, -1, wszPath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, szAppID, -1, wszAppID, MAX_PATH);
#else
    wcscpy(wszPath, szPath);
    wcscpy(wszAppID, szAppID);
#endif

    ::CoInitialize(NULL);

    if (SUCCEEDED(SHGetPropertyStoreFromParsingName(wszPath, NULL, GPS_READWRITE, IID_PPV_ARGS(&m_pps))))
    {
      PROPVARIANT propvar;
      if (SUCCEEDED(InitPropVariantFromString(wszAppID, &propvar)))
      {
        if (SUCCEEDED(m_pps->SetValue(PKEY_AppUserModel_ID, propvar)))
        {
          if (SUCCEEDED(m_pps->Commit()))
          {
            success = true;
          }
        }
      }
    }

    if (m_pps != NULL)
      m_pps->Release();

    CoUninitialize();

    pushstring(success == true ? TEXT("0") : TEXT("-1"), MAX_PATH);
  }
}

extern "C" void __declspec(dllexport) UninstallJumpLists(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  g_stringsize = string_size;
  g_stacktop   = stacktop;
  g_variables  = variables;

  ICustomDestinationList *m_cdl = NULL;
  WCHAR wszAppID[MAX_PATH];
  TCHAR szAppID[MAX_PATH];
  bool success = false;

  ZeroMemory(wszAppID, sizeof(wszAppID));
  ZeroMemory(szAppID, sizeof(szAppID));

  popstring(szAppID, MAX_PATH);

#if !defined(UNICODE)
  MultiByteToWideChar(CP_ACP, 0, szAppID, -1, wszAppID, MAX_PATH);
#else
  wcscpy(wszAppID, szAppID);
#endif

  CoInitialize(NULL);
  
  CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICustomDestinationList, (void**)&m_cdl);

  if (m_cdl) {
    if (SUCCEEDED(m_cdl->DeleteList(wszAppID))) {
      success = true;
    }
  }

  if (m_cdl)
    m_cdl->Release();

  CoUninitialize();

  pushstring(success == true ? TEXT("0") : TEXT("-1"), MAX_PATH);
}

extern "C" void __declspec(dllexport) UninstallPinnedItem(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop)
{
  g_stringsize = string_size;
  g_stacktop   = stacktop;
  g_variables  = variables;

  IShellItem *pItem = NULL;
  IStartMenuPinnedList *pPinnedList = NULL;
  WCHAR wszPath[MAX_PATH];
  TCHAR szPath[MAX_PATH];
  bool success = false;

  ZeroMemory(wszPath, sizeof(wszPath));
  ZeroMemory(szPath, sizeof(szPath));

  popstring(szPath, MAX_PATH);

#if !defined(UNICODE)
  MultiByteToWideChar(CP_ACP, 0, szPath, -1, wszPath, MAX_PATH);
#else
  wcscpy(wszPath, szPath);
#endif

  CoInitialize(NULL);

  HRESULT hr;
  hr = SHCreateItemFromParsingName(wszPath, NULL, IID_PPV_ARGS(&pItem));

  if (SUCCEEDED(hr)) {

      hr = CoCreateInstance(CLSID_StartMenuPin, 
                            NULL, 
                            CLSCTX_INPROC_SERVER, 
                            IID_PPV_ARGS(&pPinnedList));
      
      if (SUCCEEDED(hr)) {
          hr = pPinnedList->RemoveFromList(pItem);
          pPinnedList->Release();
          success = true;
      }
      
      pItem->Release();
  }

  CoUninitialize();

  pushstring(success == true ? TEXT("0") : TEXT("-1"), MAX_PATH);
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  return TRUE;
}

//Function: Removes the element from the top of the NSIS stack and puts it in the buffer
int popstring(TCHAR *str, int len)
{
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th=(*g_stacktop);
  lstrcpyn(str,th->text, len);
  *g_stacktop=th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

//Function: Adds an element to the top of the NSIS stack
void pushstring(const TCHAR *str, int len)
{
  stack_t *th;

  if (!g_stacktop) return;
  th=(stack_t*)GlobalAlloc(GPTR, sizeof(stack_t) + len);
  lstrcpyn(th->text, str, len);
  th->next=*g_stacktop;
  *g_stacktop=th;
}
