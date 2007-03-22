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

#include <stdio.h>

#include "nsError.h"
#include "nsISimpleEnumerator.h"
#include "nsMemory.h"

#include "mozStorageConnection.h"
#include "mozStorageStatement.h"
#include "mozStorageValueArray.h"

#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gStorageLog;
#endif

/**
 ** mozStorageStatementRowEnumerator
 **/
class mozStorageStatementRowEnumerator : public nsISimpleEnumerator {
public:
    // this expects a statement that has NOT had step called on it yet
    mozStorageStatementRowEnumerator (sqlite3_stmt *aDBStatement);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

private:
    ~mozStorageStatementRowEnumerator ();
protected:
    sqlite3_stmt *mDBStatement;
    PRBool mHasMore;
    PRBool mDidStep;

    void DoRealStep();
};


/**
 ** mozStorageStatement
 **/

NS_IMPL_ISUPPORTS2(mozStorageStatement, mozIStorageStatement, mozIStorageValueArray)

mozStorageStatement::mozStorageStatement()
    : mDBConnection (nsnull), mDBStatement(nsnull), mColumnNames(nsnull), mExecuting(PR_FALSE)
{
}

NS_IMETHODIMP
mozStorageStatement::Initialize(mozIStorageConnection *aDBConnection, const nsACString & aSQLStatement)
{
    int srv;

    // we can't do this if we're mid-execute
    if (mExecuting) {
        return NS_ERROR_FAILURE;
    }

    sqlite3 *db = nsnull;
    // XXX - need to implement a private iid to QI for here, to make sure
    // we have a real mozStorageConnection
    mozStorageConnection *msc = NS_STATIC_CAST(mozStorageConnection*, aDBConnection);
    db = msc->GetNativeConnection();
    NS_ENSURE_TRUE(db != nsnull, NS_ERROR_NULL_POINTER);

    // clean up old goop
    if (mDBStatement) {
        sqlite3_finalize(mDBStatement);
        mDBStatement = nsnull;
    }

    int nRetries = 0;

    while (nRetries < 2) {
        srv = sqlite3_prepare (db, nsPromiseFlatCString(aSQLStatement).get(), aSQLStatement.Length(), &mDBStatement, NULL);
        if ((srv == SQLITE_SCHEMA && nRetries != 0) ||
            (srv != SQLITE_SCHEMA && srv != SQLITE_OK))
        {
#ifdef PR_LOGGING
            PR_LOG(gStorageLog, PR_LOG_ERROR, ("Sqlite statement prepare error: %d '%s'", srv, sqlite3_errmsg(db)));
            PR_LOG(gStorageLog, PR_LOG_ERROR, ("Statement was: '%s'", nsPromiseFlatCString(aSQLStatement).get()));
#endif
            return NS_ERROR_FAILURE;
        }

        if (srv == SQLITE_OK)
            break;

        nRetries++;
    }

    mDBConnection = aDBConnection;
    mStatementString.Assign (aSQLStatement);
    mParamCount = sqlite3_bind_parameter_count (mDBStatement);
    mResultColumnCount = sqlite3_column_count (mDBStatement);
    mColumnNames.Clear();

    for (unsigned int i = 0; i < mResultColumnCount; i++) {
        const void *name = sqlite3_column_name16 (mDBStatement, i);
        if (name != nsnull)
            mColumnNames.AppendString(nsDependentString(NS_STATIC_CAST(const PRUnichar*, name)));
        else
            mColumnNames.AppendString(EmptyString());
    }

    // doing a sqlite3_prepare sets up the execution engine
    // for that statement; doing a create_function after that
    // results in badness, because there's a selected statement.
    // use this hack to clear it out -- this may be a bug.
    sqlite3_exec (db, "", 0, 0, 0);

    return NS_OK;
}

mozStorageStatement::~mozStorageStatement()
{
    if (mDBStatement)
        sqlite3_finalize (mDBStatement);
}

