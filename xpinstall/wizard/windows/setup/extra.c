/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "ifuncns.h"

ULONG (PASCAL *NS_GetDiskFreeSpace)(LPCTSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD);
ULONG (PASCAL *NS_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);

BOOL InitApplication(HINSTANCE hInstance, HINSTANCE hSetupRscInst)
{
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
  wc.lpszClassName = szClassName;

  return(RegisterClass(&wc));
}

BOOL InitInstance(HINSTANCE hInstance, DWORD dwCmdShow)
{
  HWND  hWnd;

  dwScreenX = GetSystemMetrics(SM_CXSCREEN);
  dwScreenY = GetSystemMetrics(SM_CYSCREEN);

  hInst = hInstance;
  hWnd = CreateWindow(szClassName,
                      szClassName,
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

  ShowWindow(hWnd, dwCmdShow);
  UpdateWindow(hWnd);

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

  MessageBox(NULL, szErrorString, NULL, MB_ICONEXCLAMATION);
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

HRESULT Initialize(HINSTANCE hInstance)
{
  char szBuf[MAX_BUF];

  bSDInit         = FALSE;
  bSDUserCanceled = FALSE;
  hDlgMessage     = NULL;
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

  ZeroMemory(szBuf, sizeof(MAX_BUF));
  if((szClassName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  lstrcpy(szClassName, CLASS_NAME);
  hAccelTable = LoadAccelerators(hInstance, szClassName);

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

  if((szSetupDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  getcwd(szSetupDir, MAX_BUF);

  if((szTempDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
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

  if(!FileExists(szTempDir))
  {
    CreateDirectory(szTempDir, NULL);
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
  }

  hbmpBoxChecked   = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_CHECKED));
  hbmpBoxUnChecked = LoadBitmap(hSetupRscInst, MAKEINTRESOURCE(IDB_BOX_UNCHECKED));

  DeleteIdiGetConfigIni();
  bIdiArchivesExists = DeleteIdiGetArchives();
  return(0);
}

void OutputTitle(HDC hDC, LPSTR szString)
{
  COLORREF  crTitle;
  HFONT     hfontTmp;
  HFONT     hfontOld;
  int       nHeight;

  SetBkMode(hDC, TRANSPARENT);
  nHeight = -MulDiv(36, GetDeviceCaps(hDC, LOGPIXELSY), 72);

  hfontTmp = CreateFont(nHeight,
                        0,
                        0,
                        0,
                        FW_BOLD,
                        0,
                        0,
                        0,
                        BALTIC_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        0,
                        PROOF_QUALITY,
                        VARIABLE_PITCH | FF_DECORATIVE,
                        "");
  if(hfontTmp)
  {
      hfontOld = SelectObject(hDC, hfontTmp);
  }

  crTitle = GetTextColor(hDC);

  /* Set shadow color to black and draw shadow */
  if(SetTextColor(hDC, 0) == CLR_INVALID)
    PrintError("Invalid Color", ERROR_CODE_SHOW);

  TextOut(hDC, 10+5, 10+5, szString, lstrlen(szString));

  /* Set font color and draw; color format is 0x00bbggrr - where b is blue, g is green, and r is red */
  /* 0x00088808 - green */
  if(SetTextColor(hDC, 0x00EEEEEE) == CLR_INVALID)
    PrintError("Invalid Color", ERROR_CODE_SHOW);

  TextOut(hDC, 10, 10, szString, lstrlen(szString));

  SelectObject(hDC, hfontOld);
  DeleteObject(hfontTmp);
}

HRESULT SdArchives(LPSTR szFileIdi, LPSTR szDownloadDir)
{
  SDISTRUCT sdistruct;
  HRESULT   hResult;

  if((hResult = InitializeSmartDownload()) == 0)
  {
    ZeroMemory(&sdistruct, sizeof(SDISTRUCT));

    sdistruct.dwStructSize  = sizeof(SDISTRUCT);
    sdistruct.lpFileName    = szFileIdi;
    sdistruct.lpDownloadDir = szDownloadDir;
    sdistruct.hwndOwner     = hWndMain;

    hResult = pfnNetInstall(&sdistruct);
    DeInitializeSmartDownload();
  }

  return(hResult);
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
  lstrcat(szFileIniSetup, "\\");
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

BOOL LocateJar(siC *siCObject)
{
  char szBuf[MAX_BUF * 2];
  char szSEADirTemp[MAX_BUF];
  char szSetupDirTemp[MAX_BUF];
  char szTempDirTemp[MAX_BUF];
  char szArchiveLstFile[MAX_BUF];
  char *szBufPtr;
  int  iLen;
  BOOL bRet;

  /* initialize default behavior */
  bRet = FALSE;
  siCObject->dwAttributes |= SIC_DOWNLOAD_REQUIRED;

  lstrcpy(szSEADirTemp, sgProduct.szAlternateArchiveSearchPath);
  AppendBackSlash(szSEADirTemp, sizeof(szSEADirTemp));
  lstrcat(szSEADirTemp, siCObject->szArchiveName);
  if(FileExists(szSEADirTemp))
  {
    /* jar file found.  Unset attribute to download from the net */
    siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
    bRet = TRUE;
  }
  else
  {
    lstrcpy(szSetupDirTemp, szSetupDir);
    lstrcpy(szTempDirTemp,  szTempDir);
    AppendBackSlash(szSetupDirTemp, sizeof(szSetupDirTemp));
    AppendBackSlash(szTempDirTemp, sizeof(szTempDirTemp));
    if(lstrcmpi(szTempDirTemp, szSetupDirTemp) == 0)
    {
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
            bRet = TRUE;

            /* found what we're looking for.  No need to continue */
            break;
          }

          iLen = lstrlen(szBufPtr);
          szBufPtr += iLen +1;
        }
      }
    }
    else
    {
      lstrcpy(szBuf, szSetupDirTemp);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, siCObject->szArchiveName);

      if(FileExists(szBuf))
      {
        /* jar file found.  Unset attribute to download from the net */
        siCObject->dwAttributes &= ~SIC_DOWNLOAD_REQUIRED;
        bRet = TRUE;
      }
    }
  }
  return(bRet);
}

HRESULT AddArchiveToIdiFile(siC *siCObject, char *szSComponent, char *szSFile, char *szFileIdiGetArchives)
{
  DWORD     dwIndex1;
  char      szIndex1[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szBufUrl[MAX_BUF];
  char      szBufTemp[MAX_BUF];
  char      szKUrl[MAX_BUF];
  char      szArchiveName[MAX_BUF];

  lstrcpy(szArchiveName, szTempDir);
  AppendBackSlash(szArchiveName, sizeof(szArchiveName));
  lstrcat(szArchiveName, siCObject->szArchiveName);

  WritePrivateProfileString(szSFile, "desc", siCObject->szDescriptionShort, szFileIdiGetArchives);

  dwIndex1 = 0;
  itoa(dwIndex1, szIndex1, 10);
  lstrcpy(szKUrl, "url");
  lstrcat(szKUrl, szIndex1);
  GetPrivateProfileString(szSComponent, szKUrl, "", szBufUrl, MAX_BUF, szFileIniConfig);
  while(*szBufUrl != '\0')
  {
    if(szBufUrl[strlen(szBufUrl) - 1] != '/')
      lstrcat(szBufUrl, "/");

    lstrcat(szBufUrl, siCObject->szArchiveName);

    if(WritePrivateProfileString(szSFile, szIndex1, szBufUrl, szFileIdiGetArchives) == 0)
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
    itoa(dwIndex1, szIndex1, 10);
    lstrcpy(szKUrl, "url");
    lstrcat(szKUrl, szIndex1);
    GetPrivateProfileString(szSComponent, szKUrl, "", szBufUrl, MAX_BUF, szFileIniConfig);
  }
  return(0);
}

HRESULT RetrieveArchives()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  HRESULT   hResult;
  char      szBuf[MAX_BUF];
  char      szIndex0[MAX_BUF];
  char      szFileIdiGetArchives[MAX_BUF];
  char      szSComponent[MAX_BUF];
  char      szSFile[MAX_BUF];

  lstrcpy(szFileIdiGetArchives, szTempDir);
  AppendBackSlash(szFileIdiGetArchives, sizeof(szFileIdiGetArchives));
  lstrcat(szFileIdiGetArchives, FILE_IDI_GETARCHIVES);

  dwIndex0 = 0;
  itoa(dwIndex0, szIndex0, 10);
  siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
    {
      /* only download jars if not already in the local machine */
      if(LocateJar(siCObject) == FALSE)
      {
        lstrcpy(szSComponent, "Component");
        lstrcat(szSComponent, szIndex0);

        lstrcpy(szSFile, "File");
        lstrcat(szSFile, szIndex0);

        if((hResult = AddArchiveToIdiFile(siCObject, szSComponent, szSFile, szFileIdiGetArchives)) != 0)
          return(hResult);
      }
    }

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  }

  /* the existance of the getarchives.idi file determines if there are
     any jar files needed to be downloaded */
  if(FileExists(szFileIdiGetArchives))
  {
    DecriptString(szBuf, siSDObject.szCoreDir);
    lstrcpy(siSDObject.szCoreDir, szBuf);

    WritePrivateProfileString("Netscape Install", "core_file",        siSDObject.szCoreFile,        szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "core_dir",         siSDObject.szCoreDir,         szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "no_ads",           siSDObject.szNoAds,           szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "silent",           siSDObject.szSilent,          szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "execution",        siSDObject.szExecution,       szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "confirm_install",  siSDObject.szConfirmInstall,  szFileIdiGetArchives);
    WritePrivateProfileString("Netscape Install", "extract_msg",      siSDObject.szExtractMsg,      szFileIdiGetArchives);
    WritePrivateProfileString("Execution",        "exe",              siSDObject.szExe,             szFileIdiGetArchives);
    WritePrivateProfileString("Execution",        "exe_param",        siSDObject.szExeParam,        szFileIdiGetArchives);

    if((hResult = SdArchives(szFileIdiGetArchives, szTempDir)) == 1)
      return(hResult);
  }

  return(0);
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
    if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;

  if((szInput != NULL) && (szOutput != NULL))
  {
    dwInputLen = lstrlen(szInput);
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
              break;
            }
          }
          lstrcpy(szOutput, szInput);
          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              break;
            }
          }
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

