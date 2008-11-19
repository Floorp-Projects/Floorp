/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsMemory.h"
#include "nsString.h"

#include "mozStorageStatementParams.h"

#include "sqlite3.h"

/*************************************************************************
 ****
 **** mozStorageStatementParams
 ****
 *************************************************************************/

NS_IMPL_ISUPPORTS2(mozStorageStatementParams, mozIStorageStatementParams, nsIXPCScriptable)

mozStorageStatementParams::mozStorageStatementParams(mozIStorageStatement *aStatement)
    : mStatement(aStatement)
{
    NS_ASSERTION(mStatement != nsnull, "mStatement is null");
    mStatement->GetParameterCount(&mParamCount);
}

/*
 * nsIXPCScriptable impl
 */

/* readonly attribute string className; */
NS_IMETHODIMP
mozStorageStatementParams::GetClassName(char * *aClassName)
{
    NS_ENSURE_ARG_POINTER(aClassName);
    *aClassName = (char *) nsMemory::Clone("mozStorageStatementParams", 26);
    if (!*aClassName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute PRUint32 scriptableFlags; */
NS_IMETHODIMP
mozStorageStatementParams::GetScriptableFlags(PRUint32 *aScriptableFlags)
{
    *aScriptableFlags =
        nsIXPCScriptable::WANT_SETPROPERTY |
        nsIXPCScriptable::WANT_NEWRESOLVE |
        nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE;
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

    if (JSVAL_IS_INT(id)) {
        int idx = JSVAL_TO_INT(id);

        *_retval = JSValStorageStatementBinder (cx, mStatement, idx, *vp);
    } else if (JSVAL_IS_STRING(id)) {
        sqlite3_stmt *stmt = mStatement->GetNativeStatementPointer();

        JSString *str = JSVAL_TO_STRING(id);
        nsCAutoString name(":");
        name.Append(NS_ConvertUTF16toUTF8(nsDependentString((PRUnichar *)::JS_GetStringChars(str),
                                                            ::JS_GetStringLength(str))));

        // check to see if there's a parameter with this name
        if (sqlite3_bind_parameter_index(stmt, name.get()) == 0) {
            *_retval = PR_FALSE;
            return NS_ERROR_INVALID_ARG;
        }
        
        *_retval = PR_TRUE;
        // You can use a named parameter more than once in a statement...
        int count = sqlite3_bind_parameter_count(stmt);
        for (int i = 0; (i < count) && (*_retval); i++) {
            // sqlite indices start at 1
            const char *pName = sqlite3_bind_parameter_name(stmt, i + 1);
            if (name.Equals(pName))
                *_retval = JSValStorageStatementBinder(cx, mStatement, i, *vp);
        }
    } else {
        *_retval = PR_FALSE;
    }

    return (*_retval) ? NS_OK : NS_ERROR_INVALID_ARG;
}

/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP
mozStorageStatementParams::PreCreate(nsISupports *nativeObj, JSContext * cx,
                       JSObject * globalObj, JSObject * *parentObj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::Create(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool addProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool delProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                  JSObject * obj, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
mozStorageStatementParams::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                     JSObject * obj, PRUint32 enum_op, jsval * statep, jsid *idp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
mozStorageStatementParams::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                   JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp, PRBool *_retval)
{
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

    int idx = -1;

    if (JSVAL_IS_INT(id)) {
        idx = JSVAL_TO_INT(id);
    } else if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        nsCAutoString name(":");
        name.Append(NS_ConvertUTF16toUTF8(nsDependentString((PRUnichar *)::JS_GetStringChars(str),
                                                            ::JS_GetStringLength(str))));

        // check to see if there's a parameter with this name
        idx = sqlite3_bind_parameter_index(mStatement->GetNativeStatementPointer(), name.get());
        if (idx == 0) {
            // nope.
            fprintf (stderr, "********** mozStorageStatementWrapper: Couldn't find parameter %s\n", name.get());
            *_retval = PR_FALSE;
            return NS_OK;
        } else {
            // set idx, so that the numbered property also gets defined
            idx = idx - 1;
        }

        PRBool success = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                               ::JS_GetStringLength(str),
                                               JSVAL_VOID,
                                               nsnull, nsnull, 0);
        if (!success) {
            *_retval = PR_FALSE;
            return NS_ERROR_FAILURE;
        }
    }

    if (idx == -1) {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    // is it out of range?
    if (idx < 0 || idx >= (int)mParamCount) {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    *_retval = ::JS_DefineElement(cx, obj, idx, JSVAL_VOID, nsnull, nsnull, 0);
    if (*_retval)
        *objp = obj;
    return NS_OK;
}

/* PRBool convert (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 type, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::Convert(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                JSObject * obj, PRUint32 type, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void finalize (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool checkAccess (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 mode, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, PRUint32 mode, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                             JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementParams::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                  JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
mozStorageStatementParams::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval val, PRBool *bp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void trace (in nsIXPConnectWrappedNative wrapper, in JSTracerPtr trc, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::Trace(nsIXPConnectWrappedNative *wrapper,
                                JSTracer *trc, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool equality(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val); */
NS_IMETHODIMP
mozStorageStatementParams::Equality(nsIXPConnectWrappedNative *wrapper,
                                    JSContext *cx, JSObject *obj, jsval val,
                                    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr outerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::OuterObject(nsIXPConnectWrappedNative *wrapper,
                                       JSContext *cx, JSObject *obj,
                                       JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr innerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementParams::InnerObject(nsIXPConnectWrappedNative *wrapper,
                                       JSContext *cx, JSObject *obj,
                                       JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreatePrototype (in JSContextPtr cx, in JSObjectPtr proto); */
NS_IMETHODIMP
mozStorageStatementParams::PostCreatePrototype(JSContext * cx, JSObject * proto)
{
    return NS_OK;
}
