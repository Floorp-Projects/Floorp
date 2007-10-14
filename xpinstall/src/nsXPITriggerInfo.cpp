/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "plstr.h"
#include "nsXPITriggerInfo.h"
#include "nsNetUtil.h"
#include "nsDebug.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsICryptoHash.h"

//
// nsXPITriggerItem
//

nsXPITriggerItem::nsXPITriggerItem( const PRUnichar* aName,
                                    const PRUnichar* aURL,
                                    const PRUnichar* aIconURL,
                                    const char* aHash,
                                    PRInt32 aFlags)
    : mName(aName), mURL(aURL), mIconURL(aIconURL), mHashFound(PR_FALSE), mFlags(aFlags)
{
    MOZ_COUNT_CTOR(nsXPITriggerItem);

    // check for arguments
    PRInt32 qmark = mURL.FindChar('?');
    if ( qmark != kNotFound )
    {
        mArguments = Substring( mURL, qmark+1, mURL.Length() );
    }

    // construct name if not passed in
    if ( mName.IsEmpty() )
    {
        // Use the filename as the display name by starting after the last
        // slash in the URL, looking backwards from the arguments delimiter if
        // we found one. By good fortune using kNotFound as the offset for
        // RFindChar() starts at the end, so we can use qmark in all cases.

        PRInt32 namestart = mURL.RFindChar( '/', qmark );

        // the real start is after the slash (or 0 if not found)
        namestart = ( namestart==kNotFound ) ? 0 : namestart + 1;

        PRInt32 length;
        if (qmark == kNotFound)
            length =  mURL.Length();      // no '?', slurp up rest of URL
        else
            length = (qmark - namestart); // filename stops at the '?'

        mName = Substring( mURL, namestart, length );
    }

    // parse optional hash into its parts
    if (aHash)
    {
        mHashFound = PR_TRUE;

        char * colon = PL_strchr(aHash, ':');
        if (colon)
        {
            mHasher = do_CreateInstance("@mozilla.org/security/hash;1");
            if (!mHasher) return;
            
            *colon = '\0'; // null the colon so that aHash is just the type.
            nsresult rv = mHasher->InitWithString(nsDependentCString(aHash));
            *colon = ':';  // restore the colon

            if (NS_SUCCEEDED(rv))
                mHash = colon+1;
        }
    }
}

nsXPITriggerItem::~nsXPITriggerItem()
{
    MOZ_COUNT_DTOR(nsXPITriggerItem);
}

PRBool nsXPITriggerItem::IsRelativeURL()
{
    PRInt32 cpos = mURL.FindChar(':');
    if (cpos == kNotFound)
        return PR_TRUE;

    PRInt32 spos = mURL.FindChar('/');
    return (cpos > spos);
}

const PRUnichar*
nsXPITriggerItem::GetSafeURLString()
{
    // create the safe url string the first time
    if (mSafeURL.IsEmpty() && !mURL.IsEmpty())
    {
        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), mURL);
        if (uri)
        {
            nsCAutoString spec;
            uri->SetUserPass(EmptyCString());
            uri->GetSpec(spec);
            mSafeURL = NS_ConvertUTF8toUTF16(spec);
        }
    }

    return mSafeURL.get();
}

void
nsXPITriggerItem::SetPrincipal(nsIPrincipal* aPrincipal)
{
    mPrincipal = aPrincipal;

    // aPrincipal can be null for various failure cases.
    // see bug 213894 for an example.
    // nsXPInstallManager::OnCertAvailable can be called with a null principal
    // and it can also force a null principal.
    if (!aPrincipal)
        return;

    PRBool hasCert;
    aPrincipal->GetHasCertificate(&hasCert);
    if (hasCert) {
        nsCAutoString prettyName;
        // XXXbz should this really be using the prettyName?  Perhaps
        // it wants to get the subjectName or nsIX509Cert and display
        // it sanely?
        aPrincipal->GetPrettyName(prettyName);
        CopyUTF8toUTF16(prettyName, mCertName);
    }
}

//
// nsXPITriggerInfo
//

nsXPITriggerInfo::nsXPITriggerInfo()
  : mCx(0), mCbval(JSVAL_NULL)
{
    MOZ_COUNT_CTOR(nsXPITriggerInfo);
}

nsXPITriggerInfo::~nsXPITriggerInfo()
{
    nsXPITriggerItem* item;

    for(PRUint32 i=0; i < Size(); i++)
    {
        item = Get(i);
        if (item)
            delete item;
    }
    mItems.Clear();

    if ( mCx && !JSVAL_IS_NULL(mCbval) ) {
        JS_BeginRequest(mCx);
        JS_RemoveRoot( mCx, &mCbval );
        JS_EndRequest(mCx);
    }

    MOZ_COUNT_DTOR(nsXPITriggerInfo);
}

