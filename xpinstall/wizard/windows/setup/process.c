/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "process.h"
#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include <tlhelp32.h>
#include <winperf.h>

#define INDEX_STR_LEN               10
#define PN_PROCESS                  TEXT("Process")
#define PN_PROCESS_ID               TEXT("ID Process")
#define PN_THREAD                   TEXT("Thread")

/* CW: Close Window */
#define CW_CLOSE_ALL               0x00000001
#define CW_CLOSE_VISIBLE_ONLY      0x00000002
#define CW_CHECK_VISIBLE_ONLY      0x00000003

typedef PERF_DATA_BLOCK             PERF_DATA,      *PPERF_DATA;
typedef PERF_OBJECT_TYPE            PERF_OBJECT,    *PPERF_OBJECT;
typedef PERF_INSTANCE_DEFINITION    PERF_INSTANCE,  *PPERF_INSTANCE;
typedef BOOL   (WINAPI *NS_ProcessWalk)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef HANDLE (WINAPI *NS_CreateSnapshot)(DWORD dwFlags, DWORD th32ProcessID);

TCHAR   INDEX_PROCTHRD_OBJ[2*INDEX_STR_LEN];
DWORD   PX_PROCESS;
DWORD   PX_PROCESS_ID;
DWORD   PX_THREAD;

BOOL _FindAndKillProcessNT4(LPSTR aProcessName,
                         BOOL aKillProcess);
BOOL KillProcess(char *aProcessName, HWND aHwndProcess, DWORD aProcessID);
DWORD GetTitleIdx(HWND hWnd, LPTSTR Title[], DWORD LastIndex, LPTSTR Name);
PPERF_OBJECT FindObject (PPERF_DATA pData, DWORD TitleIndex);
PPERF_OBJECT NextObject (PPERF_OBJECT pObject);
PPERF_OBJECT FirstObject (PPERF_DATA pData);
PPERF_INSTANCE   NextInstance (PPERF_INSTANCE pInst);
PPERF_INSTANCE   FirstInstance (PPERF_OBJECT pObject);
DWORD   GetPerfData    (HKEY        hPerfKey,
                        LPTSTR      szObjectIndex,
                        PPERF_DATA  *ppData,
                        DWORD       *pDataSize);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM aParam);

typedef struct _CAWOWH
{
  DWORD cwState;
  DWORD processID;
} CAWOWH;

typedef struct _kpf
{
  NS_CreateSnapshot pCreateToolhelp32Snapshot;
  NS_ProcessWalk    pProcessWalkFirst;
  NS_ProcessWalk    pProcessWalkNext;
} kpf;

BOOL _FindAndKillProcess(kpf *kpfRoutines, LPSTR aProcessName, BOOL aKillProcess);




/* Function: FindAndKillProcess()
 *
 *       in: LPSTR aProcessName: Name of process to find and kill
 *           BOOL  aKillProcess: Indicates whether to kill the process
 *                               or not.
 *  purpose: To find and kill a given process name currently running.
 */
BOOL FindAndKillProcess(LPSTR aProcessName, BOOL aKillProcess)
{
  HANDLE hKernel       = NULL;
  BOOL   bDoWin95Check = TRUE;
  kpf    kpfRoutines;

  if((hKernel = GetModuleHandle("kernel32.dll")) == NULL)
    return(FALSE);

  kpfRoutines.pCreateToolhelp32Snapshot = (NS_CreateSnapshot)GetProcAddress(hKernel, "CreateToolhelp32Snapshot");
  kpfRoutines.pProcessWalkFirst         = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32First");
  kpfRoutines.pProcessWalkNext          = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32Next");

  if(kpfRoutines.pCreateToolhelp32Snapshot && kpfRoutines.pProcessWalkFirst && kpfRoutines.pProcessWalkNext)
    return(_FindAndKillProcess(&kpfRoutines, aProcessName, aKillProcess));
  else
    return(_FindAndKillProcessNT4(aProcessName, aKillProcess));
}

/* Function: KillProcess()
 *
 *       in: LPSTR aProcessName: Name of process to find and kill
 *           BOOL  aKillProcess: Indicates whether to kill the process
 *                               or not.
 *  purpose: To find and kill a given process name currently running.
 */
