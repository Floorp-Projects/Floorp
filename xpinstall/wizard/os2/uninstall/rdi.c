/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Navigator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "extern.h"
#include "extra.h"
#include "parser.h"
#include "ifuncns.h"

char szUnreadMailKey[] = "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail";
char szMozillaDesktopKey[] = "Software\\Mozilla\\Desktop";
char szRDISection[] = "Restore Desktop Integration";

/* This function enumerates HKEY_LOCAL_MACHINE\Sofware\Mozilla\Desktop for
 * variable information on what desktop integration was done by the
 * browser/mail client.
 *
 * These variables found cannot be deleted or modified until the enumeration
 * is complete, or else this function will fail! */
void RestoreDesktopIntegration()
{
#ifdef OLDCODE
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
      if(strnicmp(szVarName, szKHKEY, strlen(szKHKEY)) == 0)
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
                        strlen(szValue));
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
#endif
}

BOOL UndoDesktopIntegration(void)
{
#ifdef OLDCODE
  char szMozillaKey[] = "Software\\Mozilla";
  char szBuf[MAX_BUF];

  /* Check to see if uninstall.ini has indicated to restore
   * the destktop integration performed by the browser/mail */
  GetPrivateProfileString(szRDISection, "Enabled", "", szBuf, sizeof(szBuf), szFileIniUninstall);
  if(stricmp(szBuf, "TRUE") == 0)
  {
    RestoreDesktopIntegration();

    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaDesktopKey, TRUE);
    DeleteWinRegKey(HKEY_LOCAL_MACHINE, szMozillaKey, FALSE);
  }
#endif
  return(0);
}

