/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/LGPL 2.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM are
 *   Copyright (C) 2002, International Business Machines Corporation.
 *   All Rights Reserved.
 *
 * Contributor(s):
 *     Troy Chevalier <troy@netscape.com>
 *     Sean Su <ssu@netscape.com>
 *
 *  Alternatively, the contents of this file may be used under the terms of
 *  either of the GNU General Public License Version 2 or later (the "GPL"),
 *  or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 *  in which case the provisions of the GPL or the LGPL are applicable instead
 *  of those above. If you wish to allow use of your version of this file only
 *  under the terms of either the GPL or the LGPL, and not to allow others to
 *  use your version of this file under the terms of the MPL, indicate your
 *  decision by deleting the provisions above and replace them with the notice
 *  and other provisions required by the LGPL or the GPL. If you do not delete
 *  the provisions above, a recipient may use your version of this file under
 *  the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INCL_PM
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys\stat.h>


#include "resource.h"
#include "nsINIParser.h"

#define MAX_BUF       4096
#define WIZ_TEMP_DIR  "ns_temp"

/* Mode of Setup to run in */
#define NORMAL                          0
#define SILENT                          1
#define AUTO                            2

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

#define CLASS_NAME_SETUP_DLG            "MozillaSetupDlg"

/////////////////////////////////////////////////////////////////////////////
// Global Declarations

char      szTitle[MAX_BUF];
char      szCmdLineToSetup[MAX_BUF];
BOOL      gbUncompressOnly;
ULONG     ulMode;
static ULONG	nTotalBytes = 0;  // sum of all the FILE resources

char szOSTempDir[CCHMAXPATH];

/////////////////////////////////////////////////////////////////////////////
// Utility Functions

BOOL isFAT(char* szPath)
{
  APIRET rc;
  ULONG ulSize;
  PFSQBUFFER2 pfsqbuf2;
  CHAR szDrive[3];

  ulSize = sizeof(FSQBUFFER2) + 3 * CCHMAXPATH;
  pfsqbuf2 = (PFSQBUFFER2)malloc(ulSize);
  strncpy(szDrive, szPath, 2);
  szDrive[2] = '\0';

  DosError(FERR_DISABLEHARDERR);
  rc = DosQueryFSAttach(szDrive, 0, FSAIL_QUERYNAME,
                        pfsqbuf2, &ulSize);
  DosError(FERR_ENABLEHARDERR);

  if (rc == NO_ERROR) {
    if (strcmp((char*)(pfsqbuf2->szFSDName + pfsqbuf2->cbName), "FAT") != 0)
      return FALSE;
  }

  return TRUE;
}

HWND FindWindow(PCSZ pszAtomString)
{
  ATOM atom;
  HENUM henum;
  HWND hwndClient, hwnd = NULLHANDLE;


  atom = WinFindAtom(WinQuerySystemAtomTable(), pszAtomString);
  if (atom) {
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwnd = WinGetNextWindow(henum)) != NULLHANDLE)
    {
      ULONG ulWindowWord;
      ulWindowWord = WinQueryWindowULong(hwnd, QWL_USER);
      if (ulWindowWord == atom) {
        break;
      }
    }
  }
  WinEndEnumWindows(henum);
  return  hwnd;
}

// this function appends a backslash at the end of a string,
// if one does not already exists.
static void AppendBackSlash(PSZ szInput, ULONG ulInputSize)
{
  if(szInput != NULL)
  {
    if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((ULONG)strlen(szInput) + 1) < ulInputSize)
      {
        strcat(szInput, "\\");
      }
    }
  }
}

// This function is similar to GetFullPathName except instead of
// using the current drive and directory it uses the path of the
// directory designated for temporary files
// If you don't specify a filename, you just get the temp dir without
// a trailing slash
static BOOL
GetFullTempPathName(PCSZ szFileName, ULONG ulBufferLength, PSZ szBuffer)
{
  strcpy(szBuffer, szOSTempDir);
  AppendBackSlash(szBuffer, MAX_BUF);
  strcat(szBuffer, WIZ_TEMP_DIR);
  AppendBackSlash(szBuffer, MAX_BUF);

  if (szFileName) {
    strcat(szBuffer, szFileName);
  }

  return TRUE;
}


