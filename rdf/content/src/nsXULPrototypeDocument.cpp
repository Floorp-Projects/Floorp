/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *   L. David Baron <dbaron@fas.harvard.edu>
 */

/*

  A "prototype" document that stores shared document information
  for the XUL cache.

*/

#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsIXULPrototypeDocument.h"
#include "nsJSUtils.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXULElement.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

class nsXULPDGlobalObject : public nsIScriptObjectOwner,
                            public nsIScriptGlobalObject,
                            public nsIScriptObjectPrincipal
{
public:
    nsXULPDGlobalObject();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIScriptObjectOwner methods
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void **aObject);
    NS_IMETHOD SetScriptObject(void *aObject);

    // nsIScriptGlobalObject methods
    NS_IMETHOD SetContext(nsIScriptContext *aContext);
    NS_IMETHOD GetContext(nsIScriptContext **aContext);
    NS_IMETHOD SetNewDocument(nsIDOMDocument *aDocument);
    NS_IMETHOD SetDocShell(nsIDocShell *aDocShell);
    NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
    NS_IMETHOD SetOpenerWindow(nsIDOMWindowInternal *aOpener);
    NS_IMETHOD SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
    NS_IMETHOD GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner);
    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                              nsEvent* aEvent, 
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);

    // nsIScriptObjectPrincipal methods
    NS_IMETHOD GetPrincipal(nsIPrincipal** aPrincipal);
    
protected:
    virtual ~nsXULPDGlobalObject();

    nsCOMPtr<nsIScriptContext> mScriptContext;
    JSObject *mScriptObject;    // XXX JS language rabies bigotry badness

    nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference

    static JSClass gSharedGlobalClass;
};

class nsXULPrototypeDocument : public nsIXULPrototypeDocument,
                               public nsIScriptGlobalObjectOwner
{
public:
    static nsresult
    Create(nsIURI* aURI, nsXULPrototypeDocument** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIXULPrototypeDocument interface
    NS_IMETHOD GetURI(nsIURI** aResult);
    NS_IMETHOD SetURI(nsIURI* aURI);

    NS_IMETHOD GetRootElement(nsXULPrototypeElement** aResult);
    NS_IMETHOD SetRootElement(nsXULPrototypeElement* aElement);

    NS_IMETHOD AddStyleSheetReference(nsIURI* aStyleSheet);
    NS_IMETHOD GetStyleSheetReferences(nsISupportsArray** aResult);

    NS_IMETHOD AddOverlayReference(nsIURI* aURI);
    NS_IMETHOD GetOverlayReferences(nsISupportsArray** aResult);

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAWritableString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAReadableString& aData);

    NS_IMETHOD GetDocumentPrincipal(nsIPrincipal** aResult);
    NS_IMETHOD SetDocumentPrincipal(nsIPrincipal* aPrincipal);

    // nsIScriptGlobalObjectOwner methods
    NS_DECL_NSISCRIPTGLOBALOBJECTOWNER


protected:
    nsCOMPtr<nsIURI> mURI;
    nsXULPrototypeElement* mRoot;
    nsCOMPtr<nsISupportsArray> mStyleSheetReferences;
    nsCOMPtr<nsISupportsArray> mOverlayReferences;
    nsCOMPtr<nsIPrincipal> mDocumentPrincipal;

    nsCOMPtr<nsIScriptGlobalObject> mGlobalObject;

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);
};

JSClass nsXULPDGlobalObject::gSharedGlobalClass = {
    "nsXULPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  nsJSUtils::nsGenericFinalize
};



//----------------------------------------------------------------------
//
// ctors, dtors, n' stuff
//

nsXULPrototypeDocument::nsXULPrototypeDocument()
    : mRoot(nsnull),
      mGlobalObject(nsnull)
{
    NS_INIT_REFCNT();
}


nsresult
nsXULPrototypeDocument::Init()
{
    nsresult rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mStyleSheetReferences));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mOverlayReferences));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsXULPrototypeDocument::~nsXULPrototypeDocument()
{
    if (mGlobalObject) {
      mGlobalObject->SetContext(nsnull); // remove circular reference
      mGlobalObject->SetGlobalObjectOwner(nsnull); // just in case
    }
    delete mRoot;
}

NS_IMPL_ADDREF(nsXULPrototypeDocument)
NS_IMPL_RELEASE(nsXULPrototypeDocument)

NS_INTERFACE_MAP_BEGIN(nsXULPrototypeDocument)
    NS_INTERFACE_MAP_ENTRY(nsIXULPrototypeDocument)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULPrototypeDocument* result = new nsXULPrototypeDocument();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}


