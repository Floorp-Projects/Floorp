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
 *   John Zhang <jzhang@aptana.com>
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
#include "mozStorage.h"

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
    mozStorageConnection *msc = static_cast<mozStorageConnection*>(aDBConnection);
    db = msc->GetNativeConnection();
    NS_ENSURE_TRUE(db != nsnull, NS_ERROR_NULL_POINTER);

    // clean up old goop
    if (mDBStatement) {
        sqlite3_finalize(mDBStatement);
        mDBStatement = nsnull;
    }

    int nRetries = 0;

    while (nRetries < 2) {
        srv = sqlite3_prepare_v2(db, nsPromiseFlatCString(aSQLStatement).get(),
                                 aSQLStatement.Length(), &mDBStatement, NULL);
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

    for (PRUint32 i = 0; i < mResultColumnCount; i++) {
        const char *name = sqlite3_column_name(mDBStatement, i);
        mColumnNames.AppendCString(nsDependentCString(name));
    }

#ifdef DEBUG
    // We want to try and test for LIKE and that consumers are using
    // escapeStringForLIKE instead of just trusting user input.  The idea to
    // check to see if they are binding a parameter after like instead of just
    // using a string.  We only do this in debug builds because it's expensive!
    const nsCaseInsensitiveCStringComparator c;
    nsACString::const_iterator start, end, e;
    aSQLStatement.BeginReading(start);
    aSQLStatement.EndReading(end);
    e = end;
    while (FindInReadable(NS_LITERAL_CSTRING(" LIKE"), start, e, c)) {
        // We have a LIKE in here, so we perform our tests
        // FindInReadable moves the iterator, so we have to get a new one for
        // each test we perform.
        nsACString::const_iterator s1, s2, s3;
        s1 = s2 = s3 = start;

        if (!(FindInReadable(NS_LITERAL_CSTRING(" LIKE ?"), s1, end, c) ||
              FindInReadable(NS_LITERAL_CSTRING(" LIKE :"), s2, end, c) ||
              FindInReadable(NS_LITERAL_CSTRING(" LIKE @"), s3, end, c))) {
            // At this point, we didn't find a LIKE statement followed by ?, :,
            // or @, all of which are valid characters for binding a parameter.
            // We will warn the consumer that they may not be safely using LIKE.
            NS_WARNING("Unsafe use of LIKE detected!  Please ensure that you "
                       "are using mozIStorageConnection::escapeStringForLIKE "
                       "and that you are binding that result to the statement "
                       "to prevent SQL injection attacks.");
        }

        // resetting start and e
        start = e;
        e = end;
    }
#endif

    // doing a sqlite3_prepare sets up the execution engine
    // for that statement; doing a create_function after that
    // results in badness, because there's a selected statement.
    // use this hack to clear it out -- this may be a bug.
    sqlite3_exec (db, "", 0, 0, 0);

    return NS_OK;
}

mozStorageStatement::~mozStorageStatement()
{
    (void)Finalize();
}

/* mozIStorageStatement clone (); */
NS_IMETHODIMP
mozStorageStatement::Clone(mozIStorageStatement **_retval)
{
    mozStorageStatement *mss = new mozStorageStatement();
    if (!mss)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = mss->Initialize (mDBConnection, mStatementString);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_retval = mss);
    return NS_OK;
}

/* void finalize(); */
NS_IMETHODIMP
mozStorageStatement::Finalize()
{
    if (mDBStatement) {
        int srv = sqlite3_finalize(mDBStatement);
        mDBStatement = NULL;
        return ConvertResultCode(srv);
    }
    return NS_OK;
}

/* readonly attribute unsigned long parameterCount; */
NS_IMETHODIMP
mozStorageStatement::GetParameterCount(PRUint32 *aParameterCount)
{
    NS_ENSURE_ARG_POINTER(aParameterCount);

    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    *aParameterCount = mParamCount;
    return NS_OK;
}