/* mozIStorageStatement clone (); */
NS_IMETHODIMP
mozStorageStatement::Clone(mozIStorageStatement **_retval)
{
    mozStorageStatement *mss = new mozStorageStatement();
    NS_ADDREF(mss);

    nsresult rv = mss->Initialize (mDBConnection, mStatementString);
    if (NS_FAILED(rv)) {
        NS_RELEASE(mss);
        return NS_ERROR_FAILURE; // XXX error code
    }

    *_retval = mss;
    return NS_OK;
}

/* readonly attribute unsigned long parameterCount; */
NS_IMETHODIMP
mozStorageStatement::GetParameterCount(PRUint32 *aParameterCount)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    NS_ENSURE_ARG_POINTER(aParameterCount);

    *aParameterCount = mParamCount;
    return NS_OK;
}

/* AUTF8String getParameterName(in unsigned long aParamIndex); */
NS_IMETHODIMP
mozStorageStatement::GetParameterName(PRUint32 aParamIndex, nsACString & _retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    const char *pname = sqlite3_bind_parameter_name(mDBStatement, aParamIndex + 1);
    if (pname == NULL) {
        // this thing had no name, so fake one
        nsCAutoString pname(":");
        pname.AppendInt(aParamIndex);
        _retval.Assign(pname);
    } else {
        _retval.Assign(nsDependentCString(pname));
    }

    return NS_OK;
}

/* void getParameterIndexes(in AUTF8String aParameterName, out unsigned long aCount, [array,size_is(aCount),retval] out unsigned long aIndexes); */
NS_IMETHODIMP
mozStorageStatement::GetParameterIndexes(const nsACString &aParameterName, PRUint32 *aCount, PRUint32 **aIndexes)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_ARG_POINTER(aIndexes);

    int *indexes, count;
    count = sqlite3_bind_parameter_indexes(mDBStatement, nsPromiseFlatCString(aParameterName).get(), &indexes);
    if (count) {
        *aIndexes = (PRUint32*) nsMemory::Alloc(sizeof(PRUint32) * count);
        for (int i = 0; i < count; i++)
            (*aIndexes)[i] = indexes[i];
        sqlite3_free_parameter_indexes(indexes);
        *aCount = count;
    } else {
        *aCount = 0;
        *aIndexes = nsnull;
    }
    return NS_OK;
}

/* readonly attribute unsigned long columnCount; */
NS_IMETHODIMP
mozStorageStatement::GetColumnCount(PRUint32 *aColumnCount)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    NS_ENSURE_ARG_POINTER(aColumnCount);

    *aColumnCount = mResultColumnCount;
    return NS_OK;
}

/* AUTF8String getColumnName(in unsigned long aColumnIndex); */
NS_IMETHODIMP
mozStorageStatement::GetColumnName(PRUint32 aColumnIndex, nsACString & _retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aColumnIndex < 0 || aColumnIndex >= mResultColumnCount)
        return NS_ERROR_FAILURE; // XXXerror

    const char *cname = sqlite3_column_name(mDBStatement, aColumnIndex);
    _retval.Assign(nsDependentCString(cname));

    return NS_OK;
}

/* void reset (); */
NS_IMETHODIMP
mozStorageStatement::Reset()
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Resetting statement: '%s'", nsPromiseFlatCString(mStatementString).get()));

    sqlite3_reset(mDBStatement);
    sqlite3_clear_bindings(mDBStatement);

    mExecuting = PR_FALSE;

    return NS_OK;
}