BOOL KillProcess(char *aProcessName, HWND aHwndProcess, DWORD aProcessID)
{
  BOOL rv = FALSE;
  HWND hwndProcess;

  hwndProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, aProcessID);
  if(!hwndProcess)
    return(rv);

  if(!TerminateProcess(hwndProcess, 1))
  {
    char errorMsg[MAX_BUF];

    if(GetPrivateProfileString("Messages", "ERROR_TERMINATING_PROCESS", "", errorMsg, sizeof(errorMsg), szFileIniInstall))
    {
      char buf[MAX_BUF];

      wsprintf(buf, errorMsg, aProcessName);
      PrintError(buf, ERROR_CODE_SHOW);
    }
  }
  else
    rv = TRUE;

  CloseHandle(hwndProcess);
  return(rv);
}

// Windows callback function that processes each window found to be running.
// This function will close all the visible and invisible windows that
// match a processes passed in from aParam.
BOOL CALLBACK EnumWindowsProc(HWND aHwnd, LPARAM aParam)
{
  BOOL   rv = TRUE;
  DWORD  processID;
  CAWOWH *closeWindowInfo = (CAWOWH *)aParam;

  GetWindowThreadProcessId(aHwnd, &processID);
  if(processID == closeWindowInfo->processID)
  {
    switch(closeWindowInfo->cwState)
    {
      case CW_CLOSE_ALL:
        SendMessageTimeout(aHwnd, WM_CLOSE, (WPARAM)1, (LPARAM)0, SMTO_NORMAL, WM_CLOSE_TIMEOUT_VALUE, NULL);
        break;

      case CW_CLOSE_VISIBLE_ONLY:
        // only close the windows that are visible
        if(GetWindowLong(aHwnd, GWL_STYLE) & WS_VISIBLE)
          SendMessageTimeout(aHwnd, WM_CLOSE, (WPARAM)1, (LPARAM)0, SMTO_NORMAL, WM_CLOSE_TIMEOUT_VALUE, NULL);
        break;

      case CW_CHECK_VISIBLE_ONLY:
        /* Check for visible windows.  If there are any, then the previous
         * call to close all visible windows had failed (most likely due
         * to user canceling a save request on a window) */
        if(GetWindowLong(aHwnd, GWL_STYLE) & WS_VISIBLE)
          rv = FALSE;
        break;
    }
  }

  return(rv); // Returning TRUE will continue with the enumeration!
              // it will automatically stop when there's no more
              // windows to process.
              // Returning FALSE will stop immediately.
}


/* Function: CloseAllWindowsOfWindowHandle()
 *
 *       in: HWND aHwndWindow: Handle to the process window to close.
 *           char *aMsgWait  : Message to show while closing the windows.
 *
 *  purpose: To quit an application by closing all of its vibible and
 *           invisible windows.  There are 3 states to closing all windows:
 *            1) Close all visible windows
 *            2) Check to make sure all visible windows have been closed.
 *               The user could be in editor and not want to close that
 *               particular window just yet.
 *            3) Close the rest of the windows (invisible or not) if 2)
 *               passed.
 */
BOOL CloseAllWindowsOfWindowHandle(HWND aHwndWindow, char *aMsgWait)
{
  CAWOWH closeWindowInfo;
  BOOL  rv = TRUE;

  assert(aHwndWindow);
  assert(aMsgWait);
  if(*aMsgWait != '\0')
    ShowMessage(aMsgWait, TRUE);

  GetWindowThreadProcessId(aHwndWindow, &closeWindowInfo.processID);

  /* only close the visible windows */
  closeWindowInfo.cwState = CW_CLOSE_VISIBLE_ONLY;
  EnumWindows(EnumWindowsProc, (LPARAM)&closeWindowInfo);

  /* only check to make sure that the visible windows were closed */
  closeWindowInfo.cwState = CW_CHECK_VISIBLE_ONLY;
  rv = EnumWindows(EnumWindowsProc, (LPARAM)&closeWindowInfo);
  if(rv)
  {
    /* All visible windows have been closed.  Close all remaining windows now */
    closeWindowInfo.cwState = CW_CLOSE_ALL;
    EnumWindows(EnumWindowsProc, (LPARAM)&closeWindowInfo);
  }
  if(*aMsgWait != '\0')
    ShowMessage(aMsgWait, FALSE);

  return(rv);
}

