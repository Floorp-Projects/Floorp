/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsToolkitCorePrivate_h___
#define nsToolkitCorePrivate_h___

//#include "nsAppCores.h"

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIDOMToolkitCore.h"
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

class nsToolkitCore : public nsBaseAppCore, 
                      public nsIDOMToolkitCore {

  public:
  
    nsToolkitCore();
    virtual ~nsToolkitCore();
                 

    NS_DECL_ISUPPORTS
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset)  { return nsBaseAppCore::SetDocumentCharset(aCharset); } 

    NS_DECL_IDOMTOOLKITCORE

  private:

    void          DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow,
                    nsCOMPtr<nsIWebShellWindow> *webWindow) const;
};

#endif // nsToolkitCorePrivate_h___
