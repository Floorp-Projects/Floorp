/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Navigator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape Communications Corp. are
 * Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Sean Su <ssu@netscape.com>
 */

#include "extern.h"
#include "parser.h"
#include "extra.h"
#include "ifuncns.h"

HKEY hkUnreadMailRootKey = HKEY_CURRENT_USER;
char szUnreadMailKey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail";
char szMozillaDesktopKey[] = "Software\\Mozilla\\Desktop";
char szRDISection[] = "Restore Desktop Integration";

/* structure for use with UnreadMail registry keys */
typedef struct sKeyNode skn;
struct sKeyNode
{
  char  szKey[MAX_BUF];
  skn   *Next;
  skn   *Prev;
};

/* Function that creates an instance of skn */
skn *CreateSknNode()
{
  skn *sknNode;

  if((sknNode = NS_GlobalAlloc(sizeof(struct sKeyNode))) == NULL)
    exit(1);

  sknNode->Next      = sknNode;
  sknNode->Prev      = sknNode;

  return(sknNode);
}

/* Function that inserts a skn structure into a linked list */
void SknNodeInsert(skn **sknHead, skn *sknTemp)
{
  if(*sknHead == NULL)
  {
    *sknHead          = sknTemp;
    (*sknHead)->Next  = *sknHead;
    (*sknHead)->Prev  = *sknHead;
  }
  else
  {
    sknTemp->Next           = *sknHead;
    sknTemp->Prev           = (*sknHead)->Prev;
    (*sknHead)->Prev->Next  = sknTemp;
    (*sknHead)->Prev        = sknTemp;
  }
}

/* Function that removes a skn structure from a skn linked list.
 * and frees memory. */
void SknNodeDelete(skn *sknTemp)
{
  if(sknTemp != NULL)
  {
    sknTemp->Next->Prev = sknTemp->Prev;
    sknTemp->Prev->Next = sknTemp->Next;
    sknTemp->Next       = NULL;
    sknTemp->Prev       = NULL;

    FreeMemory(&sknTemp);
  }
}

/* Function that traverses a skn linked list and deletes each node. */
void DeInitSknList(skn **sknHeadNode)
{
  skn *sknTemp;
  
  if(*sknHeadNode == NULL)
    return;

  sknTemp = (*sknHeadNode)->Prev;

  while(sknTemp != *sknHeadNode)
  {
    SknNodeDelete(sknTemp);
    sknTemp = (*sknHeadNode)->Prev;
  }

  SknNodeDelete(sknTemp);
  *sknHeadNode = NULL;
}

/* Function to check if [windir]\mapi32.dll belongs to mozilla or not */
int IsMapiMozMapi(BOOL *bIsMozMapi)
{
  HINSTANCE hLib;
  char szMapiFilePath[MAX_BUF];
  int iRv = WIZ_ERROR_UNDEFINED;
  int (PASCAL *GetMapiDllVersion)(void);
  char szMapiVersionKey[] = "MAPI version installed";
  char szBuf[MAX_BUF];
  int  iMapiVersionInstalled;

  /* Get the Mapi version that we installed from uninstall.ini.
   * If there is none set, then return WIZ_ERROR_UNDEFINED. */
  GetPrivateProfileString(szRDISection, szMapiVersionKey, "", szBuf, sizeof(szBuf), szFileIniUninstall);
  if(*szBuf == '\0')
    return(iRv);

  iMapiVersionInstalled = atoi(szBuf);
  if(GetSystemDirectory(szMapiFilePath, sizeof(szMapiFilePath)) == 0)
    return(iRv);

  AppendBackSlash(szMapiFilePath, sizeof(szMapiFilePath));
  lstrcat(szMapiFilePath, "Mapi32.dll");
  if(!FileExists(szMapiFilePath))
    iRv = WIZ_FILE_NOT_FOUND;
  else if((hLib = LoadLibrary(szMapiFilePath)) != NULL)
  {
    iRv = WIZ_OK;
    *bIsMozMapi = FALSE;
    if(((FARPROC)GetMapiDllVersion = GetProcAddress(hLib, "GetMapiDllVersion")) != NULL)
    {
      if(iMapiVersionInstalled == GetMapiDllVersion())
        *bIsMozMapi = TRUE;
    }
    FreeLibrary(hLib);
  }
  return(iRv);
}