/***
HRESULT SmartUpdateJars()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  char      szWorkingDir[MAX_BUF];
  char      szArchiveName[MAX_BUF];
  char      szBuf[MAX_BUF];

  ZeroMemory(szWorkingDir, MAX_BUF);
  ZeroMemory(szArchiveName, MAX_BUF);
  ZeroMemory(szBuf, MAX_BUF);
  ParsePath(siSDObject.szCoreFilePath, szWorkingDir, MAX_BUF, PP_PATH_ONLY);

  SmartUpdate();

  DecriptString(szBuf, siSDObject.szCoreFilePath);
  lstrcpy(siSDObject.szCoreFilePath, szBuf);
  
  if(!FileExists(siSDObject.szCoreFilePath))
  {
    return(0);
  }

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  while(siCObject)
  {
    if((siCObject->dwAttributes & SIC_SELECTED) && !(siCObject->dwAttributes & SIC_LAUNCHAPP))
    {
      lstrcpy(szArchiveName, szTempDir);
      AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      lstrcat(szArchiveName, siCObject->szArchiveName);

      if(FileExists(szArchiveName))
      {
        WinSpawn(siSDObject.szCoreFilePath, szArchiveName, szWorkingDir, SW_SHOWNORMAL, TRUE);
      }
    }
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  }
  return(0);
}
***/