void nsXPITriggerInfo::SaveCallback( JSContext *aCx, jsval aVal )
{
    NS_ASSERTION( mCx == 0, "callback set twice, memory leak" );
    mCx = aCx;
    JSObject *obj = JS_GetGlobalObject( mCx );

    JSClass* clazz;

#ifdef JS_THREADSAFE
    clazz = ::JS_GetClass(aCx, obj);
#else
    clazz = ::JS_GetClass(obj);
#endif

    if (clazz &&
        (clazz->flags & JSCLASS_HAS_PRIVATE) &&
        (clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
      mGlobalWrapper =
        do_QueryInterface((nsISupports*)::JS_GetPrivate(aCx, obj));
    }

    mCbval = aVal;
    mThread = do_GetCurrentThread();

    if ( !JSVAL_IS_NULL(mCbval) ) {
        JS_BeginRequest(mCx);
        JS_AddRoot( mCx, &mCbval );
        JS_EndRequest(mCx);
    }
}

XPITriggerEvent::~XPITriggerEvent()
{
    JS_BeginRequest(cx);
    JS_RemoveRoot(cx, &cbval);
    JS_EndRequest(cx);
}

NS_IMETHODIMP
XPITriggerEvent::Run()
{
    jsval  ret;
    void*  mark;
    jsval* args;

    JS_BeginRequest(cx);
    args = JS_PushArguments(cx, &mark, "Wi",
                            URL.get(),
                            status);
    if ( args )
    {
        // This code is all in a JS request, and here we're about to
        // push the context onto the context stack and also push
        // arguments. Be very very sure that no early returns creep in
        // here w/o doing the proper cleanup!

        const char *errorStr = nsnull;

        nsCOMPtr<nsIJSContextStack> stack =
            do_GetService("@mozilla.org/js/xpc/ContextStack;1");
        if (stack)
            stack->Push(cx);
        
        nsCOMPtr<nsIScriptSecurityManager> secman = 
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        
        if (!secman)
        {
            errorStr = "Could not get script security manager service";
        }

        nsCOMPtr<nsIPrincipal> principal;
        if (!errorStr)
        {
            secman->GetSubjectPrincipal(getter_AddRefs(principal));
            if (!principal)
            {
                errorStr = "Could not get principal from script security manager";
            }
        }

        if (!errorStr)
        {
            PRBool equals = PR_FALSE;
            principal->Equals(princ, &equals);

            if (!equals)
            {
                errorStr = "Principal of callback context is different than InstallTriggers";
            }
        }

        if (errorStr)
        {
            JS_ReportError(cx, errorStr);
        }
        else
        {
            JS_CallFunctionValue(cx,
                                 JSVAL_TO_OBJECT(global),
                                 cbval,
                                 2,
                                 args,
                                 &ret);
        }

        if (stack)
            stack->Pop(nsnull);

        JS_PopArguments(cx, mark);
    }
    JS_EndRequest(cx);

    return 0;
}


void nsXPITriggerInfo::SendStatus(const PRUnichar* URL, PRInt32 status)
{
    nsresult rv;

    if ( mCx && mGlobalWrapper && !JSVAL_IS_NULL(mCbval) )
    {
        // create event and post it
        nsRefPtr<XPITriggerEvent> event = new XPITriggerEvent();
        if (event)
        {
            event->URL      = URL;
            event->status   = status;
            event->cx       = mCx;
            event->princ    = mPrincipal;

            JSObject *obj = nsnull;

            mGlobalWrapper->GetJSObject(&obj);

            event->global   = OBJECT_TO_JSVAL(obj);

            event->cbval    = mCbval;
            JS_BeginRequest(event->cx);
            JS_AddNamedRoot(event->cx, &event->cbval,
                            "XPITriggerEvent::cbval" );
            JS_EndRequest(event->cx);

            // Hold a strong reference to keep the underlying
            // JSContext from dying before we handle this event.
            event->ref      = mGlobalWrapper;

            rv = mThread->Dispatch(event, NS_DISPATCH_NORMAL);
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;

        if ( NS_FAILED( rv ) )
        {
            // couldn't get event queue -- maybe window is gone or
            // some similarly catastrophic occurrance
            NS_WARNING("failed to dispatch XPITriggerEvent");
        }
    }
}
