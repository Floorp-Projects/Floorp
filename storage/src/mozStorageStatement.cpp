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

NS_IMPL_ISUPPORTS1(mozStorageStatementRowEnumerator, nsISimpleEnumerator)

mozStorageStatementRowEnumerator::mozStorageStatementRowEnumerator (sqlite3_stmt *aDBStatement)
    : mDBStatement (aDBStatement)
{
    NS_ASSERTION (aDBStatement != nsnull, "Null statement!");

    // do the first step
    DoRealStep ();
    mDidStep = PR_TRUE;
}

void
mozStorageStatementRowEnumerator::DoRealStep ()
{
    int srv = sqlite3_step (mDBStatement);

    switch (srv) {
        case SQLITE_ROW:
            mHasMore = PR_TRUE;
            break;
        case SQLITE_DONE:
            mHasMore = PR_FALSE;
            break;
        case SQLITE_BUSY:       // XXX!!!
        case SQLITE_MISUSE:
        case SQLITE_ERROR:
        default:
            mHasMore = PR_FALSE;
            break;
    }
}

mozStorageStatementRowEnumerator::~mozStorageStatementRowEnumerator ()
{
}

/* nsISimpleEnumerator interface */
NS_IMETHODIMP
mozStorageStatementRowEnumerator::HasMoreElements (PRBool *_retval)
{
    // step if we haven't already
    if (!mDidStep) {
        DoRealStep();
        mDidStep = PR_TRUE;
    }

    *_retval = mHasMore;
    return NS_OK;
}

NS_IMETHODIMP
mozStorageStatementRowEnumerator::GetNext (nsISupports **_retval)
{
    if (!mHasMore)
        return NS_ERROR_FAILURE;

    if (!mDidStep)
        DoRealStep();
    mDidStep = PR_FALSE;

    // assume this is SQLITE_ROW
    sqlite3_data_count (mDBStatement);

    mozStorageStatementRowValueArray *mssrva = new mozStorageStatementRowValueArray(mDBStatement);
    NS_ADDREF(mssrva);
    *_retval = mssrva;

    return NS_OK;
}

/**
 ** mozStorageStatement
 **/

NS_IMPL_ISUPPORTS2(mozStorageStatement, mozIStorageStatement, mozIStorageValueArray)

mozStorageStatement::mozStorageStatement()
    : mDBConnection (nsnull), mDBStatement(nsnull)
{
}

NS_IMETHODIMP
mozStorageStatement::Initialize(mozIStorageConnection *aDBConnection, const nsACString & aSQLStatement)
{
    int srv;

    sqlite3 *db = nsnull;
    // XXX - need to implement a private iid to QI for here, to make sure
    // we have a real mozStorageConnection
    mozStorageConnection *msc = NS_STATIC_CAST(mozStorageConnection*, aDBConnection);
    db = msc->GetNativeConnection();
    NS_ENSURE_TRUE(db != nsnull, NS_ERROR_NULL_POINTER);

    srv = sqlite3_prepare (db, nsPromiseFlatCString(aSQLStatement).get(), aSQLStatement.Length(), &mDBStatement, NULL);
    if (srv != SQLITE_OK) {
        fprintf (stderr, "SQLITE ERROR: Statement: '%s'\n", nsPromiseFlatCString(aSQLStatement).get());
        fprintf (stderr, "SQLITE ERROR: %d '%s'\n", srv, sqlite3_errmsg(db));
        return NS_ERROR_FAILURE;
    }

    mDBConnection = aDBConnection;
    mStatementString.Assign (aSQLStatement);
    mParamCount = sqlite3_bind_parameter_count (mDBStatement);
    mResultColumnCount = sqlite3_column_count (mDBStatement);

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
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void reset (); */
NS_IMETHODIMP
mozStorageStatement::Reset()
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    sqlite3_reset(mDBStatement);

    return NS_OK;
}