static void CreateDirectoriesAll(char* szPath)
{
  int     i;
  int     iLen = strlen(szPath);
  char    szCreatePath[CCHMAXPATH];

  memset(szCreatePath, 0, CCHMAXPATH);
  memcpy(szCreatePath, szPath, iLen);
  for(i = 0; i < iLen; i++)
  {
    if((iLen > 1) &&
      ((i != 0) && ((szPath[i] == '\\') || (szPath[i] == '/'))) &&
      (!((szPath[0] == '\\') && (i == 1)) && !((szPath[1] == ':') && (i == 2))))
    {
      szCreatePath[i] = '\0';
      DosCreateDir(szCreatePath, NULL);  
      szCreatePath[i] = szPath[i];
    }
  }
}

// This function removes a directory and its subdirectories
void DirectoryRemove(PSZ szDestination, BOOL bRemoveSubdirs)
{
  HDIR            hFile;
  FILEFINDBUF3    fdFile;
  ULONG           ulFindCount;
  ULONG           ulAttrs;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;

  if(bRemoveSubdirs == TRUE)
  {
    strcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    strcat(szDestTemp, "*");

    ulFindCount = 1;
    hFile = HDIR_CREATE;
    ulAttrs = FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_DIRECTORY | FILE_ARCHIVED;
    if((DosFindFirst(szDestTemp, &hFile, ulAttrs, &fdFile, sizeof(fdFile), &ulFindCount, FIL_STANDARD)) != NO_ERROR)
      bFound = FALSE;
    else
      bFound = TRUE;
    while(bFound == TRUE)
    {
      if((strcmpi(fdFile.achName, ".") != 0) && (strcmpi(fdFile.achName, "..") != 0))
      {
        /* create full path */
        strcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        strcat(szDestTemp, fdFile.achName);

        if(fdFile.attrFile & FILE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DosDelete(szDestTemp);
        }
      }

      ulFindCount = 1;
      if (DosFindNext(hFile, &fdFile, sizeof(fdFile), &ulFindCount) != NO_ERROR)
         bFound = FALSE;
      else
         bFound = TRUE;
    }

    DosFindClose(hFile);
  }
  
  DosDeleteDir(szDestination);
}