/* AUTF8String getParameterName(in unsigned long aParamIndex); */
NS_IMETHODIMP
mozStorageStatement::GetParameterName(PRUint32 aParamIndex, nsACString & _retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    // We have to check this index because sqlite3_bind_parameter_name returns
    // NULL if an error occurs, or if a column is unnamed.  Since we handle
    // unnamed columns, we won't be able to tell if it is an error not without
    // checking ourselves.
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_ILLEGAL_VALUE;

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

/* unsigned long getParameterIndex(in AUTF8String aParameterName); */
NS_IMETHODIMP
mozStorageStatement::GetParameterIndex(const nsACString &aName,
                                       PRUint32 *_retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int ind = sqlite3_bind_parameter_index(mDBStatement,
                                           nsPromiseFlatCString(aName).get());
    if (ind  == 0) // Named parameter not found
        return NS_ERROR_INVALID_ARG;
    
    *_retval = ind - 1; // SQLite indexes are 1-based, we are 0-based

    return NS_OK;
}

/* readonly attribute unsigned long columnCount; */
NS_IMETHODIMP
mozStorageStatement::GetColumnCount(PRUint32 *aColumnCount)
{
    NS_ENSURE_ARG_POINTER(aColumnCount);

    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    *aColumnCount = mResultColumnCount;
    return NS_OK;
}

/* AUTF8String getColumnName(in unsigned long aColumnIndex); */
NS_IMETHODIMP
mozStorageStatement::GetColumnName(PRUint32 aColumnIndex, nsACString & _retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    // We have to check this index because sqlite3_column_name returns
    // NULL if an error occurs, or if a column is unnamed.
    if (aColumnIndex < 0 || aColumnIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;

    const char *cname = sqlite3_column_name(mDBStatement, aColumnIndex);
    _retval.Assign(nsDependentCString(cname));

    return NS_OK;
}

/* unsigned long getColumnIndex(in AUTF8String aName); */
NS_IMETHODIMP
mozStorageStatement::GetColumnIndex(const nsACString &aName, PRUint32 *_retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    // Surprisingly enough, SQLite doesn't provide an API for this.  We have to
    // determine it ourselves sadly.
    for (PRUint32 i = 0; i < mResultColumnCount; i++) {
        if (mColumnNames[i]->Equals(aName)) {
            *_retval = i;
            return NS_OK;
        }
    }

    return NS_ERROR_INVALID_ARG;
}

/* void reset (); */
NS_IMETHODIMP
mozStorageStatement::Reset()
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

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
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_text (mDBStatement, aParamIndex + 1,
                                 nsPromiseFlatCString(aValue).get(),
                                 aValue.Length(), SQLITE_TRANSIENT);

    return ConvertResultCode(srv);
}

/* void bindStringParameter (in unsigned long aParamIndex, in AString aValue); */
NS_IMETHODIMP
mozStorageStatement::BindStringParameter(PRUint32 aParamIndex, const nsAString & aValue)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_text16 (mDBStatement, aParamIndex + 1,
                                   nsPromiseFlatString(aValue).get(),
                                   aValue.Length() * 2, SQLITE_TRANSIENT);

    return ConvertResultCode(srv);
}

/* void bindDoubleParameter (in unsigned long aParamIndex, in double aValue); */
NS_IMETHODIMP
mozStorageStatement::BindDoubleParameter(PRUint32 aParamIndex, double aValue)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_double (mDBStatement, aParamIndex + 1, aValue);

    return ConvertResultCode(srv);
}

/* void bindInt32Parameter (in unsigned long aParamIndex, in long aValue); */
NS_IMETHODIMP
mozStorageStatement::BindInt32Parameter(PRUint32 aParamIndex, PRInt32 aValue)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_int (mDBStatement, aParamIndex + 1, aValue);

    return ConvertResultCode(srv);
}

/* void bindInt64Parameter (in unsigned long aParamIndex, in long long aValue); */
NS_IMETHODIMP
mozStorageStatement::BindInt64Parameter(PRUint32 aParamIndex, PRInt64 aValue)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_int64 (mDBStatement, aParamIndex + 1, aValue);

    return ConvertResultCode(srv);
}

/* void bindNullParameter (in unsigned long aParamIndex); */
NS_IMETHODIMP
mozStorageStatement::BindNullParameter(PRUint32 aParamIndex)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_null (mDBStatement, aParamIndex + 1);

    return ConvertResultCode(srv);
}

/* void bindBlobParameter (in unsigned long aParamIndex, [array, const, size_is (aValueSize)] in octet aValue, in unsigned long aValueSize); */
NS_IMETHODIMP
mozStorageStatement::BindBlobParameter(PRUint32 aParamIndex, const PRUint8 *aValue, PRUint32 aValueSize)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    int srv = sqlite3_bind_blob (mDBStatement, aParamIndex + 1, aValue,
                                 aValueSize, SQLITE_TRANSIENT);

    return ConvertResultCode(srv);
}

/* void execute (); */
NS_IMETHODIMP
mozStorageStatement::Execute()
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    PRBool ret;
    nsresult rv = ExecuteStep(&ret);
    NS_ENSURE_SUCCESS(rv, rv);

    return Reset();
}