/* This function parses takes as input a registry key path string beginning
 * with HKEY_XXXX (root key) and parses out the sub key path and the root
 * key.
 *
 * It returns the root key as HKEY and the sub key path as char* */
HKEY GetRootKeyAndSubKeyPath(char *szInKeyPath, char *szOutSubKeyPath, DWORD dwOutSubKeyPathSize)
{
  char *ptr      = szInKeyPath;
  HKEY hkRootKey = HKEY_CLASSES_ROOT;

  ZeroMemory(szOutSubKeyPath, dwOutSubKeyPathSize);
  if(ptr == NULL)
    return(hkRootKey);

  /* search for the first '\' char */
  while(*ptr && (*ptr != '\\'))
    ++ptr;

  if((*ptr == '\0') ||
     (*ptr == '\\'))
  {
    BOOL bPtrModified = FALSE;

    if(*ptr == '\\')
    {
      *ptr = '\0';
      bPtrModified = TRUE;
    }
    hkRootKey = ParseRootKey(szInKeyPath);

    if(bPtrModified)
      *ptr = '\\';

    if((*ptr != '\0') &&
       ((unsigned)lstrlen(ptr + 1) + 1 <= dwOutSubKeyPathSize))
      /* copy only the sub key path after the root key string */
      lstrcpy(szOutSubKeyPath, ptr + 1);
  }
  return(hkRootKey);
}


/* This function checks for nonprintable characters.
 * If at least one is found in the input string, it will return TRUE,
 * else FALSE */
BOOL CheckForNonPrintableChars(char *szInString)
{
  int i;
  int iLen;
  BOOL bFoundNonPrintableChar = FALSE;

  if(!szInString)
    return(TRUE);

  iLen = lstrlen(szInString);

  for(i = 0; i < iLen; i++)
  {
    if(!isprint(szInString[i]))
    {
      bFoundNonPrintableChar = TRUE;
      break;
    }
  }

  return(bFoundNonPrintableChar);
}

/* This function checks to see if the key path is a ddeexec path.  If so,
 * it then checks for non printable chars */
BOOL DdeexecCheck(char *szKey, char *szValue)
{
  char szKddeexec[] = "shell\\open\\ddeexec";
  char szKeyLower[MAX_BUF];
  BOOL bPass = TRUE;

  lstrcpy(szKeyLower, szKey);
  strlwr(szKeyLower);
  if(strstr(szKeyLower, szKddeexec) && CheckForNonPrintableChars(szValue))
    bPass = FALSE;

  return(bPass);
}

/* This function enumerates HKEY_LOCAL_MACHINE\Sofware\Mozilla\Desktop for
 * variable information on what desktop integration was done by the
 * browser/mail client.
 *
 * These variables found cannot be deleted or modified until the enumeration
 * is complete, or else this function will fail! */
