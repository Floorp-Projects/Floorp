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

#include "mozStorageStatementWrapper.h"
#include "mozStorageStatementParams.h"
#include "mozStorageStatementRow.h"

#include "sqlite3.h"


/*************************************************************************
 ****
 **** mozStorageStatementWrapper
 ****
 *************************************************************************/

NS_IMPL_ISUPPORTS2(mozStorageStatementWrapper, mozIStorageStatementWrapper, nsIXPCScriptable)

mozStorageStatementWrapper::mozStorageStatementWrapper()
    : mStatement(nsnull)
{
}

mozStorageStatementWrapper::~mozStorageStatementWrapper()
{
    mStatement = nsnull;
}

NS_IMETHODIMP
mozStorageStatementWrapper::Initialize(mozIStorageStatement *aStatement)
{
    NS_ASSERTION(mStatement == nsnull, "mozStorageStatementWrapper is already initialized");
    NS_ENSURE_ARG_POINTER(aStatement);

    mStatement = static_cast<mozStorageStatement *>(aStatement);

    // fetch various things we care about
    mStatement->GetParameterCount(&mParamCount);
    mStatement->GetColumnCount(&mResultColumnCount);

    for (unsigned int i = 0; i < mResultColumnCount; i++) {
        const void *name = sqlite3_column_name16 (NativeStatement(), i);
        mColumnNames.AppendString(nsDependentString(static_cast<const PRUnichar*>(name)));
    }

    return NS_OK;
}

NS_IMETHODIMP
mozStorageStatementWrapper::GetStatement(mozIStorageStatement **aStatement)
{
    NS_IF_ADDREF(*aStatement = mStatement);
    return NS_OK;
}

NS_IMETHODIMP
mozStorageStatementWrapper::Reset()
{
    if (!mStatement)
        return NS_ERROR_FAILURE;

    return mStatement->Reset();
}

NS_IMETHODIMP
mozStorageStatementWrapper::Step(PRBool *_retval)
{
    if (!mStatement)
        return NS_ERROR_FAILURE;

    PRBool hasMore = PR_FALSE;
    nsresult rv = mStatement->ExecuteStep(&hasMore);
    if (NS_SUCCEEDED(rv) && !hasMore) {
        *_retval = PR_FALSE;
        mStatement->Reset();
        return NS_OK;
    }

    *_retval = hasMore;
    return rv;
}

NS_IMETHODIMP
mozStorageStatementWrapper::Execute()
{
    if (!mStatement)
        return NS_ERROR_FAILURE;

    return mStatement->Execute();
}

NS_IMETHODIMP
mozStorageStatementWrapper::GetRow(mozIStorageStatementRow **aRow)
{
    NS_ENSURE_ARG_POINTER(aRow);

    if (!mStatement)
        return NS_ERROR_FAILURE;

    PRInt32 state;
    mStatement->GetState(&state);
    if (state != mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING)
        return NS_ERROR_FAILURE;

    if (!mStatementRow) {
        mozStorageStatementRow *row = new mozStorageStatementRow(mStatement);
        if (!row)
            return NS_ERROR_OUT_OF_MEMORY;
        mStatementRow = row;
    }

    NS_ADDREF(*aRow = mStatementRow);
    return NS_OK;
}

NS_IMETHODIMP
mozStorageStatementWrapper::GetParams(mozIStorageStatementParams **aParams)
{
    NS_ENSURE_ARG_POINTER(aParams);

    if (!mStatementParams) {
        mozStorageStatementParams *params = new mozStorageStatementParams(mStatement);
        if (!params)
            return NS_ERROR_OUT_OF_MEMORY;
        mStatementParams = params;
    }

    NS_ADDREF(*aParams = mStatementParams);
    return NS_OK;
}

/*** nsIXPCScriptable interface ***/

/* readonly attribute string className; */
NS_IMETHODIMP
mozStorageStatementWrapper::GetClassName(char * *aClassName)
{
    NS_ENSURE_ARG_POINTER(aClassName);
    *aClassName = (char *) nsMemory::Clone("mozStorageStatementWrapper", 27);
    if (!*aClassName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute PRUint32 scriptableFlags; */
NS_IMETHODIMP
mozStorageStatementWrapper::GetScriptableFlags(PRUint32 *aScriptableFlags)
{
    *aScriptableFlags =
        nsIXPCScriptable::WANT_CALL |
        nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY |
        nsIXPCScriptable::WANT_NEWRESOLVE |
        nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE;
    return NS_OK;
}

/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                          JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    if (!mStatement) {
        *_retval = PR_TRUE;
        return NS_ERROR_FAILURE;
    }

    if (argc != mParamCount) {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    // reset
    mStatement->Reset();

    // bind parameters
    for (int i = 0; i < (int) argc; i++) {
        if (!JSValStorageStatementBinder(cx, mStatement, i, argv[i])) {
            *_retval = PR_FALSE;
            return NS_ERROR_INVALID_ARG;
        }
    }

    // if there are no results, we just execute
    if (mResultColumnCount == 0)
        mStatement->Execute();

    *vp = JSVAL_TRUE;
    *_retval = PR_TRUE;
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP
mozStorageStatementWrapper::PreCreate(nsISupports *nativeObj, JSContext * cx,
                       JSObject * globalObj, JSObject * *parentObj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::Create(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool addProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool delProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                               JSObject * obj, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
mozStorageStatementWrapper::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                  JSObject * obj, PRUint32 enum_op, jsval * statep, jsid *idp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
mozStorageStatementWrapper::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp, PRBool *_retval)
{
    *_retval = PR_TRUE;
    return NS_OK;
}

/* PRBool convert (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 type, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::Convert(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                JSObject * obj, PRUint32 type, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void finalize (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::Finalize(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                              JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool checkAccess (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 mode, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj, jsval id, PRUint32 mode, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
mozStorageStatementWrapper::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                               JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
mozStorageStatementWrapper::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                 JSObject * obj, jsval val, PRBool *bp, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void trace (in nsIXPConnectWrappedNative wrapper, in JSTracerPtr trc, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::Trace(nsIXPConnectWrappedNative *wrapper,
                                  JSTracer *trc, JSObject *obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool equality(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val); */
NS_IMETHODIMP
mozStorageStatementWrapper::Equality(nsIXPConnectWrappedNative *wrapper,
                                    JSContext *cx, JSObject *obj, jsval val,
                                    PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr outerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::OuterObject(nsIXPConnectWrappedNative *wrapper,
                                        JSContext *cx, JSObject *obj,
                                        JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* JSObjectPtr innerObject(in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
mozStorageStatementWrapper::InnerObject(nsIXPConnectWrappedNative *wrapper,
                                        JSContext *cx, JSObject *obj,
                                        JSObject **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreatePrototype (in JSContextPtr cx, in JSObjectPtr proto); */
NS_IMETHODIMP
mozStorageStatementWrapper::PostCreatePrototype(JSContext * cx,
                                                JSObject * proto)
{
    return NS_OK;
}
