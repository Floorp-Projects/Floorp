/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mike Shaver <shaver@mozilla.org>
 */


#ifdef XP_MAC
#include "macstdlibextras.h"
#endif

#include "plstr.h"
#include "prlink.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"

static PRBool gUnreg = PR_FALSE, gSilent = PR_FALSE, gQuiet = PR_FALSE;


nsresult Register(const char *path) 
{ 
  nsCOMPtr<nsILocalFile> spec;
  nsresult rv = nsComponentManager::CreateInstance(NS_LOCAL_FILE_CONTRACTID, 
                                                   nsnull, 
                                                   NS_GET_IID(nsILocalFile), 
                                                   getter_AddRefs(spec));

  if (NS_FAILED(rv) || (!spec)) 
  {
      printf("create nsILocalFile failed\n");
      return NS_ERROR_FAILURE;
  }

  rv = spec->InitWithPath(path);
  if (NS_FAILED(rv)) return rv;
  rv = nsComponentManager::AutoRegisterComponent(nsIComponentManager::NS_Startup, spec);
  return rv;
}

nsresult Unregister(const char *path) 
{
  nsCOMPtr<nsILocalFile> spec;
  nsresult rv = nsComponentManager::CreateInstance(NS_LOCAL_FILE_CONTRACTID, 
                                                   nsnull, 
                                                   NS_GET_IID(nsILocalFile), 
                                                   getter_AddRefs(spec));

  if (NS_FAILED(rv) || (!spec)) 
  {
      fputs("create nsILocalFile failed\n", stderr);
      return NS_ERROR_FAILURE;
  }

  rv = spec->InitWithPath(path);
  if (NS_FAILED(rv)) return rv;
  rv = nsComponentManager::AutoUnregisterComponent(nsIComponentManager::NS_Startup, spec);
  return rv;
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

int ProcessArgs(int argc, char *argv[])
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
      i++;
    } else {
      if (gUnreg == PR_TRUE)
        res = Unregister(argv[i]);
      else
        res = Register(argv[i]);
      if (NS_FAILED(res)) {
        ReportError(res, argv[i]);
        result = -1;
      } else {
        ReportSuccess(argv[i]);
      }
    }
  }
  return result;
}

int main(int argc, char *argv[])
{
    int ret;

#ifdef XP_MAC
#if DEBUG
    InitializeSIOUX(1);
#endif
#endif

    NS_InitXPCOM(NULL, NULL);

    /* With no arguments, RegFactory will autoregister */
    if (argc <= 1)
    {
        nsresult rv = nsComponentManager::AutoRegister(
                                                       nsIComponentManager::NS_Startup,
                                                       NULL /* default location */);
        ret = (NS_FAILED(rv)) ? -1 : 0;
    }
    else
      ret = ProcessArgs(argc, argv);

    NS_ShutdownXPCOM(NULL);
    return ret;
}