void RestoreDesktopIntegration()
{
  char      szVarName[MAX_BUF];
  char      szValue[MAX_BUF];
  char      szSubKey[MAX_BUF];
  HKEY      hkHandle;
  DWORD     dwIndex;
  DWORD     dwSubKeySize;
  DWORD     dwTotalValues;
  char      szKHKEY[]               = "HKEY";
  char      szKisHandling[]         = "isHandling";

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, 0, KEY_READ|KEY_WRITE, &hkHandle) != ERROR_SUCCESS)
    return;

  dwTotalValues  = 0;
  RegQueryInfoKey(hkHandle, NULL, NULL, NULL, NULL, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
  for(dwIndex = 0; dwIndex < dwTotalValues; dwIndex++)
  {
    /* Enumerate thru all the vars found within the Mozilla Desktop key */
    dwSubKeySize = sizeof(szVarName);
    if(RegEnumValue(hkHandle, dwIndex, szVarName, &dwSubKeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
    {
      if(strnicmp(szVarName, szKHKEY, lstrlen(szKHKEY)) == 0)
      {
        HKEY hkRootKey;

        hkRootKey = GetRootKeyAndSubKeyPath(szVarName, szSubKey, sizeof(szSubKey));
        if(*szSubKey != '\0')
        {
          GetWinReg(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, szVarName, szValue, sizeof(szValue));
          if(*szValue != '\0')
          {
            /* Due to a bug in the browser code that saves the previous HKEY
             * value it's trying to replace as garbage chars, we need to try
             * to detect it.  If found, do not restore it. This bug only
             * happens for the saved ddeexec keys. */
            if(DdeexecCheck(szSubKey, szValue))
            {
              /* Restore the previous saved setting here */
              SetWinReg(hkRootKey,
                        szSubKey,
                        NULL,
                        REG_SZ,
                        szValue,
                        lstrlen(szValue));
            }
          }
          else
            /* if the saved value is an empty string, then
             * delete the default var for this key */
            DeleteWinRegValue(hkRootKey,
                              szSubKey,
                              szValue);
        }
      }
    }
  }
  RegCloseKey(hkHandle);
  return;
}

void RestoreMozMapi()
{
  char szMozMapiBackupFile[MAX_BUF];
  BOOL bFileIsMozMapi = FALSE;

  GetWinReg(HKEY_LOCAL_MACHINE,
            szMozillaDesktopKey,
            "Mapi_backup_dll",
            szMozMapiBackupFile,
            sizeof(szMozMapiBackupFile));

  /* If the windows registry Mapi_backup_dll var name does not exist,
   * then we're not the default mail handler for the system.
   *
   * If the backup mapi file does not exist for some reason, then
   * there's no way to restore the previous saved mapi32.dll file. */
  if((*szMozMapiBackupFile == '\0') || !FileExists(szMozMapiBackupFile))
    return;

  /* A TRUE for bFileIsMozMapi indicates that we need to restore
   * the backed up mapi32.dll (if one was backed up).
   *
   * bFileIsMozMapi is TRUE in the following conditions:
   *   * mapi32.dll is not found
   *   * mapi32.dll loads and GetMapiDllVersion() exists
   *     _and_ returns the same version indicated in the uninstall.ini file:
   *
   *       [Restore Desktop Integration]
   *       Mapi version installed=94
   *
   *     94 indicates version 0.9.4 */
  if(IsMapiMozMapi(&bFileIsMozMapi) == WIZ_FILE_NOT_FOUND)
    bFileIsMozMapi = TRUE;

  if(bFileIsMozMapi)
  {
    char szDestinationFilename[MAX_BUF];

    /* Get the Windows System (or System32 under NT) directory */
    if(GetSystemDirectory(szDestinationFilename, sizeof(szDestinationFilename)))
    {
      /* Copy the backup filename into the normal Mapi32.dll filename */
      AppendBackSlash(szDestinationFilename, sizeof(szDestinationFilename));
      lstrcat(szDestinationFilename, "Mapi32.dll");
      CopyFile(szMozMapiBackupFile, szDestinationFilename, FALSE);
    }
  }
  /* Delete the backup Mapi filename */
  FileDelete(szMozMapiBackupFile);
}

BOOL UndoDesktopIntegration(void)
{
  char szMozillaKey[] = "Software\\Mozilla";
  char szBuf[MAX_BUF];

  /* Check to see if uninstall.ini has indicated to restore
   * the destktop integration performed by the browser/mail */
  GetPrivateProfileString(szRDISection, "Enabled", "", szBuf, sizeof(szBuf), szFileIniUninstall);
  if(lstrcmpi(szBuf, "TRUE") == 0)
  {
    RestoreDesktopIntegration();
    RestoreMozMapi();

    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, TRUE);
    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaKey, FALSE);
  }

  return(0);
}


