/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <iostream.h>
#include "plstr.h"
#include "prlink.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsCOMPtr.h"
#include "nsString.h"

static bool gUnreg = false;

void print_err(nsresult err)
{
  switch (err) {
  case NS_ERROR_FACTORY_NOT_LOADED:
    cerr << "Factory not loaded";
    break;
  case NS_NOINTERFACE:
    cerr << "No Interface";
    break;
  case NS_ERROR_NULL_POINTER:
    cerr << "Null pointer";
    break;
  case NS_ERROR_OUT_OF_MEMORY:
    cerr << "Out of memory";
    break;
  default:
    cerr << hex << err << dec;
  }
}

nsresult Register(nsIComponentRegistrar* registrar, const char *path) 
{ 
  nsCOMPtr<nsILocalFile> file;
  nsresult rv =
    NS_NewLocalFile(
      NS_ConvertUTF8toUTF16(path),
      PR_TRUE,
      getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;
  rv = registrar->AutoRegister(file);
  return rv;
}

nsresult Unregister(const char *path) 
{
  /* NEEDS IMPLEMENTATION */
#if 0
    nsresult res = nsComponentManager::AutoUnregisterComponent(path);
  return res;
#else
  return NS_ERROR_FAILURE;
#endif
}

int ProcessArgs(nsIComponentRegistrar* registrar, int argc, char *argv[])
{
  int i = 1;
  nsresult res;

  while (i < argc) {
    if (argv[i][0] == '-') {
      int j;
      for (j = 1; argv[i][j] != '\0'; j++) {
        switch (argv[i][j]) {
        case 'u':
          gUnreg = PR_TRUE;
          break;
        default:
          cerr << "Unknown option '" << argv[i][j] << "'\n";
        }
      }
      i++;
    } else {
      if (gUnreg) {
        res = Unregister(argv[i]);
        if (NS_SUCCEEDED(res)) {
          cout << "Successfully unregistered: " << argv[i] << "\n";
        } else {
          cerr << "Unregister failed (";
          print_err(res);
          cerr << "): " << argv[i] << "\n";
        }
      } else {
        res = Register(registrar, argv[i]);
        if (NS_SUCCEEDED(res)) {
          cout << "Successfully registered: " << argv[i] << "\n";
        } else {
          cerr << "Register failed (";
          print_err(res);
          cerr << "): " << argv[i] << "\n";
        }
      }
      i++;
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int ret = 0;
  nsresult rv;
  {
    nsCOMPtr<nsIServiceManager> servMan;
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");

    /* With no arguments, RegFactory will autoregister */
    if (argc <= 1)
    {
      rv = registrar->AutoRegister(nsnull);
      ret = (NS_FAILED(rv)) ? -1 : 0;
    }
    else
      ret = ProcessArgs(registrar, argc, argv);
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  rv = NS_ShutdownXPCOM( NULL );
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return ret;
}
