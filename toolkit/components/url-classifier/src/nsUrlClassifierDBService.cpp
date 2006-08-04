//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tony@ponderer.org> (original author)
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

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIDirectoryService.h"
#include "nsIObserverService.h"
#include "nsIProperties.h"
#include "nsIProxyObjectManager.h"
#include "nsToolkitCompsCID.h"
#include "nsUrlClassifierDBService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsXPCOMStrings.h"
#include "prlog.h"
#include "prprf.h"

// NSPR_LOG_MODULES=UrlClassifierDbService:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gUrlClassifierDbServiceLog = nsnull;
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

#define DATABASE_FILENAME "urlclassifier.sqlite"

// Singleton instance.
static nsUrlClassifierDBService* sUrlClassifierDBService;

// Thread that we do the updates on.
static nsIThread* gDbBackgroundThread = nsnull;

static const char* kNEW_TABLE_SUFFIX = "_new";

// -------------------------------------------------------------------------
// Actual worker implemenatation
class nsUrlClassifierDBServiceWorker : public nsIUrlClassifierDBServiceWorker
{
public:
  nsUrlClassifierDBServiceWorker();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE
  NS_DECL_NSIURLCLASSIFIERDBSERVICEWORKER

private:
  // No subclassing
  ~nsUrlClassifierDBServiceWorker();

  // Disallow copy constructor
  nsUrlClassifierDBServiceWorker(nsUrlClassifierDBServiceWorker&);

  // Table names have hyphens in them, which SQL doesn't allow,
  // so we convert them to underscores.
  void GetDbTableName(const nsACString& aTableName, nsCString* aDbTableName);

  // Try to open the db, DATABASE_FILENAME.
  nsresult OpenDb();

  // Create a table in the db if it doesn't exist.
  nsresult MaybeCreateTable(const nsCString& aTableName);

  // Drop a table if it exists.
  nsresult MaybeDropTable(const nsCString& aTableName);

  // If this is not an update request, swap the new table
  // in for the old table.
  nsresult MaybeSwapTables(const nsCString& aVersionLine);

  // Parse a version string of the form [table-name #.###] or
  // [table-name #.### update] and return the table name and
  // whether or not it's an update.
  nsresult ParseVersionString(const nsCSubstring& aLine,
                              nsCString* aTableName,
                              PRBool* aIsUpdate);

  // Handle a new table line of the form [table-name #.####].  We create the
  // table if it doesn't exist and set the aTableName, aUpdateStatement,
  // and aDeleteStatement.
  nsresult ProcessNewTable(const nsCSubstring& aLine,
                           nsCString* aTableName,
                           mozIStorageStatement** aUpdateStatement,
                           mozIStorageStatement** aDeleteStatement);

  // Handle an add or remove line.  We execute additional update or delete
  // statements.
  nsresult ProcessUpdateTable(const nsCSubstring& aLine,
                              const nsCString& aTableName,
                              mozIStorageStatement* aUpdateStatement,
                              mozIStorageStatement* aDeleteStatement);

  // Holds a connection to the Db.  We lazily initialize this because it has
  // to be created in the background thread (currently mozStorageConnection
  // isn't thread safe).
  mozIStorageConnection* mConnection;

  // True if we're in the middle of a streaming update.
  PRBool mHasPendingUpdate;

  // For incremental updates, keep track of tables that have been updated.
  // When finish() is called, we go ahead and pass these update lines to
  // the callback.
  nsTArray<nsCString> mTableUpdateLines;

  // We receive data in small chunks that may be broken in the middle of
  // a line.  So we save the last partial line here.
  nsCString mPendingStreamUpdate;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUrlClassifierDBServiceWorker,
                              nsIUrlClassifierDBServiceWorker)

nsUrlClassifierDBServiceWorker::nsUrlClassifierDBServiceWorker()
  : mConnection(nsnull), mHasPendingUpdate(PR_FALSE), mTableUpdateLines()
{
}
nsUrlClassifierDBServiceWorker::~nsUrlClassifierDBServiceWorker()
{
  NS_ASSERTION(mConnection == nsnull,
               "Db connection not closed, leaking memory!  Call CloseDb "
               "to close the connection.");
}