/* Function that retrieves the app name (including path) that is going to be
 * uninstalled.  The return string is in upper case. */
int GetUninstallAppPathName(char *szAppPathName, DWORD dwAppPathNameSize)
{
  char szKey[MAX_BUF];
  HKEY hkRoot;

  if(*ugUninstall.szUserAgent != '\0')
  {
    hkRoot = ugUninstall.hWrMainRoot;
    lstrcpy(szKey, ugUninstall.szWrMainKey);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, ugUninstall.szUserAgent);
    AppendBackSlash(szKey, sizeof(szKey));
    lstrcat(szKey, "Main");
  }
  else
  {
    return(CMI_APP_PATHNAME_NOT_FOUND);
  }

  GetWinReg(hkRoot, szKey, "PathToExe", szAppPathName, dwAppPathNameSize);
  strupr(szAppPathName);
  return(CMI_OK);
}

/* Function to delete the UnreadMail keys that belong to the app that is being
 * uninstalled.  The skn list that is passed in should only contain UnreadMail
 * subkeys to be deleted. */
void DeleteUnreadMailKeys(skn *sknHeadNode)
{
  skn   *sknTempNode;
  char  szUnreadMailDeleteKey[MAX_BUF];
  DWORD dwLength;
  
  if(sknHeadNode == NULL)
    return;

  sknTempNode = sknHeadNode;

  do
  {
    /* build the full UnreadMail key to be deleted */
    dwLength = sizeof(szUnreadMailDeleteKey) > lstrlen(szUnreadMailKey) ?
                      lstrlen(szUnreadMailKey) + 1: sizeof(szUnreadMailDeleteKey);
    lstrcpyn(szUnreadMailDeleteKey, szUnreadMailKey, dwLength);
    AppendBackSlash(szUnreadMailDeleteKey, sizeof(szUnreadMailDeleteKey));
    if((unsigned)(lstrlen(sknTempNode->szKey) + 1) <
       (sizeof(szUnreadMailDeleteKey) - lstrlen(szUnreadMailKey) + 1))
    {
      lstrcat(szUnreadMailDeleteKey, sknTempNode->szKey);

      /* delete the UnreadMail key (even if it has subkeys) */
      DeleteWinRegKey(hkUnreadMailRootKey, szUnreadMailDeleteKey, TRUE);
    }

    /* get the next key to delete */
    sknTempNode = sknTempNode->Next;

  }while(sknTempNode != sknHeadNode);
}

/* Function that builds a list of UnreadMail subkey names that only belong to
 * the app that is being uninstalled.  The list is a linked list of skn
 * structures. */
