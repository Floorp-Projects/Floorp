/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFormHistory.h"

#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsICategoryManager.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "nsQuickSort.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsVoidArray.h"
#include "nsCOMArray.h"

#define PREF_FORMFILL_BRANCH "browser.formfill."
#define PREF_FORMFILL_ENABLE "enable"

static const char *kFormHistoryFileName = "formhistory.dat";

NS_INTERFACE_MAP_BEGIN(nsFormHistory)
  NS_INTERFACE_MAP_ENTRY(nsIFormHistory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIFormSubmitObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsFormHistory)
NS_IMPL_THREADSAFE_RELEASE(nsFormHistory)

mdb_column nsFormHistory::kToken_ValueColumn = 0;
mdb_column nsFormHistory::kToken_NameColumn = 0;

PRBool nsFormHistory::gFormHistoryEnabled = PR_FALSE;
PRBool nsFormHistory::gPrefsInitialized = PR_FALSE;

nsFormHistory::nsFormHistory() :
  mEnv(nsnull),
  mStore(nsnull),
  mTable(nsnull)
{
}

nsFormHistory::~nsFormHistory()
{
  CloseDatabase();
}

nsresult
nsFormHistory::Init()
{
  gFormHistory = this;

  nsCOMPtr<nsIObserverService> service = do_GetService("@mozilla.org/observer-service;1");
  if (service)
    service->AddObserver(this, NS_FORMSUBMIT_SUBJECT, PR_TRUE);
  
  return NS_OK;
}

nsFormHistory *nsFormHistory::gFormHistory = nsnull;

nsFormHistory *
nsFormHistory::GetInstance()
{
  if (!gFormHistory) {
    gFormHistory = new nsFormHistory();
    if (!gFormHistory)
      return nsnull;

    NS_ADDREF(gFormHistory);  // addref for the global

    if (NS_FAILED(gFormHistory->Init())) {
      NS_RELEASE(gFormHistory);

      return nsnull;
    }  
  }

  NS_ADDREF(gFormHistory);   // addref for the getter

  return gFormHistory;
}


void
nsFormHistory::ReleaseInstance()
{
  NS_IF_RELEASE(gFormHistory);
}

/* static */ PRBool
nsFormHistory::FormHistoryEnabled()
{
  if (!gPrefsInitialized) {
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    nsCOMPtr<nsIPrefBranch> branch;
    prefService->GetBranch(PREF_FORMFILL_BRANCH, getter_AddRefs(branch));
    branch->GetBoolPref(PREF_FORMFILL_ENABLE, &gFormHistoryEnabled);

    nsCOMPtr<nsIPrefBranchInternal> branchInternal = do_QueryInterface(branch);
    branchInternal->AddObserver(PREF_FORMFILL_BRANCH, gFormHistory, PR_TRUE);

    gPrefsInitialized = PR_TRUE;
  }

  return gFormHistoryEnabled;
}


////////////////////////////////////////////////////////////////////////
//// nsIFormHistory

