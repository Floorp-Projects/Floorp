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
#ifndef nsBaseAppCore_h___
#define nsBaseAppCore_h___

#include "nsAppCores.h"

//#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMBaseAppCore.h"
#include "nsCOMPtr.h"

class nsIDOMNode;
class nsIDOMDocument;
class nsIScriptContext;
class nsIDOMWindow;

////////////////////////////////////////////////////////////////////////////////
// nsBaseAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsBaseAppCore : public nsIScriptObjectOwner, public nsIDOMBaseAppCore
{
  public:

    nsBaseAppCore();
    virtual ~nsBaseAppCore();
                 

    NS_DECL_ISUPPORTS
    //NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    SetScriptObject(void* aScriptObject);

    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId);

  protected:
    nsCOMPtr<nsIDOMNode>     FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount);
    nsCOMPtr<nsIDOMNode>     GetParentNodeFromDOMDoc(nsIDOMDocument * aDOMDoc);
    nsIScriptContext *       GetScriptContext(nsIDOMWindow * aWin);

    nsString  mId;         /* User ID */
    void     *mScriptObject;
};

#endif // nsBaseAppCore_h___