/* Function: _FindAndKillProcess()
 *
 *       in: LPSTR aProcessName: Name of process to find and kill
 *           BOOL  aKillProcess: Indicates whether to kill the process
 *                               or not.
 *  purpose: To find and kill a given process name currently running. This
 *           function only works under Win9x, Win2k, and WinXP systems.
 */
BOOL _FindAndKillProcess(kpf *kpfRoutines, LPSTR aProcessName, BOOL aKillProcess)
{
  BOOL            rv              = FALSE;
  HANDLE          hCreateSnapshot = NULL;
  PROCESSENTRY32  peProcessEntry;
  
  hCreateSnapshot = kpfRoutines->pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(hCreateSnapshot == (HANDLE)-1)
    return(rv);

  peProcessEntry.dwSize = sizeof(PROCESSENTRY32);
  if(kpfRoutines->pProcessWalkFirst(hCreateSnapshot, &peProcessEntry))
  {
    char  szBuf[MAX_BUF];

    do
    {
      ParsePath(peProcessEntry.szExeFile, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
      
      /* do process name string comparison here */
      if(lstrcmpi(szBuf, aProcessName) == 0)
      {
        rv = TRUE;
        if(aKillProcess)
          KillProcess(aProcessName, NULL, peProcessEntry.th32ProcessID);
        else
          break;
      }

    } while(kpfRoutines->pProcessWalkNext(hCreateSnapshot, &peProcessEntry));
  }

  CloseHandle(hCreateSnapshot);
  return(rv);
}

//
// The functions below were copied from MSDN 6.0:
//
//   \samples\vc98\sdk\sdktools\winnt\pviewer
//
// They were listed in the redist.txt file from the cd.
// Only _FindAndKillProcessNT4() was modified to accomodate the setup needs.
//
/******************************************************************************\
 * *       This is a part of the Microsoft Source Code Samples.
 * *       Copyright (C) 1993-1997 Microsoft Corporation.
 * *       All rights reserved.
\******************************************************************************/
BOOL _FindAndKillProcessNT4(LPSTR aProcessName, BOOL aKillProcess)
{
  BOOL          bRv;
  HKEY          hKey;
  DWORD         dwType;
  DWORD         dwSize;
  DWORD         dwTemp;
  DWORD         dwTitleLastIdx;
  DWORD         dwIndex;
  DWORD         dwLen;
  DWORD         pDataSize = 50 * 1024;
  LPSTR         szCounterValueName;
  LPSTR         szTitle;
  LPSTR         *szTitleSz;
  LPSTR         szTitleBuffer;
  PPERF_DATA    pData;
  PPERF_OBJECT  pProcessObject;

  bRv   = FALSE;
  hKey  = NULL;

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                  TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\perflib"),
                  0,
                  KEY_READ,
                  &hKey) != ERROR_SUCCESS)
    return(bRv);

  dwSize = sizeof(dwTitleLastIdx);
  if(RegQueryValueEx(hKey, TEXT("Last Counter"), 0, &dwType, (LPBYTE)&dwTitleLastIdx, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    return(bRv);
  }
    

  dwSize = sizeof(dwTemp);
  if(RegQueryValueEx(hKey, TEXT("Version"), 0, &dwType, (LPBYTE)&dwTemp, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    szCounterValueName = TEXT("Counters");
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                    TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009"),
                    0,
                    KEY_READ,
                    &hKey) != ERROR_SUCCESS)
      return(bRv);
  }
  else
  {
    RegCloseKey(hKey);
    szCounterValueName = TEXT("Counter 009");
    hKey = HKEY_PERFORMANCE_DATA;
  }

  // Find out the size of the data.
  //
  dwSize = 0;
  if(RegQueryValueEx(hKey, szCounterValueName, 0, &dwType, 0, &dwSize) != ERROR_SUCCESS)
    return(bRv);


  // Allocate memory
  //
  szTitleBuffer = (LPTSTR)LocalAlloc(LMEM_FIXED, dwSize);
  if(!szTitleBuffer)
  {
    RegCloseKey(hKey);
    return(bRv);
  }

  szTitleSz = (LPTSTR *)LocalAlloc(LPTR, (dwTitleLastIdx + 1) * sizeof(LPTSTR));
  if(!szTitleSz)
  {
    if(szTitleBuffer != NULL)
    {
      LocalFree(szTitleBuffer);
      szTitleBuffer = NULL;
    }

    RegCloseKey(hKey);
    return(bRv);
  }

  // Query the data
  //
  if(RegQueryValueEx(hKey, szCounterValueName, 0, &dwType, (BYTE *)szTitleBuffer, &dwSize) != ERROR_SUCCESS)
  {
    RegCloseKey(hKey);
    if(szTitleSz)
      LocalFree(szTitleSz);
    if(szTitleBuffer)
      LocalFree(szTitleBuffer);

    return(bRv);
  }

  // Setup the TitleSz array of pointers to point to beginning of each title string.
  // TitleBuffer is type REG_MULTI_SZ.
  //
  szTitle = szTitleBuffer;
  while(dwLen = lstrlen(szTitle))
  {
    dwIndex = atoi(szTitle);
    szTitle = szTitle + dwLen + 1;

    if(dwIndex <= dwTitleLastIdx)
      szTitleSz[dwIndex] = szTitle;

    szTitle = szTitle + lstrlen(szTitle) + 1;
  }

  PX_PROCESS    = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_PROCESS);
  PX_PROCESS_ID = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_PROCESS_ID);
  PX_THREAD     = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_THREAD);
  wsprintf(INDEX_PROCTHRD_OBJ, TEXT("%ld %ld"), PX_PROCESS, PX_THREAD);
  pData = NULL;
  if(GetPerfData(HKEY_PERFORMANCE_DATA, INDEX_PROCTHRD_OBJ, &pData, &pDataSize) == ERROR_SUCCESS)
  {
    PPERF_INSTANCE pInst;
    DWORD          i = 0;

    pProcessObject = FindObject(pData, PX_PROCESS);
    if(pProcessObject)
    {
      LPSTR szPtrStr;
      int   iLen;
      char  szProcessNamePruned[MAX_BUF];
      char  szNewProcessName[MAX_BUF];

      if(sizeof(szProcessNamePruned) < (lstrlen(aProcessName) + 1))
      {
        if(hKey)
          RegCloseKey(hKey);
        if(szTitleSz)
          LocalFree(szTitleSz);
        if(szTitleBuffer)
          LocalFree(szTitleBuffer);

        return(bRv);
      }

      /* look for .exe and remove from end of string because the Process names
       * under NT are returned without the extension */
      lstrcpy(szProcessNamePruned, aProcessName);
      iLen = lstrlen(szProcessNamePruned);
      szPtrStr = &szProcessNamePruned[iLen - 4];
      if((lstrcmpi(szPtrStr, ".exe") == 0) || (lstrcmpi(szPtrStr, ".dll") == 0))
        *szPtrStr = '\0';

      pInst = FirstInstance(pProcessObject);
      for(i = 0; i < (DWORD)(pProcessObject->NumInstances); i++)
      {
        *szNewProcessName = '\0';
        if(WideCharToMultiByte(CP_ACP,
                               0,
                               (LPCWSTR)((PCHAR)pInst + pInst->NameOffset),
                               -1,
                               szNewProcessName,
                               MAX_BUF,
                               NULL,
                               NULL) != 0)
        {
          if(lstrcmpi(szNewProcessName, szProcessNamePruned) == 0)
          {
            if(aKillProcess)
              KillProcess(aProcessName, NULL, PX_PROCESS_ID);
            bRv = TRUE;
            break;
          }
        }

        pInst = NextInstance(pInst);
      }
    }
  }

  if(hKey)
    RegCloseKey(hKey);
  if(szTitleSz)
    LocalFree(szTitleSz);
  if(szTitleBuffer)
    LocalFree(szTitleBuffer);

  return(bRv);
}


