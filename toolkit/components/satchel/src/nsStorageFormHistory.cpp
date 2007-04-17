/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   Brett Wilson <brettw@gmail.com>
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

#include "nsStorageFormHistory.h"

#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsVoidArray.h"
#include "nsCOMArray.h"
#include "mozStorageHelper.h"
#include "mozStorageCID.h"
#include "nsIAutoCompleteSimpleResult.h"
#include "nsTArray.h"

// The size of the database cache. This is the number of database PAGES that
// can be cached in memory. Normally, pages are 1K unless the size has been
// explicitly changed.
//
// 4MB should be much larger than normal form histories. Normal form histories
// will be several hundered KB at most. If the form history is smaller, the
// extra memory will never be allocated, so there is no penalty for larger
// numbers. See StartCache
#define DATABASE_CACHE_PAGES 4000

// nsFormHistoryResult is a specialized autocomplete result class that knows
// how to remove entries from the form history table.
class nsFormHistoryResult : public nsIAutoCompleteSimpleResult
{
public:
  nsFormHistoryResult(const nsAString &aFieldName)
    : mFieldName(aFieldName) {}

  nsresult Init();

  NS_DECL_ISUPPORTS

  // Forward everything except RemoveValueAt to the internal result
  NS_IMETHOD GetSearchString(nsAString &_result)
  { return mResult->GetSearchString(_result); }
  NS_IMETHOD GetSearchResult(PRUint16 *_result)
  { return mResult->GetSearchResult(_result); }
  NS_IMETHOD GetDefaultIndex(PRInt32 *_result)
  { return mResult->GetDefaultIndex(_result); }
  NS_IMETHOD GetErrorDescription(nsAString &_result)
  { return mResult->GetErrorDescription(_result); }
  NS_IMETHOD GetMatchCount(PRUint32 *_result)
  { return mResult->GetMatchCount(_result); }
  NS_IMETHOD GetValueAt(PRInt32 aIndex, nsAString &_result)
  { return mResult->GetValueAt(aIndex, _result); }
  NS_IMETHOD GetCommentAt(PRInt32 aIndex, nsAString &_result)
  { return mResult->GetCommentAt(aIndex, _result); }
  NS_IMETHOD GetStyleAt(PRInt32 aIndex, nsAString &_result)
  { return mResult->GetStyleAt(aIndex, _result); }
  NS_IMETHOD RemoveValueAt(PRInt32 aRowIndex, PRBool aRemoveFromDB);
  NS_FORWARD_NSIAUTOCOMPLETESIMPLERESULT(mResult->)

protected:
  nsCOMPtr<nsIAutoCompleteSimpleResult> mResult;
  nsString mFieldName;
};

NS_IMPL_ISUPPORTS2(nsFormHistoryResult,
                   nsIAutoCompleteResult, nsIAutoCompleteSimpleResult)

nsresult
nsFormHistoryResult::Init()
{
  nsresult rv;
  mResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  return rv;
}

NS_IMETHODIMP
nsFormHistoryResult::RemoveValueAt(PRInt32 aRowIndex, PRBool aRemoveFromDB)
{
  if (!aRemoveFromDB) {
    return mResult->RemoveValueAt(aRowIndex, aRemoveFromDB);
  }

  nsAutoString value;
  nsresult rv = mResult->GetValueAt(aRowIndex, value);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mResult->RemoveValueAt(aRowIndex, aRemoveFromDB);
  NS_ENSURE_SUCCESS(rv, rv);

  nsFormHistory* fh = nsFormHistory::GetInstance();
  NS_ENSURE_TRUE(fh, NS_ERROR_OUT_OF_MEMORY);
  return fh->RemoveEntry(mFieldName, value);
}

#define PREF_FORMFILL_BRANCH "browser.formfill."
#define PREF_FORMFILL_ENABLE "enable"

NS_INTERFACE_MAP_BEGIN(nsFormHistory)
  NS_INTERFACE_MAP_ENTRY(nsIFormHistory2)
  NS_INTERFACE_MAP_ENTRY(nsIFormHistoryPrivate)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIFormSubmitObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsFormHistory)
