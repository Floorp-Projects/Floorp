/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"
#include "time.h"
#include <winnls.h>
#include <winver.h>
#include <tlhelp32.h>
#include <winperf.h>

#define HIDWORD(l)   ((DWORD) (((ULONG) (l) >> 32) & 0xFFFF))
#define LODWORD(l)   ((DWORD) (l))

#define INDEX_STR_LEN       10
#define PN_PROCESS          TEXT("Process")
#define PN_THREAD           TEXT("Thread")

ULONG  (PASCAL *NS_GetDiskFreeSpace)(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
ULONG  (PASCAL *NS_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
typedef BOOL   (WINAPI *NS_ProcessWalk)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
typedef HANDLE (WINAPI *NS_CreateSnapshot)(DWORD dwFlags, DWORD th32ProcessID);
typedef PERF_DATA_BLOCK             PERF_DATA,      *PPERF_DATA;
typedef PERF_OBJECT_TYPE            PERF_OBJECT,    *PPERF_OBJECT;
typedef PERF_INSTANCE_DEFINITION    PERF_INSTANCE,  *PPERF_INSTANCE;
TCHAR   INDEX_PROCTHRD_OBJ[2*INDEX_STR_LEN];
DWORD   PX_PROCESS;
DWORD   PX_THREAD;

char *FontColorMap[] = {"WHITE", "0x00EEEEEE",
                        "BLACK", "0x00000000",
                        "GREEN", "0x00088808",
                        ""};

BOOL CheckProcessNT4(LPSTR szProcessName, DWORD dwProcessNameSize);
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

BOOL InitDialogClass(HINSTANCE hInstance, HINSTANCE hSetupRscInst)
{
  WNDCLASS  wc;

  wc.style         = CS_DBLCLKS | CS_SAVEBITS | CS_BYTEALIGNWINDOW;
  wc.lpfnWndProc   = DefDlgProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = DLGWINDOWEXTRA;
  wc.hInstance     = hSetupRscInst;
  wc.hIcon         = LoadIcon(hSetupRscInst, MAKEINTRESOURCE(IDI_SETUP));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = CLASS_NAME_SETUP_DLG;

  return(RegisterClass(&wc));
}

BOOL InitApplication(HINSTANCE hInstance, HINSTANCE hSetupRscInst)
{
  BOOL     bRv;
  WNDCLASS wc;

  wc.style         = CS_HREDRAW | CS_VREDRAW | CS_PARENTDC | CS_SAVEBITS;
  wc.lpfnWndProc   = (WNDPROC)DlgProcMain;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = hInstance;
  wc.hIcon         = LoadIcon(hSetupRscInst, MAKEINTRESOURCE(IDI_SETUP));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_ACTIVECAPTION + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = CLASS_NAME_SETUP;

  bRv = RegisterClass(&wc);
  if(bRv == FALSE)
    return(bRv);

  return(InitDialogClass(hInstance, hSetupRscInst));
}

BOOL InitInstance(HINSTANCE hInstance, DWORD dwCmdShow)
{
  HWND  hWnd;

  dwScreenX = GetSystemMetrics(SM_CXSCREEN);
  dwScreenY = GetSystemMetrics(SM_CYSCREEN);

  hInst = hInstance;
  hWnd = CreateWindow(CLASS_NAME_SETUP,
                      CLASS_NAME_SETUP,
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MAXIMIZE,
                      0,
                      0,
                      dwScreenX,
                      dwScreenY,
                      NULL,
                      NULL,
                      hInstance,
                      NULL);

  if(!hWnd)
    return(FALSE);

  hWndMain = hWnd;

  return(TRUE);
}

void PrintError(LPSTR szMsg, DWORD dwErrorCodeSH)
{
  DWORD dwErr;
  char  szErrorString[MAX_BUF];

  if(dwErrorCodeSH == ERROR_CODE_SHOW)
  {
    dwErr = GetLastError();
    wsprintf(szErrorString, "%d : %s", dwErr, szMsg);
  }
  else
    wsprintf(szErrorString, "%s", szMsg);

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    MessageBox(hWndMain, szErrorString, NULL, MB_ICONEXCLAMATION);
  }
  else if(sgProduct.dwMode == AUTO)
  {
    ShowMessage(szErrorString, TRUE);
    Delay(5);
    ShowMessage(szErrorString, FALSE);
  }
}

void *NS_GlobalAlloc(DWORD dwMaxBuf)
{
  LPSTR szBuf = NULL;

  if((szBuf = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, dwMaxBuf)) == NULL)
  {     
    if((szEGlobalAlloc == NULL) || (*szEGlobalAlloc == '\0'))
      PrintError(TEXT("Memory allocation error."), ERROR_CODE_HIDE);
    else
      PrintError(szEGlobalAlloc, ERROR_CODE_SHOW);

    return(NULL);
  }
  else
    return(szBuf);
}

void FreeMemory(void **vPointer)
{
  if(*vPointer != NULL)
    *vPointer = GlobalFree(*vPointer);
}

HRESULT NS_LoadStringAlloc(HANDLE hInstance, DWORD dwID, LPSTR *szStringBuf, DWORD dwStringBuf)
{
  char szBuf[MAX_BUF];

  if((*szStringBuf = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  
  if(!LoadString(hInstance, dwID, *szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      wsprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      wsprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(0);
}

HRESULT NS_LoadString(HANDLE hInstance, DWORD dwID, LPSTR szStringBuf, DWORD dwStringBuf)
{
  char szBuf[MAX_BUF];

  if(!LoadString(hInstance, dwID, szStringBuf, dwStringBuf))
  {
    if((szEStringLoad == NULL) ||(*szEStringLoad == '\0'))
      wsprintf(szBuf, "Could not load string resource ID %d", dwID);
    else
      wsprintf(szBuf, szEStringLoad, dwID);

    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }
  return(WIZ_OK);
}

void Delay(DWORD dwSeconds)
{
  SleepEx(dwSeconds * 1000, FALSE);
}

HRESULT Initialize(HINSTANCE hInstance)
{
  char szBuf[MAX_BUF];
  HWND hwndFW;

  bSDInit             = FALSE;
  bSDUserCanceled     = FALSE;
  hDlgMessage         = NULL;
  DetermineOSVersion();

  /* load strings from setup.exe */
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_GLOBALALLOC, &szEGlobalAlloc, MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_LOAD, &szEStringLoad,  MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_DLL_LOAD,    &szEDllLoad,     MAX_BUF))
    return(1);
  if(NS_LoadStringAlloc(hInstance, IDS_ERROR_STRING_NULL, &szEStringNull,  MAX_BUF))
    return(1);

  /* Allow only one instance of setup to run.
   * Detect a previous instance of setup, bring it to the 
   * foreground, and quit current instance */
  if((hwndFW = FindWindow(CLASS_NAME_SETUP, CLASS_NAME_SETUP)) != NULL)
  {
    ShowWindow(hwndFW, SW_RESTORE);
    SetForegroundWindow(hwndFW);
    return(1);
  }

  hAccelTable = LoadAccelerators(hInstance, CLASS_NAME_SETUP);

  if((hSetupRscInst = LoadLibraryEx("Setuprsc.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) == NULL)
  {
    wsprintf(szBuf, szEDllLoad, "SetupRsc.dll");
    PrintError(szBuf, ERROR_CODE_HIDE);
    return(1);
  }

  dwWizardState         = DLG_NONE;
  dwTempSetupType       = dwWizardState;
  siComponents          = NULL;
  bCreateDestinationDir = FALSE;
  bReboot               = FALSE;
  gdwUpgradeValue       = UG_NONE;

  if((szSetupDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  getcwd(szSetupDir, MAX_BUF);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szOSTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szFileIniConfig = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  // determine the system's TEMP path
  if(GetTempPath(MAX_BUF, szTempDir) == 0)
  {
    if(GetWindowsDirectory(szTempDir, MAX_BUF) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_GET_WINDOWS_DIRECTORY_FAILED, szEGetWinDirFailed, MAX_BUF) == WIZ_OK)
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);

      return(1);
    }

    AppendBackSlash(szTempDir, MAX_BUF);
    lstrcat(szTempDir, "TEMP");
  }
  lstrcpy(szOSTempDir, szTempDir);
  AppendBackSlash(szTempDir, MAX_BUF);
  lstrcat(szTempDir, WIZ_TEMP_DIR);

  if(!FileExists(szTempDir))
  {
    AppendBackSlash(szTempDir, MAX_BUF);
    CreateDirectoriesAll(szTempDir, FALSE);
    if(!FileExists(szTempDir))
    {
      char szECreateTempDir[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_CREATE_TEMP_DIR, szECreateTempDir, MAX_BUF) == WIZ_OK)
      {
        wsprintf(szBuf, szECreateTempDir, szTempDir);
        PrintError(szBuf, ERROR_CODE_HIDE);
      }
      return(1);
    }
    RemoveBackSlash(szTempDir);
  }

  hbmpBoxChecked         = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED));
  hbmpBoxCheckedDisabled = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED_DISABLED));
  hbmpBoxUnChecked       = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_UNCHECKED));

  DeleteIdiGetConfigIni();
  bIdiArchivesExists = DeleteIdiGetArchives();
  DeleteIdiGetRedirect();
  return(0);
}

void OutputSetupTitle(HDC hDC)
{
  HFONT     hfontTmp0;
  HFONT     hfontTmp1;
  HFONT     hfontTmp2;
  HFONT     hfontOld;
  LOGFONT   logFont;
  int       nHeight0;
  int       nHeight1;
  int       nHeight2;
  int       iLine0x;
  int       iLine0y;
  int       iLine1x;
  int       iLine1y;
  int       iLine2x;
  int       iLine2y;
  int       iShadowOffset;

  SetBkMode(hDC, TRANSPARENT);


/*
 * Setup Title Line 0
 */

  /* Retrieve the default font used for the system.  Pass this font name to CreateFont.
   * This is so the proper native characters will show up correctly under different
   * language OSes such as Japanese */
  SystemParametersInfo(SPI_GETICONTITLELOGFONT,
                       sizeof(logFont),
                       (PVOID)&logFont,
                       0);

  nHeight0  = -MulDiv(sgProduct.iSetupTitle0FontSize, 72, GetDeviceCaps(hDC, LOGPIXELSY));
  hfontTmp0 = CreateFont(nHeight0,
                        0,
                        0,
                        0,
                        FW_BOLD,
                        0,
                        0,
                        0,
                        logFont.lfCharSet,
                        OUT_DEFAULT_PRECIS,
                        0,
                        PROOF_QUALITY,
                        logFont.lfPitchAndFamily,
                        logFont.lfFaceName);

  if(hfontTmp0)
    hfontOld = SelectObject(hDC, hfontTmp0);

  iLine0x = 20;
  iLine0y = 20;

  /* draw shadow */
  if(sgProduct.bSetupTitle0FontShadow == TRUE)
  {
    /* Set shadow color to black and draw shadow */
    SetTextColor(hDC, 0);
    iShadowOffset = (int)(sgProduct.iSetupTitle0FontSize / 8);
    TextOut(hDC, iLine0x + iShadowOffset, iLine0y + iShadowOffset, TEXT(sgProduct.szSetupTitle0), lstrlen(sgProduct.szSetupTitle0));
  }

  /* Set font color and draw; color format is 0x00bbggrr - where b is blue, g is green, and r is red */
  /* 0x00088808 - green */
  SetTextColor(hDC, sgProduct.crSetupTitle0FontColor);

  /* draw text */
  TextOut(hDC, iLine0x, iLine0y, TEXT(sgProduct.szSetupTitle0), lstrlen(sgProduct.szSetupTitle0));


/*
 * Setup Title Line 1
 */
  nHeight1  = -MulDiv(sgProduct.iSetupTitle1FontSize, 72, GetDeviceCaps(hDC, LOGPIXELSY));
  hfontTmp1 = CreateFont(nHeight1,
                        0,
                        0,
                        0,
                        FW_BOLD,
                        0,
                        0,
                        0,
                        logFont.lfCharSet,
                        OUT_DEFAULT_PRECIS,
                        0,
                        PROOF_QUALITY,
                        logFont.lfPitchAndFamily,
                        logFont.lfFaceName);
  if(hfontTmp1)
    SelectObject(hDC, hfontTmp1);

  iLine1x = iLine0x;
  iLine1y = iLine0y - nHeight0 + 7;

  /* draw shadow */
  if(sgProduct.bSetupTitle1FontShadow == TRUE)
  {
    /* Set shadow color to black and draw shadow */
    SetTextColor(hDC, 0);
    iShadowOffset = (int)(sgProduct.iSetupTitle1FontSize / 8);
    TextOut(hDC, iLine1x + iShadowOffset, iLine1y + iShadowOffset, TEXT(sgProduct.szSetupTitle1), lstrlen(sgProduct.szSetupTitle1));
  }

  /* Set font color and draw; color format is 0x00bbggrr - where b is blue, g is green, and r is red */
  /* 0x00088808 - green */
  SetTextColor(hDC, sgProduct.crSetupTitle1FontColor);

  /* draw text */
  TextOut(hDC, iLine1x, iLine1y, TEXT(sgProduct.szSetupTitle1), lstrlen(sgProduct.szSetupTitle1));


/*
 * Setup Title Line 2
 */
  nHeight2  = -MulDiv(sgProduct.iSetupTitle2FontSize, 72, GetDeviceCaps(hDC, LOGPIXELSY));
  hfontTmp2 = CreateFont(nHeight2,
                        0,
                        0,
                        0,
                        FW_BOLD,
                        0,
                        0,
                        0,
                        logFont.lfCharSet,
                        OUT_DEFAULT_PRECIS,
                        0,
                        PROOF_QUALITY,
                        logFont.lfPitchAndFamily,
                        logFont.lfFaceName);
  if(hfontTmp2)
    SelectObject(hDC, hfontTmp2);

  iLine2x = iLine1x;
  iLine2y = iLine1y - nHeight1 + 7;

  /* draw shadow */
  if(sgProduct.bSetupTitle2FontShadow == TRUE)
  {
    /* Set shadow color to black and draw shadow */
    SetTextColor(hDC, 0);

    iShadowOffset = (int)(sgProduct.iSetupTitle2FontSize / 8);
    TextOut(hDC, iLine2x + iShadowOffset, iLine2y + iShadowOffset, TEXT(sgProduct.szSetupTitle2), lstrlen(sgProduct.szSetupTitle2));
  }

  /* Set font color and draw; color format is 0x00bbggrr - where b is blue, g is green, and r is red */
  /* 0x00088808 - green */
  SetTextColor(hDC, sgProduct.crSetupTitle2FontColor);

  /* draw text */
  TextOut(hDC, iLine2x, iLine2y, TEXT(sgProduct.szSetupTitle2), lstrlen(sgProduct.szSetupTitle2));

  SelectObject(hDC, hfontOld);
  DeleteObject(hfontTmp0);
  DeleteObject(hfontTmp1);
  DeleteObject(hfontTmp2);
}

HRESULT SdArchives(LPSTR szFileIdi, LPSTR szDownloadDir)
{
  /* 2 indicates that sdinst.dll does not exist */
  HRESULT hResult = 2;

#ifndef MOZILLA_CLIENT
  SDISTRUCT sdistruct;

  if((hResult = InitializeSmartDownload()) == 0)
  {
    ZeroMemory(&sdistruct, sizeof(SDISTRUCT));

    RemoveBackSlash(szDownloadDir);
    sdistruct.dwStructSize  = sizeof(SDISTRUCT);
    sdistruct.dwTimeOut     = SDI_DEFAULT_TIMEOUT;
    sdistruct.dwRetries     = SDI_DEFAULT_RETRIES;
    sdistruct.lpFileName    = szFileIdi;
    sdistruct.lpDownloadDir = szDownloadDir;
    sdistruct.hwndOwner     = hWndMain;

    hResult = pfnNetInstall(&sdistruct);
    DeInitializeSmartDownload();
  }
#endif

  return(hResult);
}

/* Function to remove quotes from a string */
void RemoveQuotes(LPSTR lpszSrc, LPSTR lpszDest, int iDestSize)
{
  char *lpszBegin;

  if(lstrlen(lpszSrc) > iDestSize)
    return;

  if(*lpszSrc == '\"')
    lpszBegin = &lpszSrc[1];
  else
    lpszBegin = lpszSrc;

  lstrcpy(lpszDest, lpszBegin);

  if(lpszDest[lstrlen(lpszDest) - 1] == '\"')
    lpszDest[lstrlen(lpszDest) - 1] = '\0';
}

/* Function to locate the first non space character in a string,
 * and return a pointer to it. */
LPSTR GetFirstNonSpace(LPSTR lpszString)
{
  int   i;
  int   iStrLength;

  iStrLength = lstrlen(lpszString);

  for(i = 0; i < iStrLength; i++)
  {
    if(!isspace(lpszString[i]))
      return(&lpszString[i]);
  }

  return(NULL);
}

/* Function to return the argument count given a command line input
 * format string */
int GetArgC(LPSTR lpszCommandLine)
{
  int   i;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(iArgCount);

  iStrLength   = lstrlen(lpszBeginStr);
  bFoundQuote  = FALSE;
  bFoundSpace  = TRUE;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }
  }

  return(iArgCount);
}

/* Function to return a specific argument parameter from a given command line input
 * format string. */
LPSTR GetArgV(LPSTR lpszCommandLine, int iIndex, LPSTR lpszDest, int iDestSize)
{
  int   i;
  int   j;
  int   iArgCount;
  int   iStrLength;
  LPSTR lpszBeginStr;
  LPSTR lpszDestTemp;
  BOOL  bFoundQuote;
  BOOL  bFoundSpace;

  iArgCount    = 0;
  lpszBeginStr = GetFirstNonSpace(lpszCommandLine);

  if(lpszBeginStr == NULL)
    return(NULL);

  lpszDestTemp = (char *)calloc(iDestSize, sizeof(char));
  if(lpszDestTemp == NULL)
  {
    PrintError("Out of memory", ERROR_CODE_HIDE);
    exit(1);
  }

  ZeroMemory(lpszDest, iDestSize);
  iStrLength    = lstrlen(lpszBeginStr);
  bFoundQuote   = FALSE;
  bFoundSpace   = TRUE;
  j             = 0;

  for(i = 0; i < iStrLength; i++)
  {
    if(lpszCommandLine[i] == '\"')
    {
      if(bFoundQuote == FALSE)
      {
        ++iArgCount;
        bFoundQuote = TRUE;
      }
      else
      {
        bFoundQuote = FALSE;
      }
    }
    else if(bFoundQuote == FALSE)
    {
      if(!isspace(lpszCommandLine[i]) && (bFoundSpace == TRUE))
      {
        ++iArgCount;
        bFoundSpace = FALSE;
      }
      else if(isspace(lpszCommandLine[i]))
      {
        bFoundSpace = TRUE;
      }
    }

    if((iIndex == (iArgCount - 1)) &&
      ((bFoundQuote == TRUE) || (bFoundSpace == FALSE) ||
      ((bFoundQuote == FALSE) && (lpszCommandLine[i] == '\"'))))
    {
      if(j < iDestSize)
      {
        lpszDestTemp[j] = lpszCommandLine[i];
        ++j;
      }
      else
      {
        lpszDestTemp[j] = '\0';
      }
    }
  }

  RemoveQuotes(lpszDestTemp, lpszDest, iDestSize);
  free(lpszDestTemp);
  return(lpszDest);
}

HRESULT ParseSetupIni()
{
  char szBuf[MAX_BUF];
  char szFileIniSetup[MAX_BUF];
  char szFileIdiGetConfigIni[MAX_BUF];

  lstrcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  lstrcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);

  lstrcpy(szFileIniSetup, szSetupDir);
  AppendBackSlash(szFileIniSetup, sizeof(szFileIniSetup));
  lstrcat(szFileIniSetup, FILE_INI_SETUP);

  CopyFile(szFileIniSetup, szFileIdiGetConfigIni, FALSE);

  if(!FileExists(szFileIdiGetConfigIni))
  {
    char szEFileNotFound[MAX_BUF];

    if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
    {
      wsprintf(szBuf, szEFileNotFound, szFileIdiGetConfigIni);
      PrintError(szBuf, ERROR_CODE_HIDE);
    }
    return(1);
  }

  return(0);
}

