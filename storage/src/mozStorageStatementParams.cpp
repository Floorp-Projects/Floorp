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
        nsIXPCScriptable::WANT_NEWENUMERATE |
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

        PRBool res = JSValStorageStatementBinder(cx, mStatement, idx, *vp);
        NS_ENSURE_TRUE(res, NS_ERROR_UNEXPECTED);
    }
    else if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        nsCAutoString name(":");
        name.Append(NS_ConvertUTF16toUTF8(::JS_GetStringChars(str),
                                          ::JS_GetStringLength(str)));

        // check to see if there's a parameter with this name
        PRUint32 index;
        nsresult rv = mStatement->GetParameterIndex(name, &index);
        NS_ENSURE_SUCCESS(rv, rv);

        PRBool res = JSValStorageStatementBinder(cx, mStatement, index, *vp);
        NS_ENSURE_TRUE(res, NS_ERROR_UNEXPECTED);
    }
    else {
        return NS_ERROR_INVALID_ARG;
    }

    *_retval = PR_TRUE;
    return NS_OK;
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
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

    switch (enum_op) {
        case JSENUMERATE_INIT:
        {
            // Start our internal index at zero.
            *statep = JSVAL_ZERO;

            // And set our length, if needed.
            if (idp)
                *idp = INT_TO_JSVAL(mParamCount);

            break;
        }
        case JSENUMERATE_NEXT:
        {
            NS_ASSERTION(*statep != JSVAL_NULL, "Internal state is null!");

            // Make sure we are in range first.
            PRUint32 index = static_cast<PRUint32>(JSVAL_TO_INT(*statep));
            if (index >= mParamCount) {
                *statep = JSVAL_NULL;
                return NS_OK;
            }

            // Get the name of our parameter.
            nsCAutoString name;
            nsresult rv = mStatement->GetParameterName(index, name);
            NS_ENSURE_SUCCESS(rv, rv);

            // But drop the first character, which is going to be a ':'.
            JSString *jsname = JS_NewStringCopyN(cx, &(name.get()[1]),
                                                 name.Length() - 1);
            NS_ENSURE_TRUE(jsname, NS_ERROR_OUT_OF_MEMORY);

            // Set our name.
            if (!JS_ValueToId(cx, STRING_TO_JSVAL(jsname), idp)) {
                *_retval = PR_FALSE;
                return NS_OK;
            }

            // And increment our index.
            *statep = INT_TO_JSVAL(++index);

            break;
        }
        case JSENUMERATE_DESTROY:
        {
            // Clear our state.
            *statep = JSVAL_NULL;

            break;
        }
    }

    return NS_OK;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
mozStorageStatementParams::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                   JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp, PRBool *_retval)
{
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
    // We do not throw at any point after this unless our index is out of range
    // because we want to allow the prototype chain to be checked for the
    // property.

    PRUint32 idx;

    if (JSVAL_IS_INT(id)) {
        idx = JSVAL_TO_INT(id);

        // Ensure that our index is within range.
        if (idx >= mParamCount)
            return NS_ERROR_INVALID_ARG;
    }
    else if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        jschar *nameChars = JS_GetStringChars(str);
        size_t nameLength = JS_GetStringLength(str);

        nsCAutoString name(":");
        name.Append(NS_ConvertUTF16toUTF8(nameChars, nameLength));

        // Check to see if there's a parameter with this name, and if not, let
        // the rest of the prototype chain be checked.
        nsresult rv = mStatement->GetParameterIndex(name, &idx);
        if (NS_FAILED(rv))
            return NS_OK;

        *_retval = JS_DefineUCProperty(cx, obj, nameChars, nameLength,
                                       JSVAL_VOID, nsnull, nsnull, 0);
        NS_ENSURE_TRUE(*_retval, NS_OK);
    }
    else {
        // We do not handle other types.
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
