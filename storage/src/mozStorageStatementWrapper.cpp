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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#include "jsapi.h"
#include "jsdate.h"

#include "sqlite3.h"

/**
 ** mozStorageStatementRow
 **/
class mozStorageStatementRow : public mozIStorageStatementRow,
                               public nsIXPCScriptable
{
public:
    mozStorageStatementRow(mozIStorageStatement *aStatement,
                           int aNumColumns,
                           const nsStringArray *aColumnNames);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // mozIStorageStatementRow interface (empty)
    NS_DECL_MOZISTORAGESTATEMENTROW

    // nsIXPCScriptable interface
    NS_DECL_NSIXPCSCRIPTABLE
protected:
    sqlite3_stmt* NativeStatement() {
        return mStatement->GetNativeStatementPointer();
    }

    nsCOMPtr<mozIStorageStatement> mStatement;
    int mNumColumns;
    const nsStringArray *mColumnNames;
};

/**
 ** mozStorageStatementParams
 **/
class mozStorageStatementParams : public mozIStorageStatementParams,
                                  public nsIXPCScriptable
{
public:
    mozStorageStatementParams(mozIStorageStatement *aStatement);

    // interfaces
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGESTATEMENTPARAMS
    NS_DECL_NSIXPCSCRIPTABLE

protected:
    nsCOMPtr<mozIStorageStatement> mStatement;
    PRUint32 mParamCount;
};

