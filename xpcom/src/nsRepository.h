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

#ifndef __nsRespository_h
#define __nsRespository_h

#include "prtypes.h"
#include "prmon.h"
#include "nsCom.h"
#include "nsID.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsHashtable.h"

/*
 * NSIRepository class
 */

// Errors
#define NS_ERROR_FACTORY_EXISTS               (NS_ERROR_BASE + 0x100)
#define NS_ERROR_FACTORY_NOT_REGISTERED       (NS_ERROR_BASE + 0x101)
#define NS_ERROR_FACTORY_NOT_LOADED           (NS_ERROR_BASE + 0x102)
#define NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT (NS_ERROR_BASE + 0x103)
#define NS_ERROR_FACTORY_NOT_UNREGISTERED     (NS_ERROR_BASE + 0x104)

typedef nsresult (*nsFactoryProc)(const nsCID &aCLass,
                                  nsIFactory **aFactory);
typedef PRBool (*nsCanUnloadProc)();
typedef nsresult (*nsRegisterProc)(const char *path);
typedef nsresult (*nsUnregisterProc)(const char *path);

class FactoryEntry;

class NS_COM NSRepository {
private:
  static nsHashtable *factories;
  static PRMonitor *monitor;

  static nsresult checkInitialized();

public:
  static nsresult Initialize();

  // Finds a factory for a specific class ID
  static nsresult FindFactory(const nsCID &aClass,
                              nsIFactory **aFactory);

  // Creates a class instance for a specific class ID
  static nsresult CreateInstance(const nsCID &aClass, 
                                 nsISupports *aDelegate,
                                 const nsIID &aIID,
                                 void **aResult);

  // Creates a class instance for a specific class ID
  static nsresult CreateInstance2(const nsCID &aClass, 
                                  nsISupports *aDelegate,
                                  const nsIID &aIID,
                                  void *aSignature,
                                  void **aResult);

  // Manually registry a factory for a class
  static nsresult RegisterFactory(const nsCID &aClass,
                                  nsIFactory *aFactory,
                                  PRBool aReplace);

  // Manually registry a dynamically loaded factory for a class
  static nsresult RegisterFactory(const nsCID &aClass,
                                  const char *aLibrary,
                                  PRBool aReplace,
                                  PRBool aPersist);

  // Manually unregister a factory for a class
  static nsresult UnregisterFactory(const nsCID &aClass,
                                    nsIFactory *aFactory);

  // Manually unregister a dynamically loaded factory for a class
  static nsresult UnregisterFactory(const nsCID &aClass,
                                    const char *aLibrary);

  // Unload dynamically loaded factories that are not in use
  static nsresult FreeLibraries();
};

#endif