//*********************************************************************
//
//  GetPerfData
//
//      Get a new set of performance data.
//
//      *ppData should be NULL initially.
//      This function will allocate a buffer big enough to hold the
//      data requested by szObjectIndex.
//
//      *pDataSize specifies the initial buffer size.  If the size is
//      too small, the function will increase it until it is big enough
//      then return the size through *pDataSize.  Caller should
//      deallocate *ppData if it is no longer being used.
//
//      Returns ERROR_SUCCESS if no error occurs.
//
//      Note: the trial and error loop is quite different from the normal
//            registry operation.  Normally if the buffer is too small,
//            RegQueryValueEx returns the required size.  In this case,
//            the perflib, since the data is dynamic, a buffer big enough
//            for the moment may not be enough for the next. Therefor,
//            the required size is not returned.
//
//            One should start with a resonable size to avoid the overhead
//            of reallocation of memory.
//
DWORD   GetPerfData    (HKEY        hPerfKey,
                        LPTSTR      szObjectIndex,
                        PPERF_DATA  *ppData,
                        DWORD       *pDataSize)
{
DWORD   DataSize;
DWORD   dwR;
DWORD   Type;


    if (!*ppData)
        *ppData = (PPERF_DATA) LocalAlloc (LMEM_FIXED, *pDataSize);


    do  {
        DataSize = *pDataSize;
        dwR = RegQueryValueEx (hPerfKey,
                               szObjectIndex,
                               NULL,
                               &Type,
                               (BYTE *)*ppData,
                               &DataSize);

        if (dwR == ERROR_MORE_DATA)
            {
            LocalFree (*ppData);
            *pDataSize += 1024;
            *ppData = (PPERF_DATA) LocalAlloc (LMEM_FIXED, *pDataSize);
            }

        if (!*ppData)
            {
            LocalFree (*ppData);
            return ERROR_NOT_ENOUGH_MEMORY;
            }

        } while (dwR == ERROR_MORE_DATA);

    return dwR;
}




