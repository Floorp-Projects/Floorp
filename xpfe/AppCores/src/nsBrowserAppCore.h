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
#ifndef nsBrowserAppCore_h___
#define nsBrowserAppCore_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIDOMBrowserAppCore.h"
#include "nsBaseAppCore.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;

////////////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserAppCore : public nsBaseAppCore, 
                         public nsIDOMBrowserAppCore
{
  public:

    nsBrowserAppCore();
    virtual ~nsBrowserAppCore();
                 

    NS_DECL_ISUPPORTS
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 

    NS_IMETHOD    Back();
    NS_IMETHOD    Forward();
    NS_IMETHOD    LoadUrl(const nsString& aUrl);
    NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    DisableCallback(const nsString& aScript);
    NS_IMETHOD    EnableCallback(const nsString& aScript);

  protected:
    NS_IMETHOD ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);

    nsString            mEnableScript;     
    nsString            mDisableScript;     

    nsIScriptContext   *mToolbarScriptContext;
    nsIScriptContext   *mContentScriptContext;

    nsIDOMWindow       *mToolbarWindow;
    nsIDOMWindow       *mContentWindow;
};

#endif // nsBrowserAppCore_h___