void RemoveBackSlash(PSZ szInput)
{
  int   iCounter;
  ULONG ulInputLen;

  if(szInput != NULL)
  {
    ulInputLen = strlen(szInput);

    for(iCounter = ulInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void ParsePath(PSZ szInput, PSZ szOutput, ULONG ulOutputSize, ULONG ulType)
{
  int   iCounter;
  ULONG ulCounter;
  ULONG ulInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
    ulInputLen    = strlen(szInput);
    memset(szOutput, 0, ulOutputSize);

    if(ulInputLen < ulOutputSize)
    {
      switch(ulType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = ulInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = ulInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_ROOT_ONLY:
          if(szInput[1] == ':')
          {
            szOutput[0] = szInput[0];
            szOutput[1] = szInput[1];
            AppendBackSlash(szOutput, ulOutputSize);
          }
          else if(szInput[1] == '\\')
          {
            int iFoundBackSlash = 0;
            for(ulCounter = 0; ulCounter < ulInputLen; ulCounter++)
            {
              if(szInput[ulCounter] == '\\')
              {
                szOutput[ulCounter] = szInput[ulCounter];
                ++iFoundBackSlash;
              }

              if(iFoundBackSlash == 3)
                break;
            }

            if(iFoundBackSlash != 0)
              AppendBackSlash(szOutput, ulOutputSize);
          }
          break;
      }
    }
  }
}

void ParseCommandLine(int argc, char *argv[])
{
  char  szArgVBuf[MAX_BUF];
  int   i = 1;

  memset(szCmdLineToSetup, 0, MAX_BUF);
  ulMode = NORMAL;
  gbUncompressOnly = FALSE;
  strcpy(szCmdLineToSetup, " ");
  while(i < argc)
  {
    if((strcmpi(argv[i], "-ms") == 0) || (strcmpi(argv[i], "/ms") == 0))
    {
      ulMode = SILENT;
    }
    else if((strcmpi(argv[i], "-u") == 0) || (strcmpi(argv[i], "/u") == 0))
    {
      gbUncompressOnly = TRUE;
    }
    strcat(szCmdLineToSetup, argv[i]); 
    strcat(szCmdLineToSetup, " ");
    ++i;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Extract Files Dialog

// Window proc for dialog
MRESULT EXPENTRY DialogProc(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg) {
    case WM_INITDLG:
      {
        SWP swpDlg;
        WinQueryWindowPos(hwndDlg, &swpDlg);
        WinSetWindowPos(hwndDlg,
                        HWND_TOP,
                        (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN)/2)-(swpDlg.cx/2),
                        (WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)/2)-(swpDlg.cy/2),
                        0,
                        0,
                        SWP_MOVE);
        WinSendMsg(WinWindowFromID(hwndDlg, IDC_GAUGE),SLM_SETSLIDERINFO,
                   MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
                   0);
      }
  }
  return WinDefDlgProc(hwndDlg, msg, mp1, mp2);
}

DeleteTempFiles(ULONG ulNumFiles)
{
  ULONG ulSize;
  void* pFileData;
  FILE* fhFile;
  char  szTmpFile[CCHMAXPATH] = "";
  char  szFileName[CCHMAXPATH] = "";

  for (int i=1;i<=ulNumFiles;i++) {
    WinLoadString(0, NULLHANDLE, ID_FILE_BASE+i, CCHMAXPATH, szFileName);
    GetFullTempPathName(szFileName, sizeof(szTmpFile), szTmpFile);
    DosDelete(szTmpFile);
  }
}


BOOL
ExtractFiles(ULONG ulNumFiles, HWND hwndDlg)
{
  ULONG ulSize;
  void* pFileData;
  FILE* fhFile;
  char  szTmpFile[CCHMAXPATH] = "";
  char  szFileName[CCHMAXPATH] = "";
  char  szArcLstFile[CCHMAXPATH] = "";
  char  szStatus[256];
  char  szText[256];
  ULONG nBytesWritten = 0;

  WinLoadString(0, NULLHANDLE, IDS_STATUS_EXTRACTING, sizeof(szText), szText);

  if(gbUncompressOnly == FALSE) {
    // CreateDirectoriesAll takes a fully qualified file, so we create one
    GetFullTempPathName("tempfile", sizeof(szTmpFile), szTmpFile);
    CreateDirectoriesAll(szTmpFile);
    GetFullTempPathName("Archive.lst", sizeof(szArcLstFile), szArcLstFile);
  } else {
    strcpy(szArcLstFile, "Archive.lst");
  }

  for (int i=1;i<=ulNumFiles;i++) {
    WinLoadString(0, NULLHANDLE, ID_FILE_BASE+i, CCHMAXPATH, szFileName);
    // Update the UI
    sprintf(szStatus, szText, szFileName);
    WinSetWindowText(WinWindowFromID(hwndDlg, IDC_STATUS), szStatus);
    if(gbUncompressOnly == FALSE) {
      GetFullTempPathName(szFileName, sizeof(szTmpFile), szTmpFile);
    } else {
      strcpy(szTmpFile, szFileName);
    }

    DosQueryResourceSize(NULLHANDLE, RT_RCDATA, ID_FILE_BASE+i, &ulSize);
    DosGetResource(NULLHANDLE, RT_RCDATA, ID_FILE_BASE+i, &pFileData);
    FILE* fhFile = fopen(szTmpFile, "wb+");
    fwrite(pFileData, ulSize, 1, fhFile);
    fclose(fhFile);
    DosFreeResource(pFileData);
    nBytesWritten += ulSize;
    WinSendMsg(WinWindowFromID(hwndDlg, IDC_GAUGE), SLM_SETSLIDERINFO,
                               MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
                               MPARAM(nBytesWritten * 100 / nTotalBytes));
    WritePrivateProfileString("Archives", szFileName, "TRUE", szArcLstFile);
  }
}

BOOL FileExists(PSZ szFile)
{
  struct stat st;
  int statrv;

  statrv = stat(szFile, &st);
  if (statrv == 0)
     return(TRUE);
  else
     return (FALSE);
}

RunInstaller(ULONG ulNumFiles, HWND hwndDlg)
{
  char                szCmdLine[MAX_BUF];
  char                szSetupFile[CCHMAXPATH];
  char                szUninstallFile[CCHMAXPATH];
  char                szArcLstFile[CCHMAXPATH];
  char                szText[256];
  char                szTempPath[CCHMAXPATH];
  char                szFilename[CCHMAXPATH];
  char                szBuf[MAX_BUF];
  ULONG               ulLen;

  // Update UI
  WinSendMsg(WinWindowFromID(hwndDlg, IDC_GAUGE),SLM_SETSLIDERINFO,
             MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
             0);
  WinLoadString(0, NULLHANDLE, IDS_STATUS_LAUNCHING_SETUP, sizeof(szText), szText);
  WinSetWindowText(WinWindowFromID(hwndDlg, IDC_STATUS), szText);

  // Setup program is in the directory specified for temporary files
  GetFullTempPathName("Archive.lst",   sizeof(szArcLstFile),    szArcLstFile);
  GetFullTempPathName("SETUP.EXE",     sizeof(szSetupFile),     szSetupFile);
  GetFullTempPathName("uninstall.exe", sizeof(szUninstallFile), szUninstallFile);

  GetPrivateProfileString("Archives", "uninstall.exe", "", szBuf, sizeof(szBuf), szArcLstFile);
  if(FileExists(szUninstallFile) && (*szBuf != '\0'))
  {
    strcpy(szCmdLine, szUninstallFile);
    ulLen = strlen(szUninstallFile);
  }
  else
  {
    strcpy(szCmdLine, szSetupFile);
    PPIB ppib;
    PTIB ptib;
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName(ppib->pib_hmte, sizeof(szBuf), szBuf);
    ParsePath(szBuf, szFilename, sizeof(szFilename), PP_FILENAME_ONLY);

    strcat(szCmdLine, " -n ");
    strcat(szCmdLine, szFilename);
    ulLen = strlen(szSetupFile);
  }

  strcat(szCmdLine, szCmdLineToSetup);

  // Launch the installer
  RESULTCODES rcChild;
  PID pidChild;
  CHAR szLoadError[CCHMAXPATH];

  szCmdLine[strlen(szCmdLine)] = '\0';
  szCmdLine[strlen(szCmdLine)+1] = '\0';
  szCmdLine[ulLen] = '\0';

  DosExecPgm(szLoadError,
             sizeof(szLoadError),
             EXEC_ASYNCRESULT,
             szCmdLine,
             NULL,
             &rcChild,
             szCmdLine);

  if(ulMode != SILENT)
  {
    WinDestroyWindow(hwndDlg);
  }

  DosWaitChild(DCWA_PROCESS,
               DCWW_WAIT,
               &rcChild,
               &pidChild,
               rcChild.codeTerminate);

  DeleteTempFiles(ulNumFiles);

  GetFullTempPathName("Archive.lst", sizeof(szTempPath), szTempPath);
  DosDelete(szTempPath);
  GetFullTempPathName("xpcom.ns", sizeof(szTempPath), szTempPath);
  DirectoryRemove(szTempPath, TRUE);
  GetFullTempPathName(NULL, sizeof(szTempPath), szTempPath);
  DirectoryRemove(szTempPath, FALSE);

  return TRUE;
}

main(int argc, char *argv[], char *envp[])
{
  HAB hab;
  HMQ hmq;
  HWND hwndFW, hwndDlg;
  ATOM atom;

  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab,0);

  atom = WinAddAtom(WinQuerySystemAtomTable(), "NSExtracting");

  char *tempEnvVar = NULL;

  // determine the system's TEMP path
  tempEnvVar = getenv("TMP");
  if ((tempEnvVar) && (!(isFAT(tempEnvVar)))) {
    strcpy(szOSTempDir, tempEnvVar);
  }
  else
  {
    tempEnvVar = getenv("TEMP");
    if (tempEnvVar)
      strcpy(szOSTempDir, tempEnvVar);
  }
  if ((!tempEnvVar) || (isFAT(tempEnvVar)))
  {
    ULONG ulBootDrive = 0;
    APIRET rc;
    char  buffer[] = " :\\OS2\\";
    DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                    &ulBootDrive, sizeof(ulBootDrive));
    buffer[0] = 'A' - 1 + ulBootDrive;
    if (isFAT(buffer)) {
       /* Try current disk if boot drive is FAT */
       ULONG ulDriveNum;
       ULONG ulDriveMap;
       strcpy(buffer, " :\\");
       DosQueryCurrentDisk(&ulDriveNum, &ulDriveMap);
       buffer[0] = 'A' - 1 + ulDriveNum;
       if (isFAT(buffer)) {
         int i;
         for (i = 2; i < 26; i++) {
           if ((ulDriveMap<<(31-i)) >> 31) {
             buffer[0] = 'A' + i;
             if (!(isFAT(buffer))) {
                break;
             }
           }
         }
         if (i == 26) {
            char szBuf[MAX_BUF];
            WinLoadString(0, NULLHANDLE, IDS_ERROR_NO_LONG_FILENAMES, sizeof(szBuf), szBuf);
            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, szBuf, NULL, 0, MB_ICONEXCLAMATION);
            return(1);
         }
       }
    }
    strcpy(szOSTempDir, buffer);
    strcat(szOSTempDir, "TEMP");
  }

  WinLoadString(0, NULLHANDLE, IDS_TITLE, MAX_BUF, szTitle);

  /* Allow only one instance of stubnsinstall to run.
   * Detect a previous instance of stubnsinstall, bring it to the 
   * foreground, and quit current instance */
  if(FindWindow("NSExtracting") != NULL)
    return(1);

  /* Allow only one instance of Setup to run.
   * Detect a previous instance of Setup, and quit */
  if((hwndFW = FindWindow(CLASS_NAME_SETUP_DLG)) != NULL)
  {
    WinSetActiveWindow(HWND_DESKTOP, hwndFW);
    return(1);
  }

  // Parse the command line
  ParseCommandLine(argc, argv);

  ULONG ulSize;
  ULONG ulNumFiles = 0;
  INT i = 1;
  APIRET rc = NO_ERROR;

  while(rc == NO_ERROR) {
    rc = DosQueryResourceSize(NULLHANDLE, RT_RCDATA, ID_FILE_BASE+i, &ulSize);
    if (rc == NO_ERROR) {
      nTotalBytes += ulSize;
      ulNumFiles++;
    }
    i++;
  }

  if(ulMode != SILENT)
  {
    hwndDlg = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP, DialogProc, NULLHANDLE, IDD_EXTRACTING, NULL);
    WinSetWindowULong(hwndDlg, QWL_USER, atom);
    WinSetWindowPos(hwndDlg, 0, 0, 0, 0, 0, SWP_SHOW | SWP_ACTIVATE);
  }

  // Extract the files
  ExtractFiles(ulNumFiles, hwndDlg);

  if (gbUncompressOnly == FALSE) {
    // Launch the install program and wait for it to finish
    RunInstaller(ulNumFiles, hwndDlg);
  }

  WinDeleteAtom(WinQuerySystemAtomTable(), atom);

  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
}