NS_IMPL_RELEASE(nsFormHistory)

PRBool nsFormHistory::gFormHistoryEnabled = PR_FALSE;
PRBool nsFormHistory::gPrefsInitialized = PR_FALSE;
nsFormHistory* nsFormHistory::gFormHistory = nsnull;

nsFormHistory::nsFormHistory()
{
  NS_ASSERTION(!gFormHistory, "nsFormHistory must be used as a service");
  gFormHistory = this;
}

nsFormHistory::~nsFormHistory()
{
  NS_ASSERTION(gFormHistory == this,
               "nsFormHistory must be used as a service");
  gFormHistory = nsnull;
}

nsresult
nsFormHistory::Init()
{
  nsresult rv = OpenDatabase();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> service = do_GetService("@mozilla.org/observer-service;1");
  if (service)
    service->AddObserver(this, NS_EARLYFORMSUBMIT_SUBJECT, PR_TRUE);

  return NS_OK;
}

/* static */ PRBool
nsFormHistory::FormHistoryEnabled()
{
  if (!gPrefsInitialized) {
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);

    prefService->GetBranch(PREF_FORMFILL_BRANCH,
                           getter_AddRefs(gFormHistory->mPrefBranch));
    gFormHistory->mPrefBranch->GetBoolPref(PREF_FORMFILL_ENABLE,
                                           &gFormHistoryEnabled);

    nsCOMPtr<nsIPrefBranch2> branchInternal =
      do_QueryInterface(gFormHistory->mPrefBranch);
    branchInternal->AddObserver(PREF_FORMFILL_ENABLE, gFormHistory, PR_TRUE);

    gPrefsInitialized = PR_TRUE;
  }

  return gFormHistoryEnabled;
}


////////////////////////////////////////////////////////////////////////
//// nsIFormHistory2

