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

char gszRDISection[] = "Restore Desktop Integration";

void RestoreDesktopIntegrationAssociations(HKEY hkRoot, LPSTR szWinRegDesktopKey)
{
  char      *szBufPtr = NULL;
  char      szSection[MAX_BUF];
  char      szAllKeys[MAX_BUF];
  char      szValue[MAX_BUF];
  char      szRDIName[] = "HKEY_LOCAL_MACHINE\\Software\\Classes";
  char      szName[MAX_BUF];

  wsprintf(szSection, "%s Associations", gszRDISection);
  GetPrivateProfileString(szSection, NULL, "", szAllKeys, sizeof(szAllKeys), szFileIniUninstall);
  if(*szAllKeys != '\0')
  {
    szBufPtr = szAllKeys;
    while(*szBufPtr != '\0')
    {
      GetPrivateProfileString(szSection, szBufPtr, "", szValue, sizeof(szValue), szFileIniUninstall);
      if(lstrcmpi(szValue, "TRUE") == 0)
      {
        wsprintf(szName, "%s\\%s", szRDIName, szBufPtr);
        GetWinReg(hkRoot, szWinRegDesktopKey, szName, szValue, sizeof(szValue));
        if(*szValue != '\0')
        {
          SetWinReg(HKEY_CLASSES_ROOT, szBufPtr, NULL, REG_SZ, szValue, lstrlen(szValue));
        }
      }

      /* move the pointer to the next key */
      szBufPtr += lstrlen(szBufPtr) + 1;
    }
  }
}

void ResetWinRegDIProcotol(HKEY hkRoot, LPSTR szWinRegDesktopKey, LPSTR szRDIName, LPSTR szProtocol, LPSTR szLatterKeyPath)
{
  char szKey[MAX_BUF];
  char szName[MAX_BUF];
  char szValue[MAX_BUF];

  wsprintf(szName, "%s\\%s\\%s", szRDIName, szProtocol, szLatterKeyPath);
  GetWinReg(hkRoot, szWinRegDesktopKey, szName, szValue, sizeof(szValue));
  if(*szValue != '\0')
  {
    wsprintf(szKey, "%s\\%s", szProtocol, szLatterKeyPath);
    SetWinReg(HKEY_CLASSES_ROOT, szKey, NULL, REG_SZ, szValue, lstrlen(szValue));
  }
}

void RestoreDesktopIntegrationProtocols(HKEY hkRoot, LPSTR szWinRegDesktopKey)
{
  char      *szBufPtr = NULL;
  char      szSection[MAX_BUF];
  char      szAllKeys[MAX_BUF];
  char      szValue[MAX_BUF];
  char      szRDIName[] = "HKEY_LOCAL_MACHINE\\Software\\Classes";

  wsprintf(szSection, "%s Protocols", gszRDISection);
  GetPrivateProfileString(szSection, NULL, "", szAllKeys, sizeof(szAllKeys), szFileIniUninstall);
  if(*szAllKeys != '\0')
  {
    szBufPtr = szAllKeys;
    while(*szBufPtr != '\0')
    {
      GetPrivateProfileString(szSection, szBufPtr, "", szValue, sizeof(szValue), szFileIniUninstall);
      if(lstrcmpi(szValue, "TRUE") == 0)
      {
        ResetWinRegDIProcotol(hkRoot, szWinRegDesktopKey, szRDIName, szBufPtr, "shell\\open\\command");
        ResetWinRegDIProcotol(hkRoot, szWinRegDesktopKey, szRDIName, szBufPtr, "shell\\open\\ddeexec");
        ResetWinRegDIProcotol(hkRoot, szWinRegDesktopKey, szRDIName, szBufPtr, "shell\\open\\ddeexec\\Application");
      }

      /* move the pointer to the next key */
      szBufPtr += lstrlen(szBufPtr) + 1;
    }
  }
}

BOOL UndoDesktopIntegration(void)
{
  char szMozillaDesktopKey[MAX_BUF];
  char szMozillaKey[] = "Software\\Mozilla";

  wsprintf(szMozillaDesktopKey, "%s\\%s", szMozillaKey, "Desktop");
  RestoreDesktopIntegrationAssociations(HKEY_LOCAL_MACHINE, szMozillaDesktopKey);
  RestoreDesktopIntegrationProtocols(HKEY_LOCAL_MACHINE, szMozillaDesktopKey);

  DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, TRUE);
  DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaKey, FALSE);

  return(0);
}