// Lookup a key in the db.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Exists(const nsACString& tableName,
                                       const nsACString& key,
                                       nsIUrlClassifierCallback *c)
{
  LOG(("Exists\n"));

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  nsCAutoString dbTableName;
  GetDbTableName(tableName, &dbTableName);

  nsCOMPtr<mozIStorageStatement> selectStatement;
  nsCAutoString statement;
  statement.AssignLiteral("SELECT value FROM ");
  statement.Append(dbTableName);
  statement.AppendLiteral(" WHERE key = ?1");

  rv = mConnection->CreateStatement(statement,
                                    getter_AddRefs(selectStatement));

  nsAutoString value;
  // If CreateStatment failed, this probably means the table doesn't exist.
  // That's ok, we just return an emptry string.
  if (NS_SUCCEEDED(rv)) {
    rv = selectStatement->BindUTF8StringParameter(0, key);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore = PR_FALSE;
    rv = selectStatement->ExecuteStep(&hasMore);
    // If the table has any columns, take the first value.
    if (NS_SUCCEEDED(rv) && hasMore) {
      selectStatement->GetString(0, value);
    }
  }

  c->HandleEvent(NS_ConvertUTF16toUTF8(value));
  return NS_OK;
}

// We get a comma separated list of table names.  For each table that doesn't
// exist, we return it in a comma separated list via the callback.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CheckTables(const nsACString & tableNames,
                                            nsIUrlClassifierCallback *c)
{
  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  nsCAutoString changedTables;

  // tablesNames is a comma separated list, so get each table name out for
  // checking.
  PRUint32 cur = 0;
  PRInt32 next;
  while (cur < tableNames.Length()) {
    next = tableNames.FindChar(',', cur);
    if (kNotFound == next) {
      next = tableNames.Length();
    }
    const nsCSubstring &tableName = Substring(tableNames, cur, next - cur);
    cur = next + 1;

    nsCString dbTableName;
    GetDbTableName(tableName, &dbTableName);
    PRBool exists;
    nsresult rv = mConnection->TableExists(dbTableName, &exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      if (changedTables.Length() > 0)
        changedTables.Append(",");
      changedTables.Append(tableName);
    }
  }
  
  c->HandleEvent(changedTables);
  return NS_OK;
}

