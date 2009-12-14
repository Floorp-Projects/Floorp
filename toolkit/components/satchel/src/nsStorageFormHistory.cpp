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
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Justin Dolske <dolske@mozilla.com>
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
#include "nsCOMArray.h"
#include "mozStorageHelper.h"
#include "mozStorageCID.h"
#include "nsTArray.h"
#include "nsIPrivateBrowsingService.h"
#include "nsNetCID.h"

#define DB_SCHEMA_VERSION   2
#define DB_FILENAME         NS_LITERAL_STRING("formhistory.sqlite")
#define DB_CORRUPT_FILENAME NS_LITERAL_STRING("formhistory.sqlite.corrupt")

#define PR_HOURS ((PRInt64)60 * 60 * 1000000)

// Limit the length of names and values stored in form history
#define MAX_HISTORY_NAME_LEN    200
#define MAX_HISTORY_VALUE_LEN   200
// Limit the number of fields saved in a form
#define MAX_FIELDS_SAVED        100

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
  PRBool doImport;

  nsresult rv = OpenDatabase(&doImport);
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    /* If the DB is corrupt, nuke it and try again with a new DB. */
    rv = dbCleanup();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = OpenDatabase(&doImport);
    doImport = PR_FALSE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> service = do_GetService("@mozilla.org/observer-service;1");
  if (service) {
    service->AddObserver(this, NS_EARLYFORMSUBMIT_SUBJECT, PR_TRUE);
    service->AddObserver(this, "idle-daily", PR_TRUE);
    service->AddObserver(this, "formhistory-expire-now", PR_TRUE);
  }

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
  // If the user is in private browsing mode, don't add any entry.
  nsresult rv;
  nsCOMPtr<nsIPrivateBrowsingService> pbs =
    do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  if (pbs) {
    PRBool inPrivateBrowsing = PR_TRUE;
    rv = pbs->GetPrivateBrowsingEnabled(&inPrivateBrowsing);
    if (NS_FAILED(rv))
      inPrivateBrowsing = PR_TRUE; // err on the safe side if we fail
    if (inPrivateBrowsing)
      return NS_OK;
  }

  if (!FormHistoryEnabled())
    return NS_OK;

  PRInt64 existingID = GetExistingEntryID(aName, aValue);

  if (existingID != -1) {
    mozStorageStatementScoper scope(mDBUpdateEntry);
    // lastUsed
    rv = mDBUpdateEntry->BindInt64Parameter(0, PR_Now());
    NS_ENSURE_SUCCESS(rv, rv);
    // WHERE id
    rv = mDBUpdateEntry->BindInt64Parameter(1, existingID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBUpdateEntry->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    PRInt64 now = PR_Now();

    mozStorageStatementScoper scope(mDBInsertNameValue);
    // fieldname
    rv = mDBInsertNameValue->BindStringParameter(0, aName);
    NS_ENSURE_SUCCESS(rv, rv);
    // value
    rv = mDBInsertNameValue->BindStringParameter(1, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
    // timesUsed
    rv = mDBInsertNameValue->BindInt32Parameter(2, 1);
    NS_ENSURE_SUCCESS(rv, rv);
    // firstUsed
    rv = mDBInsertNameValue->BindInt64Parameter(3, now);
    NS_ENSURE_SUCCESS(rv, rv);
    // lastUsed
    rv = mDBInsertNameValue->BindInt64Parameter(4, now);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBInsertNameValue->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/* Returns -1 if entry not found, or the ID if it was. */
PRInt64
nsFormHistory::GetExistingEntryID (const nsAString &aName, 
                                   const nsAString &aValue)
{
  mozStorageStatementScoper scope(mDBFindEntry);

  nsresult rv = mDBFindEntry->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, -1);

  rv = mDBFindEntry->BindStringParameter(1, aValue);
  NS_ENSURE_SUCCESS(rv, -1);

  PRBool hasMore;
  rv = mDBFindEntry->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, -1);

  PRInt64 ID = -1;
  if (hasMore) {
    mDBFindEntry->GetInt64(0, &ID);
    NS_ENSURE_SUCCESS(rv, -1);
  }

  return ID;
}

NS_IMETHODIMP
nsFormHistory::EntryExists(const nsAString &aName, 
                           const nsAString &aValue, PRBool *_retval)
{
  PRInt64 ID = GetExistingEntryID(aName, aValue);
  *_retval = ID != -1;
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

  // privacy cleanup, if there's an old mork formhistory around, just delete it
  nsCOMPtr<nsIFile> oldFormHistoryFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(oldFormHistoryFile));
  if (NS_FAILED(rv)) return rv;

  rv = oldFormHistoryFile->Append(NS_LITERAL_STRING("formhistory.dat"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool fileExists;
  if (NS_SUCCEEDED(oldFormHistoryFile->Exists(&fileExists)) && fileExists) {
    rv = oldFormHistoryFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return dbDeleteAll->Execute();
}


NS_IMETHODIMP
nsFormHistory::RemoveEntriesByTimeframe(PRInt64 aStartTime, PRInt64 aEndTime)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_formhistory "
    "WHERE firstUsed >= ?1 "
    "AND firstUsed <= ?2"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the times and execute statement.
  rv = stmt->BindInt64Parameter(0, aStartTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64Parameter(1, aEndTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsFormHistory::GetDBConnection(mozIStorageConnection **aResult)
{
  NS_ADDREF(*aResult = mDBConn);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsFormHistory::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData) 
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    mPrefBranch->GetBoolPref(PREF_FORMFILL_ENABLE, &gFormHistoryEnabled);
  } else if (!strcmp(aTopic, "idle-daily") ||
             !strcmp(aTopic, "formhistory-expire-now")) {
      ExpireOldEntries();
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

  PRUint32 savedCount = 0;
  PRUint32 length;
  elts->GetLength(&length);
  if (length == 0)
    return NS_OK;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);
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

      // TODO: If Login Manager marked this input, don't save it. The login
      // manager will deal with remembering it.

      nsAutoString autocomplete;
      inputElt->GetAttribute(kAutoComplete, autocomplete);
      if (!autocomplete.LowerCaseEqualsLiteral("off")) {
        // If this input has a name/id and value, add it to the database
        nsAutoString value;
        inputElt->GetValue(value);
        value.Trim(" \t", PR_TRUE, PR_TRUE);
        if (!value.IsEmpty()) {
          // Ignore the input if the value hasn't been changed from the
          // default. We only want to save data entered by the user.
          nsAutoString defaultValue;
          inputElt->GetDefaultValue(defaultValue);
          if (value.Equals(defaultValue))
            continue;

          nsAutoString name;
          inputElt->GetName(name);
          if (name.IsEmpty())
            inputElt->GetId(name);

          if (name.IsEmpty())
            continue;
          if (name.Length() > MAX_HISTORY_NAME_LEN ||
              value.Length() > MAX_HISTORY_VALUE_LEN)
            continue;
          if (savedCount++ >= MAX_FIELDS_SAVED)
            break;
          AddEntry(name, value);
        }
      }
    }
  }

  return transaction.Commit();
}

nsresult
nsFormHistory::ExpireOldEntries()
{
  // Determine how many days of history we're supposed to keep.
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 expireDays;
  rv = prefBranch->GetIntPref("browser.formfill.expire_days", &expireDays);
  if (NS_FAILED(rv)) {
    PRInt32 expireDaysMin = 0;
    rv = prefBranch->GetIntPref("browser.history_expire_days", &expireDays);
    NS_ENSURE_SUCCESS(rv, rv);
    prefBranch->GetIntPref("browser.history_expire_days_min", &expireDaysMin);
    // Make expireDays at least expireDaysMin to prevent expiring entries younger than min
    // NOTE: if history is disabled in preferences, then browser.history_expire_days == 0
    if (expireDays && expireDays < expireDaysMin)
      expireDays = expireDaysMin;
  }
  PRInt64 expireTime = PR_Now() - expireDays * 24 * PR_HOURS;


  PRInt32 beginningCount = CountAllEntries();

  // Purge the form history...
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "DELETE FROM moz_formhistory WHERE lastUsed<=?1"),
          getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = stmt->BindInt64Parameter(0, expireTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 endingCount = CountAllEntries();

  // If we expired a large batch of entries, shrink the DB to reclaim wasted
  // space. This is expected to happen when entries predating timestamps
  // (added in the v.1 schema) expire in mass, 180 days after the DB was
  // upgraded -- entries not used since then expire all at once.
  if (beginningCount - endingCount > 500) {
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

PRInt32
nsFormHistory::CountAllEntries()
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
                  "SELECT COUNT(*) FROM moz_formhistory"),
                  getter_AddRefs(stmt));

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, 0);

  PRInt32 count = 0;
  if (hasResult)
    count = stmt->AsInt32(0);

  return count;
}

