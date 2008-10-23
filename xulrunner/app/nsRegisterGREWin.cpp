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

#include "nsXPCOM.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

#include "nsAppRunner.h" // for MAXPATHLEN
#include "nsStringAPI.h"
#include "nsXPCOMGlue.h"
#include "nsCOMPtr.h"

#include "prio.h"

#include <windows.h>
#include "malloc.h"

static const wchar_t kRegKeyRoot[] = L"Software\\mozilla.org\\GRE";
static const wchar_t kRegFileGlobal[] = L"global.reginfo";
static const wchar_t kRegFileUser[] = L"user.reginfo";

static nsresult
MakeVersionKey(HKEY root, const wchar_t* keyname, const nsString &grehome,
               const GREProperty *aProperties, PRUint32 aPropertiesLen,
               const wchar_t *aGREMilestone)
{
  HKEY  subkey;
  DWORD disp;
  
  if (::RegCreateKeyExW(root, keyname, NULL, NULL, 0, KEY_WRITE, NULL,
                       &subkey, &disp) != ERROR_SUCCESS)
    return NS_ERROR_FAILURE;

  if (disp != REG_CREATED_NEW_KEY) {
    ::RegCloseKey(subkey);
    return NS_ERROR_FAILURE;
  }

  PRBool failed = PR_FALSE;
  failed |= ::RegSetValueExW(subkey, L"Version", NULL, REG_SZ,
			     (BYTE*) aGREMilestone,
			     sizeof(PRUnichar) * (wcslen(aGREMilestone) + 1)) 
    != ERROR_SUCCESS;
  failed |= ::RegSetValueExW(subkey, L"GreHome", NULL, REG_SZ,
			     (BYTE*) grehome.get(),
			     sizeof(PRUnichar) * (grehome.Length() + 1)) 
    != ERROR_SUCCESS;
  
  for (PRUint32 i = 0; i < aPropertiesLen; ++i) {
    // Properties should be ascii only 
    NS_ConvertASCIItoUTF16 prop(aProperties[i].property);
    NS_ConvertASCIItoUTF16 val(aProperties[i].value);
    failed |= ::RegSetValueExW(subkey, prop.get(), NULL, REG_SZ, 
			       (BYTE*) val.get(), 
			       sizeof(wchar_t)*(val.Length()+1)
			       ) != ERROR_SUCCESS;
  }

  ::RegCloseKey(subkey);

  if (failed) {
    // we created a key but couldn't fill it properly: delete it
    ::RegDeleteKeyW(root, keyname);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

int
RegisterXULRunner(PRBool aRegisterGlobally, nsIFile* aLocation,
                  const GREProperty *aProperties, PRUint32 aPropertiesLen,
                  const char *aGREMilestoneAscii)
{
  // Register ourself in the windows registry, and record what key we created
  // for future unregistration.

  nsresult rv;
  PRBool irv;
  int i;
  NS_ConvertASCIItoUTF16 aGREMilestone(aGREMilestoneAscii);
  nsString greHome;
  rv = aLocation->GetPath(greHome);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> savedInfoFile;
  aLocation->Clone(getter_AddRefs(savedInfoFile));
  nsCOMPtr<nsILocalFile> localSaved(do_QueryInterface(savedInfoFile));
  if (!localSaved)
    return PR_FALSE;

  const wchar_t *infoname = aRegisterGlobally ? kRegFileGlobal : kRegFileUser;
  localSaved->Append(nsDependentString(infoname));

  PRFileDesc* fd = nsnull;
  rv = localSaved->OpenNSPRFileDesc(PR_CREATE_FILE | PR_RDWR, 0664, &fd);
  if (NS_FAILED(rv)) {
    // XXX report error?
    return PR_FALSE;
  }

  HKEY rootKey = NULL;
  wchar_t keyName[MAXPATHLEN];
  PRInt32 r;

  if (::RegCreateKeyExW(aRegisterGlobally ? HKEY_LOCAL_MACHINE :
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
    if (::RegOpenKeyExW(rootKey, keyName, NULL, KEY_QUERY_VALUE, &existing) ==
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

  wcscpy(keyName, aGREMilestone.get());
  rv = MakeVersionKey(rootKey, keyName, greHome, aProperties, aPropertiesLen,
                      aGREMilestone.get());
  if (NS_SUCCEEDED(rv)) {
    NS_ConvertUTF16toUTF8 keyNameAscii(keyName);
    PR_Write(fd, keyNameAscii.get(), sizeof(char)*keyNameAscii.Length());
    irv = PR_TRUE;
    goto reg_end;
  }
  
  for (i = 0; i < 1000; ++i) {
    swprintf(keyName, L"%s_%i", aGREMilestone.get(),  i);
    rv = MakeVersionKey(rootKey, keyName, greHome,
                        aProperties, aPropertiesLen,
                        aGREMilestone.get());
    if (NS_SUCCEEDED(rv)) {
      NS_ConvertUTF16toUTF8 keyNameAscii(keyName);
      PR_Write(fd, keyNameAscii.get(), sizeof(char)*keyNameAscii.Length());
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
UnregisterXULRunner(PRBool aGlobal, nsIFile* aLocation,
                    const char *aGREMilestone)
{
  nsCOMPtr<nsIFile> savedInfoFile;
  aLocation->Clone(getter_AddRefs(savedInfoFile));
  nsCOMPtr<nsILocalFile> localSaved (do_QueryInterface(savedInfoFile));
  if (!localSaved)
    return;

  const wchar_t *infoname = aGlobal ? kRegFileGlobal : kRegFileUser;
  localSaved->Append(nsDependentString(infoname));

  PRFileDesc* fd = nsnull;
  nsresult rv = localSaved->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  if (NS_FAILED(rv)) {
    // XXX report error?
    return;
  }

  wchar_t keyName[MAXPATHLEN];
  PRInt32 r = PR_Read(fd, keyName, MAXPATHLEN);
  PR_Close(fd);

  localSaved->Remove(PR_FALSE);

  if (r <= 0)
    return;

  keyName[r] = '\0';

  HKEY rootKey = NULL;
  if (::RegOpenKeyExW(aGlobal ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
		      kRegKeyRoot, 0, KEY_READ, &rootKey) != ERROR_SUCCESS)
    return;

  HKEY subKey = NULL;
  if (::RegOpenKeyExW(rootKey, keyName, 0, KEY_READ, &subKey) == ERROR_SUCCESS) {

    char regpath[MAXPATHLEN];
    DWORD reglen = MAXPATHLEN;

    if (::RegQueryValueExW(subKey, L"GreHome", NULL, NULL,
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

  ::RegDeleteKeyW(rootKey, keyName);
  ::RegCloseKey(rootKey);
}
