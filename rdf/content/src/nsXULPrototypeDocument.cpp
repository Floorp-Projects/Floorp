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
 * Contributor(s): 
 */

/*



*/

#include "nsCOMPtr.h"
#include "nsString2.h"
#include "nsVoidArray.h"
#include "nsIPrincipal.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXULPrototypeDocument.h"
#include "nsXULElement.h"
#include "nsIJSRuntimeService.h"

class nsXULPrototypeDocument : public nsIXULPrototypeDocument,
                               public nsIScriptObjectOwner
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

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsString& aData);

    NS_IMETHOD GetDocumentPrincipal(nsIPrincipal** aResult);
    NS_IMETHOD SetDocumentPrincipal(nsIPrincipal* aPrincipal);

    // nsIScriptObjectOwner methods
    NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void **aObject);
    NS_IMETHOD SetScriptObject(void *aObject);

protected:
    nsCOMPtr<nsIURI> mURI;
    nsXULPrototypeElement* mRoot;
    nsCOMPtr<nsISupportsArray> mStyleSheetReferences;
    nsCOMPtr<nsISupportsArray> mOverlayReferences;
    nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
    JSObject *mScriptObject;    // XXX JS language rabies bigotry badness

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);
};


//----------------------------------------------------------------------

nsXULPrototypeDocument::nsXULPrototypeDocument()
    : mRoot(nsnull), mScriptObject(nsnull)
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
    delete mRoot;

    nsresult rv;
    NS_WITH_SERVICE(nsIJSRuntimeService, rtsvc, "nsJSRuntimeService", &rv);
    if (NS_SUCCEEDED(rv)) {
        JSRuntime *rt;
        rv = rtsvc->GetRuntime(&rt);
        if (NS_SUCCEEDED(rv) && rt)
            JS_RemoveRootRT(rt, &mScriptObject);
    }
}


NS_IMPL_ISUPPORTS2(nsXULPrototypeDocument, nsIXULPrototypeDocument, nsIScriptObjectOwner);

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
    mURI = aURI;
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
nsXULPrototypeDocument::GetHeaderData(nsIAtom* aField, nsString& aData) const
{
    // XXX Not implemented
    aData.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetHeaderData(nsIAtom* aField, const nsString& aData)
{
    // XXX Not implemented
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::GetDocumentPrincipal(nsIPrincipal** aResult)
{
    if (!mDocumentPrincipal) {
        nsresult rv;
        NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager,
                        NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
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

JSClass null_class = {
    "nsXULPrototypeScript compilation scope", 0,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  JS_FinalizeStub
};

//----------------------------------------------------------------------

NS_IMETHODIMP
nsXULPrototypeDocument::GetScriptObject(nsIScriptContext *aContext, void **aObject)
{
    // The prototype document will have its own special secret script
    // object that can be used to compile scripts and event handlers.
    if (!mScriptObject) {
        JSContext *cx = (JSContext *)aContext->GetNativeContext();
        if (!cx)
            return NS_ERROR_UNEXPECTED;

        mScriptObject = JS_NewObject(cx, &null_class, nsnull, nsnull);
        if (!mScriptObject)
            return NS_ERROR_OUT_OF_MEMORY;

        // Be sure to unlink the script object from the parent global
        // object. This ensures that we don't end up with a circular
        // reference back to the first document.
        JS_SetPrototype(cx, mScriptObject, nsnull);
        JS_SetParent(cx, mScriptObject, nsnull);

        JS_AddNamedRoot(cx, &mScriptObject, "nsXULPrototypeDocument::mScriptObject");
    }
    *aObject = mScriptObject;
    return NS_OK;
}

NS_IMETHODIMP
nsXULPrototypeDocument::SetScriptObject(void *aObject)
{
    mScriptObject = (JSObject *)aObject;
    return NS_OK;
}
