/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsIComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIFileSpec.h"
#include "nsIRegistry.h"
#include "nsSpecialSystemDirectory.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "xcDll.h"

#ifndef nsNativeComponentLoader_h__
#define nsNativeComponentLoader_h__

class nsNativeComponentLoader : public nsIComponentLoader {

 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMPONENTLOADER

    nsNativeComponentLoader();
    virtual ~nsNativeComponentLoader();
    PRBool IsRelativePath(nsIFileSpec *path);
    nsresult RegistryNameForLib(const char *aLibName,
                                       char **aRegistryName);
    nsresult RegistryNameForSpec(nsIFileSpec *aSpec,
                                        char **aRegistryName);

 protected:
    nsCOMPtr<nsIRegistry> mRegistry;
    nsCOMPtr<nsIComponentManager> mCompMgr;
    nsObjectHashtable*  mDllStore;
    NS_IMETHOD RegisterComponentsInDir(PRInt32 when, nsIFileSpec *dir);
    nsIRegistry::Key mXPCOMKey;
    nsSpecialSystemDirectory *mComponentsDir;
    PRUint32 mComponentsDirLen;

 private:
    nsresult CreateDll(nsIFileSpec *spec, const char *aLocation, nsDll **aDll);
    nsresult SelfRegisterDll(nsDll *dll, const char *registryLocation);
    nsresult SelfUnregisterDll(nsDll *dll);
    void GetRegistryDllInfo(const char *aLocation, PRUint32 *lastModifiedTime,
                            PRUint32 *fileSize);
                                
    nsresult GetFactoryFromModule(nsDll *aDll, const nsCID &aCID,
                                  nsIFactory **aFactory);
    /* obsolete! already! */
    nsresult GetFactoryFromNSGetFactory(nsDll *aDlll, const nsCID &aCID,
                                        nsIServiceManager *aServMgr,
                                        nsIFactory **aFactory);

};

#endif /* nsNativeComponentLoader_h__ */
