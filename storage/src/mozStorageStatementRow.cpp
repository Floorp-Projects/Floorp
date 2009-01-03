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

#include "mozStorageStatementRow.h"

#include "jsapi.h"
#include "jsdate.h"

#include "sqlite3.h"

/*************************************************************************
 ****
 **** mozStorageStatementRow
 ****
 *************************************************************************/

NS_IMPL_ISUPPORTS2(mozStorageStatementRow, mozIStorageStatementRow, nsIXPCScriptable)

mozStorageStatementRow::mozStorageStatementRow(mozStorageStatement *aStatement)
    : mStatement(aStatement)
{
}

/*
 * nsIXPCScriptable impl
 */

/* readonly attribute string className; */
NS_IMETHODIMP
mozStorageStatementRow::GetClassName(char * *aClassName)
{
    NS_ENSURE_ARG_POINTER(aClassName);
    *aClassName = (char *) nsMemory::Clone("mozStorageStatementRow", 23);
    if (!*aClassName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute PRUint32 scriptableFlags; */
NS_IMETHODIMP
mozStorageStatementRow::GetScriptableFlags(PRUint32 *aScriptableFlags)
{
    *aScriptableFlags =
        nsIXPCScriptable::WANT_GETPROPERTY |
        nsIXPCScriptable::WANT_NEWRESOLVE |
        nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE;
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

    if (JSVAL_IS_STRING(id)) {
        nsDependentCString jsid(::JS_GetStringBytes(JSVAL_TO_STRING(id)));

        PRUint32 idx;
        nsresult rv = mStatement->GetColumnIndex(jsid, &idx);
        NS_ENSURE_SUCCESS(rv, rv);
        int ctype = sqlite3_column_type(NativeStatement(), idx);

        if (ctype == SQLITE_INTEGER || ctype == SQLITE_FLOAT) {
            double dval = sqlite3_column_double(NativeStatement(), idx);
            if (!JS_NewNumberValue(cx, dval, vp)) {
                *_retval = PR_FALSE;
                return NS_OK;
            }
        } else if (ctype == SQLITE_TEXT) {
            JSString *str = JS_NewUCStringCopyN(cx,
                                                (jschar*) sqlite3_column_text16(NativeStatement(), idx),
                                                sqlite3_column_bytes16(NativeStatement(), idx)/2);
            if (!str) {
                *_retval = PR_FALSE;
                return NS_OK;
            }
            *vp = STRING_TO_JSVAL(str);
        } else if (ctype == SQLITE_BLOB) {
            JSString *str = JS_NewStringCopyN(cx,
                                              (char*) sqlite3_column_blob(NativeStatement(), idx),
                                              sqlite3_column_bytes(NativeStatement(), idx));
            if (!str) {
                *_retval = PR_FALSE;
                return NS_OK;
            }
        } else if (ctype == SQLITE_NULL) {
            *vp = JSVAL_NULL;
        } else {
            NS_ERROR("sqlite3_column_type returned unknown column type, what's going on?");
        }
    }

    return NS_OK;
}


/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP
mozStorageStatementRow::PreCreate(nsISupports *nativeObj, JSContext * cx,
                       JSObject * globalObj, JSObject * *parentObj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::Create(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool addProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool delProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                  JSObject * obj, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
mozStorageStatementRow::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                     JSObject * obj, PRUint32 enum_op, jsval * statep, jsid *idp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
mozStorageStatementRow::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                   JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp, PRBool *_retval)
{
    NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

    if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        nsDependentCString name(::JS_GetStringBytes(str));

        PRUint32 idx;
        nsresult rv = mStatement->GetColumnIndex(name, &idx);
        NS_ENSURE_SUCCESS(rv, rv);

        *_retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                         ::JS_GetStringLength(str),
                                         JSVAL_VOID,
                                         nsnull, nsnull, 0);
        *objp = obj;
        return *_retval ? NS_OK : NS_ERROR_FAILURE;
    }

    *_retval = PR_TRUE;
    return NS_OK;
}

/* PRBool convert (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 type, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::Convert(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                JSObject * obj, PRUint32 type, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void finalize (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool checkAccess (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 mode, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval id, PRUint32 mode, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                             JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementRow::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                  JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
mozStorageStatementRow::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                    JSObject * obj, jsval val, PRBool *bp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void trace (in nsIXPConnectWrappedNative wrapper, in JSTracerPtr trc, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::Trace(nsIXPConnectWrappedNative *wrapper,
                              JSTracer * trc, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool equality(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val); */
NS_IMETHODIMP
mozStorageStatementRow::Equality(nsIXPConnectWrappedNative *wrapper,
                                 JSContext *cx, JSObject *obj, jsval val,
                                 PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr outerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::OuterObject(nsIXPConnectWrappedNative *wrapper,
                                    JSContext *cx, JSObject *obj,
                                    JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr innerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementRow::InnerObject(nsIXPConnectWrappedNative *wrapper,
                                    JSContext *cx, JSObject *obj,
                                    JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreatePrototype (in JSContextPtr cx, in JSObjectPtr proto); */
NS_IMETHODIMP
mozStorageStatementRow::PostCreatePrototype(JSContext * cx, JSObject * proto)
{
    return NS_OK;
}
