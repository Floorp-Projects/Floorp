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

#include "nsError.h"
#include "nsMemory.h"
#include "nsString.h"

#include "mozStorageValueArray.h"

/***
 *** mozStorageStatementRowValueArray
 ***/

/* Implementation file */
NS_IMPL_ISUPPORTS1(mozStorageStatementRowValueArray, mozIStorageValueArray)

mozStorageStatementRowValueArray::mozStorageStatementRowValueArray(sqlite3_stmt *aSqliteStmt)
{
    mSqliteStatement = aSqliteStmt;
    mNumColumns = sqlite3_data_count (aSqliteStmt);
}

mozStorageStatementRowValueArray::~mozStorageStatementRowValueArray()
{
    /* do nothing, we don't own the stmt */
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetNumColumns(PRUint32 *aLength)
{
    *aLength = mNumColumns;
    return NS_OK;
}

/* long getTypeOfIndex (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetTypeOfIndex(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    int t = sqlite3_column_type (mSqliteStatement, aIndex);
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
mozStorageStatementRowValueArray::GetAsInt32(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    *_retval = sqlite3_column_int (mSqliteStatement, aIndex);

    return NS_OK;
}

/* long long getAsInt64 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsInt64(PRUint32 aIndex, PRInt64 *_retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    *_retval = sqlite3_column_int64 (mSqliteStatement, aIndex);

    return NS_OK;
}

/* double getAsDouble (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsDouble(PRUint32 aIndex, double *_retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    *_retval = sqlite3_column_double (mSqliteStatement, aIndex);

    return NS_OK;
}

/* string getAsCString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsCString(PRUint32 aIndex, char **_retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    int slen = sqlite3_column_bytes (mSqliteStatement, aIndex);
    const unsigned char *cstr = sqlite3_column_text (mSqliteStatement, aIndex);
    char *str = (char *) nsMemory::Clone (cstr, slen);
    if (str == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = str;
    return NS_OK;
}

/* AUTF8String getAsUTF8String (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsUTF8String(PRUint32 aIndex, nsACString & _retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    int slen = sqlite3_column_bytes (mSqliteStatement, aIndex);
    const unsigned char *cstr = sqlite3_column_text (mSqliteStatement, aIndex);
    _retval.Assign ((char *) cstr, slen);

    return NS_OK;
}

/* AString getAsString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsString(PRUint32 aIndex, nsAString & _retval)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    int slen = sqlite3_column_bytes16 (mSqliteStatement, aIndex);
    const PRUnichar *wstr = (const PRUnichar *) sqlite3_column_text16 (mSqliteStatement, aIndex);
    _retval.Assign (wstr, slen/2);

    return NS_OK;
}

/* void getAsBlob (in unsigned long aIndex, [array, size_is (aDataSize)] out octet aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetAsBlob(PRUint32 aIndex, PRUint8 **aData, PRUint32 *aDataSize)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    int blobsize = sqlite3_column_bytes (mSqliteStatement, aIndex);
    const void *blob = sqlite3_column_blob (mSqliteStatement, aIndex);

    void *blobcopy = nsMemory::Clone(blob, blobsize);
    if (blobcopy == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *aData = (PRUint8*) blobcopy;
    *aDataSize = blobsize;

    return NS_OK;
}

/* [notxpcom] long asInt32 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt32)
mozStorageStatementRowValueArray::AsInt32(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    return sqlite3_column_int (mSqliteStatement, aIndex);
}

/* [notxpcom] long long asInt64 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt64)
mozStorageStatementRowValueArray::AsInt64(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    return sqlite3_column_int64 (mSqliteStatement, aIndex);
}

/* [notxpcom] double asDouble (in unsigned long aIndex); */
NS_IMETHODIMP_(double)
mozStorageStatementRowValueArray::AsDouble(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    return sqlite3_column_double (mSqliteStatement, aIndex);
}

/* [notxpcom] string asSharedCString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(char *)
mozStorageStatementRowValueArray::AsSharedCString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_column_bytes (mSqliteStatement, aIndex);
        *aLength = slen;
    }

    return (char *) sqlite3_column_text (mSqliteStatement, aIndex);
}

/* [notxpcom] wstring asSharedWString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(PRUnichar *)
mozStorageStatementRowValueArray::AsSharedWString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_column_bytes16 (mSqliteStatement, aIndex);
        *aLength = slen;
    }

    return (PRUnichar *) sqlite3_column_text16 (mSqliteStatement, aIndex);
}

/* [noscript] void asSharedBlob (in unsigned long aIndex, [shared] out voidPtr aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::AsSharedBlob(PRUint32 aIndex, const void * *aData, PRUint32 *aDataSize)
{
    *aDataSize = sqlite3_column_bytes (mSqliteStatement, aIndex);
    *aData = sqlite3_column_blob (mSqliteStatement, aIndex);

    return NS_OK;
}

/* boolean getIsNull (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageStatementRowValueArray::GetIsNull(PRUint32 aIndex, PRBool *_retval)
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
mozStorageStatementRowValueArray::IsNull(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mNumColumns, "aIndex out of range");

    return (sqlite3_column_type (mSqliteStatement, aIndex) == SQLITE_NULL);
}

/***
 *** mozStorageArgvValueArray
 ***/

/* Implementation file */
NS_IMPL_ISUPPORTS1(mozStorageArgvValueArray, mozIStorageValueArray)

mozStorageArgvValueArray::mozStorageArgvValueArray(PRInt32 aArgc, sqlite3_value **aArgv)
    : mArgc(aArgc), mArgv(aArgv)
{
}

mozStorageArgvValueArray::~mozStorageArgvValueArray()
{
    /* do nothing, we don't own the array */
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP
mozStorageArgvValueArray::GetNumColumns(PRUint32 *aLength)
{
    *aLength = mArgc;
    return NS_OK;
}

/* long getTypeOfIndex (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetTypeOfIndex(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    int t = sqlite3_value_type (mArgv[aIndex]);
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
mozStorageArgvValueArray::GetAsInt32(PRUint32 aIndex, PRInt32 *_retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    *_retval = sqlite3_value_int (mArgv[aIndex]);

    return NS_OK;
}

/* long long getAsInt64 (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsInt64(PRUint32 aIndex, PRInt64 *_retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    *_retval = sqlite3_value_int64 (mArgv[aIndex]);

    return NS_OK;
}

/* double getAsDouble (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsDouble(PRUint32 aIndex, double *_retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    *_retval = sqlite3_value_double (mArgv[aIndex]);

    return NS_OK;
}

/* string getAsCString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsCString(PRUint32 aIndex, char **_retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    int slen = sqlite3_value_bytes (mArgv[aIndex]);
    const unsigned char *cstr = sqlite3_value_text (mArgv[aIndex]);
    char *str = (char *) nsMemory::Clone (cstr, slen);
    if (str == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = str;
    return NS_OK;
}

/* AUTF8String getAsUTF8String (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsUTF8String(PRUint32 aIndex, nsACString & _retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    int slen = sqlite3_value_bytes (mArgv[aIndex]);
    const unsigned char *cstr = sqlite3_value_text (mArgv[aIndex]);
    _retval.Assign ((char *) cstr, slen);

    return NS_OK;
}

/* AString getAsString (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsString(PRUint32 aIndex, nsAString & _retval)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    int slen = sqlite3_value_bytes16 (mArgv[aIndex]);
    const PRUnichar *wstr = (const PRUnichar *) sqlite3_value_text16 (mArgv[aIndex]);
    _retval.Assign (wstr, slen);

    return NS_OK;
}

/* void getAsBlob (in unsigned long aIndex, [array, size_is (aDataSize)] out octet aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetAsBlob(PRUint32 aIndex, PRUint8 **aData, PRUint32 *aDataSize)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    int blobsize = sqlite3_value_bytes (mArgv[aIndex]);
    const void *blob = sqlite3_value_blob (mArgv[aIndex]);

    void *blobcopy = nsMemory::Clone(blob, blobsize);
    if (blobcopy == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    *aData = (PRUint8*) blobcopy;
    *aDataSize = blobsize;

    return NS_OK;
}

/* [notxpcom] long asInt32 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt32)
mozStorageArgvValueArray::AsInt32(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    return sqlite3_value_int (mArgv[aIndex]);
}

/* [notxpcom] long long asInt64 (in unsigned long aIndex); */
NS_IMETHODIMP_(PRInt64)
mozStorageArgvValueArray::AsInt64(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    return sqlite3_value_int64 (mArgv[aIndex]);
}

/* [notxpcom] double asDouble (in unsigned long aIndex); */
NS_IMETHODIMP_(double)
mozStorageArgvValueArray::AsDouble(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    return sqlite3_value_double (mArgv[aIndex]);
}

/* [notxpcom] string asSharedCString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(char *)
mozStorageArgvValueArray::AsSharedCString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_value_bytes (mArgv[aIndex]);
        *aLength = slen;
    }

    return (char *) sqlite3_value_text (mArgv[aIndex]);
}

/* [notxpcom] wstring asSharedWString (in unsigned long aIndex, out unsigned long aLength); */
NS_IMETHODIMP_(PRUnichar *)
mozStorageArgvValueArray::AsSharedWString(PRUint32 aIndex, PRUint32 *aLength)
{
    if (aLength) {
        int slen = sqlite3_value_bytes16 (mArgv[aIndex]);
        *aLength = slen;
    }

    return (PRUnichar *) sqlite3_value_text16 (mArgv[aIndex]);
}

/* [noscript] void asSharedBlob (in unsigned long aIndex, [shared] out voidPtr aData, out unsigned long aDataSize); */
NS_IMETHODIMP
mozStorageArgvValueArray::AsSharedBlob(PRUint32 aIndex, const void * *aData, PRUint32 *aDataSize)
{
    *aDataSize = sqlite3_value_bytes (mArgv[aIndex]);
    *aData = sqlite3_value_blob (mArgv[aIndex]);

    return NS_OK;
}

/* boolean getIsNull (in unsigned long aIndex); */
NS_IMETHODIMP
mozStorageArgvValueArray::GetIsNull(PRUint32 aIndex, PRBool *_retval)
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
mozStorageArgvValueArray::IsNull(PRUint32 aIndex)
{
    NS_ASSERTION (aIndex < mArgc, "aIndex out of range");

    return (sqlite3_value_type (mArgv[aIndex]) == SQLITE_NULL);
}

/***
 *** mozStorageDataSet
 ***/

NS_IMPL_ISUPPORTS1(mozStorageDataSet, mozIStorageDataSet)

mozStorageDataSet::mozStorageDataSet(nsIArray *aRows)
    : mRows(aRows)
{
}

mozStorageDataSet::~mozStorageDataSet()
{
}

/* readonly attribute nsIArray dataRows; */
NS_IMETHODIMP
mozStorageDataSet::GetDataRows(nsIArray **aDataRows)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator getRowEnumerator (); */
NS_IMETHODIMP
mozStorageDataSet::GetRowEnumerator(nsISimpleEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