NS_IMETHODIMP
nsFormHistory::GetHasEntries(PRBool *aHasEntries)
{
  mozStorageStatementScoper scope(mDBSelectEntries);

  PRBool hasMore;
  *aHasEntries = NS_SUCCEEDED(mDBSelectEntries->ExecuteStep(&hasMore)) && hasMore;
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::AddEntry(const nsAString &aName, const nsAString &aValue)
{
  if (!FormHistoryEnabled())
    return NS_OK;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRBool exists;
  EntryExists(aName, aValue, &exists);
  if (!exists) {
    mozStorageStatementScoper scope(mDBInsertNameValue);
    nsresult rv = mDBInsertNameValue->BindStringParameter(0, aName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBInsertNameValue->BindStringParameter(1, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBInsertNameValue->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return transaction.Commit();
}

NS_IMETHODIMP
nsFormHistory::EntryExists(const nsAString &aName, 
                           const nsAString &aValue, PRBool *_retval)
{
  mozStorageStatementScoper scope(mDBFindEntry);

  nsresult rv = mDBFindEntry->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBFindEntry->BindStringParameter(1, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore;
  *_retval = NS_SUCCEEDED(mDBFindEntry->ExecuteStep(&hasMore)) && hasMore;
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::NameExists(const nsAString &aName, PRBool *_retval)
{
  mozStorageStatementScoper scope(mDBFindEntryByName);

  nsresult rv = mDBFindEntryByName->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore;
  *_retval = (NS_SUCCEEDED(mDBFindEntryByName->ExecuteStep(&hasMore)) &&
              hasMore);
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::RemoveEntry(const nsAString &aName, const nsAString &aValue)
{
  nsCOMPtr<mozIStorageStatement> dbDelete;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_formhistory WHERE fieldname=?1 AND value=?2"),
                                         getter_AddRefs(dbDelete));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = dbDelete->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = dbDelete->BindStringParameter(1, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return dbDelete->Execute();
}

NS_IMETHODIMP
nsFormHistory::RemoveEntriesForName(const nsAString &aName)
{
  nsCOMPtr<mozIStorageStatement> dbDelete;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_formhistory WHERE fieldname=?1"),
                                         getter_AddRefs(dbDelete));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = dbDelete->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv,rv);

  return dbDelete->Execute();
}

NS_IMETHODIMP
nsFormHistory::RemoveAllEntries()
{
  nsCOMPtr<mozIStorageStatement> dbDeleteAll;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM moz_formhistory"),
                                         getter_AddRefs(dbDeleteAll));
  NS_ENSURE_SUCCESS(rv,rv);

  return dbDeleteAll->Execute();
}

////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsFormHistory::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData) 
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    mPrefBranch->GetBoolPref(PREF_FORMFILL_ENABLE, &gFormHistoryEnabled);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIFormSubmitObserver

NS_IMETHODIMP
nsFormHistory::Notify(nsIDOMHTMLFormElement* formElt, nsIDOMWindowInternal* aWindow, nsIURI* aActionURL, PRBool* aCancelSubmit)
{
  if (!FormHistoryEnabled())
    return NS_OK;

  NS_NAMED_LITERAL_STRING(kAutoComplete, "autocomplete");
  nsAutoString autocomplete;
  formElt->GetAttribute(kAutoComplete, autocomplete);
  if (autocomplete.LowerCaseEqualsLiteral("off"))
    return NS_OK;

  nsCOMPtr<nsIDOMHTMLCollection> elts;
  formElt->GetElements(getter_AddRefs(elts));

  PRUint32 length;
  elts->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> node;
    elts->Item(i, getter_AddRefs(node));
    nsCOMPtr<nsIDOMHTMLInputElement> inputElt = do_QueryInterface(node);
    if (inputElt) {
      // Filter only inputs that are of type "text" without autocomplete="off"
      nsAutoString type;
      inputElt->GetType(type);
      if (!type.LowerCaseEqualsLiteral("text"))
        continue;

      nsAutoString autocomplete;
      inputElt->GetAttribute(kAutoComplete, autocomplete);
      if (!autocomplete.LowerCaseEqualsLiteral("off")) {
        // If this input has a name/id and value, add it to the database
        nsAutoString value;
        inputElt->GetValue(value);
        if (!value.IsEmpty()) {
          nsAutoString name;
          inputElt->GetName(name);
          if (name.IsEmpty())
            inputElt->GetId(name);
          if (!name.IsEmpty())
            AddEntry(name, value);
        }
      }
    }
  }

  return NS_OK;
}
nsresult
nsFormHistory::OpenDatabase()
{
  // init DB service and obtain a connection
  nsresult rv;
  mStorageService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> formHistoryFile;
  rv = GetDatabaseFile(getter_AddRefs(formHistoryFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mStorageService->OpenDatabase(formHistoryFile, getter_AddRefs(mDBConn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = formHistoryFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mStorageService->OpenDatabase(formHistoryFile, getter_AddRefs(mDBConn));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // We execute many statements before the database cache is started to create
  // the tables (which can not be done which the cache is locked in memory by
  // the dummy statement--see StartCache). This transaction will keep the cache
  // between these statements, which should improve startup performance because
  // we won't have to keep requesting pages from the OS.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRBool exists;
  mDBConn->TableExists(NS_LITERAL_CSTRING("moz_formhistory"), &exists);
  if (!exists) {
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_formhistory (id INTEGER PRIMARY KEY, fieldname LONGVARCHAR, value LONGVARCHAR)"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE INDEX moz_formhistory_index ON moz_formhistory (fieldname)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT * FROM moz_formhistory"),
                                getter_AddRefs(mDBSelectEntries));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT * FROM moz_formhistory WHERE fieldname=?1 AND value=?2"),
                                getter_AddRefs(mDBFindEntry));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT * FROM moz_formhistory WHERE fieldname=?1"),
                                getter_AddRefs(mDBFindEntryByName));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT value FROM moz_formhistory WHERE fieldname=?1"),
                                getter_AddRefs(mDBGetMatchingField));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_formhistory (fieldname, value) VALUES (?1, ?2)"),
                                getter_AddRefs(mDBInsertNameValue));
  NS_ENSURE_SUCCESS(rv, rv);

  // should commit before starting cache
  transaction.Commit();

  // ignore errors since the cache is not critical for operation
  StartCache();

#ifdef MOZ_MORKREADER
  if (!exists) {
    // Locate the old formhistory.dat file and import it.
    nsCOMPtr<nsIFile> historyFile;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(historyFile));
    if (NS_SUCCEEDED(rv)) {
      historyFile->Append(NS_LITERAL_STRING("formhistory.dat"));

      nsCOMPtr<nsIFormHistoryImporter> importer = new nsFormHistoryImporter();
      NS_ENSURE_TRUE(importer, NS_ERROR_OUT_OF_MEMORY);
      importer->ImportFormHistory(historyFile, this);
    }
  }
#endif

  return NS_OK;
}


nsresult
nsFormHistory::GetDatabaseFile(nsIFile** aFile)
{
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  return (*aFile)->Append(NS_LITERAL_STRING("formhistory.sqlite"));
}


// nsFormHistory::StartCache
//
//    This function starts the dummy statement that locks the cache in memory.
//    As long as there is an open connection sharing the same cache, the cache
//    won't be expired. Therefore, we create a dummy table with some data in
//    it, and open a statement over the data. As long as this statement is
//    open, we can go fast.
//
//    This dummy statement prevents the schema from being modified. If you
//    want to add or change a table or index schema, you must stop the dummy
//    statement first. See nsNavHistory::StartDummyStatement for a slightly
//    more detailed discussion.
//
//    Note that we should not use a transaction in this function since that
//    will commit the dummy statement and everything will break.
//
//    This function also initializes the cache.

nsresult
nsFormHistory::StartCache()
{
  // do nothing if the dummy statement is already running
  if (mDummyStatement)
    return NS_OK;

  // dummy database connection
  nsCOMPtr<nsIFile> formHistoryFile;
  nsresult rv = GetDatabaseFile(getter_AddRefs(formHistoryFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mStorageService->OpenDatabase(formHistoryFile,
                                     getter_AddRefs(mDummyConnection));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure the dummy table exists
  PRBool tableExists;
  rv = mDummyConnection->TableExists(NS_LITERAL_CSTRING("moz_dummy_table"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! tableExists) {
    rv = mDummyConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("CREATE TABLE moz_dummy_table (id INTEGER PRIMARY KEY)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This table is guaranteed to have something in it and will keep the dummy
  // statement open. If the table is empty, it won't hold the statement open.
  // the PRIMARY KEY value on ID means that it is unique. The OR IGNORE means
  // that if there is already a value of 1 there, this insert will be ignored,
  // which is what we want so as to avoid growing the table infinitely.
  rv = mDummyConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT OR IGNORE INTO moz_dummy_table VALUES (1)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDummyConnection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_dummy_table LIMIT 1"),
    getter_AddRefs(mDummyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // we have to step the dummy statement so that it will hold a lock on the DB
  PRBool dummyHasResults;
  rv = mDummyStatement->ExecuteStep(&dummyHasResults);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the cache size
  nsCAutoString cacheSizePragma("PRAGMA cache_size=");
  cacheSizePragma.AppendInt(DATABASE_CACHE_PAGES);
  rv = mDummyConnection->ExecuteSimpleSQL(cacheSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // preload the cache
  rv = mDummyConnection->Preload();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsFormHistory::StopCache
//
//    Call this before doing any schema modifying operations. You should
//    start the dummy statement again to give good performance.
//    See StartCache.

nsresult
nsFormHistory::StopCache()
{
  // do nothing if the dummy statement isn't running
  if (! mDummyStatement)
    return NS_OK;

  nsresult rv = mDummyStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  mDummyStatement = nsnull;
  return NS_OK;
}


nsresult
nsFormHistory::AutoCompleteSearch(const nsAString &aInputName,
                                  const nsAString &aInputValue,
                                  nsIAutoCompleteSimpleResult *aPrevResult,
                                  nsIAutoCompleteResult **aResult)
{
  if (!FormHistoryEnabled())
    return NS_OK;

  nsCOMPtr<nsIAutoCompleteSimpleResult> result;

  if (aPrevResult) {
    result = aPrevResult;

    PRUint32 matchCount;
    result->GetMatchCount(&matchCount);

    for (PRInt32 i = matchCount - 1; i >= 0; --i) {
      nsAutoString match;
      result->GetValueAt(i, match);
      if (!StringBeginsWith(match, aInputValue,
                            nsCaseInsensitiveStringComparator())) {
        result->RemoveValueAt(i, PR_FALSE);
      }
    }
  } else {
    nsCOMPtr<nsFormHistoryResult> fhResult =
      new nsFormHistoryResult(aInputName);
    NS_ENSURE_TRUE(fhResult, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = fhResult->Init();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_REINTERPRET_CAST(nsCOMPtr<nsIAutoCompleteSimpleResult>*, &fhResult)->swap(result);

    result->SetSearchString(aInputValue);

    // generates query string		
    mozStorageStatementScoper scope(mDBGetMatchingField);
    rv = mDBGetMatchingField->BindStringParameter(0, aInputName);
    NS_ENSURE_SUCCESS(rv,rv);

    PRBool hasMore = PR_FALSE;
    PRUint32 count = 0;
    while (NS_SUCCEEDED(mDBGetMatchingField->ExecuteStep(&hasMore)) &&
           hasMore) {
      nsAutoString entryString;
      mDBGetMatchingField->GetString(0, entryString);
      // filters out irrelevant results
      if(StringBeginsWith(entryString, aInputValue,
                          nsCaseInsensitiveStringComparator())) {
        result->AppendMatch(entryString, EmptyString());
        ++count;
      }
    }
    if (count > 0) {
      result->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS);
      result->SetDefaultIndex(0);
    } else {
      result->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
      result->SetDefaultIndex(-1);
    }
  }

  *aResult = result;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

#ifdef MOZ_MORKREADER

// Columns for form history rows
enum {
  kNameColumn,
  kValueColumn,
  kColumnCount // keep me last
};

static const char * const gColumnNames[] = {
  "Name", "Value"
};

struct FormHistoryImportClosure
{
  FormHistoryImportClosure(nsMorkReader *aReader, nsIFormHistory2 *aFormHistory)
    : reader(aReader), formHistory(aFormHistory), byteOrderColumn(-1),
      swapBytes(PR_FALSE)
  {
    for (PRUint32 i = 0; i < kColumnCount; ++i) {
      columnIndexes[i] = -1;
    }
  }

  // Back pointers to the reader and history we're operating on
  const nsMorkReader *reader;
  nsIFormHistory2 *formHistory;

  // Indexes of the columns that we care about
  PRInt32 columnIndexes[kColumnCount];
  PRInt32 byteOrderColumn;

  // Whether we need to swap bytes (file format is other-endian)
  PRPackedBool swapBytes;
};

// Reverses the high and low bytes in a PRUnichar buffer.
// This is used if the file format has a different endianness from the
// current architecture.
static void SwapBytes(PRUnichar* aBuffer)
{
  for (PRUnichar *b = aBuffer; *b; b++)
  {
    PRUnichar c = *b;
    *b = (0xff & (c >> 8)) | (c << 8);
  }
}

// Enumerator callback to add an entry to the FormHistory
/* static */ PLDHashOperator PR_CALLBACK
nsFormHistoryImporter::AddToFormHistoryCB(const nsCSubstring &aRowID,
                                          const nsTArray<nsCString> *aValues,
                                          void *aData)
{
  FormHistoryImportClosure *data = NS_STATIC_CAST(FormHistoryImportClosure*,
                                                  aData);
  const nsMorkReader *reader = data->reader;
  nsCString values[kColumnCount];
  const PRUnichar* valueStrings[kColumnCount];
  PRUint32 valueLengths[kColumnCount];
  const PRInt32 *columnIndexes = data->columnIndexes;
  PRInt32 i;

  // Values are in UTF16.

  for (i = 0; i < kColumnCount; ++i) {
    if (columnIndexes[i] == -1) {
      // We didn't find this column in the map
      continue;
    }

    values[i] = (*aValues)[columnIndexes[i]];
    reader->NormalizeValue(values[i]);

    PRUint32 length;
    const char *bytes;
    if (values[i].IsEmpty()) {
      bytes = "\0";
      length = 0;
    } else {
      length = values[i].Length() / 2;

      // add an extra null byte onto the end, so that the buffer ends
      // with a complete unicode null character.
      values[i].Append('\0');

      // Swap the bytes in the unicode characters if necessary.
      if (data->swapBytes) {
        SwapBytes(NS_REINTERPRET_CAST(PRUnichar*, values[i].BeginWriting()));
      }
      bytes = values[i].get();
    }
    valueStrings[i] = NS_REINTERPRET_CAST(const PRUnichar*, bytes);
    valueLengths[i] = length;
  }

  data->formHistory->AddEntry(nsDependentString(valueStrings[kNameColumn],
                                                valueLengths[kNameColumn]),
                              nsDependentString(valueStrings[kValueColumn],
                                                valueLengths[kValueColumn]));
  return PL_DHASH_NEXT;
}

NS_IMPL_ISUPPORTS1(nsFormHistoryImporter, nsIFormHistoryImporter)

NS_IMETHODIMP
nsFormHistoryImporter::ImportFormHistory(nsIFile *aFile,
                                         nsIFormHistory2 *aFormHistory)
{
  // Check that the file exists before we try to open it
  PRBool exists;
  aFile->Exists(&exists);
  if (!exists) {
    return NS_OK;
  }
  
  nsMorkReader reader;
  nsresult rv = reader.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = reader.Read(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Gather up the column ids so we don't need to find them on each row
  FormHistoryImportClosure data(&reader, aFormHistory);
  const nsTArray<nsMorkReader::MorkColumn> columns = reader.GetColumns();
  for (PRUint32 i = 0; i < columns.Length(); ++i) {
    const nsCSubstring &name = columns[i].name;
    for (PRUint32 j = 0; j < kColumnCount; ++j) {
      if (name.Equals(gColumnNames[j])) {
        data.columnIndexes[j] = i;
        break;
      }
    }
    if (name.EqualsLiteral("ByteOrder")) {
      data.byteOrderColumn = i;
    }
  }

  // Determine the byte order from the table's meta-row.
  const nsTArray<nsCString> *metaRow = reader.GetMetaRow();
  if (metaRow && data.byteOrderColumn != -1) {
    const nsCString &byteOrder = (*metaRow)[data.byteOrderColumn];
    // Note whether the file uses a non-native byte ordering.
    // If it does, we'll have to swap bytes for PRUnichar values.
    // "BBBB" and "llll" are the only recognized values, anything
    // else is garbage and the file will be treated as native-endian
    // (no swapping).
    nsCAutoString byteOrderValue(byteOrder);
    reader.NormalizeValue(byteOrderValue);
#ifdef IS_LITTLE_ENDIAN
    data.swapBytes = byteOrderValue.EqualsLiteral("BBBB");
#else
    data.swapBytes = byteOrderValue.EqualsLiteral("llll");
#endif
  }
#if defined(XP_MACOSX) && defined(IS_LITTLE_ENDIAN)
  // The meta row and its ByteOrder field was introduced in 1.8.0.2.
  // If it's not present, treat the formhistory db as using native byte
  // ordering (as was done prior to 1.8.0.2).
  // Exception: the ByteOrder field was always present since the initial
  // x86 Mac release, so if we're on one of those, and the file doesn't
  // have a ByteOrder field, it most likely came from a ppc Mac and needs
  // its bytes swapped.  nsFormHistory in 1.8.0.2 swapped the bytes, this
  // importer should behave the same way.
  else {
    data.swapBytes = PR_TRUE;
  }
#endif

  // Add the rows to form history
  nsCOMPtr<nsIFormHistoryPrivate> fhPrivate = do_QueryInterface(aFormHistory);
  NS_ENSURE_TRUE(fhPrivate, NS_ERROR_FAILURE);

  mozIStorageConnection *conn = fhPrivate->GetStorageConnection();
  NS_ENSURE_TRUE(conn, NS_ERROR_NOT_INITIALIZED);
  mozStorageTransaction transaction(conn, PR_FALSE);

  reader.EnumerateRows(AddToFormHistoryCB, &data);
  return transaction.Commit();
}
#endif