nsresult
nsFormHistory::CreateTable()
{
  nsresult rv;
  // When adding new columns, also update dbAreExpectedColumnsPresent()!
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
         "CREATE TABLE moz_formhistory ("
           "id INTEGER PRIMARY KEY, fieldname TEXT NOT NULL, "
           "value TEXT NOT NULL, timesUsed INTEGER, "
           "firstUsed INTEGER, lastUsed INTEGER)"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE INDEX moz_formhistory_index ON moz_formhistory (fieldname)"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE INDEX moz_formhistory_lastused_index ON moz_formhistory (lastUsed)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFormHistory::CreateStatements()
{
  nsresult rv;
  rv  = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
         "SELECT * FROM moz_formhistory"),
         getter_AddRefs(mDBSelectEntries));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
         "SELECT id FROM moz_formhistory WHERE fieldname=?1 AND value=?2"),
         getter_AddRefs(mDBFindEntry));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
         "SELECT * FROM moz_formhistory WHERE fieldname=?1"),
         getter_AddRefs(mDBFindEntryByName));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "INSERT INTO moz_formhistory (fieldname, value, timesUsed, "
        "firstUsed, lastUsed) VALUES (?1, ?2, ?3, ?4, ?5)"),
        getter_AddRefs(mDBInsertNameValue));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "UPDATE moz_formhistory "
        "SET timesUsed=timesUsed + 1, lastUsed=?1 "
        "WHERE id = ?2"),
        getter_AddRefs(mDBUpdateEntry));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFormHistory::OpenDatabase(PRBool *aDoImport)
{
  // init DB service and obtain a connection
  nsresult rv;
  mStorageService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> formHistoryFile;
  rv = GetDatabaseFile(getter_AddRefs(formHistoryFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStorageService->OpenDatabase(formHistoryFile, getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Use a transaction around initialization and migration for better performance.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRBool exists;
  mDBConn->TableExists(NS_LITERAL_CSTRING("moz_formhistory"), &exists);
  if (!exists) {
    *aDoImport = PR_TRUE;
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    *aDoImport = PR_FALSE;
  }

  // Ensure DB is at the current schema.
  rv = dbMigrate();
  NS_ENSURE_SUCCESS(rv, rv);
  
  // should commit before starting cache
  transaction.Commit();

  rv = CreateStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
  
/*
 * dbMigrate
 */
nsresult
nsFormHistory::dbMigrate()
{
  PRInt32 schemaVersion;
  nsresult rv = mDBConn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Upgrade from the version in the DB to the version we expect, step by
  // step. Note that the formhistory.sqlite in Firefox 3 didn't have a
  // schema version set, so we start at 0.
  switch (schemaVersion) {
    case 0:
      rv = MigrateToVersion1();
      NS_ENSURE_SUCCESS(rv, rv);
      // (fallthrough to the next upgrade)
    case 1:
      rv = MigrateToVersion2();
      NS_ENSURE_SUCCESS(rv, rv);
      // (fallthrough to the next upgrade)
    case DB_SCHEMA_VERSION:
      // (current version, nothing more to do)
      break;

    // Unknown schema version, it's probably a DB modified by some future
    // version of this code. Try to use the DB anyway.
    default:
      // Sanity check: make sure the columns we expect are present. If not,
      // treat it as corrupt (back it up, nuke it, start from scratch).
      if(!dbAreExpectedColumnsPresent())
        return NS_ERROR_FILE_CORRUPTED;

      // If it's ok, downgrade the schema version so the future version will
      // know to re-upgrade the DB.
      rv = mDBConn->SetSchemaVersion(DB_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
  }

  return NS_OK;
}


/*
 * MigrateToVersion1
 *
 * Updates the DB schema to v1 (bug 463154).
 * Adds firstUsed, lastUsed, timesUsed columns.
 */
nsresult
nsFormHistory::MigrateToVersion1()
{
  // Check to see if the new columns already exist (could be a v1 DB that
  // was downgraded to v0). If they exist, we don't need to add them.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
                  "SELECT timesUsed, firstUsed, lastUsed FROM moz_formhistory"),
                  getter_AddRefs(stmt));

  PRBool columnsExist = !!NS_SUCCEEDED(rv);

  if (!columnsExist) {
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_formhistory ADD COLUMN timesUsed INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_formhistory ADD COLUMN firstUsed INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_formhistory ADD COLUMN lastUsed INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the default values for the new columns.
  //
  // Note that we set the timestamps to 24 hours in the past. We want a
  // timestamp that's recent (so that "keep form history for 90 days"
  // doesn't expire things surprisingly soon), but not so recent that
  // "forget the last hour of stuff" deletes all freshly migrated data.
  nsCOMPtr<mozIStorageStatement> addDefaultValues;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
         "UPDATE moz_formhistory "
         "SET timesUsed=1, firstUsed=?1, lastUsed=?1 "
         "WHERE timesUsed isnull OR firstUsed isnull OR lastUsed isnull"),
         getter_AddRefs(addDefaultValues));
  rv = addDefaultValues->BindInt64Parameter(0, PR_Now() - 24 * PR_HOURS);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = addDefaultValues->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->SetSchemaVersion(1);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/*
 * MigrateToVersion2
 *
 * Updates the DB schema to v2 (bug 243136).
 * Adds lastUsed index, removes moz_dummy_table
 */
nsresult
nsFormHistory::MigrateToVersion2()
{
  nsresult rv;

  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE IF EXISTS moz_dummy_table"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS moz_formhistory_lastused_index ON moz_formhistory (lastUsed)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->SetSchemaVersion(2);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsFormHistory::GetDatabaseFile(nsIFile** aFile)
{
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  return (*aFile)->Append(DB_FILENAME);
}


/*
 * dbCleanup 
 *
 * Called when a DB is corrupt. We back it up to a .corrupt file, and then
 * nuke it to start from scratch.
 */
nsresult
nsFormHistory::dbCleanup()
{
  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = GetDatabaseFile(getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> backupFile;
  NS_ENSURE_TRUE(mStorageService, NS_ERROR_NOT_AVAILABLE);
  rv = mStorageService->BackupDatabaseFile(dbFile, DB_CORRUPT_FILENAME,
                                           nsnull, getter_AddRefs(backupFile));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDBConn)
    mDBConn->Close();

  rv = dbFile->Remove(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/*
 * dbAreExpectedColumnsPresent
 */
PRBool
nsFormHistory::dbAreExpectedColumnsPresent()
{
  // If the statement succeeds, all the columns are there.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
                  "SELECT fieldname, value, timesUsed, firstUsed, lastUsed "
                  "FROM moz_formhistory"), getter_AddRefs(stmt));
  return NS_SUCCEEDED(rv) ? PR_TRUE : PR_FALSE;
}
