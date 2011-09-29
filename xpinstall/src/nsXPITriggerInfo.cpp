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
 *   Dave Townsend <dtownsend@oxymoronical.com>
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

#include "jscntxt.h"
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
#include "nsIX509Cert.h"

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

    bool hasCert;
    aPrincipal->GetHasCertificate(&hasCert);
    if (hasCert) {
        nsCOMPtr<nsISupports> certificate;
        aPrincipal->GetCertificate(getter_AddRefs(certificate));

        nsCOMPtr<nsIX509Cert> x509 = do_QueryInterface(certificate);
        if (x509) {
            x509->GetCommonName(mCertName);
            if (mCertName.Length() > 0)
                return;
        }

        nsCAutoString prettyName;
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
        delete item;
    }
    mItems.Clear();

    if ( mCx && !JSVAL_IS_NULL(mCbval) ) {
        JS_BeginRequest(mCx);
        JS_RemoveValueRoot(mCx, &mCbval );
        JS_EndRequest(mCx);
    }

    MOZ_COUNT_DTOR(nsXPITriggerInfo);
}

void nsXPITriggerInfo::SaveCallback( JSContext *aCx, jsval aVal )
{
    NS_ASSERTION( mCx == 0, "callback set twice, memory leak" );
    // We'll only retain the callback if we can get a strong reference to the
    // context.
    if (!(JS_GetOptions(aCx) & JSOPTION_PRIVATE_IS_NSISUPPORTS))
        return;
    mContextWrapper = static_cast<nsISupports *>(JS_GetContextPrivate(aCx));
    if (!mContextWrapper)
        return;

    mCx = aCx;
    mCbval = aVal;
    mThread = do_GetCurrentThread();

    if ( !JSVAL_IS_NULL(mCbval) ) {
        JS_BeginRequest(mCx);
        JS_AddValueRoot(mCx, &mCbval );
        JS_EndRequest(mCx);
    }
}

XPITriggerEvent::~XPITriggerEvent()
{
    JS_BeginRequest(cx);
    JS_RemoveValueRoot(cx, &cbval);
    JS_EndRequest(cx);
}

NS_IMETHODIMP
XPITriggerEvent::Run()
{
    JSAutoRequest ar(cx);

    // If Components doesn't exist in the global object then XPConnect has
    // been torn down, probably because the page was closed. Bail out if that
    // is the case.
    JSObject* innerGlobal = JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(cbval));
    jsval components;
    if (!JS_LookupProperty(cx, innerGlobal, "Components", &components) ||
        !JSVAL_IS_OBJECT(components))
        return 0;

    // Build arguments into rooted jsval array
    jsval args[2] = { JSVAL_NULL, JSVAL_NULL };
    js::AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(args), args);

    // args[0] is the URL
    JSString *str = JS_NewUCStringCopyZ(cx, reinterpret_cast<const jschar*>(URL.get()));
    if (!str)
        return 0;
    args[0] = STRING_TO_JSVAL(str);

    // args[1] is the status
    if (!JS_NewNumberValue(cx, status, &args[1]))
        return 0;

    class StackPushGuard {
        nsCOMPtr<nsIJSContextStack> mStack;
    public:
        StackPushGuard(JSContext *cx)
          : mStack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"))
        {
            if (mStack)
                mStack->Push(cx);
        }

        ~StackPushGuard()
        {
            if (mStack)
                mStack->Pop(nsnull);
        }
    } stackPushGuard(cx);

    nsCOMPtr<nsIScriptSecurityManager> secman =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!secman)
    {
        JS_ReportError(cx, "Could not get script security manager service");
        return 0;
    }

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = secman->GetSubjectPrincipal(getter_AddRefs(principal));

    if (NS_FAILED(rv) || !principal)
    {
         JS_ReportError(cx, "Could not get principal from script security manager");
         return 0;
    }

    bool equals = false;
    principal->Equals(princ, &equals);
    if (!equals)
    {
        JS_ReportError(cx, "Principal of callback context is different than InstallTriggers");
        return 0;
    }

    jsval ret;
    JS_CallFunctionValue(cx,
                         JS_GetGlobalObject(cx),
                         cbval,
                         2,
                         args,
                         &ret);
    return 0;
}


void nsXPITriggerInfo::SendStatus(const PRUnichar* URL, PRInt32 status)
{
    nsresult rv;

    if ( mCx && mContextWrapper && !JSVAL_IS_NULL(mCbval) )
    {
        // create event and post it
        nsRefPtr<XPITriggerEvent> event = new XPITriggerEvent();
        if (event)
        {
            event->URL      = URL;
            event->status   = status;
            event->cx       = mCx;
            event->princ    = mPrincipal;

            event->cbval    = mCbval;
            JS_BeginRequest(event->cx);
            JS_AddNamedValueRoot(event->cx, &event->cbval,
                            "XPITriggerEvent::cbval" );
            JS_EndRequest(event->cx);

            // Hold a strong reference to keep the underlying
            // JSContext from dying before we handle this event.
            event->ref      = mContextWrapper;

            rv = mThread->Dispatch(event, NS_DISPATCH_NORMAL);
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;

        if ( NS_FAILED( rv ) )
        {
            // couldn't get event queue -- maybe window is gone or
            // some similarly catastrophic occurrence
            NS_WARNING("failed to dispatch XPITriggerEvent");
        }
    }
}
