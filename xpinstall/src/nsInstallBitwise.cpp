/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released March 31,  
 * 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nscore.h"
#include "nsInstall.h" // for error codes
#include "prmem.h"
#include "nsInstallBitwise.h"

#ifdef _WINDOWS
#include <windows.h>
#include <winreg.h>
#endif

#define KEY_SHARED_DLLS "Software\\Microsoft\\Windows\\CurrentVersion\\SharedDlls"

PRInt32 RegisterSharedFile(const char *file, PRBool bAlreadyExists)
{
  PRInt32 rv = nsInstall::SUCCESS;

#ifdef WIN32
  HKEY     root;
  HKEY     keyHandle = 0;
  LONG     result;
  DWORD    type = REG_DWORD;
  DWORD    dwDisposition;
  PRUint32 valbuf = 0;
  PRUint32 valbufsize;

  valbufsize = sizeof(PRUint32);
  root       = HKEY_LOCAL_MACHINE;
  result     = RegCreateKeyEx(root, KEY_SHARED_DLLS, 0, nsnull, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nsnull, &keyHandle, &dwDisposition);
  if(ERROR_SUCCESS == result)
  {
    result = RegQueryValueEx(keyHandle, file, nsnull, &type, (LPBYTE)&valbuf, (LPDWORD)&valbufsize);

    if((ERROR_SUCCESS == result) && (type == REG_DWORD))
      ++valbuf;
    else
    {
      valbuf = 1;
      if(bAlreadyExists == PR_TRUE)
        ++valbuf;
    }

    RegSetValueEx(keyHandle, file, 0, REG_DWORD, (LPBYTE)&valbuf, valbufsize);
    RegCloseKey(keyHandle);
  }
  else
    rv = nsInstall::UNEXPECTED_ERROR;

#endif

  return(rv);
}

