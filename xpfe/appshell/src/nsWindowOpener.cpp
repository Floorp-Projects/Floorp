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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 */

#include "nsWindowOpener.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsIDOMWindowInternal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"

#include "jsapi.h"
#include "jsinterp.h"

enum windowArgumentType {
    eString,
    eCString,
    eInteger,
    eBoolean,
    eObject
};

nsWindowOpener::nsWindowOpener()
{
    NS_INIT_ISUPPORTS();
}

nsWindowOpener::~nsWindowOpener()
{
    // need to destroy mArguments based on the types in
    // mArgumentTypes
    PRInt32 i = mArguments.Count();

    
    while (i > 0) {
        i--;
        void *val = mArguments[i];

        windowArgumentType type = (windowArgumentType)mArgumentTypes[i];
        switch (type) {
        case eString:
            nsCRT::free((PRUnichar *)val);
            break;

        case eCString:
            nsCRT::free((char *)val);
            break;

        case eObject:
            {
                nsISupports* supports = (nsISupports*)val;
                NS_RELEASE(supports);
            }
            break;

        case eInteger:
        case eBoolean:
            // nothing to do
            break;
        }

        mArguments.RemoveElementAt(i);
        mArgumentTypes.RemoveElementAt(i);
        
    }
}

NS_IMPL_ISUPPORTS1(nsWindowOpener, nsIWindowOpener)

NS_IMETHODIMP
nsWindowOpener::PushString(const PRUnichar * aValue)
{
    mArguments.AppendElement((void *)nsCRT::strdup(aValue));
    mArgumentTypes.AppendElement((void *)eString);
    return NS_OK;
}

NS_IMETHODIMP
nsWindowOpener::PushCharString(const char * aValue)
{
    mArguments.AppendElement((void *)nsCRT::strdup(aValue));
    mArgumentTypes.AppendElement((void *)eCString);
    return NS_OK;
}

NS_IMETHODIMP
nsWindowOpener::PushInteger(PRInt32 aValue)
{
    mArguments.AppendElement((void *)aValue);
    mArgumentTypes.AppendElement((void *)eInteger);
    return NS_OK;
}

NS_IMETHODIMP
nsWindowOpener::PushObject(nsISupports *aValue, const nsIID& aIID)
{
    void *object;
    aValue->QueryInterface(aIID, &object);
    mArguments.AppendElement(object);
    mArgumentTypes.AppendElement((void *)eObject);
    return NS_OK;
}

NS_IMETHODIMP
nsWindowOpener::OpenDialog(nsIDOMWindowInternal *aWindow,
                           const PRUnichar* aChromeURL,
                           const PRUnichar* aFlags,
                           const PRUnichar* aTargetWindow)
{
    nsresult rv;
    nsCOMPtr<nsIScriptGlobalObject>
        scriptGlobal(do_QueryInterface(aWindow, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptContext> scriptContext;
    rv = scriptGlobal->GetContext(getter_AddRefs(scriptContext));
    NS_ENSURE_SUCCESS(rv, rv);

    JSContext *cx = (JSContext *)scriptContext->GetNativeContext();

    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
    

    // insert the chrome/flags/targetwindows at the
    // front of the argument arrays

    mArguments.InsertElementAt((void *)nsCRT::strdup(aChromeURL), 0);
    mArgumentTypes.InsertElementAt((void *)eString, 0);

    mArguments.InsertElementAt((void *)nsCRT::strdup(aFlags), 1);
    mArgumentTypes.InsertElementAt((void *)eString, 1);

    mArguments.InsertElementAt((void *)nsCRT::strdup(aTargetWindow), 2);
    mArgumentTypes.InsertElementAt((void *)eString, 2);

    
    void* mark;
    jsval* argv;

    argv = createArguments(cx, &mark);
    
    nsCOMPtr<nsIDOMWindowInternal> newWindow;
    aWindow->OpenDialog(cx, argv, 3, getter_AddRefs(newWindow));

    releaseArguments(cx, mark);

    return NS_OK;
}

jsval *
nsWindowOpener::createArguments(JSContext *cx, void **mark)
{
    PRInt32 totalArgs = mArguments.Count() + 3;

    jsval* sp;

    jsval* argv;
    
    argv = sp = js_AllocStack(cx, totalArgs, mark);
    
    if (!sp) return nsnull;

    // now push everything onto the JS stack

    PRInt32 length = mArguments.Count();
    PRInt32 i;
    
    for (i=0; i<length; i++) {
        void *val = mArguments.ElementAt(i);
        windowArgumentType type = (windowArgumentType)mArgumentTypes.ElementAt(i);
        // now just need to add us to the list

        JSString *str;
        
        switch(type) {
        case eString:
            str = JS_NewUCStringCopyZ(cx, (PRUnichar *)val);
            *sp = STRING_TO_JSVAL(str);
            break;
            
        case eCString:
            str = JS_NewStringCopyZ(cx, (char *)val);
            *sp = STRING_TO_JSVAL(str);
            break;

        case eObject:
            {
                NS_ASSERTION(0, "Don't support XPC objects yet!\n");
            }
            break;

        case eInteger:
            *sp = INT_TO_JSVAL(PRInt32(val));
            break;

        case eBoolean:
            *sp = (PRBool(val) ? JSVAL_TRUE : JSVAL_FALSE);
        
        }
        sp++;
    }
    
    return argv;
}