/* void bindCStringParameter (in unsigned long aParamIndex, in string aValue); */
NS_IMETHODIMP
mozStorageStatement::BindCStringParameter(PRUint32 aParamIndex, const char *aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_text (mDBStatement, aParamIndex + 1, aValue, -1, SQLITE_TRANSIENT);
    // XXX check return value for errors?

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

/* void bindWStringParameter (in unsigned long aParamIndex, in wstring aValue); */
NS_IMETHODIMP
mozStorageStatement::BindWStringParameter(PRUint32 aParamIndex, const PRUnichar *aValue)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");
    if (aParamIndex < 0 || aParamIndex >= mParamCount)
        return NS_ERROR_FAILURE; // XXXerror

    sqlite3_bind_text16 (mDBStatement, aParamIndex + 1, aValue, -1, SQLITE_TRANSIENT);
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

/* void bindDataParameter (in unsigned long aParamIndex, [array, size_is (aValueSize)] in octet aValue, in unsigned long aValueSize); */
NS_IMETHODIMP
mozStorageStatement::BindDataParameter(PRUint32 aParamIndex, PRUint8 *aValue, PRUint32 aValueSize)
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

    int srv = sqlite3_step (mDBStatement);
    if (srv == SQLITE_MISUSE || srv == SQLITE_ERROR) {
#ifdef PR_LOGGING
        nsCAutoString errStr;
        mDBConnection->GetLastErrorString(errStr);
        PR_LOG(gStorageLog, PR_LOG_DEBUG, ("mozStorageStatement::Execute error: %s", errStr.get()));
#endif
        return NS_ERROR_FAILURE; // XXX error code
    }
    sqlite3_reset (mDBStatement);
    
    return NS_OK;
}