HRESULT GetConfigIni()
{
  char    szFileIniTempDir[MAX_BUF];
  char    szFileIniSetupDir[MAX_BUF];
  char    szFileIdiGetConfigIni[MAX_BUF];
  char    szMsgRetrieveConfigIni[MAX_BUF];
  char    szBuf[MAX_BUF];
  HRESULT hResult = 0;

  if(NS_LoadString(hSetupRscInst, IDS_MSG_RETRIEVE_CONFIGINI, szMsgRetrieveConfigIni, MAX_BUF) != WIZ_OK)
    return(1);
    
  lstrcpy(szFileIniTempDir, szTempDir);
  AppendBackSlash(szFileIniTempDir, sizeof(szFileIniTempDir));
  lstrcat(szFileIniTempDir, FILE_INI_CONFIG);

  /* set default value for szFileIniConfig here */
  lstrcpy(szFileIniConfig, szFileIniTempDir);

  lstrcpy(szFileIniSetupDir, szSetupDir);
  AppendBackSlash(szFileIniSetupDir, sizeof(szFileIniSetupDir));
  lstrcat(szFileIniSetupDir, FILE_INI_CONFIG);

  /* if config.ini exists, then use it, else download config.ini from the net */
  if(!FileExists(szFileIniTempDir))
  {
    if(FileExists(szFileIniSetupDir))
    {
      lstrcpy(szFileIniConfig, szFileIniSetupDir);
      return(0);
    }
  }
  else
    return(0);

  /* parse setup.ini to create getconfigini.idi to download config.ini*/
  if(ParseSetupIni())
    return(1);

  lstrcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  lstrcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);

  ShowMessage(szMsgRetrieveConfigIni, TRUE);
  if((hResult = SdArchives(szFileIdiGetConfigIni, szTempDir)) != 0)
  {
    char szEFileNotFound[MAX_BUF];

    if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
    {
      wsprintf(szBuf, szEFileNotFound, FILE_INI_CONFIG);
      PrintError(szBuf, ERROR_CODE_HIDE);
    }
  }
  ShowMessage(szMsgRetrieveConfigIni, FALSE);

  return(hResult);
}

BOOL LocateJar(siC *siCObject, LPSTR szPath, DWORD dwPathSize, BOOL bIncludeTempDir)
{
  BOOL bRet;
  char szBuf[MAX_BUF * 2];
  char szSEADirTemp[MAX_BUF];
  char szSetupDirTemp[MAX_BUF];
  char szTempDirTemp[MAX_BUF];

  char szArchiveLstFile[MAX_BUF];
  char *szBufPtr;
  int  iLen;

  /* initialize default behavior */
  bRet = FALSE;
  if(szPath != NULL)
    ZeroMemory(szPath, dwPathSize);
  siCObject->dwAttributes |= SIC_DOWNLOAD_REQUIRED;

  lstrcpy(szSEADirTemp, sgProduct.szAlternateArchiveSearchPath);
  AppendBackSlash(szSEADirTemp, sizeof(szSEADirTemp));
  lstrcat(szSEADirTemp, siCObject->szArchiveName);

  /* XXX_QUICK_FIX 
   * checking sgProduct.szAlternateArchiveSearchPath for empty string
   * should be done prior to AppendBackSlash() above.
   * This is a quick fix for the time frame that we are currently in. */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (FileExists(szSEADirTemp)))
  {
    /* jar file found.  Unset attribute to download from the net */
    siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
    /* save the path of where jar was found at */
    lstrcpy(siCObject->szArchivePath, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
    bRet = TRUE;

    /* save path where archive is located */
    if(szPath != NULL)
      lstrcpy(szPath, sgProduct.szAlternateArchiveSearchPath);
  }
  else
  {
    lstrcpy(szSetupDirTemp, szSetupDir);
    lstrcpy(szTempDirTemp,  szTempDir);
    AppendBackSlash(szSetupDirTemp, sizeof(szSetupDirTemp));
    AppendBackSlash(szTempDirTemp, sizeof(szTempDirTemp));
    if(lstrcmpi(szTempDirTemp, szSetupDirTemp) == 0)
    {
      /* check the temp dir for the .xpi file */
      lstrcpy(szBuf, szTempDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        if(bIncludeTempDir == TRUE)
        {
          /* jar file found.  Unset attribute to download from the net */
          siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
          /* save the path of where jar was found at */
          lstrcpy(siCObject->szArchivePath, szTempDirTemp);
          AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
          bRet = TRUE;
        }

        /* if the archive name is in the archive.lst file, then it was uncompressed
         * by the self extracting .exe file.  Assume that the .xpi file exists.
         * This is a safe assumption because the self extracting.exe creates the
         * archive.lst with what it uncompresses everytime it is run. */
        lstrcpy(szArchiveLstFile, szTempDirTemp);
        AppendBackSlash(szArchiveLstFile, sizeof(szArchiveLstFile));
        lstrcat(szArchiveLstFile, "Archive.lst");

        GetPrivateProfileString("Archives", NULL, "", szBuf, (MAX_BUF * 2), szArchiveLstFile);
        if(*szBuf != '\0')
        {
          szBufPtr = szBuf;
          while(*szBufPtr != '\0')
          {
            if(lstrcmpi(siCObject->szArchiveName, szBufPtr) == 0)
            {
              /* jar file found.  Unset attribute to download from the net */
              siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
              /* save the path of where jar was found at */
              lstrcpy(siCObject->szArchivePath, szTempDirTemp);
              AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
              bRet = TRUE;

              /* found what we're looking for.  No need to continue */
              break;
            }

            iLen = lstrlen(szBufPtr);
            szBufPtr += iLen +1;
          }
        }

        /* save path where archive is located */
        if(szPath != NULL)
          lstrcpy(szPath, szTempDirTemp);
      }
    }
    else
    {
      /* check the setup dir for the .xpi file */
      lstrcpy(szBuf, szSetupDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        /* jar file found.  Unset attribute to download from the net */
        siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
        /* save the path of where jar was found at */
        lstrcpy(siCObject->szArchivePath, szSetupDirTemp);
        AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
        bRet = TRUE;

        /* save path where archive is located */
        if(szPath != NULL)
          lstrcpy(szPath, szSetupDirTemp);
      }
      else
      {
        /* check the ns_temp dir for the .xpi file */
        lstrcpy(szBuf, szTempDirTemp);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, siCObject->szArchiveName);

        if(FileExists(szBuf))
        {
          if(bIncludeTempDir == TRUE)
          {
            /* jar file found.  Unset attribute to download from the net */
            siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
            /* save the path of where jar was found at */
            lstrcpy(siCObject->szArchivePath, szTempDirTemp);
            AppendBackSlash(siCObject->szArchivePath, MAX_BUF);
            bRet = TRUE;
          }

          /* save path where archive is located */
          if(szPath != NULL)
            lstrcpy(szPath, szTempDirTemp);
        }
      }
    }
  }
  return(bRet);
}

HRESULT AddArchiveToIdiFile(siC *siCObject, char *szSComponent, char *szSFile, char *szFileIdiGetArchives)
{
  DWORD     dwIndex1;
  DWORD     dwCounter;
  char      szFile[MAX_BUF];
  char      szIndex1[MAX_BUF];
  char      szCounter[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szBufTemp[MAX_BUF];
  char      szBufUrl[MAX_BUF];
  char      szKIdentifier[MAX_BUF];
  char      szIdentifier[MAX_BUF];
  char      szSection[MAX_BUF];
  ssi       *ssiSiteSelectorTemp;

  WritePrivateProfileString(szSFile, "desc", siCObject->szDescriptionShort, szFileIdiGetArchives);

  lstrcpy(szFile, szTempDir);
  AppendBackSlash(szFile, sizeof(szFile));
  lstrcat(szFile, FILE_INI_REDIRECT);

  ZeroMemory(szIdentifier, MAX_BUF);
  if(FileExists(szFile))
  {
    lstrcpy(szSection, "Site Selector");
    ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);
    if(ssiSiteSelectorTemp != NULL)
    {
      if(ssiSiteSelectorTemp->szIdentifier != NULL)
        lstrcpy(szIdentifier, ssiSiteSelectorTemp->szIdentifier);
    }
  }
  else
  {
    lstrcpy(szSection, "General");
    lstrcpy(szIdentifier, "url");
    lstrcpy(szFile, szFileIniConfig);
  }

  GetPrivateProfileString(szSection, szIdentifier, "", szBufUrl, MAX_BUF, szFile);

  AppendSlash(szBufUrl, sizeof(szBufUrl));
  lstrcat(szBufUrl, siCObject->szArchiveName);

  dwIndex1  = 0;
  dwCounter = 0;
  itoa(dwCounter, szCounter, 10);
  itoa(dwIndex1,  szIndex1,  10);

  if(WritePrivateProfileString(szSFile, szCounter, szBufUrl, szFileIdiGetArchives) == 0)
  {
    char szEWPPS[MAX_BUF];

    if(NS_LoadString(hSetupRscInst, IDS_ERROR_WRITEPRIVATEPROFILESTRING, szEWPPS, MAX_BUF) == WIZ_OK)
    {
      wsprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetArchives, szSFile, szIndex1, szBufUrl);
      wsprintf(szBuf, szEWPPS, szBufTemp);
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    return(1);
  }

  ++dwCounter;
  itoa(dwCounter, szCounter, 10);
  lstrcpy(szKIdentifier, szIdentifier);
  lstrcat(szKIdentifier, ".");
  lstrcat(szKIdentifier, szIndex1);

  GetPrivateProfileString(szSection, szKIdentifier, "", szBufUrl, MAX_BUF, szFile);

  /* regardless if the site selector has any info or not, add the rest of the urls pertaining for this
   * component here.  These will be the fail over urls. */
  while(*szBufUrl != '\0')
  {
    AppendSlash(szBufUrl, sizeof(szBufUrl));
    lstrcat(szBufUrl, siCObject->szArchiveName);

    if(WritePrivateProfileString(szSFile, szCounter, szBufUrl, szFileIdiGetArchives) == 0)
    {
      char szEWPPS[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_WRITEPRIVATEPROFILESTRING, szEWPPS, MAX_BUF) == WIZ_OK)
      {
        wsprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetArchives, szSFile, szIndex1, szBufUrl);
        wsprintf(szBuf, szEWPPS, szBufTemp);
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
      return(1);
    }

    ++dwIndex1;
    ++dwCounter;
    itoa(dwIndex1,  szIndex1,  10);
    itoa(dwCounter, szCounter, 10);
    lstrcpy(szKIdentifier, szIdentifier);
    lstrcat(szKIdentifier, ".");
    lstrcat(szKIdentifier, szIndex1);
    GetPrivateProfileString(szSection, szKIdentifier, "", szBufUrl, MAX_BUF, szFile);
  }
  return(0);
}

void SetSetupRunMode(LPSTR szMode)
{
  if(lstrcmpi(szMode, "NORMAL") == 0)
    sgProduct.dwMode = NORMAL;
  if(lstrcmpi(szMode, "AUTO") == 0)
    sgProduct.dwMode = AUTO;
  if(lstrcmpi(szMode, "SILENT") == 0)
    sgProduct.dwMode = SILENT;
}

long RetrieveRedirectFile()
{
  DWORD     dwIndex0;
  long      lResult;
  char      szBuf[MAX_BUF];
  char      szBufUrl[MAX_BUF];
  char      szBufTemp[MAX_BUF];
  char      szDomain[MAX_BUF];
  char      szIdentifier[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szFileIdiGetRedirect[MAX_BUF];
  char      szKServerPath[MAX_BUF];
  char      szFileIniRedirect[MAX_BUF];
  ssi       *ssiSiteSelectorTemp;

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect) == FALSE)
  {
    GetPrivateProfileString("Redirect", "Status", "", szBuf, MAX_BUF, szFileIniConfig);
    if(lstrcmpi(szBuf, "ENABLED") != 0)
      return(0);

    if(GetTotalArchivesToDownload() == 0)
      return(0);

    ssiSiteSelectorTemp = SsiGetNode(szSiteSelectorDescription);
    if(ssiSiteSelectorTemp != NULL)
    {
      if(ssiSiteSelectorTemp->szDomain != NULL)
        lstrcpy(szDomain, ssiSiteSelectorTemp->szDomain);
      if(ssiSiteSelectorTemp->szIdentifier != NULL)
        lstrcpy(szIdentifier, ssiSiteSelectorTemp->szIdentifier);
    }

    lstrcpy(szFileIdiGetRedirect, szTempDir);
    AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
    lstrcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);

    GetPrivateProfileString("Redirect", "Description", "", szBuf, MAX_BUF, szFileIniConfig);
    WritePrivateProfileString("File0", "desc", szBuf, szFileIdiGetRedirect);

    dwIndex0  = 0;
    itoa(dwIndex0,  szIndex0,  10);
    lstrcpy(szKServerPath, "Server Path");
    lstrcat(szKServerPath, szIndex0);
    GetPrivateProfileString("Redirect", szKServerPath, "", szBuf, MAX_BUF, szFileIniConfig);
    while(*szBuf != '\0')
    {
      lstrcpy(szBufUrl, szDomain);
      lstrcat(szBufUrl, szBuf);
      if(WritePrivateProfileString("File0", szIndex0, szBufUrl, szFileIdiGetRedirect) == 0)
      {
        char szEWPPS[MAX_BUF];

        if(NS_LoadString(hSetupRscInst, IDS_ERROR_WRITEPRIVATEPROFILESTRING, szEWPPS, MAX_BUF) == WIZ_OK)
        {
          wsprintf(szBufTemp, "%s\n    [%s]\n    %s=%s", szFileIdiGetRedirect, "File0", szIndex0, szBufUrl);
          wsprintf(szBuf, szEWPPS, szBufTemp);
          PrintError(szBuf, ERROR_CODE_SHOW);
        }
        return(1);
      }

      ++dwIndex0;
      itoa(dwIndex0, szIndex0, 10);
      lstrcpy(szKServerPath, "Server Path");
      lstrcat(szKServerPath, szIndex0);
      GetPrivateProfileString("Redirect", szKServerPath, "", szBuf, MAX_BUF, szFileIniConfig);
    }

    /* the existance of the getarchives.idi file determines if there are
       any jar files needed to be downloaded */
    if(FileExists(szFileIdiGetRedirect))
    {
      DecryptString(szBuf, siSDObject.szXpcomDir);
      lstrcpy(siSDObject.szXpcomDir, szBuf);

      WritePrivateProfileString("Netscape Install", "core_file",        siSDObject.szXpcomFile,       szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "core_dir",         siSDObject.szXpcomDir,        szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "no_ads",           siSDObject.szNoAds,           szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "silent",           siSDObject.szSilent,          szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "execution",        siSDObject.szExecution,       szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "confirm_install",  siSDObject.szConfirmInstall,  szFileIdiGetRedirect);
      WritePrivateProfileString("Netscape Install", "extract_msg",      siSDObject.szExtractMsg,      szFileIdiGetRedirect);
      WritePrivateProfileString("Execution",        "exe",              siSDObject.szExe,             szFileIdiGetRedirect);
      WritePrivateProfileString("Execution",        "exe_param",        siSDObject.szExeParam,        szFileIdiGetRedirect);

      lResult = SdArchives(szFileIdiGetRedirect, szTempDir);
      if((lResult != 0) && (LOWORD(lResult) != 53)) // 53 - url is valid, but file does not exist
        return(lResult);
    }
  }

  return(0);
}

long RetrieveArchives()
{
  DWORD     dwIndex0;
  DWORD     dwCounter;
  siC       *siCObject = NULL;
  long      lResult;
  char      szBuf[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szCounter[MAX_BUF];
  char      szFileIdiGetArchives[MAX_BUF];
  char      szSComponent[MAX_BUF];
  char      szSFile[MAX_BUF];

  RetrieveRedirectFile();

  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);

  lResult   = WIZ_OK;
  dwIndex0  = 0;
  dwCounter = 0;
  itoa(dwIndex0,  szIndex0,  10);
  itoa(dwCounter, szCounter, 10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      /* only download jars if not already in the local machine */
      if(LocateJar(siCObject, NULL, 0, FALSE) == FALSE)
      {
        lstrcpy(szSComponent, "Component");
        lstrcat(szSComponent, szIndex0);

        lstrcpy(szSFile, "File");
        lstrcat(szSFile, szCounter);

        if((lResult = AddArchiveToIdiFile(siCObject, szSComponent, szSFile, szFileIdiGetArchives)) != 0)
          return(lResult);

        ++dwCounter;
        itoa(dwCounter, szCounter, 10);
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  /* the existance of the getarchives.idi file determines if there are
     any jar files needed to be downloaded */
  if(FileExists(szFileIdiGetArchives))
  {
    DecryptString(szBuf, siSDObject.szXpcomDir);
    lstrcpy(siSDObject.szXpcomDir, szBuf);

    WritePrivateProfileString("Netscape Install", "core_file",        siSDObject.szXpcomFile,       szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "core_dir",         siSDObject.szXpcomDir,        szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "no_ads",           siSDObject.szNoAds,           szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "silent",           siSDObject.szSilent,          szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "execution",        siSDObject.szExecution,       szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "confirm_install",  siSDObject.szConfirmInstall,  szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "extract_msg",      siSDObject.szExtractMsg,      szFileIdiGetArchives);
    WritePrivateProfileString("Execution",        "exe",              siSDObject.szExe,             szFileIdiGetArchives);
    WritePrivateProfileString("Execution",        "exe_param",        siSDObject.szExeParam,        szFileIdiGetArchives);

    /* proxy support */
    if((*diAdvancedSettings.szProxyServer != '\0') && (*diAdvancedSettings.szProxyPort != '\0'))
    {
      WritePrivateProfileString("Proxy", "server", diAdvancedSettings.szProxyServer, szFileIdiGetArchives);
      WritePrivateProfileString("Proxy", "port",   diAdvancedSettings.szProxyPort,   szFileIdiGetArchives);
    }

    lResult = SdArchives(szFileIdiGetArchives, szTempDir);

    if((lResult == 0) || (lResult == 2))
    {
      dwIndex0  = 0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
      while(siCObject)
      {
        if(siCObject->dwAttributes & SIC_SELECTED)
        {
          /* only download jars if not already in the local machine */
          if(LocateJar(siCObject, NULL, 0, TRUE) == FALSE)
          {
            char szEFileNotFound[MAX_BUF];

            if(NS_LoadString(hSetupRscInst, IDS_ERROR_FILE_NOT_FOUND, szEFileNotFound, MAX_BUF) == WIZ_OK)
            {
              wsprintf(szBuf, szEFileNotFound, siCObject->szArchiveName);
              PrintError(szBuf, ERROR_CODE_HIDE);
            }
            lResult = 3; // not all .xpi files were downloaded
          }
        }

        ++dwIndex0;
        siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
      }
    }
  }

  return(lResult);
}

void RemoveBackSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendBackSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

void RemoveSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '/')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '/')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "/");
      }
    }
  }
}

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
    dwInputLen    = lstrlen(szInput);
    ZeroMemory(szOutput, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_ROOT_ONLY:
          if(szInput[1] == ':')
          {
            szOutput[0] = szInput[0];
            szOutput[1] = szInput[1];
            AppendBackSlash(szOutput, dwOutputSize);
          }
          else if(szInput[1] == '\\')
          {
            int iFoundBackSlash = 0;
            for(dwCounter = 0; dwCounter < dwInputLen; dwCounter++)
            {
              if(szInput[dwCounter] == '\\')
              {
                szOutput[dwCounter] = szInput[dwCounter];
                ++iFoundBackSlash;
              }

              if(iFoundBackSlash == 3)
                break;
            }

            if(iFoundBackSlash != 0)
              AppendBackSlash(szOutput, dwOutputSize);
          }
          break;
      }
    }
  }
}

