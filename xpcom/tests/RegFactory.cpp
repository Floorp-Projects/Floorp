/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <iostream.h>
#include "plstr.h"
#include "prlink.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static PRBool gUnreg = PR_FALSE;

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

nsresult Register(const char *path) 
{ 
  nsCOMPtr<nsIFileSpec> spec;
  nsresult res = NS_NewFileSpec(getter_AddRefs(spec));
  if (NS_FAILED(res)) return res;
  res = spec->SetNativePath((char *)path);
  if (NS_FAILED(res)) return res;
  res = nsComponentManager::AutoRegisterComponent(nsIComponentManager::NS_Startup, spec);
  return res;
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

int ProcessArgs(int argc, char *argv[])
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
      if (gUnreg == PR_TRUE) {
        res = Unregister(argv[i]);
        if (NS_SUCCEEDED(res)) {
          cout << "Successfully unregistered: " << argv[i] << "\n";
        } else {
          cerr << "Unregister failed (";
          print_err(res);
          cerr << "): " << argv[i] << "\n";
        }
      } else {
        res = Register(argv[i]);
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

    return ret;
}

#ifdef XP_UNIX
/* The timer code doesn't get linked in. Some components link with libraries
 * in bin/ directory that assume that app that they are used in has libtimer_s.a
 * So ensure libtimer_s.a gets linked in.
 */
#include "nsITimer.h"

void dummy()
{
  nsITimer *timer;
  (void) NS_NewTimer(&timer);
  NS_IF_RELEASE(timer);
}
#endif /* XP_UNIX */
