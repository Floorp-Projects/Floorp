/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      true,
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
          gUnreg = true;
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