//----------------------------------------------------------------------
//
// nsIXULPrototypeDocument methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::GetURI(nsIURI** aResult)
{
    *aResult = mURI;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetURI(nsIURI* aURI)
{
    mURI = dont_QueryInterface(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetRootElement(nsXULPrototypeElement** aResult)
{
    *aResult = mRoot;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetRootElement(nsXULPrototypeElement* aElement)
{
    mRoot = aElement;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::AddStyleSheetReference(nsIURI* aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    mStyleSheetReferences->AppendElement(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetStyleSheetReferences(nsISupportsArray** aResult)
{
    *aResult = mStyleSheetReferences;
    NS_ADDREF(*aResult);
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::AddOverlayReference(nsIURI* aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    mOverlayReferences->AppendElement(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetOverlayReferences(nsISupportsArray** aResult)
{
    *aResult = mOverlayReferences;
    NS_ADDREF(*aResult);
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::GetHeaderData(nsIAtom* aField, nsAWritableString& aData) const
{
    // XXX Not implemented
    aData.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetHeaderData(nsIAtom* aField, const nsAReadableString& aData)
{
    // XXX Not implemented
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::GetDocumentPrincipal(nsIPrincipal** aResult)
{
    if (!mDocumentPrincipal) {
        nsresult rv;
        NS_WITH_SERVICE(nsIScriptSecurityManager,
                        securityManager,
                        NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                        &rv);

        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

        rv = securityManager->GetCodebasePrincipal(mURI, getter_AddRefs(mDocumentPrincipal));

        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
    }

    *aResult = mDocumentPrincipal;
    NS_ADDREF(*aResult);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetDocumentPrincipal(nsIPrincipal* aPrincipal)
{
    mDocumentPrincipal = aPrincipal;
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObjectOwner methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::GetScriptGlobalObject(nsIScriptGlobalObject** _result)
{
    if (!mGlobalObject) {
        mGlobalObject = new nsXULPDGlobalObject();
        if (!mGlobalObject) {
            *_result = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
            }
        mGlobalObject->SetGlobalObjectOwner(this); // does not refcount
    }
    *_result = mGlobalObject;
    NS_ADDREF(*_result);
    return NS_OK;
}

NS_IMETHODIMP
nsXULPrototypeDocument::ReportScriptError(nsIScriptError *errorObject)
{
   nsresult rv;

   if (errorObject == nsnull)
      return NS_ERROR_NULL_POINTER;

   // Get the console service, where we're going to register the error.
   nsCOMPtr<nsIConsoleService> consoleService
      (do_GetService("mozilla.consoleservice.1"));

   if (consoleService != nsnull) {
       rv = consoleService->LogMessage(errorObject);
       if (NS_SUCCEEDED(rv)) {
           return NS_OK;
       } else {
           return rv;
       }
   } else {
       return NS_ERROR_NOT_AVAILABLE;
   }
}

//----------------------------------------------------------------------
//
// nsXULPDGlobalObject
//

nsXULPDGlobalObject::nsXULPDGlobalObject()
    : mScriptObject(nsnull),
      mGlobalObjectOwner(nsnull)
{
    NS_INIT_REFCNT();
}


nsXULPDGlobalObject::~nsXULPDGlobalObject()
{
}

NS_IMPL_ADDREF(nsXULPDGlobalObject)
NS_IMPL_RELEASE(nsXULPDGlobalObject)

NS_INTERFACE_MAP_BEGIN(nsXULPDGlobalObject)
    NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
    NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIScriptObjectOwner methods
//

NS_IMETHODIMP
nsXULPDGlobalObject::GetScriptObject(nsIScriptContext *aContext, void **aObject)
{
    // The prototype document has its own special secret script object
    // that can be used to compile scripts and event handlers.
    nsresult rv;

    nsCOMPtr<nsIScriptContext> context;

    if (mScriptContext && aContext != mScriptContext.get()) {
        rv = GetContext(getter_AddRefs(context));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        context = aContext;
    }

    if (! mScriptObject) {
        JSContext* cx = NS_REINTERPRET_CAST(JSContext*, context->GetNativeContext());
        if (! cx)
            return NS_ERROR_OUT_OF_MEMORY;

        mScriptObject = JS_NewObject(cx, &gSharedGlobalClass, nsnull, nsnull);
        if (! mScriptObject)
            return NS_ERROR_OUT_OF_MEMORY;

        // Add an owning reference from JS back to us. This'll be
        // released when the JSObject is finalized.
        ::JS_SetPrivate(cx, mScriptObject, this);
        NS_ADDREF(this);
    }

    *aObject = mScriptObject;
    return NS_OK;
}

NS_IMETHODIMP
nsXULPDGlobalObject::SetScriptObject(void *aObject)
{
    mScriptObject = (JSObject *)aObject;
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObject methods
//

NS_IMETHODIMP
nsXULPDGlobalObject::SetContext(nsIScriptContext *aContext)
{
    mScriptContext = aContext;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetContext(nsIScriptContext **aContext)
{
    // This whole fragile mess is predicated on the fact that
    // GetContext() will be called before GetScriptObject() is.
    if (! mScriptContext) {
        nsresult rv;
        rv = NS_CreateScriptContext(this, getter_AddRefs(mScriptContext));
        if (NS_FAILED(rv)) return rv;
    }

    *aContext = mScriptContext;
    NS_IF_ADDREF(*aContext);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetNewDocument(nsIDOMDocument *aDocument)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetDocShell(nsIDocShell *aDocShell)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetDocShell(nsIDocShell **aDocShell)
{
    NS_WARNING("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetOpenerWindow(nsIDOMWindowInternal *aOpener)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
    mGlobalObjectOwner = aOwner; // weak reference
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner)
{
    *aOwner = mGlobalObjectOwner;
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::HandleDOMEvent(nsIPresContext* aPresContext, 
                                       nsEvent* aEvent, 
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus* aEventStatus)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// nsIScriptObjectPrincipal methods
//

NS_IMETHODIMP
nsXULPDGlobalObject::GetPrincipal(nsIPrincipal** aPrincipal)
{
    if (!mGlobalObjectOwner) {
        *aPrincipal = nsnull;
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIXULPrototypeDocument> protoDoc
      = do_QueryInterface(mGlobalObjectOwner);
    return protoDoc->GetDocumentPrincipal(aPrincipal);
}

