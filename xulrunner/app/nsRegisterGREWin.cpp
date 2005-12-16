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
 * The Original Code is Mozilla XULRunner.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsRegisterGRE.h"

#include "nsIFile.h"
#include "nsILocalFile.h"

#include "nsBuildID.h"
#include "nsAppRunner.h" // for MAXPATHLEN
#include "nsString.h"
#include "nsXPCOMGlue.h"

#include "prio.h"

#include <windows.h>

static const char kRegKeyRoot[] = "Software\\mozilla.org\\GRE";
static const char kRegFileGlobal[] = "global.reginfo";
static const char kRegFileUser[] = "user.reginfo";

static nsresult
MakeVersionKey(HKEY root, const char* keyname, const nsCAutoString &grehome,
               const GREProperty *aProperties, PRUint32 aPropertiesLen)
{
  HKEY  subkey;
  DWORD disp;
  if (::RegCreateKeyEx(root, keyname, NULL, NULL, 0, KEY_WRITE, NULL,
                       &subkey, &disp) != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  if (disp != REG_CREATED_NEW_KEY) {
    ::RegCloseKey(subkey);
    return NS_ERROR_FAILURE;
  }

  PRBool failed = PR_FALSE;
  failed |= ::RegSetValueEx(subkey, "Version", NULL, REG_SZ,
                            (BYTE*) GRE_BUILD_ID,
                            sizeof(GRE_BUILD_ID) - 1) != ERROR_SUCCESS;
  failed |= ::RegSetValueEx(subkey, "GreHome", NULL, REG_SZ,
                            (BYTE*) grehome.get(),
                            grehome.Length()) != ERROR_SUCCESS;

  for (PRUint32 i = 0; i < aPropertiesLen; ++i) {
    failed |= ::RegSetValueEx(subkey, aProperties[i].property, NULL, REG_SZ,
                              (BYTE*) aProperties[i].value,
                              strlen(aProperties[i].value)) != ERROR_SUCCESS;
  }

  ::RegCloseKey(subkey);

  if (failed) {
    // we created a key but couldn't fill it properly: delete it
    ::RegDeleteKey(root, keyname);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

int
RegisterXULRunner(PRBool aRegisterGlobally, nsIFile* aLocation,
                  const GREProperty *aProperties, PRUint32 aPropertiesLen)
{
  // Register ourself in the windows registry, and record what key we created
  // for future unregistration.

  nsresult rv;
  PRBool irv;
  int i;

  nsCAutoString greHome;
  rv = aLocation->GetNativePath(greHome);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> savedInfoFile;
  aLocation->Clone(getter_AddRefs(savedInfoFile));
  nsCOMPtr<nsILocalFile> localSaved(do_QueryInterface(savedInfoFile));
  if (!localSaved)
    return PR_FALSE;

  const char *infoname = aRegisterGlobally ? kRegFileGlobal : kRegFileUser;
  localSaved->AppendNative(nsDependentCString(infoname));

  PRFileDesc* fd = nsnull;
  rv = localSaved->OpenNSPRFileDesc(PR_CREATE_FILE | PR_RDWR, 0664, &fd);
  if (NS_FAILED(rv)) {
    // XXX report error?
    return PR_FALSE;
  }

  HKEY rootKey = NULL;
  char keyName[MAXPATHLEN];
  PRInt32 r;

  if (::RegCreateKeyEx(aRegisterGlobally ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER,
                       kRegKeyRoot, NULL, NULL, 0, KEY_WRITE,
                       NULL, &rootKey, NULL) != ERROR_SUCCESS) {
    irv = PR_FALSE;
    goto reg_end;
  }

  r = PR_Read(fd, keyName, MAXPATHLEN);
  if (r < 0) {
    irv = PR_FALSE;
    goto reg_end;
  }

  if (r > 0) {
    keyName[r] = '\0';

    // There was already a .reginfo file, let's see if we are already
    // registered.
    HKEY existing = NULL;
    if (::RegOpenKeyEx(rootKey, keyName, NULL, KEY_QUERY_VALUE, &existing) ==
        ERROR_SUCCESS) {
      fprintf(stderr, "Warning: Registry key Software\\mozilla.org\\GRE\\%s already exists.\n"
              "No action was performed.\n",
              keyName);
      irv = PR_FALSE;
      goto reg_end;
    }

    PR_Close(fd);
    fd = nsnull;
    
    rv = localSaved->OpenNSPRFileDesc(PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE, 0664, &fd);
    if (NS_FAILED(rv)) {
      // XXX report error?
      irv = PR_FALSE;
      goto reg_end;
    }
  }

  strcpy(keyName, GRE_BUILD_ID);
  rv = MakeVersionKey(rootKey, keyName, greHome, aProperties, aPropertiesLen);
  if (NS_SUCCEEDED(rv)) {
    PR_Write(fd, keyName, strlen(keyName));
    irv = PR_TRUE;
    goto reg_end;
  }
  
  for (i = 0; i < 1000; ++i) {
    sprintf(keyName, GRE_BUILD_ID "_%i", i);
    rv = MakeVersionKey(rootKey, keyName, greHome,
                        aProperties, aPropertiesLen);
    if (NS_SUCCEEDED(rv)) {
      PR_Write(fd, keyName, strlen(keyName));
      irv = PR_TRUE;
      goto reg_end;
    }
  }

  irv = PR_FALSE;

reg_end:
  if (fd)
    PR_Close(fd);

  if (rootKey)
    ::RegCloseKey(rootKey);

  return irv;
}

void
UnregisterXULRunner(PRBool aGlobal, nsIFile* aLocation)
{
  nsCOMPtr<nsIFile> savedInfoFile;
  aLocation->Clone(getter_AddRefs(savedInfoFile));
  nsCOMPtr<nsILocalFile> localSaved (do_QueryInterface(savedInfoFile));
  if (!localSaved)
    return;

  const char *infoname = aGlobal ? kRegFileGlobal : kRegFileUser;
  localSaved->AppendNative(nsDependentCString(infoname));

  PRFileDesc* fd = nsnull;
  nsresult rv = localSaved->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    // XXX report error?
    return;
  }

  char keyName[MAXPATHLEN];
  PRInt32 r = PR_Read(fd, keyName, MAXPATHLEN);
  PR_Close(fd);

  localSaved->Remove(PR_FALSE);

  if (r <= 0)
    return;

  keyName[r] = '\0';

  HKEY rootKey = NULL;
  if (::RegOpenKeyEx(aGlobal ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                     kRegKeyRoot, 0, KEY_READ, &rootKey) != ERROR_SUCCESS)
    return;

  HKEY subKey = NULL;
  if (::RegOpenKeyEx(rootKey, keyName, 0, KEY_READ, &subKey) == ERROR_SUCCESS) {

    char regpath[MAXPATHLEN];
    DWORD reglen = MAXPATHLEN;

    if (::RegQueryValueEx(subKey, "GreHome", NULL, NULL,
                          (BYTE*) regpath, &reglen) == ERROR_SUCCESS) {

      nsCOMPtr<nsILocalFile> regpathfile;
      rv = NS_NewNativeLocalFile(nsDependentCString(regpath), PR_FALSE,
                                 getter_AddRefs(regpathfile));

      PRBool eq;
      if (NS_SUCCEEDED(rv) && 
          NS_SUCCEEDED(aLocation->Equals(regpathfile, &eq)) && !eq) {
        // We think we registered for this key, but it doesn't point to
        // us any more!
        fprintf(stderr, "Warning: Registry key Software\\mozilla.org\\GRE\\%s points to\n"
                        "alternate path '%s'; unregistration was not successful.\n",
                keyName, regpath);

        ::RegCloseKey(subKey);
        ::RegCloseKey(rootKey);

        return;
      }
    }

    ::RegCloseKey(subKey);
  }

  ::RegDeleteKey(rootKey, keyName);
  ::RegCloseKey(rootKey);
}