// Do a batch update of the database.  After we complete processing a table,
// we call the callback with the table line.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::UpdateTables(const nsACString& updateString,
                                             nsIUrlClassifierCallback *c)
{
  LOG(("Updating tables\n"));

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  rv = mConnection->BeginTransaction();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to begin transaction");

  // Split the update string into lines
  PRUint32 cur = 0;
  PRInt32 next;
  PRInt32 count = 0;
  nsCAutoString dbTableName;
  nsCAutoString lastTableLine;
  nsCOMPtr<mozIStorageStatement> updateStatement;
  nsCOMPtr<mozIStorageStatement> deleteStatement;
  while(cur < updateString.Length() &&
        (next = updateString.FindChar('\n', cur)) != kNotFound) {
    const nsCSubstring &line = Substring(updateString, cur, next - cur);
    cur = next + 1; // prepare for next run

    // Skip blank lines
    if (line.Length() == 0)
      continue;

    count++;

    if ('[' == line[0]) {
      rv = ProcessNewTable(line, &dbTableName,
                           getter_AddRefs(updateStatement),
                           getter_AddRefs(deleteStatement));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "malformed table line");
      if (NS_SUCCEEDED(rv)) {
        // If it's a new table, we may have completed a table.
        // Go ahead and post the completion to the UI thread and db.
        if (lastTableLine.Length() > 0) {
          // If it was a new table, we need to swap in the new table.
          rv = MaybeSwapTables(lastTableLine);
          if (NS_SUCCEEDED(rv)) {
            mConnection->CommitTransaction();
            c->HandleEvent(lastTableLine);
          } else {
            // failed to swap, rollback
            mConnection->RollbackTransaction();
          }
          mConnection->BeginTransaction();
        }
        lastTableLine.Assign(line);
      }
    } else {
      rv = ProcessUpdateTable(line, dbTableName, updateStatement,
                              deleteStatement);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "malformed update line");
    }
  }
  LOG(("Num update lines: %d\n", count));

  rv = MaybeSwapTables(lastTableLine);
  if (NS_SUCCEEDED(rv)) {
    mConnection->CommitTransaction();
    c->HandleEvent(lastTableLine);
  } else {
    // failed to swap, rollback
    mConnection->RollbackTransaction();
  }

  LOG(("Finishing table update\n"));
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Update(const nsACString& chunk)
{
  LOG(("Update from Stream."));
  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  nsCAutoString updateString(mPendingStreamUpdate);
  updateString.Append(chunk);
  
  nsCOMPtr<mozIStorageStatement> updateStatement;
  nsCOMPtr<mozIStorageStatement> deleteStatement;
  nsCAutoString dbTableName;

  // If we're not in the middle of an update, we start a new transaction.
  // Otherwise, we need to pick up where we left off.
  if (!mHasPendingUpdate) {
    mConnection->BeginTransaction();
    mHasPendingUpdate = PR_TRUE;
  } else {
    PRUint32 numTables = mTableUpdateLines.Length();
    if (numTables > 0) {
      const nsCSubstring &line = Substring(
              mTableUpdateLines[numTables - 1], 0);

      rv = ProcessNewTable(line, &dbTableName,
                           getter_AddRefs(updateStatement),
                           getter_AddRefs(deleteStatement));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "malformed table line");
    }
  }

  PRUint32 cur = 0;
  PRInt32 next;
  while(cur < updateString.Length() &&
        (next = updateString.FindChar('\n', cur)) != kNotFound) {
    const nsCSubstring &line = Substring(updateString, cur, next - cur);
    cur = next + 1; // prepare for next run

    // Skip blank lines
    if (line.Length() == 0)
      continue;

    if ('[' == line[0]) {
      rv = ProcessNewTable(line, &dbTableName,
                           getter_AddRefs(updateStatement),
                           getter_AddRefs(deleteStatement));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "malformed table line");
      if (NS_SUCCEEDED(rv)) {
        // Add the line to our array of table lines.
        mTableUpdateLines.AppendElement(line);
      }
    } else {
      rv = ProcessUpdateTable(line, dbTableName, updateStatement,
                              deleteStatement);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "malformed update line");
    }
  }
  // Save the remaining string fragment.
  mPendingStreamUpdate = Substring(updateString, cur);
  LOG(("pending stream update: %s", mPendingStreamUpdate.get()));

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Finish(nsIUrlClassifierCallback *c)
{
  if (!mHasPendingUpdate)
    return NS_OK;

  nsresult rv = NS_OK;
  for (PRUint32 i = 0; i < mTableUpdateLines.Length(); ++i) {
    rv = MaybeSwapTables(mTableUpdateLines[i]);
    if (NS_FAILED(rv)) {
      break;
    }
  }
  
  if (NS_SUCCEEDED(rv)) {
    LOG(("Finish, committing transaction"));
    mConnection->CommitTransaction();

    // Send update information to main thread.
    for (PRUint32 i = 0; i < mTableUpdateLines.Length(); ++i) {
      c->HandleEvent(mTableUpdateLines[i]);
    }
  } else {
    LOG(("Finish failed (swap table error?), rolling back transaction"));
    mConnection->RollbackTransaction();
  }

  mTableUpdateLines.Clear();
  mPendingStreamUpdate.Truncate();
  mHasPendingUpdate = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CancelStream()
{
  if (!mHasPendingUpdate)
    return NS_OK;

  LOG(("CancelStream, rolling back transaction"));
  mConnection->RollbackTransaction();

  mTableUpdateLines.Clear();
  mPendingStreamUpdate.Truncate();
  mHasPendingUpdate = PR_FALSE;
  
  return NS_OK;
}

// Allows the main thread to delete the connection which may be in
// a background thread.
// XXX This could be turned into a single shutdown event so the logic
// is simpler in nsUrlClassifierDBService::Shutdown.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CloseDb()
{
  if (mConnection != nsnull) {
    NS_RELEASE(mConnection);
    delete mConnection;
    mConnection = nsnull;
    LOG(("urlclassifier db closed\n"));
  }
  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ProcessNewTable(
                                    const nsCSubstring& aLine,
                                    nsCString* aDbTableName,
                                    mozIStorageStatement** aUpdateStatement,
                                    mozIStorageStatement** aDeleteStatement)
{
  // The line format is "[table-name #.####]" or "[table-name #.#### update]"
  // The additional "update" in the header means that this is a diff.
  // Otherwise, we should blow away the old table and start afresh.
  PRBool isUpdate = PR_FALSE;

  // If the version string is bad, give up.
  nsresult rv = ParseVersionString(aLine, aDbTableName, &isUpdate);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // If it's not an update, we dump the values into a new table.  Once we're
  // done with the table, we drop the original table and copy over the values
  // from the old table into the new table.
  if (!isUpdate)
    aDbTableName->Append(kNEW_TABLE_SUFFIX);

  // Create the table
  rv = MaybeCreateTable(*aDbTableName);
  if (NS_FAILED(rv))
    return rv;

  // insert statement
  nsCAutoString statement;
  statement.AssignLiteral("INSERT OR REPLACE INTO ");
  statement.Append(*aDbTableName);
  statement.AppendLiteral(" VALUES (?1, ?2)");
  rv = mConnection->CreateStatement(statement, aUpdateStatement);
  NS_ENSURE_SUCCESS(rv, rv);

  // delete statement
  statement.AssignLiteral("DELETE FROM ");
  statement.Append(*aDbTableName);
  statement.AppendLiteral(" WHERE key = ?1");
  rv = mConnection->CreateStatement(statement, aDeleteStatement);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ProcessUpdateTable(
                                   const nsCSubstring& aLine,
                                   const nsCString& aTableName,
                                   mozIStorageStatement* aUpdateStatement,
                                   mozIStorageStatement* aDeleteStatement)
{
  // We should have seen a table name line by now.
  if (aTableName.Length() == 0)
    return NS_ERROR_FAILURE;

  if (!aUpdateStatement || !aDeleteStatement) {
    NS_NOTREACHED("Statements NULL but table is not");
    return NS_ERROR_FAILURE;
  }
  // There should at least be an op char and a key
  if (aLine.Length() < 2)
    return NS_ERROR_FAILURE;

  char op = aLine[0];
  PRInt32 spacePos = aLine.FindChar('\t');
  nsresult rv = NS_ERROR_FAILURE;

  if ('+' == op && spacePos != kNotFound) {
    // Insert operation of the form "+KEY\tVALUE"
    const nsCSubstring &key = Substring(aLine, 1, spacePos - 1);
    const nsCSubstring &value = Substring(aLine, spacePos + 1);
    aUpdateStatement->BindUTF8StringParameter(0, key);
    aUpdateStatement->BindUTF8StringParameter(1, value);

    rv = aUpdateStatement->Execute();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to update");
  } else if ('-' == op) {
    // Remove operation of the form "-KEY"
    if (spacePos == kNotFound) {
      // No trailing tab
      const nsCSubstring &key = Substring(aLine, 1);
      aDeleteStatement->BindUTF8StringParameter(0, key);
    } else {
      // With trailing tab
      const nsCSubstring &key = Substring(aLine, 1, spacePos - 1);
      aDeleteStatement->BindUTF8StringParameter(0, key);
    }

    rv = aDeleteStatement->Execute();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to delete");
  }

  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::OpenDb()
{
  // Connection already open, don't do anything.
  if (mConnection != nsnull)
    return NS_OK;

  LOG(("Opening db\n"));
  // Compute database filename
  nsCOMPtr<nsIFile> dbFile;

  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbFile->Append(NS_LITERAL_STRING(DATABASE_FILENAME));
  NS_ENSURE_SUCCESS(rv, rv);

  // open the connection
  nsCOMPtr<mozIStorageService> storageService =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storageService->OpenDatabase(dbFile, &mConnection);
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = storageService->OpenDatabase(dbFile, &mConnection);
  }
  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::MaybeCreateTable(const nsCString& aTableName)
{
  LOG(("MaybeCreateTable %s\n", aTableName.get()));

  nsCOMPtr<mozIStorageStatement> createStatement;
  nsCString statement;
  statement.Assign("CREATE TABLE IF NOT EXISTS ");
  statement.Append(aTableName);
  statement.Append(" (key TEXT PRIMARY KEY, value TEXT)");
  nsresult rv = mConnection->CreateStatement(statement,
                                             getter_AddRefs(createStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  return createStatement->Execute();
}

nsresult
nsUrlClassifierDBServiceWorker::MaybeDropTable(const nsCString& aTableName)
{
  LOG(("MaybeDropTable %s\n", aTableName.get()));
  nsCAutoString statement("DROP TABLE IF EXISTS ");
  statement.Append(aTableName);
  return mConnection->ExecuteSimpleSQL(statement);
}

nsresult
nsUrlClassifierDBServiceWorker::MaybeSwapTables(const nsCString& aVersionLine)
{
  if (aVersionLine.Length() == 0)
    return NS_ERROR_FAILURE;

  // Check to see if this was a full table update or not.
  nsCAutoString tableName;
  PRBool isUpdate;
  nsresult rv = ParseVersionString(aVersionLine, &tableName, &isUpdate);
  NS_ENSURE_SUCCESS(rv, rv);

  // Updates don't require any fancy logic.
  if (isUpdate)
    return NS_OK;

  // Not an update, so we need to swap tables by dropping the original table
  // and copying in the values from the new table.
  rv = MaybeDropTable(tableName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString newTableName(tableName);
  newTableName.Append(kNEW_TABLE_SUFFIX);

  // Bring over new table
  nsCAutoString sql("ALTER TABLE ");
  sql.Append(newTableName);
  sql.Append(" RENAME TO ");
  sql.Append(tableName);
  rv = mConnection->ExecuteSimpleSQL(sql);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("tables swapped (%s)\n", tableName.get()));

  return NS_OK;
}

// The line format is "[table-name #.####]" or "[table-name #.#### update]".
nsresult
nsUrlClassifierDBServiceWorker::ParseVersionString(const nsCSubstring& aLine,
                                                   nsCString* aTableName,
                                                   PRBool* aIsUpdate)
{
  // Blank lines are not valid
  if (aLine.Length() == 0)
    return NS_ERROR_FAILURE;

  // Max size for an update line (so we don't buffer overflow when sscanf'ing).
  const PRUint32 MAX_LENGTH = 2048;
  if (aLine.Length() > MAX_LENGTH)
    return NS_ERROR_FAILURE;

  nsCAutoString lineData(aLine);
  char tableNameBuf[MAX_LENGTH], endChar = ' ';
  PRInt32 majorVersion, minorVersion, numConverted;
  // Use trailing endChar to make sure the update token gets parsed.
  numConverted = PR_sscanf(lineData.get(), "[%s %d.%d update%c", tableNameBuf,
                           &majorVersion, &minorVersion, &endChar);
  if (numConverted != 4 || endChar != ']') {
    // Check to see if it's not an update request
    numConverted = PR_sscanf(lineData.get(), "[%s %d.%d%c", tableNameBuf,
                             &majorVersion, &minorVersion, &endChar);
    if (numConverted != 4 || endChar != ']')
      return NS_ERROR_FAILURE;
    *aIsUpdate = PR_FALSE;
  } else {
    // First sscanf worked, so it's an update string.
    *aIsUpdate = PR_TRUE;
  }

  LOG(("Is update? %d\n", *aIsUpdate));

  // Table header looks valid, go ahead and copy over the table name into the
  // return variable.
  GetDbTableName(nsCAutoString(tableNameBuf), aTableName);
  return NS_OK;
}

void
nsUrlClassifierDBServiceWorker::GetDbTableName(const nsACString& aTableName,
                                               nsCString* aDbTableName)
{
  aDbTableName->Assign(aTableName);
  aDbTableName->ReplaceChar('-', '_');
}

// -------------------------------------------------------------------------
// Proxy class implementation

NS_IMPL_THREADSAFE_ISUPPORTS2(nsUrlClassifierDBService,
                              nsIUrlClassifierDBService,
                              nsIObserver)

/* static */ nsUrlClassifierDBService*
nsUrlClassifierDBService::GetInstance()
{
  if (!sUrlClassifierDBService) {
    sUrlClassifierDBService = new nsUrlClassifierDBService();
    if (!sUrlClassifierDBService)
      return nsnull;

    NS_ADDREF(sUrlClassifierDBService);   // addref the global

    if (NS_FAILED(sUrlClassifierDBService->Init())) {
      NS_RELEASE(sUrlClassifierDBService);
      return nsnull;
    }
  } else {
    // Already exists, just add a ref
    NS_ADDREF(sUrlClassifierDBService);   // addref the return result
  }
  return sUrlClassifierDBService;
}


nsUrlClassifierDBService::nsUrlClassifierDBService()
{
}

nsUrlClassifierDBService::~nsUrlClassifierDBService()
{
  sUrlClassifierDBService = nsnull;
}

nsresult
nsUrlClassifierDBService::Init()
{
#if defined(PR_LOGGING)
  if (!gUrlClassifierDbServiceLog)
    gUrlClassifierDbServiceLog = PR_NewLogModule("UrlClassifierDbService");
#endif

  // Force the storage service to be created on the main thread.
  nsresult rv;
  nsCOMPtr<mozIStorageService> storageService =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Start the background thread.
  rv = NS_NewThread(&gDbBackgroundThread);
  if (NS_FAILED(rv))
    return rv;

  mWorker = new nsUrlClassifierDBServiceWorker();
  if (!mWorker)
    return NS_ERROR_OUT_OF_MEMORY;

  // Add an observer for shutdown
  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::Exists(const nsACString& tableName,
                                 const nsACString& key,
                                 nsIUrlClassifierCallback *c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback;
  rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                            NS_GET_IID(nsIUrlClassifierCallback),
                            c,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxyCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->Exists(tableName, key, proxyCallback);
}

NS_IMETHODIMP
nsUrlClassifierDBService::CheckTables(const nsACString & tableNames,
                                      nsIUrlClassifierCallback *c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback;
  rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                            NS_GET_IID(nsIUrlClassifierCallback),
                            c,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxyCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->CheckTables(tableNames, proxyCallback);
}

NS_IMETHODIMP
nsUrlClassifierDBService::UpdateTables(const nsACString& updateString,
                                       nsIUrlClassifierCallback *c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback;
  rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                            NS_GET_IID(nsIUrlClassifierCallback),
                            c,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxyCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->UpdateTables(updateString, proxyCallback);
}

NS_IMETHODIMP
nsUrlClassifierDBService::Update(const nsACString& aUpdateChunk)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->Update(aUpdateChunk);
}

NS_IMETHODIMP
nsUrlClassifierDBService::Finish(nsIUrlClassifierCallback *c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback;
  rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                            NS_GET_IID(nsIUrlClassifierCallback),
                            c,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxyCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->Finish(proxyCallback);
}

NS_IMETHODIMP
nsUrlClassifierDBService::CancelStream()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->CancelStream();
}

NS_IMETHODIMP
nsUrlClassifierDBService::Observe(nsISupports *aSubject, const char *aTopic,
                                  const PRUnichar *aData)
{
  if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }
  return NS_OK;
}

// Join the background thread if it exists.
nsresult
nsUrlClassifierDBService::Shutdown()
{
  if (!gDbBackgroundThread)
    return NS_OK;

  nsresult rv;
  // First close the db connection.
  if (mWorker) {
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
    rv = NS_GetProxyForObject(gDbBackgroundThread,
                              NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                              mWorker,
                              NS_PROXY_ASYNC,
                              getter_AddRefs(proxy));
    proxy->CloseDb();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  LOG(("joining background thread"));

  gDbBackgroundThread->Shutdown();
  NS_RELEASE(gDbBackgroundThread);

  return NS_OK;
}