NS_IMETHODIMP
nsFormHistory::GetRowCount(PRUint32 *aRowCount)
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_err err = mTable->GetCount(mEnv, aRowCount);
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::GetEntryAt(PRUint32 aIndex, nsAString &aName, nsAString &aValue)
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  mdb_err err = mTable->PosToRow(mEnv, aIndex, getter_AddRefs(row));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  GetRowValue(row, kToken_NameColumn, aName);
  GetRowValue(row, kToken_ValueColumn, aValue);
    
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::GetNameAt(PRUint32 aIndex, nsAString &aName)
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  mdb_err err = mTable->PosToRow(mEnv, aIndex, getter_AddRefs(row));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  GetRowValue(row, kToken_NameColumn, aName);
    
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::GetValueAt(PRUint32 aIndex, nsAString &aValue)
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  mdb_err err = mTable->PosToRow(mEnv, aIndex, getter_AddRefs(row));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  GetRowValue(row, kToken_ValueColumn, aValue);
    
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::AddEntry(const nsAString &aName, const nsAString &aValue)
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  AppendRow(aName, aValue, getter_AddRefs(row));
  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::RemoveEntryAt(PRUint32 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFormHistory::RemoveEntriesForName(const nsAString & name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFormHistory::RemoveAllEntries()
{
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mTable) return NS_OK;

  mdb_err err;
  mdb_count count;
  err = mTable->GetCount(mEnv, &count);
  if (err != 0) return NS_ERROR_FAILURE;

  // Begin the batch.
  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;

  // XXX from here until end batch, no early returns!
  for (mdb_pos pos = count - 1; pos >= 0; --pos) {
    nsCOMPtr<nsIMdbRow> row;
    err = mTable->PosToRow(mEnv, pos, getter_AddRefs(row));
    NS_ASSERTION(err == 0, "unable to get row");
    if (err != 0)
      break;

    NS_ASSERTION(row != nsnull, "no row");
    if (! row)
      continue;

    // Officially cut the row *now*, before notifying any observers:
    // that way, any re-entrant calls won't find the row.
    err = mTable->CutRow(mEnv, row);
    NS_ASSERTION(err == 0, "couldn't cut row");
    if (err != 0)
      continue;
  
    // possibly avoid leakage
    err = row->CutAllColumns(mEnv);
    NS_ASSERTION(err == 0, "couldn't cut all columns");
    // we'll notify regardless of whether we could successfully
    // CutAllColumns or not.
  }
  
  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");

  return (err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFormHistory::EntryExists(const nsAString &aName, const nsAString &aValue, PRBool *_retval)
{
  // Unfortunately we have to do a brute force search through the database
  // because mork didn't bother to implement any indexing functionality
  
  *_retval = PR_FALSE;
  
  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  // Get a cursor to iterate through all rows in the database
  nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
  mdb_err err = mTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(rowCursor));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIMdbRow> row;
  mdb_pos pos;
  do {
    rowCursor->NextRow(mEnv, getter_AddRefs(row), &pos);
    NS_ENSURE_TRUE(row != nsnull, NS_ERROR_FAILURE);

    // Check if the name and value combination match this row
    nsAutoString name;
    GetRowValue(row, kToken_NameColumn, name);
    if (Compare(name, aName, nsCaseInsensitiveStringComparator()) == 0) {
      nsAutoString value;
      GetRowValue(row, kToken_ValueColumn, value);
      if (Compare(value, aValue, nsCaseInsensitiveStringComparator()) == 0) {
        *_retval = PR_TRUE;
        break;
      }
    }
  } while (row);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsFormHistory::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData) 
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
    branch->GetBoolPref(PREF_FORMFILL_ENABLE, &gFormHistoryEnabled);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIFormSubmitObserver

NS_IMETHODIMP
nsFormHistory::Notify(nsIContent* aFormNode, nsIDOMWindowInternal* aWindow, nsIURI* aActionURL, PRBool* aCancelSubmit)
{
  if (!FormHistoryEnabled())
    return NS_OK;

  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMHTMLFormElement> formElt = do_QueryInterface(aFormNode);
  NS_ENSURE_TRUE(formElt, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLCollection> elts;
  formElt->GetElements(getter_AddRefs(elts));
  
  const char *textString = "text";
  
  PRUint32 length;
  elts->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> node;
    elts->Item(i, getter_AddRefs(node));
    nsCOMPtr<nsIDOMHTMLInputElement> inputElt = do_QueryInterface(node);
    if (inputElt) {
      // Filter only inputs that are of type "text"
      nsAutoString type;
      inputElt->GetType(type);
      if (type.EqualsIgnoreCase(textString)) {
        // If this input has a name/id and value, add it to the database
        nsAutoString value;
        inputElt->GetValue(value);
        if (!value.IsEmpty()) {
          nsAutoString name;
          inputElt->GetName(name);
          if (name.IsEmpty())
            inputElt->GetId(name);
          
          if (!name.IsEmpty())
            AppendRow(name, value, nsnull);
        }
      }
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// Database I/O

nsresult
nsFormHistory::OpenDatabase()
{
  if (mStore)
    return NS_OK;
  
  // Get a handle to the database file
  nsCOMPtr <nsIFile> historyFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(historyFile));
  NS_ENSURE_SUCCESS(rv, rv);
  historyFile->Append(NS_ConvertUTF8toUCS2(kFormHistoryFileName));

  // Get an Mdb Factory
  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
  nsCOMPtr<nsIMdbFactoryFactory> mdbFactory =
      do_CreateInstance(kMorkCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mdbFactory->GetMdbFactory(getter_AddRefs(mMdbFactory));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the Mdb environment
  mdb_err err = mMdbFactory->MakeEnv(nsnull, &mEnv);
  NS_ASSERTION(err == 0, "ERROR: Unable to create Form History mdb");
  mEnv->SetAutoClear(PR_TRUE);
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);

  nsCAutoString filePath;
  historyFile->GetNativePath(filePath);
  PRBool exists = PR_TRUE;
  historyFile->Exists(&exists);
  
  if (!exists || NS_FAILED(rv = OpenExistingFile(filePath.get()))) {
    // If the file doesn't exist, or we fail trying to open it,
    // then make sure it is deleted and then create an empty database file
    historyFile->Remove(PR_FALSE);
    rv = CreateNewFile(filePath.get());
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the initial size of the file, needed later for Commit
  historyFile->GetFileSize(&mFileSizeOnDisk);

  /* // TESTING: Add a row to the database
  nsAutoString foopy;
  foopy.AssignLiteral("foopy");
  nsAutoString oogly;
  oogly.AssignLiteral("oogly");
  AppendRow(foopy, oogly, nsnull);
  Flush(); */
  
  /* // TESTING: Dump the contents of the database
  PRUint32 count = 0;
  mdb_err err = mTable->GetCount(mEnv, &count);
  printf("%d rows in form history\n", count);

  for (mdb_pos pos = count - 1; pos >= 0; --pos) {
    nsCOMPtr<nsIMdbRow> row;
    err = mTable->PosToRow(mEnv, pos, getter_AddRefs(row));
    
    nsAutoString name;
    GetRowValue(row, kToken_NameColumn, name);
    nsAutoString value;
    GetRowValue(row, kToken_ValueColumn, value);
    printf("ROW: %s - %s\n", ToNewCString(name), ToNewCString(value));
  } */

  return NS_OK;
}

nsresult
nsFormHistory::OpenExistingFile(const char *aPath)
{
  nsCOMPtr<nsIMdbFile> oldFile;
  nsIMdbHeap* dbHeap = 0;
  mdb_err err = mMdbFactory->OpenOldFile(mEnv, dbHeap, aPath, mdbBool_kFalse, getter_AddRefs(oldFile));
  NS_ENSURE_TRUE(!err && oldFile, NS_ERROR_FAILURE);

  mdb_bool canOpen = 0;
  mdbYarn outFormat = {nsnull, 0, 0, 0, 0, nsnull};
  err = mMdbFactory->CanOpenFilePort(mEnv, oldFile, &canOpen, &outFormat);
  NS_ENSURE_TRUE(!err && canOpen, NS_ERROR_FAILURE);

  nsCOMPtr<nsIMdbThumb> thumb;
  mdbOpenPolicy policy = {{0, 0}, 0, 0};
  err = mMdbFactory->OpenFileStore(mEnv, dbHeap, oldFile, &policy, getter_AddRefs(thumb));
  NS_ENSURE_TRUE(!err && thumb, NS_ERROR_FAILURE);

  PRBool done;
  mdb_err thumbErr = UseThumb(thumb, &done);

  if (err == 0 && done)
    err = mMdbFactory->ThumbToOpenStore(mEnv, thumb, &mStore);
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);

  nsresult rv = CreateTokens();
  NS_ENSURE_SUCCESS(rv, rv);

  mdbOid oid = {kToken_RowScope, 1};
  err = mStore->GetTable(mEnv, &oid, &mTable);
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  if (!mTable) {
    NS_WARNING("ERROR: Form history file is corrupt, now deleting it.");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(thumbErr))
    err = thumbErr;

  return err ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsFormHistory::CreateNewFile(const char *aPath)
{
  nsIMdbHeap* dbHeap = 0;
  nsCOMPtr<nsIMdbFile> newFile;
  mdb_err err = mMdbFactory->CreateNewFile(mEnv, dbHeap, aPath, getter_AddRefs(newFile));
  NS_ENSURE_TRUE(!err && newFile, NS_ERROR_FAILURE);

  nsCOMPtr <nsIMdbTable> oldTable = mTable;;
  nsCOMPtr <nsIMdbStore> oldStore = mStore;
  mdbOpenPolicy policy = {{0, 0}, 0, 0};
  err = mMdbFactory->CreateNewFileStore(mEnv, dbHeap, newFile, &policy, &mStore);
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  nsresult rv = CreateTokens();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the one and only table in the database
  err = mStore->NewTable(mEnv, kToken_RowScope, kToken_Kind, PR_TRUE, nsnull, &mTable);
  NS_ENSURE_TRUE(!err && mTable, NS_ERROR_FAILURE);

   // oldTable will only be set if we detected a corrupt db, and are 
   // trying to restore data from it.
  if (oldTable)
    CopyRowsFromTable(oldTable);

  // Force a commit now to get it written out.
  nsCOMPtr<nsIMdbThumb> thumb;
  err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);

  PRBool done;
  err = UseThumb(thumb, &done);

  return err || !done ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsFormHistory::CloseDatabase()
{
  Flush();

  if (mTable)
    mTable->Release();

  if (mStore)
    mStore->Release();

  if (mEnv)
    mEnv->Release();

  mTable = nsnull;
  mEnv = nsnull;
  mStore = nsnull;

  return NS_OK;
}

nsresult
nsFormHistory::CreateTokens()
{
  mdb_err err;

  if (!mStore)
    return NS_ERROR_NOT_INITIALIZED;

  err = mStore->StringToToken(mEnv, "ns:formhistory:db:row:scope:formhistory:all", &kToken_RowScope);
  if (err != 0) return NS_ERROR_FAILURE;
  
  err = mStore->StringToToken(mEnv, "ns:formhistory:db:table:kind:formhistory", &kToken_Kind);
  if (err != 0) return NS_ERROR_FAILURE;
  
  err = mStore->StringToToken(mEnv, "Value", &kToken_ValueColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Name", &kToken_NameColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsFormHistory::Flush()
{
  if (!mStore || !mTable)
    return NS_OK;

  mdb_err err;

  nsCOMPtr<nsIMdbThumb> thumb;
  err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));

  if (err == 0)
    err = UseThumb(thumb, nsnull);
  
  return err ? NS_ERROR_FAILURE : NS_OK;
}

mdb_err
nsFormHistory::UseThumb(nsIMdbThumb *aThumb, PRBool *aDone)
{
  mdb_count total;
  mdb_count current;
  mdb_bool done;
  mdb_bool broken;
  mdb_err err;
  
  do {
    err = aThumb->DoMore(mEnv, &total, &current, &done, &broken);
  } while ((err == 0) && !broken && !done);
  
  if (aDone)
    *aDone = done;
  
  return err ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsFormHistory::CopyRowsFromTable(nsIMdbTable *sourceTable)
{
  nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
  mdb_err err = sourceTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(rowCursor));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIMdbRow> row;
  mdb_pos pos;
  do {
    rowCursor->NextRow(mEnv, getter_AddRefs(row), &pos);
    if (!row)
      break;

    mdbOid rowId;
    rowId.mOid_Scope = kToken_RowScope;
    rowId.mOid_Id = mdb_id(-1);

    nsCOMPtr<nsIMdbRow> newRow;
    mdb_err err = mTable->NewRow(mEnv, &rowId, getter_AddRefs(newRow));
    newRow->SetRow(mEnv, row);
    mTable->AddRow(mEnv, newRow);
  } while (row);
  return NS_OK;
}

nsresult
nsFormHistory::AppendRow(const nsAString &aName, const nsAString &aValue, nsIMdbRow **aResult)
{  
  if (!mTable)
    return NS_ERROR_NOT_INITIALIZED;

  PRBool exists;
  EntryExists(aName, aValue, &exists);
  if (exists)
    return NS_OK;

  mdbOid rowId;
  rowId.mOid_Scope = kToken_RowScope;
  rowId.mOid_Id = mdb_id(-1);

  nsCOMPtr<nsIMdbRow> row;
  mdb_err err = mTable->NewRow(mEnv, &rowId, getter_AddRefs(row));
  if (err != 0)
    return NS_ERROR_FAILURE;

  SetRowValue(row, kToken_NameColumn, aName);
  SetRowValue(row, kToken_ValueColumn, aValue);

  if (aResult) {
    *aResult = row;
    NS_ADDREF(*aResult);
  }
  
  return NS_OK;  
}

nsresult
nsFormHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const nsAString &aValue)
{
  PRInt32 len = aValue.Length() * sizeof(PRUnichar);

  mdbYarn yarn = {(void *)ToNewUnicode(aValue), len, len, 0, 0, nsnull};
  mdb_err err = aRow->AddColumn(mEnv, aCol, &yarn);
  
  return err ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsFormHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString &aValue)
{
  mdbYarn yarn;
  mdb_err err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0)
    return NS_ERROR_FAILURE;

  aValue.Truncate(0);
  if (!yarn.mYarn_Fill)
    return NS_OK;
  
  switch (yarn.mYarn_Form) {
    case 0: // unicode
      aValue.Assign((const PRUnichar *)yarn.mYarn_Buf, yarn.mYarn_Fill/sizeof(PRUnichar));
      break;
    default:
      return NS_ERROR_UNEXPECTED;
  }
  
  return NS_OK;
}

nsresult
nsFormHistory::AutoCompleteSearch(const nsAString &aInputName,
                                  const nsAString &aInputValue,
                                  nsIAutoCompleteMdbResult *aPrevResult,
                                  nsIAutoCompleteResult **aResult)
{
  if (!FormHistoryEnabled())
    return NS_OK;

  nsresult rv = OpenDatabase(); // lazily ensure that the database is open
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAutoCompleteMdbResult> result;
  
  if (aPrevResult) {
    result = aPrevResult;
    
    PRUint32 rowCount;
    result->GetMatchCount(&rowCount);
    
    for (PRInt32 i = rowCount-1; i >= 0; --i) {
      nsIMdbRow *row;
      result->GetRowAt(i, &row);
      if (!RowMatch(row, aInputName, aInputValue, nsnull))
        result->RemoveValueAt(i, PR_FALSE);
    }
  } else {
    result = do_CreateInstance("@mozilla.org/autocomplete/mdb-result;1");

    result->SetSearchString(aInputValue);
    result->Init(mEnv, mTable);
    result->SetTokens(kToken_ValueColumn, nsIAutoCompleteMdbResult::kUnicharType, nsnull, nsIAutoCompleteMdbResult::kUnicharType);

    // Get a cursor to iterate through all rows in the database
    nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
    mdb_err err = mTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(rowCursor));
    NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);
    
    // Store only the matching values
    nsAutoVoidArray matchingValues;
    nsCOMArray<nsIMdbRow> matchingRows;

    nsCOMPtr<nsIMdbRow> row;
    mdb_pos pos;
    do {
      rowCursor->NextRow(mEnv, getter_AddRefs(row), &pos);
      if (!row)
        break;

      PRUnichar *value = 0; // We will own the allocated string value
      if (RowMatch(row, aInputName, aInputValue, &value)) {
        matchingRows.AppendObject(row);
        matchingValues.AppendElement(value);
      }
    } while (row);

    // Turn auto array into flat array for quick sort, now that we
    // know how many items there are
    PRUint32 count = matchingRows.Count();

    if (count > 0) {
      PRUint32* items = new PRUint32[count];
      PRUint32 i;
      for (i = 0; i < count; ++i)
        items[i] = i;

      NS_QuickSort(items, count, sizeof(PRUint32),
                   SortComparison, &matchingValues);

      for (i = 0; i < count; ++i) {
        // Place the sorted result into the autocomplete result
        result->AddRow(matchingRows[items[i]]);

        // Free up these strings we owned.
        delete (PRUnichar *) matchingValues[i];
      }

      delete[] items;
    }

    PRUint32 matchCount;
    result->GetMatchCount(&matchCount);
    if (matchCount > 0) {
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

int PR_CALLBACK 
nsFormHistory::SortComparison(const void *v1, const void *v2, void *closureVoid) 
{
  PRUint32 *index1 = (PRUint32 *)v1;
  PRUint32 *index2 = (PRUint32 *)v2;
  nsAutoVoidArray *array = (nsAutoVoidArray *)closureVoid;
  
  PRUnichar *s1 = (PRUnichar *)array->ElementAt(*index1);
  PRUnichar *s2 = (PRUnichar *)array->ElementAt(*index2);
  
  return nsCRT::strcmp(s1, s2);
}

PRBool
nsFormHistory::RowMatch(nsIMdbRow *aRow, const nsAString &aInputName, const nsAString &aInputValue, PRUnichar **aValue)
{
  nsAutoString name;
  GetRowValue(aRow, kToken_NameColumn, name);

  if (name.Equals(aInputName)) {
    nsAutoString value;
    GetRowValue(aRow, kToken_ValueColumn, value);
    if (Compare(Substring(value, 0, aInputValue.Length()), aInputValue, nsCaseInsensitiveStringComparator()) == 0) {
      if (aValue)
        *aValue = ToNewUnicode(value);
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}