HRESULT LaunchApps()
{
  DWORD     dwIndex0;
  BOOL      bArchiveFound;
  siC       *siCObject = NULL;
  char      szArchive[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szMsg[MAX_BUF];

  if(NS_LoadString(hSetupRscInst, IDS_MSG_CONFIGURING, szMsg, MAX_BUF) != WIZ_OK)
    return(1);

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    /* launch 3rd party executable */
    if((siCObject->dwAttributes & SIC_SELECTED) && (siCObject->dwAttributes & SIC_LAUNCHAPP))
    {
      bArchiveFound = TRUE;
      lstrcpy(szArchive, sgProduct.szAlternateArchiveSearchPath);
      AppendBackSlash(szArchive, sizeof(szArchive));
      lstrcat(szArchive, siCObject->szArchiveName);
      if(!FileExists(szArchive))
      {
        lstrcpy(szArchive, szSetupDir);
        AppendBackSlash(szArchive, sizeof(szArchive));
        lstrcat(szArchive, siCObject->szArchiveName);
        if(!FileExists(szArchive))
        {
          lstrcpy(szArchive, szTempDir);
          AppendBackSlash(szArchive, sizeof(szArchive));
          lstrcat(szArchive, siCObject->szArchiveName);
          if(!FileExists(szArchive))
          {
            bArchiveFound = FALSE;
          }
        }
      }

      if(bArchiveFound)
      {
        wsprintf(szBuf, szMsg, siCObject->szDescriptionShort);
        ShowMessage(szBuf, TRUE);
        WinSpawn(szArchive, siCObject->szParameter, szTempDir, SW_SHOWNORMAL, TRUE);
        ShowMessage(szBuf, FALSE);
      }
    }
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
  return(0);
}

void DetermineOSVersion()
{
  DWORD         dwVersion;
  DWORD         dwWindowsMajorVersion;
  DWORD         dwWindowsMinorVersion;
  DWORD         dwWindowsVersion;
  BOOL          bIsWin95Debute;
  char          szESetupRequirement[MAX_BUF];

  ulOSType        = 0;
  dwVersion       = GetVersion();
  bIsWin95Debute  = IsWin95Debute();

  // Get major and minor version numbers of Windows
  dwWindowsMajorVersion =  (DWORD)(LOBYTE(LOWORD(dwVersion)));
  dwWindowsMinorVersion =  (DWORD)(HIBYTE(LOWORD(dwVersion)));
  dwWindowsVersion      =  (DWORD)(HIWORD(dwVersion));

  // Get build numbers for Windows NT or Win95/Win98
  if(dwVersion < 0x80000000) // Windows NT
  {
    ulOSType |= OS_NT;
    if(dwWindowsMajorVersion == 3)
      ulOSType |= OS_NT3;
    else if(dwWindowsMajorVersion == 4)
      ulOSType |= OS_NT4;
    else
      ulOSType |= OS_NT5;
  }
  else if(dwWindowsMajorVersion == 4)
  {
    ulOSType |= OS_WIN9x;
    if(dwWindowsMinorVersion == 0)
    {
      ulOSType |= OS_WIN95;

      if(bIsWin95Debute)
        ulOSType |= OS_WIN95_DEBUTE;
    }
    else
      ulOSType |= OS_WIN98;
  }
  else
  {
    if(NS_LoadString(hSetupRscInst, IDS_ERROR_SETUP_REQUIREMENT, szESetupRequirement, MAX_BUF) == WIZ_OK)
      PrintError(szESetupRequirement, ERROR_CODE_HIDE);

    exit(1);
  }
}

HRESULT WinSpawn(LPSTR szClientName, LPSTR szParameters, LPSTR szCurrentDir, int iShowCmd, BOOL bWait)
{
  SHELLEXECUTEINFO seInfo;

  seInfo.cbSize       = sizeof(SHELLEXECUTEINFO);
  seInfo.fMask        = SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_NOCLOSEPROCESS;
  seInfo.hwnd         = hWndMain;
  seInfo.lpVerb       = NULL;
  seInfo.lpFile       = szClientName;
  seInfo.lpParameters = szParameters;
  seInfo.lpDirectory  = szCurrentDir;
  seInfo.nShow        = SW_SHOWNORMAL;
  seInfo.hInstApp     = 0;
  seInfo.lpIDList     = NULL;
  seInfo.lpClass      = NULL;
  seInfo.hkeyClass    = 0;
  seInfo.dwHotKey     = 0;
  seInfo.hIcon        = 0;
  seInfo.hProcess     = 0;

  if((ShellExecuteEx(&seInfo) != 0) && (seInfo.hProcess != NULL))
  {
    if(bWait)
    {
      for(;;)
      {
        if(WaitForSingleObject(seInfo.hProcess, 200) == WAIT_OBJECT_0)
          break;

        ProcessWindowsMessages();
      }
    }
    return(TRUE);
  }
  return(FALSE);
}

HRESULT InitDlgWelcome(diW *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage2 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgWelcome(diW *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
  FreeMemory(&(diDialog->szMessage2));
}

HRESULT InitDlgLicense(diL *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szLicenseFilename = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgLicense(diL *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szLicenseFilename));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
}

HRESULT InitDlgSetupType(diST *diDialog)
{
  diDialog->bShowDialog = FALSE;

  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szReadmeFilename = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szReadmeApp = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  diDialog->stSetupType0.dwCItems = 0;
  diDialog->stSetupType1.dwCItems = 0;
  diDialog->stSetupType2.dwCItems = 0;
  diDialog->stSetupType3.dwCItems = 0;
  if((diDialog->stSetupType0.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType0.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType1.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType1.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType2.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType2.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->stSetupType3.szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->stSetupType3.szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgSetupType(diST *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));

  FreeMemory(&(diDialog->szReadmeFilename));
  FreeMemory(&(diDialog->szReadmeApp));
  FreeMemory(&(diDialog->stSetupType0.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType0.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType1.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType1.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType2.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType2.szDescriptionLong));
  FreeMemory(&(diDialog->stSetupType3.szDescriptionShort));
  FreeMemory(&(diDialog->stSetupType3.szDescriptionLong));
}

HRESULT InitDlgSelectComponents(diSC *diDialog, DWORD dwSM)
{
  diDialog->bShowDialog = FALSE;

  /* set to show the Single dialog or the Multi dialog for the SelectComponents dialog */
  diDialog->bShowDialogSM = dwSM;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgSelectComponents(diSC *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
}

HRESULT InitDlgWindowsIntegration(diWI *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((diDialog->szMessage1 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  diDialog->wiCB0.bEnabled = FALSE;
  diDialog->wiCB1.bEnabled = FALSE;
  diDialog->wiCB2.bEnabled = FALSE;
  diDialog->wiCB3.bEnabled = FALSE;

  diDialog->wiCB0.bCheckBoxState = FALSE;
  diDialog->wiCB1.bCheckBoxState = FALSE;
  diDialog->wiCB2.bCheckBoxState = FALSE;
  diDialog->wiCB3.bCheckBoxState = FALSE;

  if((diDialog->wiCB0.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB1.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB2.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB3.szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((diDialog->wiCB0.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB1.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB2.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->wiCB3.szArchive = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgWindowsIntegration(diWI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));

  FreeMemory(&(diDialog->wiCB0.szDescription));
  FreeMemory(&(diDialog->wiCB1.szDescription));
  FreeMemory(&(diDialog->wiCB2.szDescription));
  FreeMemory(&(diDialog->wiCB3.szDescription));
  FreeMemory(&(diDialog->wiCB0.szArchive));
  FreeMemory(&(diDialog->wiCB1.szArchive));
  FreeMemory(&(diDialog->wiCB2.szArchive));
  FreeMemory(&(diDialog->wiCB3.szArchive));
}

HRESULT InitDlgProgramFolder(diPF *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgProgramFolder(diPF *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
}

HRESULT InitDlgDownloadOptions(diDO *diDialog)
{
  diDialog->bShowDialog       = FALSE;
  diDialog->bSaveInstaller    = FALSE;
  if((diDialog->szTitle       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0    = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage1    = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgDownloadOptions(diDO *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szMessage1));
}

HRESULT InitDlgAdvancedSettings(diAS *diDialog)
{
  diDialog->bShowDialog       = FALSE;
  if((diDialog->szTitle       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0    = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyServer = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szProxyPort   = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgAdvancedSettings(diAS *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
  FreeMemory(&(diDialog->szProxyServer));
  FreeMemory(&(diDialog->szProxyPort));
}

HRESULT InitDlgStartInstall(diSI *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessageInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessageDownload = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgStartInstall(diSI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessageInstall));
  FreeMemory(&(diDialog->szMessageDownload));
}

DWORD InitDlgReboot(diR *diDialog)
{
  diDialog->dwShowDialog = FALSE;
  if(NS_LoadStringAlloc(hSetupRscInst, IDS_DLG_REBOOT_TITLE, &(diDialog->szTitle), MAX_BUF))
    return(1);

  return(0);
}

void DeInitDlgReboot(diR *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
}

HRESULT InitSetupGeneral()
{
  char szBuf[MAX_BUF];

  sgProduct.dwMode               = NORMAL;
  sgProduct.dwCustomType         = ST_RADIO0;
  sgProduct.dwNumberOfComponents = 0;

  if((sgProduct.szPath                        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szSubPath                     = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProductName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderName           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderPath           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szAlternateArchiveSearchPath  = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szParentProcessFilename       = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((szTempSetupPath                         = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szSetupTitle0                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szSetupTitle1                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szSetupTitle2                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if((szSiteSelectorDescription               = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  if(NS_LoadString(hSetupRscInst, IDS_CB_DEFAULT, szBuf, MAX_BUF) == WIZ_OK);
    lstrcpy(szSiteSelectorDescription, szBuf);

  return(0);
}

void DeInitSetupGeneral()
{
  FreeMemory(&(sgProduct.szPath));
  FreeMemory(&(sgProduct.szSubPath));
  FreeMemory(&(sgProduct.szProgramName));
  FreeMemory(&(sgProduct.szProductName));
  FreeMemory(&(sgProduct.szProgramFolderName));
  FreeMemory(&(sgProduct.szProgramFolderPath));
  FreeMemory(&(sgProduct.szAlternateArchiveSearchPath));
  FreeMemory(&(sgProduct.szParentProcessFilename));
  FreeMemory(&(szTempSetupPath));
  FreeMemory(&(sgProduct.szSetupTitle0));
  FreeMemory(&(sgProduct.szSetupTitle1));
  FreeMemory(&(sgProduct.szSetupTitle2));

  FreeMemory(&(szSiteSelectorDescription));
}

HRESULT InitSDObject()
{
  if((siSDObject.szXpcomFile = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szXpcomDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szNoAds = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szSilent = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExecution = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szConfirmInstall = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExtractMsg = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExe = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szExeParam = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szXpcomFilePath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitSDObject()
{
  FreeMemory(&(siSDObject.szXpcomFile));
  FreeMemory(&(siSDObject.szXpcomDir));
  FreeMemory(&(siSDObject.szNoAds));
  FreeMemory(&(siSDObject.szSilent));
  FreeMemory(&(siSDObject.szExecution));
  FreeMemory(&(siSDObject.szConfirmInstall));
  FreeMemory(&(siSDObject.szExtractMsg));
  FreeMemory(&(siSDObject.szExe));
  FreeMemory(&(siSDObject.szExeParam));
  FreeMemory(&(siSDObject.szXpcomFilePath));
}

HRESULT InitSXpcomFile()
{
  if((siCFXpcomFile.szSource = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFXpcomFile.szDestination = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFXpcomFile.szMessage = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  siCFXpcomFile.bCleanup         = TRUE;
  siCFXpcomFile.ullInstallSize   = 0;
  return(0);
}

void DeInitSXpcomFile()
{
  FreeMemory(&(siCFXpcomFile.szSource));
  FreeMemory(&(siCFXpcomFile.szDestination));
  FreeMemory(&(siCFXpcomFile.szMessage));
}

siC *CreateSiCNode()
{
  siC *siCNode;

  if((siCNode = NS_GlobalAlloc(sizeof(struct sinfoComponent))) == NULL)
    exit(1);

  siCNode->dwAttributes             = 0;
  siCNode->ullInstallSize           = 0;
  siCNode->ullInstallSizeSystem     = 0;
  siCNode->ullInstallSizeArchive    = 0;
  siCNode->lRandomInstallPercentage = 0;
  siCNode->lRandomInstallValue      = 0;

  if((siCNode->szArchiveName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szArchivePath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDestinationPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szParameter = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  siCNode->siCDDependencies = NULL;
  siCNode->siCDDependees    = NULL;
  siCNode->Next             = NULL;
  siCNode->Prev             = NULL;

  return(siCNode);
}

void SiCNodeInsert(siC **siCHead, siC *siCTemp)
{
  if(*siCHead == NULL)
  {
    *siCHead          = siCTemp;
    (*siCHead)->Next  = *siCHead;
    (*siCHead)->Prev  = *siCHead;
  }
  else
  {
    siCTemp->Next           = *siCHead;
    siCTemp->Prev           = (*siCHead)->Prev;
    (*siCHead)->Prev->Next  = siCTemp;
    (*siCHead)->Prev        = siCTemp;
  }
}

void SiCNodeDelete(siC *siCTemp)
{
  if(siCTemp != NULL)
  {
    DeInitSiCDependencies(siCTemp->siCDDependencies);
    DeInitSiCDependencies(siCTemp->siCDDependees);

    siCTemp->Next->Prev = siCTemp->Prev;
    siCTemp->Prev->Next = siCTemp->Next;
    siCTemp->Next       = NULL;
    siCTemp->Prev       = NULL;

    FreeMemory(&(siCTemp->szDestinationPath));
    FreeMemory(&(siCTemp->szArchivePath));
    FreeMemory(&(siCTemp->szArchiveName));
    FreeMemory(&(siCTemp->szParameter));
    FreeMemory(&(siCTemp->szDescriptionLong));
    FreeMemory(&(siCTemp->szDescriptionShort));
    FreeMemory(&siCTemp);
  }
}

siCD *CreateSiCDepNode()
{
  siCD *siCDepNode;

  if((siCDepNode = NS_GlobalAlloc(sizeof(struct sinfoComponentDep))) == NULL)
    exit(1);

  if((siCDepNode->szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  siCDepNode->Next = NULL;
  siCDepNode->Prev = NULL;

  return(siCDepNode);
}

void SiCDepNodeInsert(siCD **siCDepHead, siCD *siCDepTemp)
{
  if(*siCDepHead == NULL)
  {
    *siCDepHead          = siCDepTemp;
    (*siCDepHead)->Next  = *siCDepHead;
    (*siCDepHead)->Prev  = *siCDepHead;
  }
  else
  {
    siCDepTemp->Next           = *siCDepHead;
    siCDepTemp->Prev           = (*siCDepHead)->Prev;
    (*siCDepHead)->Prev->Next  = siCDepTemp;
    (*siCDepHead)->Prev        = siCDepTemp;
  }
}

void SiCDepNodeDelete(siCD *siCDepTemp)
{
  if(siCDepTemp != NULL)
  {
    siCDepTemp->Next->Prev = siCDepTemp->Prev;
    siCDepTemp->Prev->Next = siCDepTemp->Next;
    siCDepTemp->Next       = NULL;
    siCDepTemp->Prev       = NULL;

    FreeMemory(&(siCDepTemp->szDescriptionShort));
    FreeMemory(&siCDepTemp);
  }
}

ssi *CreateSsiSiteSelectorNode()
{
  ssi *ssiNode;

  if((ssiNode = NS_GlobalAlloc(sizeof(struct ssInfo))) == NULL)
    exit(1);
  if((ssiNode->szDescription = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((ssiNode->szDomain = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((ssiNode->szIdentifier = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);

  ssiNode->Next = NULL;
  ssiNode->Prev = NULL;

  return(ssiNode);
}

void SsiSiteSelectorNodeInsert(ssi **ssiHead, ssi *ssiTemp)
{
  if(*ssiHead == NULL)
  {
    *ssiHead          = ssiTemp;
    (*ssiHead)->Next  = *ssiHead;
    (*ssiHead)->Prev  = *ssiHead;
  }
  else
  {
    ssiTemp->Next           = *ssiHead;
    ssiTemp->Prev           = (*ssiHead)->Prev;
    (*ssiHead)->Prev->Next  = ssiTemp;
    (*ssiHead)->Prev        = ssiTemp;
  }
}

void SsiSiteSelectorNodeDelete(ssi *ssiTemp)
{
  if(ssiTemp != NULL)
  {
    ssiTemp->Next->Prev = ssiTemp->Prev;
    ssiTemp->Prev->Next = ssiTemp->Next;
    ssiTemp->Next       = NULL;
    ssiTemp->Prev       = NULL;

    FreeMemory(&(ssiTemp->szDescription));
    FreeMemory(&(ssiTemp->szDomain));
    FreeMemory(&(ssiTemp->szIdentifier));
    FreeMemory(&ssiTemp);
  }
}

HRESULT SiCNodeGetAttributes(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->dwAttributes);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->dwAttributes);

        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

void SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount  = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
      {
        if(bSet)
          siCTemp->dwAttributes |= dwAttributes;
        else
          siCTemp->dwAttributes &= ~dwAttributes;
      }

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
        {
          if(bSet)
            siCTemp->dwAttributes |= dwAttributes;
          else
            siCTemp->dwAttributes &= ~dwAttributes;
        }

        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
}

BOOL IsInList(DWORD dwCurrentItem, DWORD dwItems, DWORD *dwItemsSelected)
{
  DWORD i;

  for(i = 0; i < dwItems; i++)
  {
    if(dwItemsSelected[i] == dwCurrentItem)
      return(TRUE);
  }

  return(FALSE);
}

void SiCNodeSetItemsSelected(DWORD dwItems, DWORD *dwItemsSelected)
{
  DWORD i;
  siC   *siCTemp;

  for(i = 0; i < sgProduct.dwNumberOfComponents; i++)
  {
    siCTemp = SiCNodeGetObject(i, TRUE, AC_ALL);
    if(IsInList(i, dwItems, dwItemsSelected))
    {
      if((siCTemp->lRandomInstallPercentage != 0) &&
         (siCTemp->lRandomInstallPercentage <= siCTemp->lRandomInstallValue))
        siCTemp->dwAttributes &= ~SIC_SELECTED;
    }
    else
      siCTemp->dwAttributes &= ~SIC_SELECTED;
  }
}

char *SiCNodeGetDescriptionShort(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->szDescriptionShort);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->szDescriptionShort);
      
        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

char *SiCNodeGetDescriptionLong(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->szDescriptionLong);

      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->szDescriptionLong);
      
        ++dwCount;
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

ULONGLONG SiCNodeGetInstallSize(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSize);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSize);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

ULONGLONG SiCNodeGetInstallSizeSystem(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSizeSystem);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSizeSystem);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

ULONGLONG SiCNodeGetInstallSizeArchive(DWORD dwIndex, BOOL bIncludeInvisible, DWORD dwACFlag)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
       ((dwACFlag == AC_ALL) ||
       ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
       ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      if(dwIndex == 0)
        return(siCTemp->ullInstallSizeArchive);

      ++dwCount;
    }
    
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(((bIncludeInvisible == TRUE) || ((bIncludeInvisible == FALSE) && (!(siCTemp->dwAttributes & SIC_INVISIBLE)))) &&
         ((dwACFlag == AC_ALL) ||
         ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
         ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        if(dwIndex == dwCount)
          return(siCTemp->ullInstallSizeArchive);
      
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(0L);
}

/* retrieve Index of node containing short description */
int SiCNodeGetIndexDS(char *szInDescriptionShort)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(lstrcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
      return(dwCount);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(lstrcmpi(szInDescriptionShort, siCTemp->szDescriptionShort) == 0)
        return(dwCount);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

siC *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs, DWORD dwACFlag)
{
  DWORD dwCount = -1;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if((bIncludeInvisibleObjs) &&
      ((dwACFlag == AC_ALL) ||
      ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
      ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      ++dwCount;
    }
    else if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) &&
           ((dwACFlag == AC_ALL) ||
           ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
           ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
    {
      ++dwCount;
    }

    if(dwIndex == dwCount)
      return(siCTemp);

    siCTemp = siCTemp->Next;
    while((siCTemp != siComponents) && (siCTemp != NULL))
    {
      if(bIncludeInvisibleObjs)
      {
        ++dwCount;
      }
      else if((!(siCTemp->dwAttributes & SIC_INVISIBLE)) &&
             ((dwACFlag == AC_ALL) ||
             ((dwACFlag == AC_COMPONENTS)            && (!(siCTemp->dwAttributes & SIC_ADDITIONAL))) ||
             ((dwACFlag == AC_ADDITIONAL_COMPONENTS) &&   (siCTemp->dwAttributes & SIC_ADDITIONAL))))
      {
        ++dwCount;
      }

      if(dwIndex == dwCount)
        return(siCTemp);
      
      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

DWORD GetAdditionalComponentsCount()
{
  DWORD dwCount  = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_ADDITIONAL)
    {
      ++dwCount;
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != siComponents) && (siCTemp != NULL))
    {
      if(siCTemp->dwAttributes & SIC_ADDITIONAL)
      {
        ++dwCount;
      }
      
      siCTemp = siCTemp->Next;
    }
  }
  return(dwCount);
}

dsN *CreateDSNode()
{
  dsN *dsNode;

  if((dsNode = NS_GlobalAlloc(sizeof(struct diskSpaceNode))) == NULL)
    exit(1);

  dsNode->ullSpaceRequired = 0;

  if((dsNode->szPath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  dsNode->Next             = dsNode;
  dsNode->Prev             = dsNode;

  return(dsNode);
}

void DsNodeInsert(dsN **dsNHead, dsN *dsNTemp)
{
  if(*dsNHead == NULL)
  {
    *dsNHead          = dsNTemp;
    (*dsNHead)->Next  = *dsNHead;
    (*dsNHead)->Prev  = *dsNHead;
  }
  else
  {
    dsNTemp->Next           = *dsNHead;
    dsNTemp->Prev           = (*dsNHead)->Prev;
    (*dsNHead)->Prev->Next  = dsNTemp;
    (*dsNHead)->Prev        = dsNTemp;
  }
}

void DsNodeDelete(dsN **dsNTemp)
{
  if(*dsNTemp != NULL)
  {
    (*dsNTemp)->Next->Prev = (*dsNTemp)->Prev;
    (*dsNTemp)->Prev->Next = (*dsNTemp)->Next;
    (*dsNTemp)->Next       = NULL;
    (*dsNTemp)->Prev       = NULL;

    FreeMemory(&((*dsNTemp)->szPath));
    FreeMemory(dsNTemp);
  }
}

BOOL IsWin95Debute()
{
  HINSTANCE hLib;
  BOOL      bIsWin95Debute;

  bIsWin95Debute = FALSE;
  if((hLib = LoadLibraryEx("kernel32.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
  {
    if(((FARPROC)NS_GetDiskFreeSpaceEx = GetProcAddress(hLib, "GetDiskFreeSpaceExA")) == NULL)
    {
      (FARPROC)NS_GetDiskFreeSpace = GetProcAddress(hLib, "GetDiskFreeSpaceA");
      bIsWin95Debute = TRUE;
    }

    FreeLibrary(hLib);
  }
  return(bIsWin95Debute);
}

/* returns the value in kilobytes */
ULONGLONG GetDiskSpaceRequired(DWORD dwType)
{
  ULONGLONG ullTotalSize = 0;
  siC       *siCTemp     = siComponents;

  if(siCTemp != NULL)
  {
    if(siCTemp->dwAttributes & SIC_SELECTED)
    {
      switch(dwType)
      {
        case DSR_DESTINATION:
          ullTotalSize += siCTemp->ullInstallSize;
          break;

        case DSR_SYSTEM:
          ullTotalSize += siCTemp->ullInstallSizeSystem;
          break;

        case DSR_TEMP:
        case DSR_DOWNLOAD_SIZE:
          if((LocateJar(siCTemp, NULL, 0, FALSE) == FALSE) || (dwType == DSR_DOWNLOAD_SIZE))
            ullTotalSize += siCTemp->ullInstallSizeArchive;
          break;
      }
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(siCTemp->dwAttributes & SIC_SELECTED)
      {
        switch(dwType)
        {
          case DSR_DESTINATION:
            ullTotalSize += siCTemp->ullInstallSize;
            break;

          case DSR_SYSTEM:
            ullTotalSize += siCTemp->ullInstallSizeSystem;
            break;

          case DSR_TEMP:
          case DSR_DOWNLOAD_SIZE:
            if((LocateJar(siCTemp, NULL, 0, FALSE) == FALSE) || (dwType == DSR_DOWNLOAD_SIZE))
              ullTotalSize += siCTemp->ullInstallSizeArchive;
            break;
        }
      }

      siCTemp = siCTemp->Next;
    }
  }

  /* add the amount of disk space it will take for the 
     xpinstall engine in the TEMP area */
  if(dwType == DSR_TEMP)
    ullTotalSize += siCFXpcomFile.ullInstallSize;

  return(ullTotalSize);
}

int LocateExistingPath(char *szPath, char *szExistingPath, DWORD dwExistingPathSize)
{
  char szBuf[MAX_BUF];

  lstrcpy(szExistingPath, szPath);
  AppendBackSlash(szExistingPath, dwExistingPathSize);
  while((FileExists(szExistingPath) == FALSE))
  {
    RemoveBackSlash(szExistingPath);
    ParsePath(szExistingPath, szBuf, sizeof(szBuf), PP_PATH_ONLY);
    lstrcpy(szExistingPath, szBuf);
    AppendBackSlash(szExistingPath, dwExistingPathSize);
  }
  return(WIZ_OK);
}

/* returns the value in bytes */
ULONGLONG GetDiskSpaceAvailable(LPSTR szPath)
{
  char            szTempPath[MAX_BUF];
  char            szBuf[MAX_BUF];
  char            szExistingPath[MAX_BUF];
  char            szBuf2[MAX_BUF];
  ULARGE_INTEGER  uliFreeBytesAvailableToCaller;
  ULARGE_INTEGER  uliTotalNumberOfBytesToCaller;
  ULARGE_INTEGER  uliTotalNumberOfFreeBytes;
  ULONGLONG       ullReturn = 0;
  DWORD           dwSectorsPerCluster;
  DWORD           dwBytesPerSector;
  DWORD           dwNumberOfFreeClusters;
  DWORD           dwTotalNumberOfClusters;

  if((ulOSType & OS_WIN95_DEBUTE) && (NS_GetDiskFreeSpace != NULL))
  {
    ParsePath(szPath, szTempPath, MAX_BUF, PP_ROOT_ONLY);
    NS_GetDiskFreeSpace(szTempPath, 
                        &dwSectorsPerCluster,
                        &dwBytesPerSector,
                        &dwNumberOfFreeClusters,
                        &dwTotalNumberOfClusters);
    ullReturn = ((ULONGLONG)dwBytesPerSector * (ULONGLONG)dwSectorsPerCluster * (ULONGLONG)dwNumberOfFreeClusters);
  }
  else if(NS_GetDiskFreeSpaceEx != NULL)
  {
    LocateExistingPath(szPath, szExistingPath, sizeof(szExistingPath));
    AppendBackSlash(szExistingPath, sizeof(szExistingPath));
    if(NS_GetDiskFreeSpaceEx(szExistingPath,
                             &uliFreeBytesAvailableToCaller,
                             &uliTotalNumberOfBytesToCaller,
                             &uliTotalNumberOfFreeBytes) == FALSE)
    {
      char szEDeterminingDiskSpace[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_DETERMINING_DISK_SPACE, szEDeterminingDiskSpace, MAX_BUF) == WIZ_OK)
      {
        lstrcpy(szBuf2, "\n    ");
        lstrcat(szBuf2, szPath);
        wsprintf(szBuf, szEDeterminingDiskSpace, szBuf2);
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
    }
    ullReturn = uliFreeBytesAvailableToCaller.QuadPart;
  }

  if(ullReturn > 1024)
    ullReturn /= 1024;
  else
    ullReturn = 0;

  return(ullReturn);
}

HRESULT ErrorMsgDiskSpace(ULONGLONG ullDSAvailable, ULONGLONG ullDSRequired, LPSTR szPath, BOOL bCrutialMsg)
{
  char      szBuf0[MAX_BUF];
  char      szBuf1[MAX_BUF];
  char      szBuf2[MAX_BUF];
  char      szBuf3[MAX_BUF];
  char      szBufRootPath[MAX_BUF];
  char      szBufMsg[MAX_BUF];
  char      szDSAvailable[MAX_BUF];
  char      szDSRequired[MAX_BUF];
  char      szDlgDiskSpaceCheckTitle[MAX_BUF];
  char      szDlgDiskSpaceCheckMsg[MAX_BUF];
  DWORD     dwDlgType;

  if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_TITLE, szDlgDiskSpaceCheckTitle, MAX_BUF) != WIZ_OK)
    exit(1);

  if(bCrutialMsg)
  {
    dwDlgType = MB_RETRYCANCEL;
    if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_CRUTIAL_MSG, szDlgDiskSpaceCheckMsg, MAX_BUF) != WIZ_OK)
      exit(1);
  }
  else
  {
    dwDlgType = MB_OK;
    if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_MSG, szDlgDiskSpaceCheckMsg, MAX_BUF) != WIZ_OK)
      exit(1);
  }

  ParsePath(szPath, szBufRootPath, sizeof(szBufRootPath), PP_ROOT_ONLY);
  RemoveBackSlash(szBufRootPath);
  lstrcpy(szBuf0, szPath);
  RemoveBackSlash(szBuf0);

  _ui64toa(ullDSAvailable, szDSAvailable, 10);
  _ui64toa(ullDSRequired, szDSRequired, 10);

  lstrcpy(szBuf1, "\n\n    ");
  lstrcat(szBuf1, szBuf0);
  lstrcat(szBuf1, "\n\n    ");
  lstrcpy(szBuf2, szDSRequired);
  lstrcat(szBuf2, " K\n    ");
  lstrcpy(szBuf3, szDSAvailable);
  lstrcat(szBuf3, " K\n\n");
  wsprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBufRootPath, szBuf1, szBuf2, szBuf3);

  if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
  {
    return(MessageBox(hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, dwDlgType | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND));
  }
  else if(sgProduct.dwMode == AUTO)
  {
    ShowMessage(szBufMsg, TRUE);
    Delay(5);
    ShowMessage(szBufMsg, FALSE);
    exit(1);
  }

  return(IDCANCEL);
}

void UpdatePathDiskSpaceRequired(LPSTR szPath, ULONGLONG ullSize, dsN **dsnComponentDSRequirement)
{
  BOOL  bFound = FALSE;
  dsN   *dsnTemp = *dsnComponentDSRequirement;

  if(ullSize > 0)
  {
    do
    {
      if(*dsnComponentDSRequirement == NULL)
      {
        *dsnComponentDSRequirement = CreateDSNode();
        dsnTemp = *dsnComponentDSRequirement;
        strcpy(dsnTemp->szPath, szPath);
        dsnTemp->ullSpaceRequired = ullSize;
        bFound = TRUE;
      }
      else if(lstrcmpi(dsnTemp->szPath, szPath) == 0)
      {
        dsnTemp->ullSpaceRequired += ullSize;
        bFound = TRUE;
      }
      else
        dsnTemp = dsnTemp->Next;

    } while((dsnTemp != *dsnComponentDSRequirement) && (dsnTemp != NULL) && (bFound == FALSE));

    if(bFound == FALSE)
    {
      dsnTemp = CreateDSNode();
      strcpy(dsnTemp->szPath, szPath);
      dsnTemp->ullSpaceRequired = ullSize;
      DsNodeInsert(dsnComponentDSRequirement, dsnTemp);
    }
  }
}

HRESULT InitComponentDiskSpaceInfo(dsN **dsnComponentDSRequirement)
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  HRESULT   hResult    = 0;
  char      szBuf[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szSysPath[MAX_BUF];
  char      szBufSysPath[MAX_BUF];
  char      szBufTempPath[MAX_BUF];

  if(GetSystemDirectory(szSysPath, MAX_BUF) == 0)
  {
    ZeroMemory(szSysPath, MAX_BUF);
    ZeroMemory(szBufSysPath, MAX_BUF);
  }
  else
  {
    ParsePath(szSysPath, szBufSysPath, sizeof(szBufSysPath), PP_ROOT_ONLY);
    AppendBackSlash(szBufSysPath, sizeof(szBufSysPath));
  }

  ParsePath(szTempDir, szBufTempPath, sizeof(szBufTempPath), PP_ROOT_ONLY);
  AppendBackSlash(szBufTempPath, sizeof(szBufTempPath));

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      if(*(siCObject->szDestinationPath) == '\0')
        lstrcpy(szBuf, sgProduct.szPath);
      else
        lstrcpy(szBuf, siCObject->szDestinationPath);

      AppendBackSlash(szBuf, sizeof(szBuf));
      UpdatePathDiskSpaceRequired(szBuf, siCObject->ullInstallSize, dsnComponentDSRequirement);

      if(*szSysPath != '\0')
        UpdatePathDiskSpaceRequired(szSysPath, siCObject->ullInstallSizeSystem, dsnComponentDSRequirement);

      if(*szTempDir != '\0')
        UpdatePathDiskSpaceRequired(szTempDir, siCObject->ullInstallSizeArchive, dsnComponentDSRequirement);
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  /* take the uncompressed size of Xpcom into account */
  if(*szBufTempPath != '\0')
    UpdatePathDiskSpaceRequired(szBufTempPath, siCFXpcomFile.ullInstallSize, dsnComponentDSRequirement);

  return(hResult);
}

HRESULT VerifyDiskSpace()
{
  ULONGLONG ullDSAvailable;
  HRESULT   hRetValue = FALSE;
  dsN       *dsnComponentDSRequirement = NULL;
  dsN       *dsnTemp = NULL;


  InitComponentDiskSpaceInfo(&dsnComponentDSRequirement);
  if(dsnComponentDSRequirement != NULL)
  {
    dsnTemp = dsnComponentDSRequirement;

    do
    {
      if(dsnTemp != NULL)
      {
        ullDSAvailable = GetDiskSpaceAvailable(dsnTemp->szPath);
        if(ullDSAvailable < dsnTemp->ullSpaceRequired)
        {
          hRetValue = ErrorMsgDiskSpace(ullDSAvailable, dsnTemp->ullSpaceRequired, dsnTemp->szPath, FALSE);
          break;
        }

        dsnTemp = dsnTemp->Next;
      }
    } while((dsnTemp != dsnComponentDSRequirement) && (dsnTemp != NULL));
  }

  DeInitDSNode(&dsnComponentDSRequirement);
  return(hRetValue);
}

HRESULT ParseComponentAttributes(char *szAttribute)
{
  char  szBuf[MAX_BUF];
  DWORD dwAttributes = 0;

  lstrcpy(szBuf, szAttribute);
  strupr(szBuf);

  if(strstr(szBuf, "SELECTED") != NULL)
    dwAttributes |= SIC_SELECTED;
  if(strstr(szBuf, "INVISIBLE") != NULL)
    dwAttributes |= SIC_INVISIBLE;
  if(strstr(szBuf, "LAUNCHAPP") != NULL)
    dwAttributes |= SIC_LAUNCHAPP;
  if(strstr(szBuf, "DOWNLOAD_ONLY") != NULL)
    dwAttributes |= SIC_DOWNLOAD_ONLY;
  if(strstr(szBuf, "ADDITIONAL") != NULL)
    dwAttributes |= SIC_ADDITIONAL;
  if(strstr(szBuf, "DISABLED") != NULL)
    dwAttributes |= SIC_DISABLED;

  return(dwAttributes);
}

long RandomSelect()
{
  long lArbitrary = 0;

  srand((unsigned)time(NULL));
  lArbitrary = rand() % 100;
  return(lArbitrary);
}

void InitSiComponents(char *szFileIni)
{
  DWORD dwIndex0;
  DWORD dwIndex1;
  char  szIndex0[MAX_BUF];
  char  szIndex1[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentItem[MAX_BUF];
  char  szDependency[MAX_BUF];
  char  szDependee[MAX_BUF];
  char  szDPSection[MAX_BUF];
  siC   *siCTemp;
  siCD  *siCDepTemp;
  siCD  *siCDDependeeTemp;

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  lstrcpy(szComponentItem, "Component");
  lstrcat(szComponentItem, szIndex0);
  GetPrivateProfileString(szComponentItem, "Archive", "", szBuf, MAX_BUF, szFileIni);
  while(*szBuf != '\0')
  {
    /* create and initialize empty node */
    siCTemp = CreateSiCNode();

    /* store name of archive for component */
    lstrcpy(siCTemp->szArchiveName, szBuf);
    
    /* get short description of component */
    GetPrivateProfileString(szComponentItem, "Description Short", "", szBuf, MAX_BUF, szFileIni);
    lstrcpy(siCTemp->szDescriptionShort, szBuf);

    /* get long description of component */
    GetPrivateProfileString(szComponentItem, "Description Long", "", szBuf, MAX_BUF, szFileIni);
    lstrcpy(siCTemp->szDescriptionLong, szBuf);

    /* get commandline parameter for component */
    GetPrivateProfileString(szComponentItem, "Parameter", "", szBuf, MAX_BUF, szFileIni);
    DecryptString(siCTemp->szParameter, szBuf);

    /* get install size required in destination for component.  Sould be in Kilobytes */
    GetPrivateProfileString(szComponentItem, "Install Size", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
      siCTemp->ullInstallSize = _atoi64(szBuf);
    else
      siCTemp->ullInstallSize = 0;

    /* get install size required in system for component.  Sould be in Kilobytes */
    GetPrivateProfileString(szComponentItem, "Install Size System", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
      siCTemp->ullInstallSizeSystem = _atoi64(szBuf);
    else
      siCTemp->ullInstallSizeSystem = 0;

    /* get install size required in temp for component.  Sould be in Kilobytes */
    GetPrivateProfileString(szComponentItem, "Install Size Archive", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
      siCTemp->ullInstallSizeArchive = _atoi64(szBuf);
    else
      siCTemp->ullInstallSizeArchive = 0;

    /* get attributes of component */
    GetPrivateProfileString(szComponentItem, "Attributes", "", szBuf, MAX_BUF, szFileIni);
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf);

    /* get the random percentage value and select or deselect the component (by default) for
     * installation */
    GetPrivateProfileString(szComponentItem, "Random Install Percentage", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
    {
      siCTemp->lRandomInstallPercentage = atol(szBuf);
      if(siCTemp->lRandomInstallPercentage != 0)
        siCTemp->lRandomInstallValue = RandomSelect();
    }

    /* get all dependencies for this component */
    dwIndex1 = 0;
    itoa(dwIndex1, szIndex1, 10);
    lstrcpy(szDependency, "Dependency");
    lstrcat(szDependency, szIndex1);
    GetPrivateProfileString(szComponentItem, szDependency, "", szBuf, MAX_BUF, szFileIni);
    while(*szBuf != '\0')
    {
      /* create and initialize empty node */
      siCDepTemp = CreateSiCDepNode();

      /* store name of archive for component */
      lstrcpy(siCDepTemp->szDescriptionShort, szBuf);

      /* inserts the newly created component into the global component queue */
      SiCDepNodeInsert(&(siCTemp->siCDDependencies), siCDepTemp);

      ++dwIndex1;
      itoa(dwIndex1, szIndex1, 10);
      lstrcpy(szDependency, "Dependency");
      lstrcat(szDependency, szIndex1);
      GetPrivateProfileString(szComponentItem, szDependency, "", szBuf, MAX_BUF, szFileIni);
    }

    /* get all dependees for this component */
    dwIndex1 = 0;
    itoa(dwIndex1, szIndex1, 10);
    lstrcpy(szDependee, "Dependee");
    lstrcat(szDependee, szIndex1);
    GetPrivateProfileString(szComponentItem, szDependee, "", szBuf, MAX_BUF, szFileIni);
    while(*szBuf != '\0')
    {
      /* create and initialize empty node */
      siCDDependeeTemp = CreateSiCDepNode();

      /* store name of archive for component */
      lstrcpy(siCDDependeeTemp->szDescriptionShort, szBuf);

      /* inserts the newly created component into the global component queue */
      SiCDepNodeInsert(&(siCTemp->siCDDependees), siCDDependeeTemp);

      ++dwIndex1;
      itoa(dwIndex1, szIndex1, 10);
      lstrcpy(szDependee, "Dependee");
      lstrcat(szDependee, szIndex1);
      GetPrivateProfileString(szComponentItem, szDependee, "", szBuf, MAX_BUF, szFileIni);
    }

    // locate previous path if necessary
    lstrcpy(szDPSection, szComponentItem);
    lstrcat(szDPSection, "-Destination Path");
    if(LocatePreviousPath(szDPSection, siCTemp->szDestinationPath, MAX_PATH) == FALSE)
      ZeroMemory(siCTemp->szDestinationPath, MAX_PATH);

    /* inserts the newly created component into the global component queue */
    SiCNodeInsert(&siComponents, siCTemp);

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szComponentItem, "Component");
    lstrcat(szComponentItem, szIndex0);
    GetPrivateProfileString(szComponentItem, "Archive", "", szBuf, MAX_BUF, szFileIni);
  }

  sgProduct.dwNumberOfComponents = dwIndex0;
}

void ResetComponentAttributes(char *szFileIni)
{
  char  szIndex[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szComponentItem[MAX_BUF];
  siC   *siCTemp;
  DWORD dwCounter;

  for(dwCounter = 0; dwCounter < sgProduct.dwNumberOfComponents; dwCounter++)
  {
    itoa(dwCounter, szIndex, 10);
    lstrcpy(szComponentItem, "Component");
    lstrcat(szComponentItem, szIndex);

    siCTemp = SiCNodeGetObject(dwCounter, TRUE, AC_ALL);
    GetPrivateProfileString(szComponentItem, "Attributes", "", szBuf, MAX_BUF, szFileIni);
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf);
  }
}

void UpdateSiteSelector()
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szFileIniRedirect[MAX_BUF];
  ssi   *ssiSiteSelectorTemp;

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);

  if(FileExists(szFileIniRedirect) == FALSE)
    return;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKDescription, "Description");
  lstrcpy(szKDomain,      "Domain");
  lstrcat(szKDescription, szIndex);
  lstrcat(szKDomain,      szIndex);
  GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, MAX_BUF, szFileIniRedirect);
  while(*szDescription != '\0')
  {
    if(lstrcmpi(szDescription, szSiteSelectorDescription) == 0)
    {
      GetPrivateProfileString("Site Selector", szKDomain, "", szDomain, MAX_BUF, szFileIniRedirect);
      if(*szDomain != '\0')
      {
        ssiSiteSelectorTemp = SsiGetNode(szDescription);
        if(ssiSiteSelectorTemp != NULL)
        {
          lstrcpy(ssiSiteSelectorTemp->szDomain, szDomain);
        }
        else
        {
          /* no match found for the given domain description, so assume there's nothing
           * to change. just return. */
          return;
        }
      }
      else
      {
        /* found matched description, but domain was not set, so assume there's no
         * redirect required, just return. */
        return;
      }
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKDescription, "Description");
    lstrcpy(szKDomain,      "Domain");
    lstrcat(szKDescription, szIndex);
    lstrcat(szKDomain,      szIndex);
    ZeroMemory(szDescription, sizeof(szDescription));
    ZeroMemory(szDomain,      sizeof(szDomain));
    GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, MAX_BUF, szFileIniRedirect);
  }
}

void InitSiteSelector(char *szFileIni)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szKDescription[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szKDomain[MAX_BUF];
  char  szDomain[MAX_BUF];
  char  szKIdentifier[MAX_BUF];
  char  szIdentifier[MAX_BUF];
  ssi   *ssiSiteSelectorNewNode;

  ssiSiteSelector = NULL;

  /* get all dependees for this component */
  dwIndex = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKDescription, "Description");
  lstrcpy(szKDomain,      "Domain");
  lstrcpy(szKIdentifier,  "Identifier");
  lstrcat(szKDescription, szIndex);
  lstrcat(szKDomain,      szIndex);
  lstrcat(szKIdentifier,  szIndex);
  GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, MAX_BUF, szFileIni);
  while(*szDescription != '\0')
  {
    /* if the Domain and Identifier are not set, then skip */
    GetPrivateProfileString("Site Selector", szKDomain,     "", szDomain,     MAX_BUF, szFileIni);
    GetPrivateProfileString("Site Selector", szKIdentifier, "", szIdentifier, MAX_BUF, szFileIni);
    if((*szDomain != '\0') && (*szIdentifier != '\0'))
    {
      /* create and initialize empty node */
      ssiSiteSelectorNewNode = CreateSsiSiteSelectorNode();

      lstrcpy(ssiSiteSelectorNewNode->szDescription, szDescription);
      lstrcpy(ssiSiteSelectorNewNode->szDomain,      szDomain);
      lstrcpy(ssiSiteSelectorNewNode->szIdentifier,  szIdentifier);

      /* inserts the newly created node into the global node list */
      SsiSiteSelectorNodeInsert(&(ssiSiteSelector), ssiSiteSelectorNewNode);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKDescription, "Description");
    lstrcpy(szKDomain,      "Domain");
    lstrcpy(szKIdentifier,  "Identifier");
    lstrcat(szKDescription, szIndex);
    lstrcat(szKDomain,      szIndex);
    lstrcat(szKIdentifier,  szIndex);
    ZeroMemory(szDescription, sizeof(szDescription));
    ZeroMemory(szDomain,      sizeof(szDomain));
    ZeroMemory(szIdentifier,  sizeof(szIdentifier));
    GetPrivateProfileString("Site Selector", szKDescription, "", szDescription, MAX_BUF, szFileIni);
  }
}

void ViewSiComponents()
{
  char  szBuf[MAX_BUF];
  siC   *siCTemp = siComponents;
  siCD  *siCDepTemp;

  if(siCTemp != NULL)
  {
    siCDepTemp = siCTemp->siCDDependencies;
    lstrcpy(szBuf, siCTemp->szDescriptionShort);
    lstrcat(szBuf, ":\n");

    if(siCDepTemp != NULL)
    {
      lstrcat(szBuf, "    ");
      lstrcat(szBuf, siCDepTemp->szDescriptionShort);
      lstrcat(szBuf, "\n");

      siCDepTemp = siCDepTemp->Next;
      while((siCDepTemp != NULL) && (siCDepTemp != siCTemp->siCDDependencies))
      {
        lstrcat(szBuf, "    ");
        lstrcat(szBuf, siCDepTemp->szDescriptionShort);
        lstrcat(szBuf, "\n");

        siCDepTemp = siCDepTemp->Next;
      }
    }

    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      siCDepTemp = siCTemp->siCDDependencies;
      lstrcat(szBuf, siCTemp->szDescriptionShort);
      lstrcat(szBuf, ":\n");

      if(siCDepTemp != NULL)
      {
        lstrcat(szBuf, "    ");
        lstrcat(szBuf, siCDepTemp->szDescriptionShort);
        lstrcat(szBuf, "\n");

        siCDepTemp = siCDepTemp->Next;
        while((siCDepTemp != NULL) && (siCDepTemp != siCTemp->siCDDependencies))
        {
          lstrcat(szBuf, "    ");
          lstrcat(szBuf, siCDepTemp->szDescriptionShort);
          lstrcat(szBuf, "\n");

          siCDepTemp = siCDepTemp->Next;
        }
      }
      siCTemp = siCTemp->Next;
    }

    MessageBox(hWndMain, szBuf, NULL, MB_ICONEXCLAMATION);
  }
}

void DeInitSiCDependencies(siCD *siCDDependencies)
{
  siCD   *siCDepTemp;
  
  if(siCDDependencies == NULL)
  {
    return;
  }
  else if((siCDDependencies->Prev == NULL) || (siCDDependencies->Prev == siCDDependencies))
  {
    SiCDepNodeDelete(siCDDependencies);
    return;
  }
  else
  {
    siCDepTemp = siCDDependencies->Prev;
  }

  while(siCDepTemp != siCDDependencies)
  {
    SiCDepNodeDelete(siCDepTemp);
    siCDepTemp = siCDDependencies->Prev;
  }
  SiCDepNodeDelete(siCDepTemp);
}

void DeInitSiComponents()
{
  siC   *siCTemp;
  
  if(siComponents == NULL)
  {
    return;
  }
  else if((siComponents->Prev == NULL) || (siComponents->Prev == siComponents))
  {
    SiCNodeDelete(siComponents);
    return;
  }
  else
  {
    siCTemp = siComponents->Prev;
  }

  while(siCTemp != siComponents)
  {
    SiCNodeDelete(siCTemp);
    siCTemp = siComponents->Prev;
  }
  SiCNodeDelete(siCTemp);
}

void DeInitDSNode(dsN **dsnComponentDSRequirement)
{
  dsN *dsNTemp;
  
  if(*dsnComponentDSRequirement == NULL)
  {
    return;
  }
  else if(((*dsnComponentDSRequirement)->Prev == NULL) || ((*dsnComponentDSRequirement)->Prev == *dsnComponentDSRequirement))
  {
    DsNodeDelete(dsnComponentDSRequirement);
    return;
  }
  else
  {
    dsNTemp = (*dsnComponentDSRequirement)->Prev;
  }

  while(dsNTemp != *dsnComponentDSRequirement)
  {
    DsNodeDelete(&dsNTemp);
    dsNTemp = (*dsnComponentDSRequirement)->Prev;
  }
  DsNodeDelete(&dsNTemp);
}

BOOL ResolveComponentDependency(siCD *siCDInDependency)
{
  int     dwIndex;
  siCD    *siCDepTemp = siCDInDependency;
  BOOL    bMoreToResolve = FALSE;

  if(siCDepTemp != NULL)
  {
    if((dwIndex = SiCNodeGetIndexDS(siCDepTemp->szDescriptionShort)) != -1)
    {
      if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == FALSE)
      {
        bMoreToResolve = TRUE;
        SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
      }
    }

    siCDepTemp = siCDepTemp->Next;
    while((siCDepTemp != NULL) && (siCDepTemp != siCDInDependency))
    {
      if((dwIndex = SiCNodeGetIndexDS(siCDepTemp->szDescriptionShort)) != -1)
      {
        if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == FALSE)
        {
          bMoreToResolve = TRUE;
          SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
        }
      }

      siCDepTemp = siCDepTemp->Next;
    }
  }
  return(bMoreToResolve);
}

BOOL ResolveDependencies(DWORD dwIndex)
{
  BOOL  bMoreToResolve  = FALSE;
  DWORD dwCount         = 0;
  siC   *siCTemp        = siComponents;

  if(siCTemp != NULL)
  {
    /* can resolve specific component or all components (-1) */
    if((dwIndex == dwCount) || (dwIndex == -1))
    {
      if(SiCNodeGetAttributes(dwCount, TRUE, AC_ALL) & SIC_SELECTED)
      {
         bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies);
         if(dwIndex == dwCount)
         {
           return(bMoreToResolve);
         }
      }
    }

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      /* can resolve specific component or all components (-1) */
      if((dwIndex == dwCount) || (dwIndex == -1))
      {
        if(SiCNodeGetAttributes(dwCount, TRUE, AC_ALL) & SIC_SELECTED)
        {
           bMoreToResolve = ResolveComponentDependency(siCTemp->siCDDependencies);
           if(dwIndex == dwCount)
           {
             return(bMoreToResolve);
           }
        }
      }

      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(bMoreToResolve);
}

BOOL ResolveComponentDependee(siCD *siCDInDependee)
{
  int     dwIndex;
  siCD    *siCDDependeeTemp   = siCDInDependee;
  BOOL    bAtLeastOneSelected = FALSE;

  if(siCDDependeeTemp != NULL)
  {
    if((dwIndex = SiCNodeGetIndexDS(siCDDependeeTemp->szDescriptionShort)) != -1)
    {
      if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == TRUE)
      {
        bAtLeastOneSelected = TRUE;
      }
    }

    siCDDependeeTemp = siCDDependeeTemp->Next;
    while((siCDDependeeTemp != NULL) && (siCDDependeeTemp != siCDInDependee))
    {
      if((dwIndex = SiCNodeGetIndexDS(siCDDependeeTemp->szDescriptionShort)) != -1)
      {
        if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == TRUE)
        {
          bAtLeastOneSelected = TRUE;
        }
      }

      siCDDependeeTemp = siCDDependeeTemp->Next;
    }
  }
  return(bAtLeastOneSelected);
}

ssi* SsiGetNode(LPSTR szDescription)
{
  ssi *ssiSiteSelectorTemp = ssiSiteSelector;

  do
  {
    if(ssiSiteSelectorTemp == NULL)
      break;

    if(lstrcmpi(ssiSiteSelectorTemp->szDescription, szDescription) == 0)
      return(ssiSiteSelectorTemp);

    ssiSiteSelectorTemp = ssiSiteSelectorTemp->Next;
  } while((ssiSiteSelectorTemp != NULL) && (ssiSiteSelectorTemp != ssiSiteSelector));

  return(NULL);
}

void ResolveDependees(LPSTR szToggledDescriptionShort)
{
  BOOL  bAtLeastOneSelected;
  BOOL  bMoreToResolve  = FALSE;
  siC   *siCTemp        = siComponents;
  DWORD dwIndex;

  do
  {
    if(siCTemp == NULL)
      break;

    if((siCTemp->siCDDependees != NULL) &&
       (lstrcmpi(siCTemp->szDescriptionShort, szToggledDescriptionShort) != 0))
    {
      bAtLeastOneSelected = ResolveComponentDependee(siCTemp->siCDDependees);
      if(bAtLeastOneSelected == FALSE)
      {
        if((dwIndex = SiCNodeGetIndexDS(siCTemp->szDescriptionShort)) != -1)
        {
          if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == TRUE)
          {
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, FALSE, TRUE, AC_ALL);
            bMoreToResolve = TRUE;
          }
        }
      }
      else
      {
        if((dwIndex = SiCNodeGetIndexDS(siCTemp->szDescriptionShort)) != -1)
        {
          if((SiCNodeGetAttributes(dwIndex, TRUE, AC_ALL) & SIC_SELECTED) == FALSE)
          {
            SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE, TRUE, AC_ALL);
            bMoreToResolve = TRUE;
          }
        }
      }
    }

    siCTemp = siCTemp->Next;
  } while((siCTemp != NULL) && (siCTemp != siComponents));

  if(bMoreToResolve == TRUE)
    ResolveDependees(szToggledDescriptionShort);
}

void ParseCommandLine(LPSTR lpszCmdLine)
{
  char  szArgVBuf[MAX_BUF];
  int   i;
  int   iArgC;

#ifdef XXX_DEBUG
  char  szBuf[MAX_BUF];
  char  szOutputStr[MAX_BUF];
#endif

  iArgC = GetArgC(lpszCmdLine);

#ifdef XXX_DEBUG
  wsprintf(szOutputStr, "ArgC: %d\n", iArgC);
#endif

  i = 0;
  while(i < iArgC)
  {
    GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));

    if((lstrcmpi(szArgVBuf, "-a") == 0) || (lstrcmpi(szArgVBuf, "/a") == 0))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szAlternateArchiveSearchPath, szArgVBuf);
    }
    else if((lstrcmpi(szArgVBuf, "-n") == 0) || (lstrcmpi(szArgVBuf, "/n") == 0))
    {
      ++i;
      GetArgV(lpszCmdLine, i, szArgVBuf, sizeof(szArgVBuf));
      lstrcpy(sgProduct.szParentProcessFilename, szArgVBuf);
    }
    else if((lstrcmpi(szArgVBuf, "-ma") == 0) || (lstrcmpi(szArgVBuf, "/ma") == 0))
    {
      SetSetupRunMode("AUTO");
    }
    else if((lstrcmpi(szArgVBuf, "-ms") == 0) || (lstrcmpi(szArgVBuf, "/ms") == 0))
    {
      SetSetupRunMode("SILENT");
    }

#ifdef XXX_DEBUG
    itoa(i, szBuf, 10);
    lstrcat(szOutputStr, "    ");
    lstrcat(szOutputStr, szBuf);
    lstrcat(szOutputStr, ": ");
    lstrcat(szOutputStr, szArgVBuf);
    lstrcat(szOutputStr, "\n");
#endif

    ++i;
  }

#ifdef XXX_DEBUG
  MessageBox(NULL, szOutputStr, "Output", MB_OK);
#endif
}

void GetAlternateArchiveSearchPath(LPSTR lpszCmdLine)
{
  char  szBuf[MAX_PATH];
  LPSTR lpszAASPath;
  LPSTR lpszEndPath;
  LPSTR lpszEndQuote;

  if(lstrcpy(szBuf, lpszCmdLine))
  {
    if((lpszAASPath = strstr(szBuf, "-a")) == NULL)
      return;
    else
      lpszAASPath += 2;

    if(*lpszAASPath == '\"')
    {
      lpszAASPath = lpszAASPath + 1;
      if((lpszEndQuote = strstr(lpszAASPath, "\"")) != NULL)
      {
        *lpszEndQuote = '\0';
      }
    }
    else if((lpszEndPath = strstr(lpszAASPath, " ")) != NULL)
    {
      *lpszEndPath = '\0';
    }

    lstrcpy(sgProduct.szAlternateArchiveSearchPath, lpszAASPath);
  }
}

BOOL CheckProcessWin95(NS_CreateSnapshot pCreateToolhelp32Snapshot, NS_ProcessWalk pProcessWalkFirst, NS_ProcessWalk pProcessWalkNext, LPSTR szProcessName)
{
  BOOL            bRv             = FALSE;
  HANDLE          hCreateSnapshot = NULL;
  PROCESSENTRY32  peProcessEntry;
  
  hCreateSnapshot = pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(hCreateSnapshot == (HANDLE)-1)
    return(bRv);

  peProcessEntry.dwSize = sizeof(PROCESSENTRY32);
  if(pProcessWalkFirst(hCreateSnapshot, &peProcessEntry))
  {
    do
    {
      char  szBuf[MAX_BUF];

      ParsePath(peProcessEntry.szExeFile, szBuf, sizeof(szBuf), PP_FILENAME_ONLY);
      
      /* do process name string comparison here! */
      if(lstrcmpi(szBuf, szProcessName) == 0)
      {
        bRv = TRUE;
        break;
      }

    } while((bRv == FALSE) && pProcessWalkNext(hCreateSnapshot, &peProcessEntry));
  }

  CloseHandle(hCreateSnapshot);
  return(bRv);
}

BOOL CheckForProcess(LPSTR szProcessName, DWORD dwProcessName)
{
  HANDLE            hKernel                   = NULL;
  NS_CreateSnapshot pCreateToolhelp32Snapshot = NULL;
  NS_ProcessWalk    pProcessWalkFirst         = NULL;
  NS_ProcessWalk    pProcessWalkNext          = NULL;
  BOOL              bDoWin95Check             = TRUE;

  if((hKernel = GetModuleHandle("kernel32.dll")) == NULL)
    return(FALSE);

  pCreateToolhelp32Snapshot = (NS_CreateSnapshot)GetProcAddress(hKernel, "CreateToolhelp32Snapshot");
  pProcessWalkFirst         = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32First");
  pProcessWalkNext          = (NS_ProcessWalk)GetProcAddress(hKernel,    "Process32Next");

  if(pCreateToolhelp32Snapshot && pProcessWalkFirst && pProcessWalkNext)
    return(CheckProcessWin95(pCreateToolhelp32Snapshot, pProcessWalkFirst, pProcessWalkNext, szProcessName));
  else
    return(CheckProcessNT4(szProcessName, dwProcessName));
}

HRESULT CheckInstances()
{
  char  szSection[MAX_BUF];
  char  szProcessName[MAX_BUF];
  char  szClassName[MAX_BUF];
  char  szWindowName[MAX_BUF];
  char  szMessage[MAX_BUF];
  char  szIndex[MAX_BUF];
  int   iIndex;
  BOOL  bContinue;
  HWND  hwndFW;
  LPSTR szWN;
  LPSTR szCN;
  DWORD dwRv0;
  DWORD dwRv1;

  bContinue = TRUE;
  iIndex    = -1;
  while(bContinue)
  {
    ZeroMemory(szClassName,  sizeof(szClassName));
    ZeroMemory(szWindowName, sizeof(szWindowName));
    ZeroMemory(szMessage,    sizeof(szMessage));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    lstrcpy(szSection, "Check Instance");
    lstrcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "Message", "", szMessage, MAX_BUF, szFileIniConfig);
    if(GetPrivateProfileString(szSection, "Process Name", "", szProcessName, MAX_BUF, szFileIniConfig) != 0L)
    {
      if(*szProcessName != '\0')
      {
        if(CheckForProcess(szProcessName, sizeof(szProcessName)) == TRUE)
        {
          if(*szMessage != '\0')
          {
            if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
            {
              switch(MessageBox(hWndMain, szMessage, NULL, MB_ICONEXCLAMATION | MB_RETRYCANCEL))
              {
                case IDCANCEL:
                  /* User selected to cancel Setup */
                  return(TRUE);

                case IDRETRY:
                  /* User selected to retry.  Reset counter */
                  iIndex = -1;
                  break;
              }
            }
            else if(sgProduct.dwMode == AUTO)
            {
              ShowMessage(szMessage, TRUE);
              Delay(5);
              ShowMessage(szMessage, FALSE);

              /* Setup mode is AUTO.  Show message, timeout, then cancel because we can't allow user to continue */
              return(TRUE);
            }
          }
          else
          {
            /* No message to display.  Assume cancel because we can't allow user to continue */
            return(TRUE);
          }
        }
      }

      /* Process Name= key existed, and has been processed, so continue looking for more */
      continue;
    }

    /* Process Name= key did not exist, so look for other keys */
    dwRv0 = GetPrivateProfileString(szSection, "Class Name",  "", szClassName,  MAX_BUF, szFileIniConfig);
    dwRv1 = GetPrivateProfileString(szSection, "Window Name", "", szWindowName, MAX_BUF, szFileIniConfig);
    if((dwRv0 == 0L) &&
       (dwRv1 == 0L))
    {
      bContinue = FALSE;
    }
    else if((*szClassName != '\0') || (*szWindowName != '\0'))
    {
      if(*szClassName == '\0')
        szCN = NULL;
      else
        szCN = szClassName;

      if(*szWindowName == '\0')
        szWN = NULL;
      else
        szWN = szWindowName;

      if((hwndFW = FindWindow(szClassName, szWN)) != NULL)
      {
        if(*szMessage != '\0')
        {
          if((sgProduct.dwMode != SILENT) && (sgProduct.dwMode != AUTO))
          {
            switch(MessageBox(hWndMain, szMessage, NULL, MB_ICONEXCLAMATION | MB_RETRYCANCEL))
            {
              case IDCANCEL:
                /* User selected to cancel Setup */
                return(TRUE);

              case IDRETRY:
                /* User selected to retry.  Reset counter */
                iIndex = -1;
                break;
            }
          }
          else if(sgProduct.dwMode == AUTO)
          {
            ShowMessage(szMessage, TRUE);
            Delay(5);
            ShowMessage(szMessage, FALSE);

            /* Setup mode is AUTO.  Show message, timeout, then cancel because we can't allow user to continue */
            return(TRUE);
          }
        }
        else
        {
          /* No message to display.  Assume cancel because we can't allow user to continue */
          return(TRUE);
        }
      }
    }
  }

  return(FALSE);
}

BOOL GetFileVersion(LPSTR szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;

  vbVersion->ullMajor   = 0;
  vbVersion->ullMinor   = 0;
  vbVersion->ullRelease = 0;
  vbVersion->ullBuild   = 0;
  if(FileExists(szFile))
  {
    bRv    = TRUE;
    dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
    lpData = (LPVOID)malloc(sizeof(long)*dwLen);
    uLen   = 0;

    if(GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0)
    {
      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
      {
        lpBuffer2             = (VS_FIXEDFILEINFO *)lpBuffer;
        vbVersion->ullMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->ullMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->ullRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        vbVersion->ullBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }
    free(lpData);
  }
  else
    /* File does not exist */
    bRv = FALSE;

  return(bRv);
}

void TranslateVersionStr(LPSTR szVersion, verBlock *vbVersion)
{
  LPSTR szNum1 = NULL;
  LPSTR szNum2 = NULL;
  LPSTR szNum3 = NULL;
  LPSTR szNum4 = NULL;

  szNum1 = strtok(szVersion, ".");
  szNum2 = strtok(NULL,      ".");
  szNum3 = strtok(NULL,      ".");
  szNum4 = strtok(NULL,      ".");

  vbVersion->ullMajor   = _atoi64(szNum1);
  vbVersion->ullMinor   = _atoi64(szNum2);
  vbVersion->ullRelease = _atoi64(szNum3);
  vbVersion->ullBuild   = _atoi64(szNum4);
}

int CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew)
{
  if(vbVersionOld.ullMajor > vbVersionNew.ullMajor)
    return(4);
  else if(vbVersionOld.ullMajor < vbVersionNew.ullMajor)
    return(-4);

  if(vbVersionOld.ullMinor > vbVersionNew.ullMinor)
    return(3);
  else if(vbVersionOld.ullMinor < vbVersionNew.ullMinor)
    return(-3);

  if(vbVersionOld.ullRelease > vbVersionNew.ullRelease)
    return(2);
  else if(vbVersionOld.ullRelease < vbVersionNew.ullRelease)
    return(-2);

  if(vbVersionOld.ullBuild > vbVersionNew.ullBuild)
    return(1);
  else if(vbVersionOld.ullBuild < vbVersionNew.ullBuild)
    return(-1);

  /* the versions are all the same */
  return(0);
}

BOOL CheckLegacy(HWND hDlg)
{
  char      szSection[MAX_BUF];
  char      szFilename[MAX_BUF];
  LPSTR     szMessage[4];
  char      szIndex[MAX_BUF];
  char      szVersionNew[MAX_BUF];
  char      szDecryptedFilePath[MAX_BUF];
  int       iIndex;
  BOOL      bContinue;
  BOOL      bRv;
  DWORD     dwRv0;
  DWORD     dwRv1;
  verBlock  vbVersionNew;
  verBlock  vbVersionOld;

  bRv             = FALSE;
  gdwUpgradeValue = UG_NONE;
  szMessage[0]    = NULL;
  szMessage[1]    = NULL;
  szMessage[2]    = NULL;
  bContinue       = TRUE;
  iIndex          = -1;
  while(bContinue)
  {
    ZeroMemory(szFilename,      sizeof(szFilename));
    ZeroMemory(szVersionNew,    sizeof(szVersionNew));

    ++iIndex;
    itoa(iIndex, szIndex, 10);
    lstrcpy(szSection, "Legacy Check");
    lstrcat(szSection, szIndex);

    dwRv0 = GetPrivateProfileString(szSection, "Filename", "", szFilename, MAX_BUF, szFileIniConfig);
    dwRv1 = GetPrivateProfileString(szSection, "Version", "", szVersionNew, MAX_BUF, szFileIniConfig);
    if(dwRv0 == 0L)
    {
      bContinue = FALSE;
    }
    else if(*szFilename != '\0')
    {
      if((szMessage[0] = NS_GlobalAlloc(MAX_BUF)) == NULL)
      {
        bRv = TRUE;
        break;
      }
      if((szMessage[1] = NS_GlobalAlloc(MAX_BUF)) == NULL)
      {
        bRv = TRUE;
        break;
      }
      if((szMessage[2] = NS_GlobalAlloc(MAX_BUF)) == NULL)
      {
        bRv = TRUE;
        break;
      }
      if((szMessage[3] = NS_GlobalAlloc(MAX_BUF)) == NULL)
      {
        bRv = TRUE;
        break;
      }

      lstrcpy(szMessage[0], sgProduct.szPath);
      if(*sgProduct.szSubPath != '\0')
      {
        AppendBackSlash(szMessage[0], MAX_BUF);
        lstrcat(szMessage[0], sgProduct.szSubPath);
      }

      GetPrivateProfileString(szSection, "Message0", "", szMessage[1], MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Message1", "", szMessage[2], MAX_BUF, szFileIniConfig);
      GetPrivateProfileString(szSection, "Message2", "", szMessage[3], MAX_BUF, szFileIniConfig);
      if((*szMessage[1] == '\0') && (*szMessage[2] == '\0') && (*szMessage[3] == '\0'))
        /* no message string input. so just continue with the next check */
        continue;

      DecryptString(szDecryptedFilePath, szFilename);
      if((dwRv1 == 0L) || (*szVersionNew == '\0'))
      {
        if(FileExists(szDecryptedFilePath))
        {
          MessageBeep(MB_ICONEXCLAMATION);
          if((gdwUpgradeValue = DialogBoxParam(hSetupRscInst, MAKEINTRESOURCE(DLG_UPGRADE), hDlgCurrent, DlgProcUpgrade, (LPARAM)szMessage)) == UG_GOBACK)
          {
            bRv = TRUE;
            break;
          }
        }
        /* file does not exist, so it's okay.  Continue with the next check */
        continue;
      }

      if(GetFileVersion(szDecryptedFilePath, &vbVersionOld))
      {
        TranslateVersionStr(szVersionNew, &vbVersionNew);
        if(CompareVersion(vbVersionOld, vbVersionNew) < 0)
        {
          MessageBeep(MB_ICONEXCLAMATION);
          if((gdwUpgradeValue = DialogBoxParam(hSetupRscInst, MAKEINTRESOURCE(DLG_UPGRADE), hDlgCurrent, DlgProcUpgrade, (LPARAM)szMessage)) == UG_GOBACK)
          {
            bRv = TRUE;
            break;
          }
        }
      }
    }
  }

  FreeMemory(&szMessage[0]);
  FreeMemory(&szMessage[1]);
  FreeMemory(&szMessage[2]);

  /* returning TRUE means the user wants to go back and choose a different destination path
   * returning FALSE means the user is ignoring the warning
   */
  return(bRv);
}

COLORREF DecryptFontColor(LPSTR szColor)
{
  int  i = 0;
  long lFontColor = 0x00EEEEEE;

  while(TRUE)
  {
    if(*FontColorMap[i] == '\0')
      break;

    if(lstrcmpi(szColor, FontColorMap[i]) == 0)
    {
      if(*FontColorMap[i + 1] != '\0')
        lFontColor = atol(FontColorMap[i + 1]);

      break;
    }

    ++i;
  }

  return(lFontColor);
}

HRESULT ParseConfigIni(LPSTR lpszCmdLine)
{
  HDC  hdc;
  char szBuf[MAX_BUF];
  char szPreviousPath[MAX_BUF];
  char szShowDialog[MAX_BUF];

  if(CheckInstances())
    return(1);

  if(InitSetupGeneral())
    return(1);
  if(InitDlgWelcome(&diWelcome))
    return(1);
  if(InitDlgLicense(&diLicense))
    return(1);
  if(InitDlgSetupType(&diSetupType))
    return(1);
  if(InitDlgSelectComponents(&diSelectComponents, SM_SINGLE))
    return(1);
  if(InitDlgSelectComponents(&diSelectAdditionalComponents, SM_SINGLE))
    return(1);
  if(InitDlgWindowsIntegration(&diWindowsIntegration))
    return(1);
  if(InitDlgProgramFolder(&diProgramFolder))
    return(1);
  if(InitDlgDownloadOptions(&diDownloadOptions))
    return(1);
  if(InitDlgAdvancedSettings(&diAdvancedSettings))
    return(1);
  if(InitDlgStartInstall(&diStartInstall))
    return(1);
  if(InitSDObject())
    return(1);
  if(InitSXpcomFile())
    return(1);
 
  /* get install Mode information */
  GetPrivateProfileString("General", "Run Mode", "", szBuf, MAX_BUF, szFileIniConfig);
  SetSetupRunMode(szBuf);
  ParseCommandLine(lpszCmdLine);

  if((sgProduct.dwMode == NORMAL) || (sgProduct.dwMode == AUTO))
  {
    /* show blue background here */
    ShowWindow(hWndMain, SW_MAXIMIZE);
    UpdateWindow(hWndMain);
  }

  /* get product name description */
  GetPrivateProfileString("General", "Product Name", "", sgProduct.szProductName, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Sub Path",     "", sgProduct.szSubPath,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Program Name", "", sgProduct.szProgramName, MAX_BUF, szFileIniConfig);
  
  /* get main install path */
  if(LocatePreviousPath("Locate Previous Product Path", szPreviousPath, sizeof(szPreviousPath)) == FALSE)
  {
    GetPrivateProfileString("General", "Path", "", szBuf, MAX_BUF, szFileIniConfig);
    DecryptString(sgProduct.szPath, szBuf);
  }
  else
  {
    /* If the previous path is located in the regsitry, then we need to check to see if the path from
     * the regsitry plus the Sub Path contains the Program Name file.  If it does, then we have the
     * correct path, so just use it.
     *
     * If it does not contain the Program Name file, then check the parent path (from the registry) +
     * SubPath.  If this path contains the Program Name file, then we found an older path format.  We
     * then need to use the parent path as the default path.
     *
     * Only do the older path format checking if the Sub Path= and Program Name= keys exist.  If
     * either are not set, then assume that the path from the registry is what we want.
     */
    if((*sgProduct.szSubPath != '\0') && (*sgProduct.szProgramName != '\0'))
    {
      /* If the Sub Path= and Program Name= keys exist, do extra parsing for the correct path */
      lstrcpy(szBuf, szPreviousPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szSubPath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, sgProduct.szProgramName);

      /* Check to see if PreviousPath + SubPath + ProgramName exists.  If it does, then we have the
       * correct path.
       */
      if(FileExists(szBuf))
      {
        lstrcpy(sgProduct.szPath, szPreviousPath);
      }
      else
      {
        /* If not, try parent of PreviousPath + SubPath + ProgramName.
         * If this exists, then we need to use the parent path of PreviousPath.
         */
        RemoveBackSlash(szPreviousPath);
        ParsePath(szPreviousPath, szBuf, sizeof(szBuf), PP_PATH_ONLY);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, sgProduct.szSubPath);
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, sgProduct.szProgramName);

        if(FileExists(szBuf))
        {
          RemoveBackSlash(szPreviousPath);
          ParsePath(szPreviousPath, szBuf, sizeof(szBuf), PP_PATH_ONLY);
          lstrcpy(sgProduct.szPath, szBuf);
        }
        else
        {
          /* If we still can't locate ProgramName file, then use the default in the config.ini */
          GetPrivateProfileString("General", "Path", "", szBuf, MAX_BUF, szFileIniConfig);
          DecryptString(sgProduct.szPath, szBuf);
        }
      }
    }
    else
    {
      lstrcpy(sgProduct.szPath, szPreviousPath);
    }
  }
  RemoveBackSlash(sgProduct.szPath);

  /* make a copy of sgProduct.szPath to be used in the Setup Type dialog */
  lstrcpy(szTempSetupPath, sgProduct.szPath);
  
  /* get main program folder path */
  GetPrivateProfileString("General", "Program Folder Path", "", szBuf, MAX_BUF, szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderPath, szBuf);
  
  /* get main program folder name */
  GetPrivateProfileString("General", "Program Folder Name", "", szBuf, MAX_BUF, szFileIniConfig);
  DecryptString(sgProduct.szProgramFolderName, szBuf);

  /* get setup title strings */
  GetPrivateProfileString("General", "Setup Title0", "", sgProduct.szSetupTitle0, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Setup Title1", "", sgProduct.szSetupTitle1, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("General", "Setup Title2", "", sgProduct.szSetupTitle2, MAX_BUF, szFileIniConfig);

  /* get setup title font color */
  GetPrivateProfileString("General", "Setup Title0 Font Color", "", szBuf, sizeof(szBuf), szFileIniConfig);
  sgProduct.crSetupTitle0FontColor = DecryptFontColor(szBuf);
  GetPrivateProfileString("General", "Setup Title1 Font Color", "", szBuf, sizeof(szBuf), szFileIniConfig);
  sgProduct.crSetupTitle1FontColor = DecryptFontColor(szBuf);
  GetPrivateProfileString("General", "Setup Title2 Font Color", "", szBuf, sizeof(szBuf), szFileIniConfig);
  sgProduct.crSetupTitle2FontColor = DecryptFontColor(szBuf);

  /* get setup title font size */
  sgProduct.iSetupTitle0FontSize = 42;
  sgProduct.iSetupTitle1FontSize = 42;
  sgProduct.iSetupTitle2FontSize = 42;
  GetPrivateProfileString("General", "Setup Title0 Font Size", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    sgProduct.iSetupTitle0FontSize = atoi(szBuf);
  GetPrivateProfileString("General", "Setup Title1 Font Size", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    sgProduct.iSetupTitle1FontSize = atoi(szBuf);
  GetPrivateProfileString("General", "Setup Title2 Font Size", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(*szBuf != '\0')
    sgProduct.iSetupTitle2FontSize = atoi(szBuf);

  /* get setup title font shadow */
  GetPrivateProfileString("General", "Setup Title0 Font Shadow", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    sgProduct.bSetupTitle0FontShadow = FALSE;
  else
    sgProduct.bSetupTitle0FontShadow = TRUE;

  GetPrivateProfileString("General", "Setup Title1 Font Shadow", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    sgProduct.bSetupTitle1FontShadow = FALSE;
  else
    sgProduct.bSetupTitle1FontShadow = TRUE;

  GetPrivateProfileString("General", "Setup Title2 Font Shadow", "", szBuf, sizeof(szBuf), szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
    sgProduct.bSetupTitle2FontShadow = FALSE;
  else
    sgProduct.bSetupTitle2FontShadow = TRUE;

  /* Welcome dialog */
  GetPrivateProfileString("Dialog Welcome",             "Show Dialog",     "", szShowDialog,                  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Title",           "", diWelcome.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message0",        "", diWelcome.szMessage0,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message1",        "", diWelcome.szMessage1,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message2",        "", diWelcome.szMessage2,          MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diWelcome.bShowDialog = TRUE;

  /* License dialog */
  GetPrivateProfileString("Dialog License",             "Show Dialog",     "", szShowDialog,                  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Title",           "", diLicense.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "License File",    "", diLicense.szLicenseFilename,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message0",        "", diLicense.szMessage0,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message1",        "", diLicense.szMessage1,          MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diLicense.bShowDialog = TRUE;

  /* Setup Type dialog */
  GetPrivateProfileString("Dialog Setup Type",          "Show Dialog",     "", szShowDialog,                  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Title",           "", diSetupType.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Message0",        "", diSetupType.szMessage0,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme Filename", "", diSetupType.szReadmeFilename,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Readme App",      "", diSetupType.szReadmeApp,       MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSetupType.bShowDialog = TRUE;

  /* Get Setup Types info */
  GetPrivateProfileString("Setup Type0", "Description Short", "", diSetupType.stSetupType0.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type0", "Description Long",  "", diSetupType.stSetupType0.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STGetComponents("Setup Type0", &diSetupType.stSetupType0, szFileIniConfig);

  GetPrivateProfileString("Setup Type1", "Description Short", "", diSetupType.stSetupType1.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type1", "Description Long",  "", diSetupType.stSetupType1.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STGetComponents("Setup Type1", &diSetupType.stSetupType1, szFileIniConfig);

  GetPrivateProfileString("Setup Type2", "Description Short", "", diSetupType.stSetupType2.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type2", "Description Long",  "", diSetupType.stSetupType2.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STGetComponents("Setup Type2", &diSetupType.stSetupType2, szFileIniConfig);

  GetPrivateProfileString("Setup Type3", "Description Short", "", diSetupType.stSetupType3.szDescriptionShort, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Setup Type3", "Description Long",  "", diSetupType.stSetupType3.szDescriptionLong,  MAX_BUF, szFileIniConfig);
  STGetComponents("Setup Type3", &diSetupType.stSetupType3, szFileIniConfig);

  /* remember the radio button that is considered the Custom type (the last radio button) */
  SetCustomType();

  /* Select Components dialog */
  GetPrivateProfileString("Dialog Select Components",   "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Components",   "Title",        "", diSelectComponents.szTitle,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Components",   "Message0",     "", diSelectComponents.szMessage0,   MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSelectComponents.bShowDialog = TRUE;

  /* Select Additional Components dialog */
  GetPrivateProfileString("Dialog Select Additional Components",   "Show Dialog",  "", szShowDialog,                              MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Title",        "", diSelectAdditionalComponents.szTitle,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Select Additional Components",   "Message0",     "", diSelectAdditionalComponents.szMessage0,   MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diSelectAdditionalComponents.bShowDialog = TRUE;

  /* Windows Integration dialog */
  GetPrivateProfileString("Dialog Windows Integration", "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Title",        "", diWindowsIntegration.szTitle,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Message0",     "", diWindowsIntegration.szMessage0, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Windows Integration", "Message1",     "", diWindowsIntegration.szMessage1, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diWindowsIntegration.bShowDialog = TRUE;

  /* Program Folder dialog */
  GetPrivateProfileString("Dialog Program Folder",      "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Program Folder",      "Title",        "", diProgramFolder.szTitle,         MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Program Folder",      "Message0",     "", diProgramFolder.szMessage0,      MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diProgramFolder.bShowDialog = TRUE;

  /* Download Options dialog */
  GetPrivateProfileString("Dialog Download Options",       "Show Dialog",    "", szShowDialog,                     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Title",          "", diDownloadOptions.szTitle,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Message0",       "", diDownloadOptions.szMessage0,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Message1",       "", diDownloadOptions.szMessage1,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Download Options",       "Save Installer", "", szBuf,                            MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diDownloadOptions.bSaveInstaller = TRUE;
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diDownloadOptions.bShowDialog = TRUE;

  /* Advanced Settings dialog */
  GetPrivateProfileString("Dialog Advanced Settings",       "Show Dialog",    "", szShowDialog,                     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Title",          "", diAdvancedSettings.szTitle,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Message0",       "", diAdvancedSettings.szMessage0,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Server",   "", diAdvancedSettings.szProxyServer, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Advanced Settings",       "Proxy Port",     "", diAdvancedSettings.szProxyPort,   MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diAdvancedSettings.bShowDialog = TRUE;

  /* Start Install dialog */
  GetPrivateProfileString("Dialog Start Install",       "Show Dialog",      "", szShowDialog,                     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Title",            "", diStartInstall.szTitle,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Install",  "", diStartInstall.szMessageInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message Download", "", diStartInstall.szMessageDownload, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diStartInstall.bShowDialog = TRUE;

  /* Reboot dialog */
  GetPrivateProfileString("Dialog Reboot", "Show Dialog", "", szShowDialog, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diReboot.dwShowDialog = TRUE;
  else if(lstrcmpi(szShowDialog, "AUTO") == 0)
    diReboot.dwShowDialog = AUTO;

  GetPrivateProfileString("Windows Integration-Item0", "CheckBoxState", "", szBuf,                                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Description",   "", diWindowsIntegration.wiCB0.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Archive",       "", diWindowsIntegration.wiCB0.szArchive,     MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB0.szDescription != '\0')
    diWindowsIntegration.wiCB0.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB0.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item1", "CheckBoxState", "", szBuf,                           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item1", "Description",   "", diWindowsIntegration.wiCB1.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item1", "Archive",       "", diWindowsIntegration.wiCB1.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB1.szDescription != '\0')
    diWindowsIntegration.wiCB1.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB1.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item2", "CheckBoxState", "", szBuf,                           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item2", "Description",   "", diWindowsIntegration.wiCB2.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item2", "Archive",       "", diWindowsIntegration.wiCB2.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB2.szDescription != '\0')
    diWindowsIntegration.wiCB2.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB2.bCheckBoxState = TRUE;

  GetPrivateProfileString("Windows Integration-Item3", "CheckBoxState", "", szBuf,                           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item3", "Description",   "", diWindowsIntegration.wiCB3.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item3", "Archive",       "", diWindowsIntegration.wiCB3.szArchive, MAX_BUF, szFileIniConfig);
  /* Check to see if the checkbox need to be shown at all or not */
  if(*diWindowsIntegration.wiCB3.szDescription != '\0')
    diWindowsIntegration.wiCB3.bEnabled = TRUE;
  /* check to see if the checkbox needs to be checked by default or not */
  if(lstrcmpi(szBuf, "TRUE") == 0)
    diWindowsIntegration.wiCB3.bCheckBoxState = TRUE;

  switch(sgProduct.dwMode)
  {
    case AUTO:
    case SILENT:
      diWelcome.bShowDialog                     = FALSE;
      diLicense.bShowDialog                     = FALSE;
      diSetupType.bShowDialog                   = FALSE;
      diSelectComponents.bShowDialog            = FALSE;
      diSelectAdditionalComponents.bShowDialog  = FALSE;
      diWindowsIntegration.bShowDialog          = FALSE;
      diProgramFolder.bShowDialog               = FALSE;
      diDownloadOptions.bShowDialog             = FALSE;
      diAdvancedSettings.bShowDialog            = FALSE;
      diStartInstall.bShowDialog                = FALSE;
      break;
  }

  InitSiComponents(szFileIniConfig);
  InitSiteSelector(szFileIniConfig);

  /* get Default Setup Type */
  GetPrivateProfileString("General", "Default Setup Type", "", szBuf, MAX_BUF, szFileIniConfig);
  if((lstrcmpi(szBuf, "Setup Type 0") == 0) && diSetupType.stSetupType0.bVisible)
  {
    dwSetupType     = ST_RADIO0;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType0.dwCItems, diSetupType.stSetupType0.dwCItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 1") == 0) && diSetupType.stSetupType1.bVisible)
  {
    dwSetupType     = ST_RADIO1;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType1.dwCItems, diSetupType.stSetupType1.dwCItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 2") == 0) && diSetupType.stSetupType2.bVisible)
  {
    dwSetupType     = ST_RADIO2;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType2.dwCItems, diSetupType.stSetupType2.dwCItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 3") == 0) && diSetupType.stSetupType3.bVisible)
  {
    dwSetupType     = ST_RADIO3;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType3.dwCItems, diSetupType.stSetupType3.dwCItemsSelected);
  }
  else
  {
    if(diSetupType.stSetupType0.bVisible)
    {
      dwSetupType     = ST_RADIO0;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType0.dwCItems, diSetupType.stSetupType0.dwCItemsSelected);
    }
    else if(diSetupType.stSetupType1.bVisible)
    {
      dwSetupType     = ST_RADIO1;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType1.dwCItems, diSetupType.stSetupType1.dwCItemsSelected);
    }
    else if(diSetupType.stSetupType2.bVisible)
    {
      dwSetupType     = ST_RADIO2;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType2.dwCItems, diSetupType.stSetupType2.dwCItemsSelected);
    }
    else if(diSetupType.stSetupType3.bVisible)
    {
      dwSetupType     = ST_RADIO3;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType3.dwCItems, diSetupType.stSetupType3.dwCItemsSelected);
    }
  }

  /* get install size required in temp for component Xpcom.  Sould be in Kilobytes */
  GetPrivateProfileString("Xpcom", "Install Size", "", szBuf, MAX_BUF, szFileIniConfig);
  if(*szBuf != '\0')
    siCFXpcomFile.ullInstallSize = _atoi64(szBuf);
  else
    siCFXpcomFile.ullInstallSize = 0;

  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file",        "", siSDObject.szXpcomFile,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file_path",   "", siSDObject.szXpcomFilePath,   MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "xpcom_dir",        "", siSDObject.szXpcomDir,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "no_ads",           "", siSDObject.szNoAds,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "silent",           "", siSDObject.szSilent,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "execution",        "", siSDObject.szExecution,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "confirm_install",  "", siSDObject.szConfirmInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "extract_msg",      "", siSDObject.szExtractMsg,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe",              "", siSDObject.szExe,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe_param",        "", siSDObject.szExeParam,        MAX_BUF, szFileIniConfig);

  GetPrivateProfileString("Core",                           "Source",           "", szBuf,                        MAX_BUF, szFileIniConfig);
  DecryptString(siCFXpcomFile.szSource, szBuf);
  GetPrivateProfileString("Core",                           "Destination",      "", szBuf,                        MAX_BUF, szFileIniConfig);
  DecryptString(siCFXpcomFile.szDestination, szBuf);
  GetPrivateProfileString("Core",                           "Message",          "", siCFXpcomFile.szMessage,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Core",                           "Cleanup",          "", szBuf,                        MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
  {
    siCFXpcomFile.bCleanup = FALSE;
  }
  else
  {
    siCFXpcomFile.bCleanup = TRUE;
  }

  hdc = GetDC(hWndMain);

  OutputSetupTitle(hdc);
  ReleaseDC(hWndMain, hdc);

  return(0);
}

BOOL LocatePreviousPath(LPSTR szMainSectionName, LPSTR szPath, DWORD dwPathSize)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szValue[MAX_BUF];
  BOOL  bFound;

  bFound  = FALSE;
  dwIndex = -1;
  while(!bFound)
  {
    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szSection, szMainSectionName);
    lstrcat(szSection, szIndex);

    GetPrivateProfileString(szSection, "Key", "", szValue, MAX_BUF, szFileIniConfig);
    if(*szValue != '\0')
      bFound = LocatePathNscpReg(szSection, szPath, dwPathSize);
    else
    {
      GetPrivateProfileString(szSection, "HKey", "", szValue, MAX_BUF, szFileIniConfig);
      if(*szValue != '\0')
        bFound = LocatePathWinReg(szSection, szPath, dwPathSize);
      else
      {
        GetPrivateProfileString(szSection, "Path", "", szValue, MAX_BUF, szFileIniConfig);
        if(*szValue != '\0')
          bFound = LocatePath(szSection, szPath, dwPathSize);
        else
          break;
      }
    }
  }

  return(bFound);
}

BOOL LocatePathNscpReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szKey[MAX_BUF];
  char  szContainsFilename[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bReturn;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "Key", "", szKey, MAX_BUF, szFileIniConfig);
  if(*szKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    VR_GetPath(szKey, MAX_BUF, szBuf);
    if(*szBuf != '\0')
    {
      GetPrivateProfileString(szSection, "Contains Filename", "", szContainsFilename, MAX_BUF, szFileIniConfig);
      if(lstrcmpi(szContainsFilename, "TRUE") == 0)
        ParsePath(szBuf, szPath, dwPathSize, PP_PATH_ONLY);
      else
        lstrcpy(szPath, szBuf);

      bReturn = TRUE;
    }
  }

  return(bReturn);
}

DWORD GetTotalArchivesToDownload()
{
  DWORD     dwIndex0;
  DWORD     dwTotalArchivesToDownload;
  siC       *siCObject = NULL;
  char      szIndex0[MAX_BUF];

  dwTotalArchivesToDownload = 0;
  dwIndex0                  = 0;
  itoa(dwIndex0,  szIndex0,  10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      if(LocateJar(siCObject, NULL, 0, FALSE) == FALSE)
      {
        ++dwTotalArchivesToDownload;
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }

  return(dwTotalArchivesToDownload);
}

BOOL LocatePathWinReg(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szHKey[MAX_BUF];
  char  szHRoot[MAX_BUF];
  char  szName[MAX_BUF];
  char  szVerifyExistance[MAX_BUF];
  char  szBuf[MAX_BUF];
  BOOL  bDecryptKey;
  BOOL  bContainsFilename;
  BOOL  bReturn;
  HKEY  hkeyRoot;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "HKey", "", szHKey, MAX_BUF, szFileIniConfig);
  if(*szHKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    GetPrivateProfileString(szSection, "HRoot",               "", szHRoot,           MAX_BUF, szFileIniConfig);
    GetPrivateProfileString(szSection, "Name",                "", szName,            MAX_BUF, szFileIniConfig);
    GetPrivateProfileString(szSection, "Verify Existance",    "", szVerifyExistance, MAX_BUF, szFileIniConfig);
    GetPrivateProfileString(szSection, "Decrypt HKey",        "", szBuf,             MAX_BUF, szFileIniConfig);
    if(lstrcmpi(szBuf, "FALSE") == 0)
      bDecryptKey = FALSE;
    else
      bDecryptKey = TRUE;

    GetPrivateProfileString(szSection, "Contains Filename",   "", szBuf, MAX_BUF, szFileIniConfig);
    if(lstrcmpi(szBuf, "TRUE") == 0)
      bContainsFilename = TRUE;
    else
      bContainsFilename = FALSE;

    if(lstrcmpi(szHRoot, "HKEY_CLASSES_ROOT") == 0)
      hkeyRoot = HKEY_CLASSES_ROOT;
    else if(lstrcmpi(szHRoot, "HKEY_CURRENT_CONFIG") == 0)
      hkeyRoot = HKEY_CURRENT_CONFIG;
    else if(lstrcmpi(szHRoot, "HKEY_CURRENT_USER") == 0)
      hkeyRoot = HKEY_CURRENT_USER;
    else if(lstrcmpi(szHRoot, "HKEY_LOCAL_MACHINE") == 0)
      hkeyRoot = HKEY_LOCAL_MACHINE;
    else if(lstrcmpi(szHRoot, "HKEY_USERS") == 0)
      hkeyRoot = HKEY_USERS;

    if(bDecryptKey == TRUE)
    {
      DecryptString(szBuf, szHKey);
      lstrcpy(szHKey, szBuf);
    }

    GetWinReg(hkeyRoot, szHKey, szName, szBuf, sizeof(szBuf));
    if(*szBuf != '\0')
    {
      if(lstrcmpi(szVerifyExistance, "FILE") == 0)
      {
        if(FileExists(szBuf))
        {
          if(bContainsFilename == TRUE)
            ParsePath(szBuf, szPath, dwPathSize, PP_PATH_ONLY);
          else
            lstrcpy(szPath, szBuf);

          bReturn = TRUE;
        }
        else
          bReturn = FALSE;
      }
      else if(lstrcmpi(szVerifyExistance, "PATH") == 0)
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, dwPathSize, PP_PATH_ONLY);
        else
          lstrcpy(szPath, szBuf);

        if(FileExists(szPath))
          bReturn = TRUE;
        else
          bReturn = FALSE;
      }
      else
      {
        if(bContainsFilename == TRUE)
          ParsePath(szBuf, szPath, dwPathSize, PP_PATH_ONLY);
        else
          lstrcpy(szPath, szBuf);

        bReturn = TRUE;
      }
    }
  }

  return(bReturn);
}

BOOL LocatePath(LPSTR szSection, LPSTR szPath, DWORD dwPathSize)
{
  char  szPathKey[MAX_BUF];
  BOOL  bReturn;

  bReturn = FALSE;
  GetPrivateProfileString(szSection, "Path", "", szPathKey, MAX_BUF, szFileIniConfig);
  if(*szPathKey != '\0')
  {
    bReturn = FALSE;
    ZeroMemory(szPath, dwPathSize);

    DecryptString(szPath, szPathKey);
    bReturn = TRUE;
  }

  return(bReturn);
}

void SetCustomType()
{
  if(diSetupType.stSetupType3.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO3;
  else if(diSetupType.stSetupType2.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO2;
  else if(diSetupType.stSetupType1.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO1;
  else if(diSetupType.stSetupType0.bVisible == TRUE)
    sgProduct.dwCustomType = ST_RADIO0;
}

void STGetComponents(LPSTR szSection, st *stSetupType, LPSTR szFileIniConfig)
{
  DWORD dwIndex;
  char  szIndex[MAX_BUF];
  char  szKey[MAX_BUF];
  char  szBuf[MAX_BUF];

  if(*(stSetupType->szDescriptionShort) == '\0')
    stSetupType->bVisible = FALSE;
  else
    stSetupType->bVisible = TRUE;

  dwIndex = 0;
  stSetupType->dwCItems = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKey, "C");
  lstrcat(szKey, szIndex);
  GetPrivateProfileString(szSection, szKey, "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    /* hack used to determine the numerical value of the component */
    if(lstrlen(szBuf) > 8)
    {
      ++stSetupType->dwCItems;
      stSetupType->dwCItemsSelected[dwIndex] = atoi(&szBuf[9]);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKey, "C");
    lstrcat(szKey, szIndex);
    GetPrivateProfileString(szSection, szKey, "", szBuf, MAX_BUF, szFileIniConfig);
  }
}

HRESULT DecryptVariable(LPSTR szVariable, DWORD dwVariableSize)
{
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];

  /* zero out the memory allocations */
  ZeroMemory(szBuf,       sizeof(szBuf));
  ZeroMemory(szKey,       sizeof(szKey));
  ZeroMemory(szName,      sizeof(szName));
  ZeroMemory(szValue,     sizeof(szValue));

  if(lstrcmpi(szVariable, "PROGRAMFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMONFILESDIR") == 0)
  {
    /* parse for the "c:\Program Files\Common Files" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "CommonFilesDir", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "MEDIAPATH") == 0)
  {
    /* parse for the "c:\Winnt40\Media" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "MediaPath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "CONFIGPATH") == 0)
  {
    /* parse for the "c:\Windows\Config" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "ConfigPath", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "DEVICEPATH") == 0)
  {
    /* parse for the "c:\Winnt40\INF" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "DevicePath", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs\\Startup" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Startup", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Programs", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Start Menu", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "COMMON_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Desktop" directory */
    if(ulOSType & OS_WIN9x)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
    }
    else
    {
      GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Desktop", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTUP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs\Startup" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu\Programs" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Programs", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_STARTMENU") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Start Menu" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Start Menu", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_DESKTOP") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Desktop" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Desktop", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_APPDATA") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Application Data" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "AppData", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_CACHE") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Temporary Internet Files" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cache", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_COOKIES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Cookies" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Cookies", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FAVORITES") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Favorites" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Favorites", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_FONTS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Fonts" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Fonts", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_HISTORY") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\History" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "History", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_NETHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\NetHood" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "NetHood", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PERSONAL") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Personal" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Personal", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_PRINTHOOD") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\PrintHood" directory */
    if(ulOSType & OS_NT)
    {
      GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "PrintHood", szVariable, dwVariableSize);
    }
  }
  else if(lstrcmpi(szVariable, "PERSONAL_RECENT") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\Recent" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Recent", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_SENDTO") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\%USERNAME%\SendTo" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "SendTo", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PERSONAL_TEMPLATES") == 0)
  {
    /* parse for the "C:\WINNT40\ShellNew" directory */
    GetWinReg(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Templates", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERNAME") == 0)
  {
    /* parse for the "Netscape Communicator" folder name */
    lstrcpy(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATHNAME") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs\Netscape Communicator" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
    lstrcat(szVariable, "\\");
    lstrcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "PROGRAMFOLDERPATH") == 0)
  {
    /* parse for the "c:\Windows\profiles\All Users\Start Menu\Programs" path */
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
  }
  else if(lstrcmpi(szVariable, "WIZTEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    lstrcpy(szVariable, szTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(lstrcmpi(szVariable, "TEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    lstrcpy(szVariable, szOSTempDir);
    if(szVariable[strlen(szVariable) - 1] == '\\')
      szVariable[strlen(szVariable) - 1] = '\0';
  }
  else if(lstrcmpi(szVariable, "WINDISK") == 0)
  {
    /* Locate the drive that Windows is installed on, and only use the drive letter and the ':' character (C:). */
    if(GetWindowsDirectory(szBuf, MAX_BUF) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_GET_WINDOWS_DIRECTORY_FAILED, szEGetWinDirFailed, MAX_BUF) == WIZ_OK)
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);

      exit(1);
    }
    else
    {
      /* Copy the first 2 characters from the path..        */
      /* This is the drive letter and the ':' character for */
      /* where Windows is installed at.                     */
      memset(szVariable, '\0', MAX_BUF);
      szVariable[0] = szBuf[0];
      szVariable[1] = szBuf[1];
    }
  }
  else if(lstrcmpi(szVariable, "WINDIR") == 0)
  {
    /* Locate the "c:\Windows" directory */
    if(GetWindowsDirectory(szVariable, dwVariableSize) == 0)
    {
      char szEGetWinDirFailed[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_GET_WINDOWS_DIRECTORY_FAILED, szEGetWinDirFailed, MAX_BUF) == WIZ_OK)
        PrintError(szEGetWinDirFailed, ERROR_CODE_SHOW);
      exit(1);
    }
  }
  else if(lstrcmpi(szVariable, "WINSYSDIR") == 0)
  {
    /* Locate the "c:\Windows\System" (for Win95/Win98) or "c:\Windows\System32" (for NT) directory */
    if(GetSystemDirectory(szVariable, dwVariableSize) == 0)
    {
      char szEGetSysDirFailed[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_GET_SYSTEM_DIRECTORY_FAILED, szEGetSysDirFailed, MAX_BUF) == WIZ_OK)
        PrintError(szEGetSysDirFailed, ERROR_CODE_SHOW);

      exit(1);
    }
  }
  else if(lstrcmpi(szVariable, "JRE BIN PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3\Bin" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\javaw.Exe", NULL, szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
    else
    {
      ParsePath(szVariable, szBuf, MAX_BUF, PP_PATH_ONLY);
      lstrcpy(szVariable, szBuf);
    }
  }
  else if(lstrcmpi(szVariable, "JRE PATH") == 0)
  {
    /* Locate the "c:\Program Files\JavaSoft\JRE\1.3" directory */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\JavaSoft\\Java Plug-in\\1.3", "JavaHome", szVariable, dwVariableSize);
    if(*szVariable == '\0')
      return(FALSE);
  }
  else if(lstrcmpi(szVariable, "SETUP PATH") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(lstrcmpi(szVariable, "Default Path") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szVariable, dwVariableSize);
      lstrcat(szVariable, sgProduct.szSubPath);
    }
  }
  else if(lstrcmpi(szVariable, "SETUP STARTUP PATH") == 0)
  {
    lstrcpy(szVariable, szSetupDir);
  }
  else if(lstrcmpi(szVariable, "Default Folder") == 0)
  {
    lstrcpy(szVariable, sgProduct.szProgramFolderPath);
    AppendBackSlash(szVariable, dwVariableSize);
    lstrcat(szVariable, sgProduct.szProgramFolderName);
  }
  else if(lstrcmpi(szVariable, "Netscape Seamonkey CurrentVersion") == 0)
  {
    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Netscape\\Netscape Seamonkey", "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\Netscape\\Netscape Seamonkey\\%s", szBuf);
  }
  else if(lstrcmpi(szVariable, "Netscape 6 CurrentVersion") == 0)
  {
    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Netscape\\Netscape 6", "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\Netscape\\Netscape 6\\%s", szBuf);
  }
  else if(lstrcmpi(szVariable, "Netscape Netbusiness Messenger CurrentVersion") == 0)
  {
    /* parse for the current Netscape WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Netscape\\Netscape Netbusiness Messenger", "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\Netscape\\Netscape Netbusiness Messenger\\%s", szBuf);
  }
  else if(lstrcmpi(szVariable, "Mozilla Seamonkey CurrentVersion") == 0)
  {
    /* parse for the current Mozilla WinReg key */
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Mozilla Seamonkey", "CurrentVersion", szBuf, sizeof(szBuf));

    if(*szBuf == '\0')
      return(FALSE);

    wsprintf(szVariable, "Software\\Mozilla\\Mozilla Seamonkey\\%s", szBuf);
  }
  else
    return(FALSE);

  return(TRUE);
}

HRESULT DecryptString(LPSTR szOutputStr, LPSTR szInputStr)
{
  DWORD dwLenInputStr;
  DWORD dwCounter;
  DWORD dwVar;
  DWORD dwPrepend;
  char  szBuf[MAX_BUF];
  char  szOutuptStrTemp[MAX_BUF];
  char  szVariable[MAX_BUF];
  char  szPrepend[MAX_BUF];
  char  szAppend[MAX_BUF];
  char  szResultStr[MAX_BUF];
  BOOL  bFoundVar;
  BOOL  bBeginParse;
  BOOL  bDecrypted;

  /* zero out the memory addresses */
  memset(szBuf,       '\0', MAX_BUF);
  memset(szVariable,  '\0', MAX_BUF);
  memset(szPrepend,   '\0', MAX_BUF);
  memset(szAppend,    '\0', MAX_BUF);
  memset(szResultStr, '\0', MAX_BUF);

  lstrcpy(szPrepend, szInputStr);
  dwLenInputStr = lstrlen(szInputStr);
  bBeginParse   = FALSE;
  bFoundVar     = FALSE;

  for(dwCounter = 0; dwCounter < dwLenInputStr; dwCounter++)
  {
    if((szInputStr[dwCounter] == ']') && bBeginParse)
      break;

    if(bBeginParse)
      szVariable[dwVar++] = szInputStr[dwCounter];

    if((szInputStr[dwCounter] == '[') && !bBeginParse)
    {
      dwVar        = 0;
      dwPrepend    = dwCounter;
      bBeginParse  = TRUE;
    }
  }

  if(dwCounter == dwLenInputStr)
    /* did not find anything to expand. */
    dwCounter = 0;
  else
  {
    bFoundVar = TRUE;
    ++dwCounter;
  }

  if(bFoundVar)
  {
    lstrcpy(szAppend, &szInputStr[dwCounter]);

    szPrepend[dwPrepend] = '\0';

    /* if Variable is "XPI PATH", do special processing */
    if(lstrcmpi(szVariable, "XPI PATH") == 0)
    {
      lstrcpy(szBuf, sgProduct.szAlternateArchiveSearchPath);
      RemoveBackSlash(szBuf);
      lstrcpy(szOutuptStrTemp, szPrepend);
      lstrcat(szOutuptStrTemp, szBuf);
      lstrcat(szOutuptStrTemp, szAppend);

      if(FileExists(szOutuptStrTemp))
      {
        lstrcpy(szVariable, sgProduct.szAlternateArchiveSearchPath);
      }
      else
      {
        lstrcpy(szBuf, szSetupDir);
        RemoveBackSlash(szBuf);
        lstrcpy(szOutuptStrTemp, szPrepend);
        lstrcat(szOutuptStrTemp, szBuf);
        lstrcat(szOutuptStrTemp, szAppend);

        if(!FileExists(szOutuptStrTemp))
          lstrcpy(szVariable, szTempDir);
        else
          lstrcpy(szVariable, szSetupDir);
      }

      RemoveBackSlash(szVariable);
      bDecrypted = TRUE;
    }
    else
    {
      bDecrypted = DecryptVariable(szVariable, sizeof(szVariable));
    }

    if(!bDecrypted)
    {
      /* Variable was not able to be dcripted. */
      /* Leave the variable as it was read in by adding the '[' and ']' */
      /* characters back to the variable. */
      lstrcpy(szBuf, "[");
      lstrcat(szBuf, szVariable);
      lstrcat(szBuf, "]");
      lstrcpy(szVariable, szBuf);
    }

    lstrcpy(szOutputStr, szPrepend);
    lstrcat(szOutputStr, szVariable);
    lstrcat(szOutputStr, szAppend);

    if(bDecrypted)
    {
      DecryptString(szResultStr, szOutputStr);
      lstrcpy(szOutputStr, szResultStr);
    }
  }
  else
    lstrcpy(szOutputStr, szInputStr);

  return(TRUE);
}

int ExtractDirEntries(char* directory, void* vZip)
{
  int   err;
  int   result;
  char  buf[512];  // XXX: need an XP "max path"

  int paths = 1;
  if(paths)
  {
    void* find = ZIP_FindInit(vZip, directory);

    if(find)
    {
      int prefix_length = 0;
      
      if(directory)
        prefix_length = strlen(directory) - 1;

      if(prefix_length >= sizeof(buf)-1)
        return ZIP_ERR_GENERAL;

      err = ZIP_FindNext( find, buf, sizeof(buf) );
      while ( err == ZIP_OK ) 
      {
        CreateDirectoriesAll(buf, FALSE);
        if(buf[lstrlen(buf) - 1] != '/')
          // only extract if it's a file
          result = ZIP_ExtractFile(vZip, buf, buf);
        err = ZIP_FindNext( find, buf, sizeof(buf) );
      }
      ZIP_FindFree( find );
    }
    else
      err = ZIP_ERR_GENERAL;

    if ( err == ZIP_ERR_FNF )
      return 1;   // found them all
  }

  return ZIP_ERR_GENERAL;
}

HRESULT FileExists(LPSTR szFile)
{
  DWORD rv;

  if((rv = GetFileAttributes(szFile)) == -1)
  {
    return(FALSE);
  }
  else
  {
    return(rv);
  }
}

BOOL NeedReboot()
{
   if(diReboot.dwShowDialog == AUTO)
     return(bReboot);
   else
     return(diReboot.dwShowDialog);
}

HRESULT InitializeSmartDownload()
{
  char szBuf[MAX_BUF];
  char szSdInstFile[MAX_BUF];

  if(bSDInit)
    return(0);

  bSDInit       = TRUE;
  pfnNetInstall = NULL;
  lstrcpy(szSdInstFile, "sdinst.dll");
  if(FileExists(szSdInstFile) == FALSE)
  {
    return(2);
  }
  else if((hSDInst = LoadLibraryEx(szSdInstFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) == NULL)
  {
    wsprintf(szBuf, szEDllLoad, szSdInstFile);
    PrintError(szBuf, ERROR_CODE_SHOW);
    return(1);
  }

  if((pfnNetInstall = (SDI_NETINSTALL)GetProcAddress(hSDInst, "SDI_NetInstall")) == NULL)
  {
    char szEGetProcAddress[MAX_BUF];

    if(NS_LoadString(hSetupRscInst, IDS_ERROR_GETPROCADDRESS, szEGetProcAddress, MAX_BUF) == WIZ_OK)
    {
      wsprintf(szBuf, szEGetProcAddress, "SDI_NetInstall");
      PrintError(szBuf, ERROR_CODE_SHOW);
    }
    return(1);
  }

  return(0);
}

HRESULT DeInitializeSmartDownload()
{
  if(!bSDInit)
    return(0);

  bSDInit       = FALSE;
  pfnNetInstall = NULL;

  FreeLibrary(hSDInst);

  return(0);
}

BOOL DeleteIdiGetConfigIni()
{
  char  szFileIdiGetConfigIni[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));

  lstrcpy(szFileIdiGetConfigIni, szTempDir);
  AppendBackSlash(szFileIdiGetConfigIni, sizeof(szFileIdiGetConfigIni));
  lstrcat(szFileIdiGetConfigIni, FILE_IDI_GETCONFIGINI);
  if(FileExists(szFileIdiGetConfigIni))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetConfigIni);
  return(bFileExists);
}

BOOL DeleteIniRedirect()
{
  char  szFileIniRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIniRedirect, sizeof(szFileIniRedirect));

  lstrcpy(szFileIniRedirect, szTempDir);
  AppendBackSlash(szFileIniRedirect, sizeof(szFileIniRedirect));
  lstrcat(szFileIniRedirect, FILE_INI_REDIRECT);
  if(FileExists(szFileIniRedirect))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIniRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetRedirect()
{
  char  szFileIdiGetRedirect[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));

  lstrcpy(szFileIdiGetRedirect, szTempDir);
  AppendBackSlash(szFileIdiGetRedirect, sizeof(szFileIdiGetRedirect));
  lstrcat(szFileIdiGetRedirect, FILE_IDI_GETREDIRECT);
  if(FileExists(szFileIdiGetRedirect))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetRedirect);
  return(bFileExists);
}

BOOL DeleteIdiGetArchives()
{
  char  szFileIdiGetArchives[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));

  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);
  if(FileExists(szFileIdiGetArchives))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIdiGetArchives);
  return(bFileExists);
}

BOOL DeleteIdiFileIniConfig()
{
  char  szFileIniConfig[MAX_BUF];
  BOOL  bFileExists = FALSE;

  ZeroMemory(szFileIniConfig, sizeof(szFileIniConfig));

  lstrcpy(szFileIniConfig, szTempDir);
  AppendBackSlash(szFileIniConfig, sizeof(szFileIniConfig));
  lstrcat(szFileIniConfig, FILE_INI_CONFIG);
  if(FileExists(szFileIniConfig))
  {
    bFileExists = TRUE;
  }
  DeleteFile(szFileIniConfig);
  return(bFileExists);
}

void DeleteArchives()
{
  DWORD dwIndex0;
  char  szArchiveName[MAX_BUF];
  siC   *siCObject = NULL;

  ZeroMemory(szArchiveName, sizeof(szArchiveName));

  if(!bSDUserCanceled)
  {
    dwIndex0 = 0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    while(siCObject)
    {
      lstrcpy(szArchiveName, szTempDir);
	    AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      lstrcat(szArchiveName, siCObject->szArchiveName);

      DeleteFile(szArchiveName);

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
    }

    DeleteIniRedirect();
  }
}

void CleanTempFiles()
{
  DeleteIdiGetConfigIni();
  DeleteIdiGetArchives();
  DeleteIdiGetRedirect();

  /* do not delete config.ini file.
     if it was uncompressed from the self-extracting .exe file,
     then it will be deleted automatically.
     Right now the config.ini does not get downloaded, so we
     don't have to worry about that case
  */
  DeleteIdiFileIniConfig();
  DeleteArchives();
}

void DeInitialize()
{
  char szBuf[MAX_BUF];

  if(bCreateDestinationDir)
  {
    lstrcpy(szBuf, sgProduct.szPath);
    AppendBackSlash(szBuf, sizeof(szBuf));
    DirectoryRemove(szBuf, FALSE);
  }

  if(hbmpBoxChecked)
    DeleteObject(hbmpBoxChecked);
  if(hbmpBoxCheckedDisabled)
    DeleteObject(hbmpBoxCheckedDisabled);
  if(hbmpBoxUnChecked)
    DeleteObject(hbmpBoxUnChecked);

  CleanTempFiles();

  DeInitSiComponents();
  DeInitSXpcomFile();
  DeInitSDObject();
  DeInitDlgReboot(&diReboot);
  DeInitDlgStartInstall(&diStartInstall);
  DeInitDlgDownloadOptions(&diDownloadOptions);
  DeInitDlgAdvancedSettings(&diAdvancedSettings);
  DeInitDlgProgramFolder(&diProgramFolder);
  DeInitDlgWindowsIntegration(&diWindowsIntegration);
  DeInitDlgSelectComponents(&diSelectAdditionalComponents);
  DeInitDlgSelectComponents(&diSelectComponents);
  DeInitDlgSetupType(&diSetupType);
  DeInitDlgLicense(&diLicense);
  DeInitDlgWelcome(&diWelcome);
  DeInitSetupGeneral();

  FreeMemory(&szTempDir);
  FreeMemory(&szOSTempDir);
  FreeMemory(&szSetupDir);
  FreeMemory(&szFileIniConfig);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);

  FreeLibrary(hSetupRscInst);
  DeInitializeSmartDownload();
}

char *GetSaveInstallerPath(char *szBuf, DWORD dwBufSize)
{
  char szBuf2[MAX_BUF];

  /* determine the path to where the setup and downloaded files will be saved to */
  lstrcpy(szBuf, sgProduct.szPath);
  AppendBackSlash(szBuf, dwBufSize);
  if(*sgProduct.szSubPath != '\0')
  {
    lstrcat(szBuf, sgProduct.szSubPath);
    lstrcat(szBuf, " ");
  }

  if(NS_LoadString(hSetupRscInst, IDS_STR_SETUP, szBuf2, MAX_BUF) == WIZ_OK)
    lstrcat(szBuf, szBuf2);
  else
    lstrcat(szBuf, "Setup");

  return(szBuf);
}

void SaveInstallerFiles()
{
  int       i;
  char      szBuf[MAX_BUF];
  char      szSource[MAX_BUF];
  char      szDestination[MAX_BUF];
  char      szMFN[MAX_BUF];
  char      szArchivePath[MAX_BUF];
  DWORD     dwIndex0;
  siC       *siCObject = NULL;

  GetSaveInstallerPath(szDestination, sizeof(szDestination));
  AppendBackSlash(szDestination, sizeof(szDestination));

  /* copy all files from the ns_temp dir to the install dir */
  CreateDirectoriesAll(szDestination, TRUE);

  /* copy the self extracting file that spawned setup.exe, if one exists */
  if((*sgProduct.szAlternateArchiveSearchPath != '\0') && (*sgProduct.szParentProcessFilename != '\0'))
  {
    lstrcpy(szSource, szSetupDir);
    AppendBackSlash(szSource, sizeof(szSource));
    lstrcat(szSource, "*.*");

    lstrcpy(szSource, sgProduct.szAlternateArchiveSearchPath);
    AppendBackSlash(szSource, sizeof(szSource));
    lstrcat(szSource, sgProduct.szParentProcessFilename);
    FileCopy(szSource, szDestination, FALSE);
  }
  else
  {
    /* Else if self extracting file does not exist, copy the setup files */
    /* First get the current process' filename (in case it's not really named setup.exe */
    /* Then copy it to the install folder */
    GetModuleFileName(NULL, szBuf, sizeof(szBuf));
    ParsePath(szBuf, szMFN, sizeof(szMFN), PP_FILENAME_ONLY);

    lstrcpy(szBuf, szSetupDir);
    AppendBackSlash(szBuf, sizeof(szBuf));
    lstrcat(szBuf, szMFN);
    FileCopy(szBuf, szDestination, FALSE);

    /* now copy the rest of the setup files */
    i = 0;
    while(TRUE)
    {
      if(*SetupFileList[i] == '\0')
        break;

      lstrcpy(szBuf, szSetupDir);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, SetupFileList[i]);
      FileCopy(szBuf, szDestination, FALSE);

      ++i;
    }
  }

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    LocateJar(siCObject, szArchivePath, sizeof(szArchivePath), FALSE);
    if(*szArchivePath != '\0')
    {
      lstrcpy(szBuf, szArchivePath);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);
      FileCopy(szBuf, szDestination, FALSE);
    }

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  }
}





//
// The functions below were copied from MSDN 6.0:
//
//   \samples\vc98\sdk\sdktools\winnt\pviewer
//
// They were listed in the redist.txt file from the cd.
// Only CheckProcessNT4() was modified to accomodate the setup needs.
//
/******************************************************************************\
 * *       This is a part of the Microsoft Source Code Samples.
 * *       Copyright (C) 1993-1997 Microsoft Corporation.
 * *       All rights reserved.
\******************************************************************************/
BOOL CheckProcessNT4(LPSTR szProcessName, DWORD dwProcessNameSize)
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

  PX_PROCESS = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_PROCESS);
  PX_THREAD  = GetTitleIdx (NULL, szTitleSz, dwTitleLastIdx, PN_THREAD);
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

      if(sizeof(szProcessNamePruned) < (lstrlen(szProcessName) + 1))
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
      lstrcpy(szProcessNamePruned, szProcessName);
      iLen = lstrlen(szProcessNamePruned);
      szPtrStr = &szProcessNamePruned[iLen - 4];
      if((lstrcmpi(szPtrStr, ".exe") == 0) || (lstrcmpi(szPtrStr, ".dll") == 0))
        *szPtrStr = '\0';

      pInst = FirstInstance(pProcessObject);
      for(i = 0; i < (DWORD)(pProcessObject->NumInstances); i++)
      {
        ZeroMemory(szNewProcessName, MAX_BUF);
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

  MessageBox(hWnd, Name, TEXT("Pviewer cannot find index"), MB_OK);
  return 0;
}