static PRBool
JSValStorageStatementBinder (JSContext *cx,
                             mozIStorageStatement *aStatement,
                             int *aParamIndexes,
                             int aNumIndexes,
                             jsval val)
{
    int i;
    if (JSVAL_IS_INT(val)) {
        int v = JSVAL_TO_INT(val);
        for (i = 0; i < aNumIndexes; i++)
            aStatement->BindInt32Parameter(aParamIndexes[i], v);
    } else if (JSVAL_IS_DOUBLE(val)) {
        double d = *JSVAL_TO_DOUBLE(val);
        for (i = 0; i < aNumIndexes; i++)
            aStatement->BindDoubleParameter(aParamIndexes[i], d);
    } else if (JSVAL_IS_STRING(val)) {
        JSString *str = JSVAL_TO_STRING(val);
        for (i = 0; i < aNumIndexes; i++)
            aStatement->BindWStringParameter(aParamIndexes[i], NS_REINTERPRET_CAST(PRUnichar*, JS_GetStringChars(str)));
    } else if (JSVAL_IS_BOOLEAN(val)) {
        if (val == JSVAL_TRUE) {
            for (i = 0; i < aNumIndexes; i++)
                aStatement->BindInt32Parameter(aParamIndexes[i], 1);
        } else {
            for (i = 0; i < aNumIndexes; i++)
                aStatement->BindInt32Parameter(aParamIndexes[i], 0);
        }
    } else if (JSVAL_IS_NULL(val)) {
        for (i = 0; i < aNumIndexes; i++)
            aStatement->BindNullParameter(aParamIndexes[i]);
    } else if (JSVAL_IS_OBJECT(val)) {
        JSObject *obj = JSVAL_TO_OBJECT(val);
        // some special things
        if (js_DateIsValid (cx, obj)) {
            double msecd = js_DateGetMsecSinceEpoch(cx, obj);
            msecd *= 1000.0;
            PRInt64 msec;
            LL_D2L(msec, msecd);

            for (i = 0; i < aNumIndexes; i++)
                aStatement->BindInt64Parameter(aParamIndexes[i], msec);
        } else {
            return PR_FALSE;
        }
    } else {
        return PR_FALSE;
    }

    return PR_TRUE;
}


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

    mStatement = aStatement;

    // fetch various things we care about
    mStatement->GetParameterCount(&mParamCount);
    mStatement->GetNumColumns(&mResultColumnCount);

    for (unsigned int i = 0; i < mResultColumnCount; i++) {
        const void *name = sqlite3_column_name16 (NativeStatement(), i);
        mColumnNames.AppendString(nsDependentString(NS_STATIC_CAST(const PRUnichar*, name)));
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
        mozStorageStatementRow *row = new mozStorageStatementRow(mStatement, mResultColumnCount, &mColumnNames);
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
    for (int i = 0; i < argc; i++) {
        if (!JSValStorageStatementBinder(cx, mStatement, &i, 1, argv[i])) {
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

/* PRUint32 mark (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in voidPtr arg); */
NS_IMETHODIMP
mozStorageStatementWrapper::Mark(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                          JSObject * obj, void * arg, PRUint32 *_retval)
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

/*************************************************************************
 ****
 **** mozStorageStatementRow
 ****
 *************************************************************************/

NS_IMPL_ISUPPORTS2(mozStorageStatementRow, mozIStorageStatementRow, nsIXPCScriptable)

mozStorageStatementRow::mozStorageStatementRow(mozIStorageStatement *aStatement,
                                               int aNumColumns,
                                               const nsStringArray *aColumnNames)
    : mStatement(aStatement),
      mNumColumns(aNumColumns),
      mColumnNames(aColumnNames)
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
    *_retval = PR_FALSE;

    if (JSVAL_IS_STRING(id)) {
        nsDependentString jsid((PRUnichar *)::JS_GetStringChars(JSVAL_TO_STRING(id)),
                               ::JS_GetStringLength(JSVAL_TO_STRING(id)));

        for (int i = 0; i < mNumColumns; i++) {
            if (jsid.Equals(*(*mColumnNames)[i])) {
                int ctype = sqlite3_column_type(NativeStatement(), i);

                if (ctype == SQLITE_INTEGER || ctype == SQLITE_FLOAT) {
                    double dval = sqlite3_column_double(NativeStatement(), i);
                    if (!JS_NewNumberValue(cx, dval, vp)) {
                        *_retval = PR_FALSE;
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                } else if (ctype == SQLITE_TEXT) {
                    JSString *str = JS_NewUCStringCopyN(cx,
                                                        (jschar*) sqlite3_column_text16(NativeStatement(), i),
                                                        sqlite3_column_bytes16(NativeStatement(), i)/2);
                    if (!str) {
                        *_retval = PR_FALSE;
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                    *vp = STRING_TO_JSVAL(str);
                } else if (ctype == SQLITE_BLOB) {
                    JSString *str = JS_NewStringCopyN(cx,
                                                      (char*) sqlite3_column_blob(NativeStatement(), i),
                                                      sqlite3_column_bytes(NativeStatement(), i));
                    if (!str) {
                        *_retval = PR_FALSE;
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                } else if (ctype == SQLITE_NULL) {
                    *vp = JSVAL_NULL;
                } else {
                    NS_ERROR("sqlite3_column_type returned unknown column type, what's going on?");
                }

                *_retval = PR_TRUE;
                break;
            }
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
    if (JSVAL_IS_STRING(id)) {
        JSString *str = JSVAL_TO_STRING(id);
        nsDependentString name((PRUnichar *)::JS_GetStringChars(str),
                               ::JS_GetStringLength(str));

        for (int i = 0; i < mNumColumns; i++) {
            if (name.Equals(*(*mColumnNames)[i])) {
                *_retval = ::JS_DefineUCProperty(cx, obj, ::JS_GetStringChars(str),
                                                 ::JS_GetStringLength(str),
                                                 JSVAL_VOID,
                                                 nsnull, nsnull, 0);
                *objp = obj;
                return *_retval ? NS_OK : NS_ERROR_FAILURE;
            }
        }
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

/* PRUint32 mark (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in voidPtr arg); */
NS_IMETHODIMP
mozStorageStatementRow::Mark(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                             JSObject * obj, void * arg, PRUint32 *_retval)
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
    if (JSVAL_IS_INT(id)) {
        int idx = JSVAL_TO_INT(id);

        *_retval = JSValStorageStatementBinder (cx, mStatement, &idx, 1, *vp);
    } else if (JSVAL_IS_STRING(id)) {
        int indexCount = 0, *indexes;

        JSString *str = JSVAL_TO_STRING(id);
        nsCAutoString name(":");
        name.Append(NS_ConvertUTF16toUTF8(nsDependentString((PRUnichar *)::JS_GetStringChars(str),
                                                            ::JS_GetStringLength(str))));

        // check to see if there's a parameter with this name
        indexCount = sqlite3_bind_parameter_indexes(mStatement->GetNativeStatementPointer(), name.get(), &indexes);
        if (indexCount == 0) {
            // er, not found? How'd we get past NewResolve?
            fprintf (stderr, "********** mozStorageStatementWrapper: Couldn't find parameter %s\n", name.get());
            *_retval = PR_FALSE;
            return NS_ERROR_FAILURE;
        } else {
            // drop this by 1, to account for sqlite's indexes being 1-based
            for (int i = 0; i < indexCount; i++)
                indexes[i]--;

            *_retval = JSValStorageStatementBinder (cx, mStatement, indexes, indexCount, *vp);
            sqlite3_free_parameter_indexes(indexes);
        }
    } else {
        *_retval = PR_FALSE;
    }

    if (*_retval)
        return NS_OK;
    else
        return NS_ERROR_INVALID_ARG;
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

/* PRUint32 mark (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in voidPtr arg); */
NS_IMETHODIMP
mozStorageStatementParams::Mark(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                             JSObject * obj, void * arg, PRUint32 *_retval)
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