/* boolean executeStep (); */
NS_IMETHODIMP
mozStorageStatement::ExecuteStep(PRBool *_retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    if (mExecuting == PR_FALSE) {
        // check if we need to recreate this statement before executing
        if (sqlite3_expired(mDBStatement)) {
            PR_LOG(gStorageLog, PR_LOG_DEBUG, ("Statement expired, recreating before step"));
            rv = Recreate();
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

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
    } else if (srv == SQLITE_BUSY || srv == SQLITE_MISUSE) {
        mExecuting = PR_FALSE;
        return NS_ERROR_FAILURE;
    } else if (mExecuting == PR_TRUE) {
#ifdef PR_LOGGING
        PR_LOG(gStorageLog, PR_LOG_ERROR, ("SQLite error after mExecuting was true!"));
#endif
        mExecuting = PR_FALSE;
    }

    return ConvertResultCode(srv);
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
        return ConvertResultCode(srv);
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
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    if (aIndex < 0 || aIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;

    if (!mExecuting)
        return NS_ERROR_UNEXPECTED;

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
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    if (aIndex < 0 || aIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;
    
    if (!mExecuting)
        return NS_ERROR_UNEXPECTED;

    *_retval = sqlite3_column_int (mDBStatement, aIndex);

    return NS_OK;
}

/* long long getInt64 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetInt64(PRUint32 aIndex, PRInt64 *_retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    if (aIndex < 0 || aIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;
    
    if (!mExecuting)
        return NS_ERROR_UNEXPECTED;

    *_retval = sqlite3_column_int64 (mDBStatement, aIndex);

    return NS_OK;
}

/* double getDouble (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetDouble(PRUint32 aIndex, double *_retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    if (aIndex < 0 || aIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;
    
    if (!mExecuting)
        return NS_ERROR_UNEXPECTED;

    *_retval = sqlite3_column_double (mDBStatement, aIndex);

    return NS_OK;
}

/* AUTF8String getUTF8String (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetUTF8String(PRUint32 aIndex, nsACString & _retval)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    // Get type of Index will check aIndex for us, so we don't have to.
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
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    // Get type of Index will check aIndex for us, so we don't have to.
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
        const PRUnichar *wstr = static_cast<const PRUnichar *>(text);
        _retval.Assign (wstr, slen/2);
    }
    return NS_OK;
}

/* void getBlob (in unsigned long aIndex, out unsigned long aDataSize, [array, size_is (aDataSize)] out octet aData); */
NS_IMETHODIMP
mozStorageStatement::GetBlob(PRUint32 aIndex, PRUint32 *aDataSize, PRUint8 **aData)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;

    if (aIndex < 0 || aIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;
    
    if (!mExecuting)
        return NS_ERROR_UNEXPECTED;

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
    // Get type of Index will check aIndex for us, so we don't have to.
    PRInt32 t;
    nsresult rv = GetTypeOfIndex (aIndex, &t);
    NS_ENSURE_SUCCESS(rv, rv);

    if (t == VALUE_TYPE_NULL)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;

    return NS_OK;
}

/* AString escapeStringForLIKE(in AString aValue, in char aEscapeChar); */
NS_IMETHODIMP
mozStorageStatement::EscapeStringForLIKE(const nsAString & aValue, 
                                         const PRUnichar aEscapeChar, 
                                         nsAString &aEscapedString)
{
    const PRUnichar MATCH_ALL('%');
    const PRUnichar MATCH_ONE('_');

    aEscapedString.Truncate(0);

    for (PRInt32 i = 0; i < aValue.Length(); i++) {
        if (aValue[i] == aEscapeChar || aValue[i] == MATCH_ALL || 
            aValue[i] == MATCH_ONE)
            aEscapedString += aEscapeChar;
        aEscapedString += aValue[i];
    }
    return NS_OK;
}

/* AString getColumnDecltype(in unsigned long aParamIndex); */
NS_IMETHODIMP
mozStorageStatement::GetColumnDecltype(PRUint32 aParamIndex,
                                       nsACString& aDeclType)
{
    if (!mDBConnection || !mDBStatement)
        return NS_ERROR_NOT_INITIALIZED;
    
    if (aParamIndex < 0 || aParamIndex >= mResultColumnCount)
        return NS_ERROR_ILLEGAL_VALUE;

    const char *declType = sqlite3_column_decltype(mDBStatement, aParamIndex);
    aDeclType.Assign(declType);
    
    return NS_OK;
}
