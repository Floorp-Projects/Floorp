/*****************************************************************
 *               nsProcess NSIS plugin v1.5                      *
 *                                                               *
 * 2006 Shengalts Aleksander aka Instructor (Shengalts@mail.ru)  *
 *                                                               *
 * Source function FIND_PROC_BY_NAME based                       *
 *   upon the Ravi Kochhar (kochhar@physiology.wisc.edu) code    *
 * Thanks iceman_k (FindProcDLL plugin) and                      *
 *   DITMan (KillProcDLL plugin) for point me up                 *
 *****************************************************************/

#define UNICODE
#define _UNICODE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Tlhelp32.h>
#include "ConvFunc.h"

/* Defines */
#define NSIS_MAX_STRLEN 1024

#define SystemProcessInformation     5
#define STATUS_SUCCESS               0x00000000L
#define STATUS_INFO_LENGTH_MISMATCH  0xC0000004L

typedef struct _SYSTEM_THREAD_INFO {
  FILETIME ftCreationTime;
  DWORD dwUnknown1;
  DWORD dwStartAddress;
  DWORD dwOwningPID;
  DWORD dwThreadID;
  DWORD dwCurrentPriority;
  DWORD dwBasePriority;
  DWORD dwContextSwitches;
  DWORD dwThreadState;
  DWORD dwUnknown2;
  DWORD dwUnknown3;
  DWORD dwUnknown4;
  DWORD dwUnknown5;
  DWORD dwUnknown6;
  DWORD dwUnknown7;
} SYSTEM_THREAD_INFO;

typedef struct _SYSTEM_PROCESS_INFO {
  DWORD dwOffset;
  DWORD dwThreadCount;
  DWORD dwUnkown1[6];
  FILETIME ftCreationTime;
  DWORD dwUnkown2;
  DWORD dwUnkown3;
  DWORD dwUnkown4;
  DWORD dwUnkown5;
  DWORD dwUnkown6;
  WCHAR *pszProcessName;
  DWORD dwBasePriority;
  DWORD dwProcessID;
  DWORD dwParentProcessID;
  DWORD dwHandleCount;
  DWORD dwUnkown7;
  DWORD dwUnkown8;
  DWORD dwVirtualBytesPeak;
  DWORD dwVirtualBytes;
  DWORD dwPageFaults;
  DWORD dwWorkingSetPeak;
  DWORD dwWorkingSet;
  DWORD dwUnkown9;
  DWORD dwPagedPool;
  DWORD dwUnkown10;
  DWORD dwNonPagedPool;
  DWORD dwPageFileBytesPeak;
  DWORD dwPageFileBytes;
  DWORD dwPrivateBytes;
  DWORD dwUnkown11;
  DWORD dwUnkown12;
  DWORD dwUnkown13;
  DWORD dwUnkown14;
  SYSTEM_THREAD_INFO ati[ANYSIZE_ARRAY];
} SYSTEM_PROCESS_INFO;


/* Include conversion functions */
#ifdef UNICODE
#define xatoiW
#define xitoaW
#else
#define xatoi
#define xitoa
#endif
#include "ConvFunc.h"

/* NSIS stack structure */
typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[1];
} stack_t;

stack_t **g_stacktop;
TCHAR *g_variables;
unsigned int g_stringsize;

#define EXDLL_INIT()        \
{                           \
  g_stacktop=stacktop;      \
  g_variables=variables;    \
  g_stringsize=string_size; \
}

/* Global variables */
TCHAR szBuf[NSIS_MAX_STRLEN];

/* Funtions prototypes and macros */
int FIND_PROC_BY_NAME(TCHAR *szProcessName, BOOL bTerminate);
int popinteger();
void pushinteger(int integer);
int popstring(TCHAR *str, int len);
void pushstring(const TCHAR *str, int len);


/* NSIS functions code */
void __declspec(dllexport) _FindProcess(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop)
{
  EXDLL_INIT();
  {
    int nError;

    popstring(szBuf, NSIS_MAX_STRLEN);
    nError=FIND_PROC_BY_NAME(szBuf, FALSE);
    pushinteger(nError);
  }
}

void __declspec(dllexport) _KillProcess(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop)
{
  EXDLL_INIT();
  {
    int nError;

    popstring(szBuf, NSIS_MAX_STRLEN);
    nError=FIND_PROC_BY_NAME(szBuf, TRUE);
    pushinteger(nError);
  }
}

