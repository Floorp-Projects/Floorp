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
#ifndef nsDOMPropsCorePrivate_h___
#define nsDOMPropsCorePrivate_h___

//#include "nsAppCores.h"

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIDOMDOMPropsCore.h"
#include "nsBaseAppCore.h"


class nsIDOMWindow;
class nsIScriptContext;

// Cannot forward declare a class used with an nsCOMPtr.
// see: http://www.mozilla.org/projects/xpcom/nsCOMPtr.html
//class nsIWebShellWindow;

#include "nsIWebShellWindow.h"

////////////////////////////////////////////////////////////////////////////////
// nsToolbarCore:
////////////////////////////////////////////////////////////////////////////////

class nsDOMPropsCore : public nsBaseAppCore, 
                       public nsIDOMDOMPropsCore {

  public:
  
    nsDOMPropsCore();
    virtual ~nsDOMPropsCore();
                 

    NS_DECL_ISUPPORTS
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset)  { return nsBaseAppCore::SetDocumentCharset(aCharset); } 

    NS_DECL_IDOMDOMPROPSCORE
#if 0
    NS_IMETHOD    GetNode(nsIDOMNode& aNode);
    NS_IMETHOD    SetNode(const nsIDOMNode& aNode);
    NS_IMETHOD    ShowProperties(const nsString& aUrl, nsIDOMWindow* aParent, nsIDOMNode* aNode);
	  NS_IMETHOD    Commit();
	  NS_IMETHOD    Cancel();
#endif /*0*/

  private:

    nsCOMPtr<nsIWebShellWindow>
                  DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow) const;
};

#endif // nsDOMPropsCorePrivate_h___
