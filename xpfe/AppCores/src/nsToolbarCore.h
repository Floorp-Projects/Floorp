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
#ifndef nsToolbarCorePrivate_h___
#define nsToolbarCorePrivate_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"

#include "nsIDOMToolbarCore.h"
#include "nsBaseAppCore.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIDOMDocument;
class nsIDOMCharacterData;

////////////////////////////////////////////////////////////////////////////////
// nsToolbarCore:
////////////////////////////////////////////////////////////////////////////////

class nsToolbarCore : public nsBaseAppCore, 
                      public nsIDOMToolbarCore
{
  public:
  
    nsToolbarCore();
    virtual ~nsToolbarCore();
                 

    NS_DECL_ISUPPORTS
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 

    NS_IMETHOD    SetWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetStatus(const nsString& aMsg);

  private:

    nsIDOMWindow        *mWindow;
    nsIDOMCharacterData *mStatusText;
};

#endif // nsToolbarCore_h___