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
#include "nsRepository.h"

static PRBool gUnreg = PR_FALSE;

PRLibrary *GetLib(const char *path) {
  return PR_LoadLibrary(path);
}

int Register(const char *path) 
{
  int res = NS_OK;
  PRLibrary *instance = GetLib(path);
  if (instance != NULL) {
    nsRegisterProc proc = (nsRegisterProc) PR_FindSymbol(instance,
                                                         "NSRegisterSelf");
    if (proc != NULL) {
      res = proc(path);
    }
    PR_UnloadLibrary(instance);
  } else {
    res = NS_ERROR_FACTORY_NOT_LOADED;
  }

  return res;
}

int Unregister(const char *path) 
{
  int res = NS_OK;
  PRLibrary *instance = GetLib(path);
  if (instance != NULL) {
    nsUnregisterProc proc = (nsUnregisterProc) PR_FindSymbol(instance,
                                                           "NSUnregisterSelf");
    if (proc != NULL) {
      res = proc(path);
    }
    PR_UnloadLibrary(instance);
  } else {
    res = NS_ERROR_FACTORY_NOT_LOADED;
  }

  return res;
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
          cerr << "Unregister failed (" << res << "): " << argv[i] << "\n";
        }
      } else {
        res = Register(argv[i]);
        if (NS_SUCCEEDED(res)) {
          cout << "Successfully registered: " << argv[i] << "\n";
        } else {
          cerr << "Register failed (" << res << "): " << argv[i] << "\n";
        }
      }
      i++;
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  return ProcessArgs(argc, argv);
}
