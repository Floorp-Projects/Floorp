/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifdef XP_MAC
#include "macstdlibextras.h"
#endif

#include "plstr.h"
#include "prlink.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsDependentString.h"

static PRBool gUnreg = PR_FALSE, gSilent = PR_FALSE, gQuiet = PR_FALSE;


nsresult Register(nsIComponentRegistrar* registrar, const char *path) 
{ 
  nsresult rv;
  nsCOMPtr<nsILocalFile> spec( do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv) );

  if (NS_FAILED(rv) || (!spec)) 
  {
      printf("create nsILocalFile failed\n");
      return NS_ERROR_FAILURE;
  }

  rv = spec->InitWithNativePath(nsDependentCString(path));
  if (NS_FAILED(rv)) return rv;
  return registrar->AutoRegister(spec);
}

nsresult Unregister(nsIComponentRegistrar *registrar, const char *path) 
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> spec( do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv) );

  if (NS_FAILED(rv) || (!spec)) 
  {
      fputs("create nsILocalFile failed\n", stderr);
      return NS_ERROR_FAILURE;
  }

  rv = spec->InitWithNativePath(nsDependentCString(path));
  if (NS_FAILED(rv)) return rv;
  return registrar->AutoUnregister(spec);
}

void ReportSuccess(const char *file)
{
  if (gQuiet)
    return;

  if (gUnreg)
    printf("Unregistration successful for %s\n", file);
  else
    printf("Registration successful for %s\n", file);
}

void ReportError(nsresult err, const char *file)
{
  if (gSilent)
    return;

  if (gUnreg)
    fputs("Unregistration failed: (", stderr);
  else
    fputs("Registration failed: (", stderr);
  
  switch (err) {
  case NS_ERROR_FACTORY_NOT_LOADED:
    fputs("Factory not loaded", stderr);
    break;
  case NS_NOINTERFACE:
    fputs("No Interface", stderr);
    break;
  case NS_ERROR_NULL_POINTER:
    fputs("Null pointer", stderr);
    break;
  case NS_ERROR_OUT_OF_MEMORY:
    fputs("Out of memory", stderr);
    break;
  default:
    fprintf(stderr, "%x", (unsigned)err);
  }

  fprintf(stderr, ") %s\n", file);
}

int ProcessArgs(nsIComponentRegistrar *registrar, int argc, char *argv[])
{
  int i = 1, result = 0;
  nsresult res;

  while (i < argc) {
    if (argv[i][0] == '-') {
      int j;
      for (j = 1; argv[i][j] != '\0'; j++) {
        switch (argv[i][j]) {
        case 'u':
          gUnreg = PR_TRUE;
          break;
        case 'Q':
          gSilent = PR_TRUE;
          /* fall through */
        case 'q':
          gQuiet = PR_TRUE;
          break;
        default:
          fprintf(stderr, "Unknown option '%c'\n", argv[i][j]);
        }
      }
    } else {
      if (gUnreg == PR_TRUE)
        res = Unregister(registrar, argv[i]);
      else
        res = Register(registrar, argv[i]);
      if (NS_FAILED(res)) {
        ReportError(res, argv[i]);
        result = -1;
      } else {
        ReportSuccess(argv[i]);
      }
    }
    i++;
  }
  return result;
}

int main(int argc, char *argv[])
{
  int ret;
  nsresult rv;

#ifdef XP_MAC
#if DEBUG
  InitializeSIOUX(1);
#endif
#endif
  {
    nsCOMPtr<nsIServiceManager> servMan;
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    
    if (NS_FAILED(rv)) {
      printf("Can not initialize XPCOM\n");
      return -1;
    }
    
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
 
    /* With no arguments, RegFactory will autoregister */
    if (argc <= 1)
    {
      rv = registrar->AutoRegister(nsnull);
      ret = (NS_FAILED(rv)) ? -1 : 0;
    }
    else
    {
      ret = ProcessArgs(registrar, argc, argv);
    }
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return ret;
}