void __declspec(dllexport) _Unload(HWND hwndParent, int string_size,
                                      TCHAR *variables, stack_t **stacktop)
{
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  return TRUE;
}

int FIND_PROC_BY_NAME(TCHAR *szProcessName, BOOL bTerminate)
// Find the process "szProcessName" if it is currently running.
// This works for Win95/98/ME and also WinNT/2000/XP.
// The process name is case-insensitive, i.e. "notepad.exe" and "NOTEPAD.EXE"
// will both work. If bTerminate is TRUE, then process will be terminated.
//
// Return codes are as follows:
//   0   = Success
//   601 = No permission to terminate process
//   602 = Not all processes terminated successfully
//   603 = Process was not currently running
//   604 = Unable to identify system type
//   605 = Unsupported OS
//   606 = Unable to load NTDLL.DLL
//   607 = Unable to get procedure address from NTDLL.DLL
//   608 = NtQuerySystemInformation failed
//   609 = Unable to load KERNEL32.DLL
//   610 = Unable to get procedure address from KERNEL32.DLL
//   611 = CreateToolhelp32Snapshot failed
//
// Change history:
//   created  06/23/2000  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                            http://www.neurophys.wisc.edu/ravi/software/
//   modified 03/08/2002  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                          - Borland-C compatible if BORLANDC is defined as
//                            suggested by Bob Christensen
//   modified 03/10/2002  - Ravi Kochhar (kochhar@physiology.wisc.edu)
//                          - Removed memory leaks as suggested by
//                            Jonathan Richard-Brochu (handles to Proc and Snapshot
//                            were not getting closed properly in some cases)
//   modified 14/11/2005  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Combine functions FIND_PROC_BY_NAME and KILL_PROC_BY_NAME
//                          - Code has been optimized
//                          - Now kill all processes with specified name (not only one)
//                          - Cosmetic improvements
//                          - Removed error 632 (Invalid process name)
//                          - Changed error 602 (Unable to terminate process for some other reason)
//                          - BORLANDC define not needed
//   modified 04/01/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed CRT dependency
//   modified 21/04/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by {_trueparuex^}
//                            (handle to hSnapShot was not getting closed properly in some cases)
//   modified 21/04/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by {_trueparuex^}
//                            (handle to hSnapShot was not getting closed properly in some cases)
//   modified 19/07/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Code for WinNT/2000/XP has been rewritten
//                          - Changed error codes
//   modified 31/08/2006  - Shengalts Aleksander aka Instructor (Shengalts@mail.ru):
//                          - Removed memory leak as suggested by Daniel Vanesse
{
#ifndef UNICODE
  char szName[MAX_PATH];
#endif
  OSVERSIONINFO osvi;
  HMODULE hLib;
  HANDLE hProc;
  ULONG uError;
  BOOL bFound=FALSE;
  BOOL bSuccess=FALSE;
  BOOL bFailed=FALSE;

  // First check what version of Windows we're in
  osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
  if (!GetVersionEx(&osvi)) return 604;

  if (osvi.dwPlatformId != VER_PLATFORM_WIN32_NT &&
      osvi.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS)
    return 605;

  if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
  {
    // WinNT/2000/XP

    SYSTEM_PROCESS_INFO *spi;
    SYSTEM_PROCESS_INFO *spiCount;
    DWORD dwSize=0x4000;
    DWORD dwData;
    ULONG (WINAPI *NtQuerySystemInformationPtr)(ULONG, PVOID, LONG, PULONG);

    if (hLib=LoadLibraryA("NTDLL.DLL"))
    {
      NtQuerySystemInformationPtr=(ULONG(WINAPI *)(ULONG, PVOID, LONG, PULONG))GetProcAddress(hLib, "NtQuerySystemInformation");

      if (NtQuerySystemInformationPtr)
      {
        while (1)
        {
          if (spi=LocalAlloc(LMEM_FIXED, dwSize))
          {
            uError=(*NtQuerySystemInformationPtr)(SystemProcessInformation, spi, dwSize, &dwData);

            if (uError == STATUS_SUCCESS) break;

            LocalFree(spi);

            if (uError != STATUS_INFO_LENGTH_MISMATCH)
            {
              uError=608;
              break;
            }
          }
          else
          {
            uError=608;
            break;
          }
          dwSize*=2;
        }
      }
      else uError=607;

      FreeLibrary(hLib);
    }
    else uError=606;

    if (uError != STATUS_SUCCESS) return uError;

    spiCount=spi;

    while (1)
    {
      if (spiCount->pszProcessName)
      {
#ifdef UNICODE
        if (!lstrcmpi(spiCount->pszProcessName, szProcessName))
#else
        WideCharToMultiByte(CP_ACP, 0, spiCount->pszProcessName, -1, szName, MAX_PATH, NULL, NULL);

        if (!lstrcmpi(szName, szProcessName))
#endif
        {
          // Process found
          bFound=TRUE;

          if (bTerminate == TRUE)
          {
            // Open for termination
            if (hProc=OpenProcess(PROCESS_TERMINATE, FALSE, spiCount->dwProcessID))
            {
              if (TerminateProcess(hProc, 0))
                bSuccess=TRUE;
              else
                bFailed=TRUE;
              CloseHandle(hProc);
            }
          }
          else break;
        }
      }
      if (spiCount->dwOffset == 0) break;
      spiCount=(SYSTEM_PROCESS_INFO *)((char *)spiCount + spiCount->dwOffset);
    }
    LocalFree(spi);
  }
  else
  {
    // Win95/98/ME

    PROCESSENTRY32 pe;
    TCHAR *pName;
    HANDLE hSnapShot;
    BOOL bResult;
    HANDLE (WINAPI *CreateToolhelp32SnapshotPtr)(DWORD, DWORD);
    BOOL (WINAPI *Process32FirstPtr)(HANDLE, LPPROCESSENTRY32);
    BOOL (WINAPI *Process32NextPtr)(HANDLE, LPPROCESSENTRY32);

    if (hLib=LoadLibraryA("KERNEL32.DLL"))
    {
      CreateToolhelp32SnapshotPtr=(HANDLE(WINAPI *)(DWORD, DWORD)) GetProcAddress(hLib, "CreateToolhelp32Snapshot");
      Process32FirstPtr=(BOOL(WINAPI *)(HANDLE, LPPROCESSENTRY32)) GetProcAddress(hLib, "Process32First");
      Process32NextPtr=(BOOL(WINAPI *)(HANDLE, LPPROCESSENTRY32)) GetProcAddress(hLib, "Process32Next");

      if (CreateToolhelp32SnapshotPtr && Process32NextPtr && Process32FirstPtr)
      {
        // Get a handle to a Toolhelp snapshot of all the systems processes.
        if ((hSnapShot=(*CreateToolhelp32SnapshotPtr)(TH32CS_SNAPPROCESS, 0)) != INVALID_HANDLE_VALUE)
        {
          // Get the first process' information.
          pe.dwSize=sizeof(PROCESSENTRY32);
          bResult=(*Process32FirstPtr)(hSnapShot, &pe);

          // While there are processes, keep looping and checking.
          while (bResult)
          {
            //Get file name
            for (pName=pe.szExeFile + lstrlen(pe.szExeFile) - 1; *pName != '\\' && *pName != '\0'; --pName);

            if (!lstrcmpi(++pName, szProcessName))
            {
              // Process found
              bFound=TRUE;

              if (bTerminate == TRUE)
              {
                // Open for termination
                if (hProc=OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID))
                {
                  if (TerminateProcess(hProc, 0))
                    bSuccess=TRUE;
                  else
                    bFailed=TRUE;
                  CloseHandle(hProc);
                }
              }
              else break;
            }
            //Keep looking
            bResult=(*Process32NextPtr)(hSnapShot, &pe);
          }
          CloseHandle(hSnapShot);
        }
        else uError=611;
      }
      else uError=610;

      FreeLibrary(hLib);
    }
    else uError=609;
  }

  if (bFound == FALSE) return 603;
  if (bTerminate == TRUE)
  {
    if (bSuccess == FALSE) return 601;
    if (bFailed == TRUE) return 602;
  }
  return 0;
}

int popinteger()
{
  TCHAR szInt[32];

  popstring(szInt, 32);
#ifdef UNICODE
  return xatoiW(szInt);
#else
  return xatoi(szInt);
#endif
}

void pushinteger(int integer)
{
  TCHAR szInt[32];

#ifdef UNICODE
  xitoaW(integer, szInt, 0);
#else
  xitoa(integer, szInt, 0);
#endif
  pushstring(szInt, 32);
}

//Function: Removes the element from the top of the NSIS stack and puts it in the buffer
int popstring(TCHAR *str, int len)
{
  stack_t *th;

  if (!g_stacktop || !*g_stacktop) return 1;
  th=(*g_stacktop);
  lstrcpyn(str, th->text, len);
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
