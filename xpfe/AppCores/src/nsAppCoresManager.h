/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsAppCoresManager_h___
#define nsAppCoresManager_h___

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMAppCoresManager.h"
#include "prio.h"
#include "nsVoidArray.h"

class nsIScriptContext;
class nsIDOMBaseAppCore;

////////////////////////////////////////////////////////////////////////////////
// nsAppCoresManager:
////////////////////////////////////////////////////////////////////////////////
class nsAppCoresManager : public nsIScriptObjectOwner, public nsIDOMAppCoresManager
{
  public:

    nsAppCoresManager();
    virtual ~nsAppCoresManager();
       
    NS_DECL_ISUPPORTS

    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    SetScriptObject(void* aScriptObject);

    NS_IMETHOD    Startup();

    NS_IMETHOD    Shutdown();

    NS_IMETHOD    Add(nsIDOMBaseAppCore* aAppcore);

    NS_IMETHOD    Remove(nsIDOMBaseAppCore* aAppcore);

    NS_IMETHOD    Find(const nsString& aId, nsIDOMBaseAppCore** aReturn);

  private:
    void        *mScriptObject;
    static nsVoidArray  mList;

        
};

#endif // nsAppCoresManager_h___