/* mozIStorageDataSet executeDataSet (); */
NS_IMETHODIMP
mozStorageStatement::ExecuteDataSet(mozIStorageDataSet **_retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean executeStep (); */
NS_IMETHODIMP
mozStorageStatement::ExecuteStep(PRBool *_retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    int srv = sqlite3_step (mDBStatement);

#ifdef PR_LOGGING
    if (srv != SQLITE_ROW && srv != SQLITE_DONE) {
        nsCAutoString errStr;
        mDBConnection->GetLastErrorString(errStr);
        PR_LOG(gStorageLog, PR_LOG_DEBUG, ("mozStorageStatement::ExecuteStep error: %s", errStr.get()));
    }
#endif

    // SQLITE_ROW and SQLITE_DONE are non-errors
    if (srv == SQLITE_ROW) {
        // we got a row back
        *_retval = PR_TRUE;
        return NS_OK;
    } else if (srv == SQLITE_DONE) {
        // statement is done (no row returned)
        *_retval = PR_FALSE;
        return NS_OK;
    } else if (srv == SQLITE_BUSY) {
        // ??? what to do?
        return NS_ERROR_FAILURE;
    } else if (srv == SQLITE_MISUSE) {
        // bad stuff happened
        return NS_ERROR_FAILURE;
    } else if (srv == SQLITE_ERROR) {
        // even worse stuff happened
    } else {
        // something that shouldn't happen happened
        NS_ERROR ("sqlite3_step returned an error code we don't know about!");
    }

    // shouldn't get here
    return NS_ERROR_FAILURE;
}

#if 0
/* nsISimpleEnumerator executeEnumerator (); */
NS_IMETHODIMP
mozStorageStatement::ExecuteEnumerator(nsISimpleEnumerator **_retval)
{
    NS_ASSERTION (mDBConnection && mDBStatement, "statement not initialized");

    mozStorageStatementRowEnumerator *mssre = new mozStorageStatementRowEnumerator (mDBStatement);
    NS_ADDREF(mssre);
    *_retval = mssre;

    return NS_OK;
}
#endif

/***
 *** mozIStorageValueArray
 ***/

/* readonly attribute unsigned long length; */
NS_IMETHODIMP
mozStorageStatement::GetNumColumns(PRUint32 *aLength)
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

/* long getAsInt32 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsInt32(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    *_retval = sqlite3_column_int (mDBStatement, aIndex);

    return NS_OK;
}

/* long long getAsInt64 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsInt64(PRUint32 aIndex, PRInt64 *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    *_retval = sqlite3_column_int64 (mDBStatement, aIndex);

    return NS_OK;
}

/* double getAsDouble (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsDouble(PRUint32 aIndex, double *_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    *_retval = sqlite3_column_double (mDBStatement, aIndex);

    return NS_OK;
}

/* string getAsCString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsCString(PRUint32 aIndex, char **_retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    int slen = sqlite3_column_bytes (mDBStatement, aIndex);
    const unsigned char *cstr = sqlite3_column_text (mDBStatement, aIndex);
    char *str = (char *) nsMemory::Clone (cstr, slen);
    if (str == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = str;
    return NS_OK;
}

/* AUTF8String getAsUTF8String (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsUTF8String(PRUint32 aIndex, nsACString & _retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    int slen = sqlite3_column_bytes (mDBStatement, aIndex);
    const unsigned char *cstr = sqlite3_column_text (mDBStatement, aIndex);
    _retval.Assign ((char *) cstr, slen);

    return NS_OK;
}

/* AString getAsString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatement::GetAsString(PRUint32 aIndex, nsAString & _retval)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    int slen = sqlite3_column_bytes16 (mDBStatement, aIndex);
    const PRUnichar *wstr = (const PRUnichar *) sqlite3_column_text16 (mDBStatement, aIndex);
    _retval.Assign (wstr, slen/2);

    return NS_OK;
}

/* void getAsBlob (in unsigned long aIndex, [array, size_is (aDataSize)] out octet aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageStatement::GetAsBlob(PRUint32 aIndex, PRUint8 **aData, PRUint32 *aDataSize)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    int blobsize = sqlite3_column_bytes (mDBStatement, aIndex);
    const void *blob = sqlite3_column_blob (mDBStatement, aIndex);

    void *blobcopy = nsMemory::Clone(blob, blobsize);
    if (blobcopy == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *aData = (PRUint8*) blobcopy;
    *aDataSize = blobsize;

    return NS_OK;
}

/* [notxpcom] long asInt32 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt32)
mozStorageStatement::AsInt32(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    return sqlite3_column_int (mDBStatement, aIndex);
}

/* [notxpcom] long long asInt64 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt64)
mozStorageStatement::AsInt64(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    return sqlite3_column_int64 (mDBStatement, aIndex);
}

/* [notxpcom] double asDouble (in unsigned long aIndex); */
NS_IMETHODIMP_(double)
mozStorageStatement::AsDouble(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    return sqlite3_column_double (mDBStatement, aIndex);
}

/* [notxpcom] string asSharedCString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(char *)
mozStorageStatement::AsSharedCString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_column_bytes (mDBStatement, aIndex);
        *aLength = slen;
    }

    return (char *) sqlite3_column_text (mDBStatement, aIndex);
}

/* [notxpcom] wstring asSharedWString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(PRUnichar *)
mozStorageStatement::AsSharedWString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_column_bytes16 (mDBStatement, aIndex);
        *aLength = slen;
    }

    return (PRUnichar *) sqlite3_column_text16 (mDBStatement, aIndex);
}

/* [noscript] void asSharedBlob (in unsigned long aIndex, [shared] out voidPtr aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageStatement::AsSharedBlob(PRUint32 aIndex, const void * *aData, PRUint32 *aDataSize)
{
    *aDataSize = sqlite3_column_bytes (mDBStatement, aIndex);
    *aData = sqlite3_column_blob (mDBStatement, aIndex);

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


/* [notxpcom] boolean isNull (in unsigned long aIndex); */
PRBool
mozStorageStatement::IsNull(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mResultColumnCount, "aIndex out of range");

    return (sqlite3_column_type (mDBStatement, aIndex) == SQLITE_NULL);
}