BOOL GetUnreadMailKeyList(char *szUninstallAppPathName, skn **sknWinRegKeyList)
{
  HKEY  hkSubKeyHandle;
  HKEY  hkHandle;
  DWORD dwErr = ERROR_SUCCESS;
  DWORD dwTotalSubKeys;
  DWORD dwSubKeySize;
  DWORD dwIndex;
  DWORD dwBufSize;
  BOOL  bFoundAtLeastOne = FALSE;
  char  szSubKey[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szNewKey[MAX_BUF];
  skn   *sknTempNode;

  /* open the UnreadMail key so we can enumerate its subkeys */
  dwErr = RegOpenKeyEx(hkUnreadMailRootKey,
                       szUnreadMailKey,
                       0,
                       KEY_READ|KEY_QUERY_VALUE,
                       &hkHandle);
  if(dwErr == ERROR_SUCCESS)
  {
    dwTotalSubKeys = 0;
    RegQueryInfoKey(hkHandle,
                    NULL,
                    NULL,
                    NULL,
                    &dwTotalSubKeys,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL);

    if(dwTotalSubKeys != 0)
    {
      dwIndex = 0;
      do
      {
        /* For each UnreadMail subkey, parse it's 'Application' var name for
         * the app we're uninstalling (full path).  Compare against both long
         * path name and short path name.  If match found, add to linked list
         * of skn structures.
         *
         * No key deletion is performed at this time! */

        sknTempNode = NULL; /* reset temporaty node pointer */
        dwSubKeySize = sizeof(szSubKey);
        if((dwErr = RegEnumKeyEx(hkHandle,
                                 dwIndex,
                                 szSubKey,
                                 &dwSubKeySize,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL)) == ERROR_SUCCESS)
        {
          lstrcpy(szNewKey, szUnreadMailKey);
          AppendBackSlash(szNewKey, sizeof(szNewKey));
          lstrcat(szNewKey, szSubKey);

          if(RegOpenKeyEx(hkUnreadMailRootKey,
                          szNewKey,
                          0,
                          KEY_READ,
                          &hkSubKeyHandle) == ERROR_SUCCESS)
          {
            dwBufSize = sizeof(szBuf);
            if(RegQueryValueEx(hkSubKeyHandle,
                               "Application",
                               NULL,
                               NULL,
                               szBuf,
                               &dwBufSize) == ERROR_SUCCESS)
            {
              strupr(szBuf);
              if(strstr(szBuf, szUninstallAppPathName) != NULL)
              {
                bFoundAtLeastOne = TRUE;
                sknTempNode = CreateSknNode();
                lstrcpyn(sknTempNode->szKey, szSubKey, dwSubKeySize + 1);
              }
              else
              {
                char szUninstallAppPathNameShort[MAX_BUF];

                GetShortPathName(szUninstallAppPathName,
                                 szUninstallAppPathNameShort,
                                 sizeof(szUninstallAppPathNameShort));
                if(strstr(szBuf, szUninstallAppPathNameShort) != NULL)
                {
                  bFoundAtLeastOne = TRUE;
                  sknTempNode = CreateSknNode();
                  lstrcpyn(sknTempNode->szKey, szSubKey, dwSubKeySize + 1);
                }
              }
            }
            RegCloseKey(hkSubKeyHandle);
          }
        }

        if(sknTempNode)
          SknNodeInsert(sknWinRegKeyList, sknTempNode);

        ++dwIndex;
      } while(dwErr != ERROR_NO_MORE_ITEMS);
    }
    RegCloseKey(hkHandle);
  }
  return(bFoundAtLeastOne);
}

/* Main function that deals with cleaning up the UnreadMail subkeys belonging
 * to the app were currently uninstalling. */
int CleanupMailIntegration(void)
{
  char szCMISection[] = "Cleanup Mail Integration";
  char szUninstallApp[MAX_BUF];
  char szBuf[MAX_BUF];
  skn  *sknWinRegKeyList = NULL;

  /* Check to see if uninstall.ini has indicated to cleanup
   * the mail integration performed by mail */
  GetPrivateProfileString(szCMISection,
                          "Enabled",
                          "",
                          szBuf,
                          sizeof(szBuf),
                          szFileIniUninstall);
  if(lstrcmpi(szBuf, "TRUE") != 0)
    return(CMI_OK);

  /* Get the full app name we're going to uninstall */
  if(GetUninstallAppPathName(szUninstallApp, sizeof(szUninstallApp)) != CMI_OK)
    return(CMI_APP_PATHNAME_NOT_FOUND);

  /* Build a list of UnreadMail subkeys that needs to be deleted */
  if(GetUnreadMailKeyList(szUninstallApp, &sknWinRegKeyList))
    /* Delete the UnreadMail subkeys using the list built by
     * GetUnreadMailKeyList() */
    DeleteUnreadMailKeys(sknWinRegKeyList);

  /* Clean up the linked list */
  DeInitSknList(&sknWinRegKeyList);
  return(CMI_OK);
}