HRESULT LaunchApps()
{
  DWORD     dwIndex0;
  siC       *siCObject = NULL;
  char      szArchiveName[MAX_BUF];

  dwIndex0 = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  while(siCObject)
  {
    /* launch smartupdate engine for earch jar to be installed */
    if((siCObject->dwAttributes & SIC_SELECTED) && (siCObject->dwAttributes & SIC_LAUNCHAPP))
    {
      lstrcpy(szArchiveName, szTempDir);
      AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      lstrcat(szArchiveName, siCObject->szArchiveName);

      if(FileExists(szArchiveName))
      {
        WinSpawn(szArchiveName, siCObject->szParameter, szTempDir, SW_SHOWNORMAL, TRUE);
      }
    }
    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
  }
  return(0);
}

void DetermineOSVersion()
{
  OSVERSIONINFO osvi;
  BOOL          bIsWin95Debute;

  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&osvi);
  bIsWin95Debute = IsWin95Debute();

  switch(osvi.dwPlatformId)
  {
    case VER_PLATFORM_WIN32_WINDOWS:
      if((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion == 0))
      {
        dwOSType |= OS_WIN95;
        if(bIsWin95Debute)
          dwOSType |= OS_WIN95_DEBUTE;
      }
      else if((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion > 0))
        dwOSType = OS_WIN98;
      break;

    case VER_PLATFORM_WIN32_NT:
      if(osvi.dwMajorVersion == 3)
        dwOSType = OS_NT3;
      else if(osvi.dwMajorVersion > 3)
        dwOSType = OS_NT4;
      break;
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

  diDialog->stSetupType0.dwItems = 0;
  diDialog->stSetupType1.dwItems = 0;
  diDialog->stSetupType2.dwItems = 0;
  diDialog->stSetupType3.dwItems = 0;
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

HRESULT InitDlgStartInstall(diSI *diDialog)
{
  diDialog->bShowDialog = FALSE;
  if((diDialog->szTitle = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((diDialog->szMessage0 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitDlgStartInstall(diSI *diDialog)
{
  FreeMemory(&(diDialog->szTitle));
  FreeMemory(&(diDialog->szMessage0));
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
  sgProduct.dwMode        = NORMAL;
  sgProduct.dwCustomType  = ST_RADIO0;

  if((sgProduct.szPath                        = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProductName                 = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderName           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szProgramFolderPath           = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((sgProduct.szAlternateArchiveSearchPath  = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((szTempSetupPath                         = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitSetupGeneral()
{
  FreeMemory(&(sgProduct.szPath));
  FreeMemory(&(sgProduct.szProductName));
  FreeMemory(&(sgProduct.szProgramFolderName));
  FreeMemory(&(sgProduct.szProgramFolderPath));
  FreeMemory(&(sgProduct.szAlternateArchiveSearchPath));
  FreeMemory(&(szTempSetupPath));
}

HRESULT InitSDObject()
{
  if((siSDObject.szCoreFile = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siSDObject.szCoreDir = NS_GlobalAlloc(MAX_BUF)) == NULL)
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
  if((siSDObject.szCoreFilePath = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  return(0);
}

void DeInitSDObject()
{
  FreeMemory(&(siSDObject.szCoreFile));
  FreeMemory(&(siSDObject.szCoreDir));
  FreeMemory(&(siSDObject.szNoAds));
  FreeMemory(&(siSDObject.szSilent));
  FreeMemory(&(siSDObject.szExecution));
  FreeMemory(&(siSDObject.szConfirmInstall));
  FreeMemory(&(siSDObject.szExtractMsg));
  FreeMemory(&(siSDObject.szExe));
  FreeMemory(&(siSDObject.szExeParam));
  FreeMemory(&(siSDObject.szCoreFilePath));
}

HRESULT InitSCoreFile()
{
  if((siCFCoreFile.szSource = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFCoreFile.szDestination = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);
  if((siCFCoreFile.szMessage = NS_GlobalAlloc(MAX_BUF)) == NULL)
    return(1);

  siCFCoreFile.bCleanup = TRUE;
  return(0);
}

void DeInitSCoreFile()
{
  FreeMemory(&(siCFCoreFile.szSource));
  FreeMemory(&(siCFCoreFile.szDestination));
  FreeMemory(&(siCFCoreFile.szMessage));
}

siC *CreateSiCNode()
{
  siC *siCNode;

  if((siCNode = NS_GlobalAlloc(sizeof(struct sinfoComponent))) == NULL)
    exit(1);

  siCNode->dwAttributes         = 0;
  siCNode->ullInstallSize       = 0;
  siCNode->ullInstallSizeSystem = 0;

  if((siCNode->szArchiveName = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionShort = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szDescriptionLong = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  if((siCNode->szParameter = NS_GlobalAlloc(MAX_BUF)) == NULL)
    exit(1);
  siCNode->siCDDependencies = NULL;
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

    siCTemp->Next->Prev = siCTemp->Prev;
    siCTemp->Prev->Next = siCTemp->Next;
    siCTemp->Next       = NULL;
    siCTemp->Prev       = NULL;

    FreeMemory(&(siCTemp->szArchiveName));
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

HRESULT SiCNodeGetAttributes(DWORD dwIndex)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(dwIndex == 0)
      return(siCTemp->dwAttributes);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(dwIndex == dwCount)
        return(siCTemp->dwAttributes);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(-1);
}

void SiCNodeSetAttributes(DWORD dwIndex, DWORD dwAttributes, BOOL bSet)
{
  DWORD dwCount  = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(dwIndex == 0)
    {
      if(bSet)
        siCTemp->dwAttributes |= dwAttributes;
      else
        siCTemp->dwAttributes &= ~dwAttributes;
    }

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(dwIndex == dwCount)
      {
      if(bSet)
        siCTemp->dwAttributes |= dwAttributes;
      else
        siCTemp->dwAttributes &= ~dwAttributes;
      }
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
}

void SiCNodeSetItemsSelected(DWORD dwItems, DWORD *dwItemsSelected)
{
  DWORD i;
  DWORD dwCount;
  siC   *siCTemp;

  dwCount = 0;
  siCTemp = siComponents;
  if(siCTemp != NULL)
  {
    for(i = 0; i < dwItems; i++)
    {
      if(dwItemsSelected[i] == dwCount)
      {
        /* Found the item that was selected, set the */
        /* SIC_SELECTED attribute and break*/
        siCTemp->dwAttributes |= SIC_SELECTED;
        break;
      }
      else
        /* unset the SIC_SELECTED attribute and */
        /* keep looking for the selected item (if any) */
        siCTemp->dwAttributes &= ~SIC_SELECTED;
    }
  
    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      for(i = 0; i < dwItems; i++)
      {
        if(dwItemsSelected[i] == dwCount)
        {
          /* Found the item that was selected, set the */
          /* SIC_SELECTED attribute and break*/
          siCTemp->dwAttributes |= SIC_SELECTED;
          break;
        }
        else
          /* unset the SIC_SELECTED attribute and */
          /* keep looking for the selected item (if any) */
          siCTemp->dwAttributes &= ~SIC_SELECTED;
      }
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
}

char *SiCNodeGetDescriptionLong(DWORD dwIndex)
{
  DWORD dwCount = 0;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(dwIndex == 0)
      return(siCTemp->szDescriptionLong);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(dwIndex == dwCount)
        return(siCTemp->szDescriptionLong);
      
      ++dwCount;
      siCTemp = siCTemp->Next;
    }
  }
  return(NULL);
}

ULONGLONG SiCNodeGetInstallSize(DWORD dwIndex)
{
  DWORD dwCount   = 0;
  siC   *siCTemp  = siComponents;

  if(siCTemp != NULL)
  {
    if(dwIndex == 0)
      return(siCTemp->ullInstallSize);

    ++dwCount;
    siCTemp = siCTemp->Next;
    while((siCTemp != NULL) && (siCTemp != siComponents))
    {
      if(dwIndex == dwCount)
        return(siCTemp->ullInstallSize);
      
      ++dwCount;
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

siC *SiCNodeGetObject(DWORD dwIndex, BOOL bIncludeInvisibleObjs)
{
  DWORD dwCount = -1;
  siC   *siCTemp = siComponents;

  if(siCTemp != NULL)
  {
    if(bIncludeInvisibleObjs)
    {
      ++dwCount;
    }
    else if(!(siCTemp->dwAttributes & SIC_INVISIBLE))
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
      else if(!(siCTemp->dwAttributes & SIC_INVISIBLE))
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

BOOL IsWin95Debute()
{
  HINSTANCE hLib;
  BOOL      bIsWin95Debute;
  DWORD dwErr;

  bIsWin95Debute = FALSE;
  if((hLib = LoadLibraryEx("kernel32.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
  {
    if(((FARPROC)NS_GetDiskFreeSpaceEx = GetProcAddress(hLib, "GetDiskFreeSpaceExA")) == NULL)
    {
      if((dwErr = GetLastError()) == ERROR_CALL_NOT_IMPLEMENTED)
      {
        (FARPROC)NS_GetDiskFreeSpace = GetProcAddress(hLib, "GetDiskFreeSpaceA");
        bIsWin95Debute = TRUE;
      }
    }
    FreeLibrary(hLib);
  }
  return(bIsWin95Debute);
}

/* returns the value in kilobytes */
ULONGLONG GetDiskSpaceRequired(DWORD dwType)
{
  ULONGLONG ullTotalSize = 0;
  siC       *siCTemp   = siComponents;

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
        }
      }

      siCTemp = siCTemp->Next;
    }
  }
  return(ullTotalSize);
}

/* returns the value in bytes */
ULONGLONG GetDiskSpaceAvailable(LPSTR szPath)
{
  char            szTempPath[MAX_BUF];
  char            szBuf[MAX_BUF];
  char            szBuf2[MAX_BUF];
  ULARGE_INTEGER  uliFreeBytesAvailableToCaller;
  ULARGE_INTEGER  uliTotalNumberOfBytesToCaller;
  ULARGE_INTEGER  uliTotalNumberOfFreeBytes;
  ULONGLONG       ullReturn = 0;
  DWORD           dwSectorsPerCluster;
  DWORD           dwBytesPerSector;
  DWORD           dwNumberOfFreeClusters;
  DWORD           dwTotalNumberOfClusters;

  if((dwOSType & OS_WIN95_DEBUTE) && (NS_GetDiskFreeSpace != NULL))
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
    lstrcpy(szTempPath, szPath);
    AppendBackSlash(szTempPath, MAX_BUF);
    if(NS_GetDiskFreeSpaceEx(szTempPath,
                             &uliFreeBytesAvailableToCaller,
                             &uliTotalNumberOfBytesToCaller,
                             &uliTotalNumberOfFreeBytes) == FALSE)
    {
      char szEDeterminingDiskSpace[MAX_BUF];

      if(NS_LoadString(hSetupRscInst, IDS_ERROR_DETERMINING_DISK_SPACE, szEDeterminingDiskSpace, MAX_BUF) == WIZ_OK)
      {
        lstrcpy(szBuf2, "\n    ");
        lstrcat(szBuf2, szTempPath);
        wsprintf(szBuf, szEDeterminingDiskSpace, szBuf2);
        PrintError(szBuf, ERROR_CODE_SHOW);
      }
    }
    ullReturn = uliFreeBytesAvailableToCaller.QuadPart;
  }
  return(ullReturn);
}

HRESULT VerifyDiskSpace()
{
  ULONGLONG ullDSAPath;
  ULONGLONG ullDSRPath;
  ULONGLONG ullDSASysPath;
  ULONGLONG ullDSRSysPath;
  ULONGLONG ullDSTotalAvailable;
  ULONGLONG ullDSTotalRequired;
  char      szDSAPath[MAX_BUF];
  char      szDSRPath[MAX_BUF];
  char      szDSASysPath[MAX_BUF];
  char      szDSRSysPath[MAX_BUF];
  char      szSysPath[MAX_BUF];
  char      szBuf1[MAX_BUF];
  char      szBuf2[MAX_BUF];
  char      szBuf3[MAX_BUF];
  char      szBufPath[MAX_BUF];
  char      szBufSysPath[MAX_BUF];
  char      szBufMsg[MAX_BUF];
  char      szBufSysMsg[MAX_BUF];
  char      szDlgDiskSpaceCheckTitle[MAX_BUF];
  char      szDlgDiskSpaceCheckMsg[MAX_BUF];
  char      szDlgDiskSpaceCheckSysMsg[MAX_BUF];

  if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_TITLE, szDlgDiskSpaceCheckTitle, MAX_BUF) != WIZ_OK)
    exit(1);
  if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_MSG, szDlgDiskSpaceCheckMsg, MAX_BUF) != WIZ_OK)
    exit(1);
  if(NS_LoadString(hSetupRscInst, IDS_DLG_DISK_SPACE_CHECK_SYS_MSG, szDlgDiskSpaceCheckSysMsg, MAX_BUF) != WIZ_OK)
    exit(1);

  ullDSAPath = GetDiskSpaceAvailable(sgProduct.szPath);
  ullDSRPath = GetDiskSpaceRequired(DSR_DESTINATION);

  if(GetSystemDirectory(szSysPath, MAX_BUF) != 0)
  {
    ullDSASysPath = GetDiskSpaceAvailable(szSysPath);
    ullDSRSysPath = GetDiskSpaceRequired(DSR_SYSTEM);

    ParsePath(sgProduct.szPath, szBufPath,    sizeof(szBufPath),    PP_ROOT_ONLY);
    ParsePath(szSysPath,        szBufSysPath, sizeof(szBufSysPath), PP_ROOT_ONLY);

    AppendBackSlash(szBufPath, sizeof(szBufPath));
    AppendBackSlash(szBufSysPath, sizeof(szBufSysPath));

    /* check to see if system path is the same as sgProduct.szPath */
    if(lstrcmpi(szBufPath, szBufSysPath) == 0)
    {
      /* check disk space for both system and destination */
      ullDSTotalRequired  = ullDSRPath + ullDSRSysPath;
      ullDSTotalAvailable = ullDSAPath;

      if(ullDSTotalAvailable < ullDSTotalRequired)
      {
        _ui64toa(ullDSTotalAvailable, szDSAPath, 10);
        _ui64toa(ullDSTotalRequired,  szDSRPath, 10);
        lstrcpy(szBuf1, "\n\n    ");
        lstrcat(szBuf1, sgProduct.szPath);
        lstrcat(szBuf1, "\n\n    ");
        lstrcpy(szBuf2, szDSRPath);
        lstrcat(szBuf2, "\n    ");
        lstrcpy(szBuf3, szDSAPath);
        lstrcat(szBuf3, "\n\n");
        wsprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBuf1, szBuf2, szBuf3);
        if(MessageBox(hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES)
        {
          return(FALSE);
        }
        else
        {
          return(TRUE);
        }
      }
    }
    else
    {
      /* check disk space for system only */
      if(ullDSASysPath < ullDSRSysPath)
      {
        _ui64toa(ullDSASysPath, szDSASysPath, 10);
        _ui64toa(ullDSRSysPath, szDSRSysPath, 10);
        lstrcpy(szBuf1, "\n\n    ");
        lstrcat(szBuf1, sgProduct.szPath);
        lstrcat(szBuf1, "\n\n    ");
        lstrcpy(szBuf2, szDSRPath);
        lstrcat(szBuf2, "\n    ");
        lstrcpy(szBuf3, szDSAPath);
        lstrcat(szBuf3, "\n\n");
        wsprintf(szBufSysMsg, szDlgDiskSpaceCheckSysMsg, szBuf1, szBuf2, szBuf3);
        return(MessageBox(hWndMain, szBufSysMsg, szDlgDiskSpaceCheckTitle, MB_RETRYCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_SETFOREGROUND));
      }
    }
  }

  /* check disk space for destination only */
  if(ullDSAPath < ullDSRPath)
  {
    _ui64toa(ullDSAPath, szDSAPath, 10);
    _ui64toa(ullDSRPath, szDSRPath, 10);
    lstrcpy(szBuf1, "\n\n    ");
    lstrcat(szBuf1, sgProduct.szPath);
    lstrcat(szBuf1, "\n\n    ");
    lstrcpy(szBuf2, szDSRPath);
    lstrcat(szBuf2, "\n    ");
    lstrcpy(szBuf3, szDSAPath);
    lstrcat(szBuf3, "\n\n");
    wsprintf(szBufMsg, szDlgDiskSpaceCheckMsg, szBuf1, szBuf2, szBuf3);
    if(MessageBox(hWndMain, szBufMsg, szDlgDiskSpaceCheckTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_APPLMODAL | MB_SETFOREGROUND) == IDYES)
    {
      return(FALSE);
    }
    else
    {
      return(TRUE);
    }
  }
  return(TRUE);
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

  return(dwAttributes);
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
  siC   *siCTemp;
  siCD  *siCDepTemp;

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
    DecriptString(siCTemp->szParameter, szBuf);

    /* get install size required in destination for component.  Sould be in Kilobytes */
    GetPrivateProfileString(szComponentItem, "Install Size", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
      siCTemp->ullInstallSize = _atoi64(szBuf);
    else
      siCTemp->ullInstallSize = 0;

    /* get install size required in system for component.  Sould be in Kilobytes */
    GetPrivateProfileString(szComponentItem, "Install Size System", "", szBuf, MAX_BUF, szFileIni);
    if(*szBuf != '\0')
      siCTemp->ullInstallSizeSystem = atol(szBuf);
    else
      siCTemp->ullInstallSizeSystem = 0;

    /* get attributes of component */
    GetPrivateProfileString(szComponentItem, "Attributes", "", szBuf, MAX_BUF, szFileIni);
    siCTemp->dwAttributes = ParseComponentAttributes(szBuf);

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

    /* inserts the newly created component into the global component queue */
    SiCNodeInsert(&siComponents, siCTemp);

    ++dwIndex0;
    itoa(dwIndex0, szIndex0, 10);
    lstrcpy(szComponentItem, "Component");
    lstrcat(szComponentItem, szIndex0);
    GetPrivateProfileString(szComponentItem, "Archive", "", szBuf, MAX_BUF, szFileIni);
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

    MessageBox(NULL, szBuf, NULL, MB_ICONEXCLAMATION);
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

BOOL ResolveComponentDependency(siCD *siCDInDependency)
{
  int     dwIndex;
  siCD    *siCDepTemp = siCDInDependency;
  BOOL    bMoreToResolve = FALSE;

  if(siCDepTemp != NULL)
  {
    if((dwIndex = SiCNodeGetIndexDS(siCDepTemp->szDescriptionShort)) != -1)
    {
      if((SiCNodeGetAttributes(dwIndex) & SIC_SELECTED) == FALSE)
      {
        bMoreToResolve = TRUE;
        SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE);
      }
    }

    siCDepTemp = siCDepTemp->Next;
    while((siCDepTemp != NULL) && (siCDepTemp != siCDInDependency))
    {
      if((dwIndex = SiCNodeGetIndexDS(siCDepTemp->szDescriptionShort)) != -1)
      {
        if((SiCNodeGetAttributes(dwIndex) & SIC_SELECTED) == FALSE)
        {
          bMoreToResolve = TRUE;
          SiCNodeSetAttributes(dwIndex, SIC_SELECTED, TRUE);
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
      if(SiCNodeGetAttributes(dwCount) & SIC_SELECTED)
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
        if(SiCNodeGetAttributes(dwCount) & SIC_SELECTED)
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

HRESULT ParseConfigIni(LPSTR lpszCmdLine)
{
  HDC  hdc;
  char szBuf[MAX_BUF];
  char szShowDialog[MAX_BUF];

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
  if(InitDlgWindowsIntegration(&diWindowsIntegration))
    return(1);
  if(InitDlgProgramFolder(&diProgramFolder))
    return(1);
  if(InitDlgStartInstall(&diStartInstall))
    return(1);
  if(InitSDObject())
    return(1);
  if(InitSCoreFile())
    return(1);
 
  /* get Alternate Archive Search Path, if there is one */
  GetAlternateArchiveSearchPath(lpszCmdLine);

  /* get install Mode information */
  GetPrivateProfileString("General", "Mode", "", szBuf, MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szBuf, "NORMAL") == 0)
    sgProduct.dwMode = NORMAL;
  if(lstrcmpi(szBuf, "AUTO") == 0)
    sgProduct.dwMode = AUTO;
  if(lstrcmpi(szBuf, "SILENT") == 0)
    sgProduct.dwMode = SILENT;

  /* get product name description */
  GetPrivateProfileString("General", "Product Name", "", sgProduct.szProductName, MAX_BUF, szFileIniConfig);
  
  /* get main install path */
  GetPrivateProfileString("General", "Path", "", szBuf, MAX_BUF, szFileIniConfig);
  DecriptString(sgProduct.szPath, szBuf);
  /* make a copy of sgProduct.szPath to be used in the Setup Type dialog */
  lstrcpy(szTempSetupPath, sgProduct.szPath);
  
  /* get main program folder path */
  GetPrivateProfileString("General", "Program Folder Path", "", szBuf, MAX_BUF, szFileIniConfig);
  DecriptString(sgProduct.szProgramFolderPath, szBuf);
  
  /* get main program folder name */
  GetPrivateProfileString("General", "Program Folder Name", "", szBuf, MAX_BUF, szFileIniConfig);
  DecriptString(sgProduct.szProgramFolderName, szBuf);

  /* Welcome dialog */
  GetPrivateProfileString("Dialog Welcome",             "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Title",        "", diWelcome.szTitle,               MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message0",     "", diWelcome.szMessage0,            MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message1",     "", diWelcome.szMessage1,            MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Welcome",             "Message2",     "", diWelcome.szMessage2,            MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diWelcome.bShowDialog = TRUE;

  /* License dialog */
  GetPrivateProfileString("Dialog License",             "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Title",        "", diLicense.szTitle,               MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "License File", "", diLicense.szLicenseFilename,     MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog License",             "Message0",     "", diLicense.szMessage0,            MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diLicense.bShowDialog = TRUE;

  /* Setup Type dialog */
  GetPrivateProfileString("Dialog Setup Type",          "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Title",        "", diSetupType.szTitle,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Setup Type",          "Message0",     "", diSetupType.szMessage0,          MAX_BUF, szFileIniConfig);
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

  /* Start Install dialog */
  GetPrivateProfileString("Dialog Start Install",       "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Title",        "", diStartInstall.szTitle,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Dialog Start Install",       "Message0",     "", diStartInstall.szMessage0,       MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diStartInstall.bShowDialog = TRUE;

  /* Reboot dialog */
  GetPrivateProfileString("Dialog Reboot",              "Show Dialog",  "", szShowDialog,                    MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szShowDialog, "TRUE") == 0)
    diReboot.dwShowDialog = TRUE;
  else if(lstrcmpi(szShowDialog, "AUTO") == 0)
    diReboot.dwShowDialog = AUTO;

  GetPrivateProfileString("Windows Integration-Item0", "CheckBoxState", "", szBuf,                           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Description",   "", diWindowsIntegration.wiCB0.szDescription, MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Windows Integration-Item0", "Archive",       "", diWindowsIntegration.wiCB0.szArchive, MAX_BUF, szFileIniConfig);
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
      diWelcome.bShowDialog             = FALSE;
      diLicense.bShowDialog             = FALSE;
      diSetupType.bShowDialog           = FALSE;
      diSelectComponents.bShowDialog    = FALSE;
      diWindowsIntegration.bShowDialog  = FALSE;
      diProgramFolder.bShowDialog       = FALSE;
      diStartInstall.bShowDialog        = FALSE;
      break;
  }

  InitSiComponents(szFileIniConfig);

  /* get Default Setup Type */
  GetPrivateProfileString("General", "Default Setup Type", "", szBuf, MAX_BUF, szFileIniConfig);
  if((lstrcmpi(szBuf, "Setup Type 0") == 0) && diSetupType.stSetupType0.bVisible)
  {
    dwSetupType     = ST_RADIO0;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType0.dwItems, diSetupType.stSetupType0.dwItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 1") == 0) && diSetupType.stSetupType1.bVisible)
  {
    dwSetupType     = ST_RADIO1;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType1.dwItems, diSetupType.stSetupType1.dwItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 2") == 0) && diSetupType.stSetupType2.bVisible)
  {
    dwSetupType     = ST_RADIO2;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType2.dwItems, diSetupType.stSetupType2.dwItemsSelected);
  }
  else if((lstrcmpi(szBuf, "Setup Type 3") == 0) && diSetupType.stSetupType3.bVisible)
  {
    dwSetupType     = ST_RADIO3;
    dwTempSetupType = dwSetupType;
    SiCNodeSetItemsSelected(diSetupType.stSetupType3.dwItems, diSetupType.stSetupType3.dwItemsSelected);
  }
  else
  {
    if(diSetupType.stSetupType0.bVisible)
    {
      dwSetupType     = ST_RADIO0;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType0.dwItems, diSetupType.stSetupType0.dwItemsSelected);
    }
    else if(diSetupType.stSetupType1.bVisible)
    {
      dwSetupType     = ST_RADIO1;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType1.dwItems, diSetupType.stSetupType1.dwItemsSelected);
    }
    else if(diSetupType.stSetupType2.bVisible)
    {
      dwSetupType     = ST_RADIO2;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType2.dwItems, diSetupType.stSetupType2.dwItemsSelected);
    }
    else if(diSetupType.stSetupType3.bVisible)
    {
      dwSetupType     = ST_RADIO3;
      dwTempSetupType = dwSetupType;
      SiCNodeSetItemsSelected(diSetupType.stSetupType3.dwItems, diSetupType.stSetupType3.dwItemsSelected);
    }
  }

  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file",        "", siSDObject.szCoreFile,        MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "core_file_path",   "", siSDObject.szCoreFilePath,    MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "core_dir",         "", siSDObject.szCoreDir,         MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "no_ads",           "", siSDObject.szNoAds,           MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "silent",           "", siSDObject.szSilent,          MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "execution",        "", siSDObject.szExecution,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "confirm_install",  "", siSDObject.szConfirmInstall,  MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Netscape Install", "extract_msg",      "", siSDObject.szExtractMsg,      MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe",              "", siSDObject.szExe,             MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("SmartDownload-Execution",        "exe_param",        "", siSDObject.szExeParam,        MAX_BUF, szFileIniConfig);

  GetPrivateProfileString("Core",                           "Source",           "", szBuf,                        MAX_BUF, szFileIniConfig);
  DecriptString(siCFCoreFile.szSource, szBuf);
  GetPrivateProfileString("Core",                           "Destination",      "", szBuf,                        MAX_BUF, szFileIniConfig);
  DecriptString(siCFCoreFile.szDestination, szBuf);
  GetPrivateProfileString("Core",                           "Message",          "", siCFCoreFile.szMessage,       MAX_BUF, szFileIniConfig);
  GetPrivateProfileString("Core",                           "Cleanup",          "", szBuf,                        MAX_BUF, szFileIniConfig);
  if(lstrcmpi(szBuf, "FALSE") == 0)
  {
    siCFCoreFile.bCleanup = FALSE;
  }
  else
  {
    siCFCoreFile.bCleanup = TRUE;
  }

  hdc = GetDC(hWndMain);

  OutputTitle(hdc, sgProduct.szProductName);
  ReleaseDC(hWndMain, hdc);

  return(0);
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
  stSetupType->dwItems = 0;
  itoa(dwIndex, szIndex, 10);
  lstrcpy(szKey, "C");
  lstrcat(szKey, szIndex);
  GetPrivateProfileString(szSection, szKey, "", szBuf, MAX_BUF, szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(lstrlen(szBuf) > 8)
    {
      ++stSetupType->dwItems;
      stSetupType->dwItemsSelected[dwIndex] = atoi(&szBuf[9]);
    }

    ++dwIndex;
    itoa(dwIndex, szIndex, 10);
    lstrcpy(szKey, "C");
    lstrcat(szKey, szIndex);
    GetPrivateProfileString(szSection, szKey, "", szBuf, MAX_BUF, szFileIniConfig);
  }
}

void GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwSize)
{
  HKEY  hkResult;
  DWORD dwErr;
  char  szBuf[MAX_BUF];

  memset(szBuf, '\0', MAX_BUF);

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwErr  = RegQueryValueEx(hkResult, szName, 0, NULL, szBuf, &dwSize);

    if((*szReturnValue != '\0') && (dwErr == ERROR_SUCCESS))
      ExpandEnvironmentStrings(szBuf, szReturnValue, MAX_BUF);
    else
      *szReturnValue = '\0';

    RegCloseKey(hkResult);
  }
}

HRESULT DecriptVariable(LPSTR szVariable, DWORD dwVariableSize)
{
  char szBuf[MAX_BUF];
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];

  /* zero out the memory allocations */
  ZeroMemory(szBuf,   sizeof(szBuf));
  ZeroMemory(szKey,   sizeof(szKey));
  ZeroMemory(szName,  sizeof(szName));
  ZeroMemory(szValue, sizeof(szValue));

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
    if((dwOSType & OS_WIN95) || (dwOSType & OS_WIN98))
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
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Startup", szVariable, dwVariableSize);
  }
  else if(lstrcmpi(szVariable, "COMMON_PROGRAMS") == 0)
  {
    /* parse for the "C:\WINNT40\Profiles\All Users\Start Menu\\Programs" directory */
    if((dwOSType & OS_WIN95) || (dwOSType & OS_WIN98))
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
    if((dwOSType & OS_WIN95) || (dwOSType & OS_WIN98))
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
    GetWinReg(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Desktop", szVariable, dwVariableSize);
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
    if((dwOSType & OS_NT3) || (dwOSType & OS_NT4))
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
  else if(lstrcmpi(szVariable, "TEMP") == 0)
  {
    /* parse for the "c:\Temp" path */
    lstrcpy(szVariable, szTempDir);
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
  else if(lstrcmpi(szVariable, "SETUP PATH") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
  }
  else if(lstrcmpi(szVariable, "Default Path") == 0)
  {
    lstrcpy(szVariable, sgProduct.szPath);
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
  else
    return(FALSE);

  return(TRUE);
}

HRESULT DecriptString(LPSTR szOutputStr, LPSTR szInputStr)
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
  BOOL  bDecripted;

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

    /* if Variable is "JAR PATH", do special processing */
    if(lstrcmpi(szVariable, "JAR PATH") == 0)
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
      bDecripted = TRUE;
    }
    else
    {
      bDecripted = DecriptVariable(szVariable, MAX_BUF);
    }

    if(!bDecripted)
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

    if(bDecripted)
    {
      DecriptString(szResultStr, szOutputStr);
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
        CreateDirectoriesAll(buf);
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
    siCObject = SiCNodeGetObject(dwIndex0, TRUE);
    while(siCObject)
    {
      lstrcpy(szArchiveName, szTempDir);
	    AppendBackSlash(szArchiveName, sizeof(szArchiveName));
      lstrcat(szArchiveName, siCObject->szArchiveName);

      DeleteFile(szArchiveName);

      ++dwIndex0;
      siCObject = SiCNodeGetObject(dwIndex0, TRUE);
    }
  }
}

void CleanTempFiles()
{
  DeleteIdiGetConfigIni();
  DeleteIdiGetArchives();

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
  if(bCreateDestinationDir)
    DirectoryRemove(sgProduct.szPath, FALSE);
  if(hbmpBoxChecked)
    DeleteObject(hbmpBoxChecked);
  if(hbmpBoxUnChecked)
    DeleteObject(hbmpBoxUnChecked);

  CleanTempFiles();

  DeInitSiComponents();
  DeInitSCoreFile();
  DeInitSDObject();
  DeInitDlgReboot(&diReboot);
  DeInitDlgStartInstall(&diStartInstall);
  DeInitDlgProgramFolder(&diProgramFolder);
  DeInitDlgWindowsIntegration(&diWindowsIntegration);
  DeInitDlgSelectComponents(&diSelectComponents);
  DeInitDlgSetupType(&diSetupType);
  DeInitDlgLicense(&diLicense);
  DeInitDlgWelcome(&diWelcome);
  DeInitSetupGeneral();

  FreeMemory(&szTempDir);
  FreeMemory(&sgProduct.szProgramFolderPath);
  FreeMemory(&sgProduct.szProgramFolderName);
  FreeMemory(&sgProduct.szProductName);
  FreeMemory(&sgProduct.szPath);
  FreeMemory(&szSetupDir);
  FreeMemory(&szFileIniConfig);
  FreeMemory(&szEGlobalAlloc);
  FreeMemory(&szEDllLoad);
  FreeMemory(&szEStringLoad);
  FreeMemory(&szEStringNull);

  FreeLibrary(hSetupRscInst);
  DeInitializeSmartDownload();
}
