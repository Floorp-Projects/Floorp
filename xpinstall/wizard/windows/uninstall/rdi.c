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
  char      szWRMozillaDesktopKey[] = "Software\\Mozilla\\Desktop";
  char      szKHKEY[]               = "HKEY";
  char      szKisHandling[]         = "isHandling";

  if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, szWRMozillaDesktopKey, 0, KEY_READ|KEY_WRITE, &hkHandle) != ERROR_SUCCESS)
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
          GetWinReg(HKEY_LOCAL_MACHINE, szWRMozillaDesktopKey, szVarName, szValue, sizeof(szValue));
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

BOOL UndoDesktopIntegration(void)
{
  char szMozillaDesktopKey[MAX_BUF];
  char szMozillaKey[] = "Software\\Mozilla";
  char szRDISection[] = "Restore Desktop Integration";
  char szBuf[MAX_BUF];

  /* Check to see if uninstall.ini has indicated to restore
   * the destktop integration performed by the browser/mail */
  GetPrivateProfileString(szRDISection, "Enabled", "", szBuf, sizeof(szBuf), szFileIniUninstall);
  if(lstrcmpi(szBuf, "TRUE") == 0)
  {
    wsprintf(szMozillaDesktopKey, "%s\\%s", szMozillaKey, "Desktop");
    RestoreDesktopIntegration();

    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, TRUE);
    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaKey, FALSE);
  }

  return(0);
}