/* void bindUTF8StringParameter (in unsigned long aParamIndex, in AUTF8String aValue); */
NS_IMETHODIMP
mozStorageStatement::BindUTF8StringParameter(PRUint32 aParamIndex, const nsACString & aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_text (mDBStatement, aParamIndex + 1,
                       nsPromiseFlatCString(aValue).get(), aValue.Length(),
                       SQLITE_TRANSIENT);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindStringParameter (in unsigned long aParamIndex, in AString aValue); */
NS_IMETHODIMP
mozStorageStatement::BindStringParameter(PRUint32 aParamIndex, const nsAString & aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_text16 (mDBStatement, aParamIndex + 1,
                         nsPromiseFlatString(aValue).get(), aValue.Length() * 2,
                         SQLITE_TRANSIENT);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindDoubleParameter (in unsigned long aParamIndex, in double aValue); */
NS_IMETHODIMP
mozStorageStatement::BindDoubleParameter(PRUint32 aParamIndex, double aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_double (mDBStatement, aParamIndex + 1, aValue);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindInt32Parameter (in unsigned long aParamIndex, in long aValue); */
NS_IMETHODIMP
mozStorageStatement::BindInt32Parameter(PRUint32 aParamIndex, PRInt32 aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_int (mDBStatement, aParamIndex + 1, aValue);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindInt64Parameter (in unsigned long aParamIndex, in long long aValue); */
NS_IMETHODIMP
mozStorageStatement::BindInt64Parameter(PRUint32 aParamIndex, PRInt64 aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_int64 (mDBStatement, aParamIndex + 1, aValue);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindNullParameter (in unsigned long aParamIndex); */
NS_IMETHODIMP
mozStorageStatement::BindNullParameter(PRUint32 aParamIndex)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_null (mDBStatement, aParamIndex + 1);
    // XXX check return value for errors?

    return NS_OK;
}

/* void bindBlobParameter (in unsigned long aParamIndex, [array, const, size_is (aValueSize)] in octet aValue, in unsigned long aValueSize); */
NS_IMETHODIMP
mozStorageStatement::BindBlobParameter(PRUint32 aParamIndex, const PRUint8 *aValue, PRUint32 aValueSize)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_blob (mDBStatement, aParamIndex + 1, aValue, aValueSize, SQLITE_TRANSIENT);
    // XXX check return value for errors?

    return NS_OK;
}

/* void execute (); */
NS_IMETHODIMP
mozStorageStatement::Execute()
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    PRBool ret = PR_FALSE;
    nsresult rv = ExecuteStep(&ret);
    if (NS_FAILED(rv))
        return rv;
    return Reset();
}

/* boolean executeStep (); */
NS_IMETHODIMP
mozStorageStatement::ExecuteStep(PRBool *_retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    nsresult rv;

    if (mExecuting == PR_FALSE) {
        // check if we need to recreate this statement before executing
        if (sqlite3_expired(mDBStatement)) {
            PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Statement expired, recreating before step"));
            rv = Recreate();
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    int nRetries = 0;

    while (nRetries < 2) {
        int srv = sqlite3_step (mDBStatement);

#ifdef PR_LOGGING
        if (srv != SQLITE_ROW && srv != SQLITE_DONE)
        {
            nsCAutoString errStr;
            mDBConnection->GetLastErrorString(errStr);
            PR_LOG(gStorageLog, PR_LOG_DEBUG, ("mozStorageStatement::ExecuteStep error: %s", errStr.get()));
        }
#endif

        // SQLITE_ROW and SQLITE_DONE are non-errors
        if (srv == SQLITE_ROW) {
            // we got a row back
            mExecuting = PR_TRUE;
            *_retval = PR_TRUE;
            return NS_OK;
        } else if (srv == SQLITE_DONE) {
            // statement is done (no row returned)
            mExecuting = PR_FALSE;
            *_retval = PR_FALSE;
            return NS_OK;
        } else if (srv == SQLITE_BUSY ||
                   srv == SQLITE_MISUSE)
        {
            mExecuting = PR_FALSE;
            return NS_ERROR_FAILURE;
        } else if (srv == SQLITE_SCHEMA) {
            // step should never return SQLITE_SCHEMA
            NS_NOTREACHED("sqlite3_step returned SQLITE_SCHEMA!");
            return NS_ERROR_FAILURE;
        } else if (srv == SQLITE_ERROR) {
            // so we may end up with a SQLITE_ERROR/SQLITE_SCHEMA only after
            // we reset, because SQLite's error reporting story
            // sucks.
            if (mExecuting == PR_TRUE) {
                PR_LOG(gStorageLog, PR_LOG_ERROR, ("SQLITE_ERROR after mExecuting was true!"));

                mExecuting = PR_FALSE;
                return NS_ERROR_FAILURE;
            }

            srv = sqlite3_reset(mDBStatement);
            if (srv == SQLITE_SCHEMA) {
                rv = Recreate();
                NS_ENSURE_SUCCESS(rv, rv);

                nRetries++;
            } else {
                return NS_ERROR_FAILURE;
            }
        } else {
            // something that shouldn't happen happened
            NS_ERROR ("sqlite3_step returned an error code we don't know about!");
        }
    }

    // shouldn't get here
    return NS_ERROR_FAILURE;
}

/* [noscript,notxpcom] sqlite3stmtptr getNativeStatementPointer(); */
sqlite3_stmt*
mozStorageStatement::GetNativeStatementPointer()
{
    return mDBStatement;
}

/* readonly attribute long state; */
NS_IMETHODIMP
mozStorageStatement::GetState(PRInt32 *_retval)
{
    if (!mDBConnection || !mDBStatement) {
        *_retval = MOZ_STORAGE_STATEMENT_INVALID;
    } else if (mExecuting) {
        *_retval = MOZ_STORAGE_STATEMENT_EXECUTING;
    } else {
        *_retval = MOZ_STORAGE_STATEMENT_READY;
    }

    return NS_OK;
}

nsresult
mozStorageStatement::Recreate()
{
    nsresult rv;
    int srv;
    sqlite3_stmt *savedStmt = mDBStatement;
    mDBStatement = nsnull;
    rv = Initialize(mDBConnection, mStatementString);
    NS_ENSURE_SUCCESS(rv, rv);

    // copy over the param bindings
    srv = sqlite3_transfer_bindings(savedStmt, mDBStatement);

    // we're always going to finalize this, so no need to
    // error check
    sqlite3_finalize(savedStmt);

    if (srv != SQLITE_OK) {
        PR_LOG(gStorageLog, PR_LOG_ERROR, ("sqlite3_transfer_bindings returned: %d", srv));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/***
 *** mozIStorageValueArray
 ***/

/* readonly attribute unsigned long numEntries; */
NS_IMETHODIMP
mozStorageStatement::GetNumEntries(PRUint32 *aLength)
{
    *aLength = mResultColumnCount;
    return NS_OK;
}

/* long getTypeOfIndex (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetTypeOfIndex(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    int t = sqlite3_column_type (mDBStatement, aIndex);
    switch (t) {
        case SQLITE_INTEGER:
            *_retval = VALUE_TYPE_INTEGER;
            break;
        case SQLITE_FLOAT:
            *_retval = VALUE_TYPE_FLOAT;
            break;
        case SQLITE_TEXT:
            *_retval = VALUE_TYPE_TEXT;
            break;
        case SQLITE_BLOB:
            *_retval = VALUE_TYPE_BLOB;
            break;
        case SQLITE_NULL:
            *_retval = VALUE_TYPE_NULL;
            break;
        default:
            // ???
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* long getInt32 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetInt32(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    *_retval = sqlite3_column_int (mDBStatement, aIndex);

    return NS_OK;
}

/* long long getInt64 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetInt64(PRUint32 aIndex, PRInt64 *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    *_retval = sqlite3_column_int64 (mDBStatement, aIndex);

    return NS_OK;
}

/* double getDouble (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetDouble(PRUint32 aIndex, double *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    *_retval = sqlite3_column_double (mDBStatement, aIndex);

    return NS_OK;
}

/* AUTF8String getUTF8String (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetUTF8String(PRUint32 aIndex, nsACString & _retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    PRInt32 t;
    nsresult rv = GetTypeOfIndex (aIndex, &t);
    NS_ENSURE_SUCCESS(rv, rv);
    if (t == VALUE_TYPE_NULL) {
        // null columns get IsVoid set to distinguish them from empty strings
        _retval.Truncate(0);
        _retval.SetIsVoid(PR_TRUE);
    } else {
        int slen = sqlite3_column_bytes (mDBStatement, aIndex);
        const unsigned char *cstr = sqlite3_column_text (mDBStatement, aIndex);
        _retval.Assign ((char *) cstr, slen);
    }
    return NS_OK;
}

/* AString getString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetString(PRUint32 aIndex, nsAString & _retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    PRInt32 t;
    nsresult rv = GetTypeOfIndex (aIndex, &t);
    NS_ENSURE_SUCCESS(rv, rv);
    if (t == VALUE_TYPE_NULL) {
        // null columns get IsVoid set to distinguish them from empty strings
        _retval.Truncate(0);
        _retval.SetIsVoid(PR_TRUE);
    } else {
        int slen = sqlite3_column_bytes16 (mDBStatement, aIndex);
        const void *text = sqlite3_column_text16 (mDBStatement, aIndex);
        const PRUnichar *wstr = NS_STATIC_CAST(const PRUnichar *, text);
        _retval.Assign (wstr, slen/2);
    }
    return NS_OK;
}

/* void getBlob (in unsigned long aIndex, out unsigned long aDataSize, [array, size_is (aDataSize)] out octet aData); */
NS_IMETHODIMP
mozStorageStatement::GetBlob(PRUint32 aIndex, PRUint32 *aDataSize, PRUint8 **aData)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");
    if (!mExecuting)
        return NS_ERROR_FAILURE;

    int blobsize = sqlite3_column_bytes (mDBStatement, aIndex);
    if (blobsize == 0) {
      // empty column
      *aData = nsnull;
      *aDataSize = 0;
      return NS_OK;
    }
    const void *blob = sqlite3_column_blob (mDBStatement, aIndex);

    void *blobcopy = nsMemory::Clone(blob, blobsize);
    if (blobcopy == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *aData = (PRUint8*) blobcopy;
    *aDataSize = blobsize;

    return NS_OK;
}

/* [noscript] void getSharedUTF8String(in unsigned long aIndex, out unsigned long aLength, [shared,retval] out string aResult); */
NS_IMETHODIMP
mozStorageStatement::GetSharedUTF8String(PRUint32 aIndex, PRUint32 *aLength, const char **_retval)
{
    if (aLength) {
        int slen = sqlite3_column_bytes (mDBStatement, aIndex);
        *aLength = slen;
    }

    *_retval = (const char *) sqlite3_column_text (mDBStatement, aIndex);
    return NS_OK;
}

/* [noscript] void getSharedString(in unsigned long aIndex, out unsigned long aLength, [shared,retval] out wstring aResult); */
NS_IMETHODIMP
mozStorageStatement::GetSharedString(PRUint32 aIndex, PRUint32 *aLength, const PRUnichar **_retval)
{
    if (aLength) {
        int slen = sqlite3_column_bytes16 (mDBStatement, aIndex);
        *aLength = slen;
    }

    *_retval = (const PRUnichar *) sqlite3_column_text16 (mDBStatement, aIndex);
    return NS_OK;
}

/* [noscript] void getSharedBlob(in unsigned long aIndex, out unsigned long aLength, [shared,retval] out octetPtr aResult); */
NS_IMETHODIMP
mozStorageStatement::GetSharedBlob(PRUint32 aIndex, PRUint32 *aDataSize, const PRUint8 **aData)
{
    *aDataSize = sqlite3_column_bytes (mDBStatement, aIndex);
    *aData = (const PRUint8*) sqlite3_column_blob (mDBStatement, aIndex);

    return NS_OK;
}

/* boolean getIsNull (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetIsNull(PRUint32 aIndex, PRBool *_retval)
{
    PRInt32 t;
    nsresult rv = GetTypeOfIndex (aIndex, &t);
    if (NS_FAILED(rv)) return rv;

    if (t == VALUE_TYPE_NULL)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;

    return NS_OK;
}