//*********************************************************************
//
//  FirstInstance
//
//      Returns pointer to the first instance of pObject type.
//      If pObject is NULL then NULL is returned.
//
PPERF_INSTANCE   FirstInstance (PPERF_OBJECT pObject)
{
    if (pObject)
        return (PPERF_INSTANCE)((PCHAR) pObject + pObject->DefinitionLength);
    else
        return NULL;
}

//*********************************************************************
//
//  NextInstance
//
//      Returns pointer to the next instance following pInst.
//
//      If pInst is the last instance, bogus data maybe returned.
//      The caller should do the checking.
//
//      If pInst is NULL, then NULL is returned.
//
PPERF_INSTANCE   NextInstance (PPERF_INSTANCE pInst)
{
PERF_COUNTER_BLOCK *pCounterBlock;

    if (pInst)
        {
        pCounterBlock = (PERF_COUNTER_BLOCK *)((PCHAR) pInst + pInst->ByteLength);
        return (PPERF_INSTANCE)((PCHAR) pCounterBlock + pCounterBlock->ByteLength);
        }
    else
        return NULL;
}

//*********************************************************************
//
//  FirstObject
//
//      Returns pointer to the first object in pData.
//      If pData is NULL then NULL is returned.
//
PPERF_OBJECT FirstObject (PPERF_DATA pData)
{
    if (pData)
        return ((PPERF_OBJECT) ((PBYTE) pData + pData->HeaderLength));
    else
        return NULL;
}




//*********************************************************************
//
//  NextObject
//
//      Returns pointer to the next object following pObject.
//
//      If pObject is the last object, bogus data maybe returned.
//      The caller should do the checking.
//
//      If pObject is NULL, then NULL is returned.
//
PPERF_OBJECT NextObject (PPERF_OBJECT pObject)
{
    if (pObject)
        return ((PPERF_OBJECT) ((PBYTE) pObject + pObject->TotalByteLength));
    else
        return NULL;
}

//*********************************************************************
//
//  FindObject
//
//      Returns pointer to object with TitleIndex.  If not found, NULL
//      is returned.
//
PPERF_OBJECT FindObject (PPERF_DATA pData, DWORD TitleIndex)
{
PPERF_OBJECT pObject;
DWORD        i = 0;

    if (pObject = FirstObject (pData))
        while (i < pData->NumObjectTypes)
            {
            if (pObject->ObjectNameTitleIndex == TitleIndex)
                return pObject;

            pObject = NextObject (pObject);
            i++;
            }

    return NULL;
}

//********************************************************
//
//  GetTitleIdx
//
//      Searches Titles[] for Name.  Returns the index found.
//
DWORD GetTitleIdx(HWND hWnd, LPTSTR Title[], DWORD LastIndex, LPTSTR Name)
{
  DWORD Index;

  for(Index = 0; Index <= LastIndex; Index++)
    if(Title[Index])
      if(!lstrcmpi (Title[Index], Name))
        return(Index);

  MessageBox(hWnd, Name, TEXT("Setup cannot find process title index"), MB_OK);
  return 0;
}

