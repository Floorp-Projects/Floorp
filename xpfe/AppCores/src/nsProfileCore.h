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

#ifndef nsProfileCorePrivate_h___

#define nsProfileCorePrivate_h___



//#include "nsAppCores.h"



#include "nscore.h"

#include "nsISupports.h"



#include "nsIDOMProfileCore.h"

#include "nsBaseAppCore.h"

#include "prtypes.h"



class nsIBrowserWindow;

class nsIWebShell;

class nsIScriptContext;

class nsIDOMWindow;

class nsIProfile;

class nsIDOMNode;

class nsIDOMHTMLInputElement;

class nsString;

class nsIDOMnsStringArray;



//========================================================================================

class nsProfileCore

//========================================================================================

  : public nsBaseAppCore 

  , public nsIDOMProfileCore

{

  public:



    nsProfileCore();

    virtual ~nsProfileCore();

                 



    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD           GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

    NS_IMETHOD           Init(const nsString& aId);

    NS_IMETHOD           GetId(nsString& aId)

                         {

                             return nsBaseAppCore::GetId(aId);

                         } 

    NS_IMETHOD           SetDocumentCharset(const nsString& aCharset)

                         {

                             return nsBaseAppCore::SetDocumentCharset(aCharset);

                         } 

	NS_IMETHOD			 CreateNewProfile(const nsString& aData);



  protected:

    

    nsresult             InitializeProfileManager();





  protected:



    nsIProfile*          mProfile;

};



#endif // nsProfileCore_h___

