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
 *   Dave Camp <dcamp@mozilla.com>
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

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozStorageHelper.h"
#include "mozStorageCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAutoLock.h"
#include "nsCRT.h"
#include "nsICryptoHash.h"
#include "nsIDirectoryService.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsIProperties.h"
#include "nsIProxyObjectManager.h"
#include "nsToolkitCompsCID.h"
#include "nsIUrlClassifierUtils.h"
#include "nsUrlClassifierDBService.h"
#include "nsURILoader.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsVoidArray.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "nsXPCOMStrings.h"
#include "prlog.h"
#include "prlock.h"
#include "prprf.h"
#include "zlib.h"

/**
 * The DBServices stores a set of Fragments.  A fragment is one URL
 * fragment containing two or more domain components and some number
 * of path components.
 *
 * Fragment examples:
 *   example.com/
 *   www.example.com/foo/bar
 *   www.mail.example.com/mail
 *
 * Fragments are described in "Simplified Regular Expression Lookup"
 * section of the protocol document at
 * http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec
 *
 * A set of fragments is associated with a domain.  The domain for a given
 * fragment is the three-host-component domain of the fragment (two host
 * components for URLs with only two components) with a trailing slash.
 * So for the fragments listed above, the domains are example.com/,
 * www.example.com/ and mail.example.com/.  A collection of fragments for
 * a given domain is referred to in this code as an Entry.
 *
 * Entries are associated with the table from which its fragments came.
 *
 * Fragments are added to the database in chunks.  Each fragment in an entry
 * keeps track of which chunk it came from, and as a chunk is added it keeps
 * track of which entries contain its fragments.
 *
 * Fragments and domains are hashed in the database.  The hash is described
 * in the protocol document, but it's basically a truncated SHA256 hash.
 */

// NSPR_LOG_MODULES=UrlClassifierDbService:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gUrlClassifierDbServiceLog = nsnull;
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (PR_FALSE)
#endif

// Change filename each time we change the db schema.
#define DATABASE_FILENAME "urlclassifier3.sqlite"

#define MAX_HOST_COMPONENTS 5
#define MAX_PATH_COMPONENTS 4

// Updates will fail if fed chunks larger than this
#define MAX_CHUNK_SIZE (1024 * 1024)

#define KEY_LENGTH 16

// Prefs for implementing nsIURIClassifier to block page loads
#define CHECK_MALWARE_PREF      "browser.safebrowsing.malware.enabled"
#define CHECK_MALWARE_DEFAULT   PR_FALSE

// Singleton instance.
static nsUrlClassifierDBService* sUrlClassifierDBService;

// Thread that we do the updates on.
static nsIThread* gDbBackgroundThread = nsnull;

// Once we've committed to shutting down, don't do work in the background
// thread.
static PRBool gShuttingDownThread = PR_FALSE;

// -------------------------------------------------------------------------
// Hash class implementation

// A convenience wrapper around the 16-byte hash for a domain or fragment.

struct nsUrlClassifierHash
{
  PRUint8 buf[KEY_LENGTH];

  nsresult FromPlaintext(const nsACString& plainText, nsICryptoHash *hash);
  void Assign(const nsACString& str);

  const PRBool operator==(const nsUrlClassifierHash& hash) const {
    return (memcmp(buf, hash.buf, sizeof(buf)) == 0);
  }
};

nsresult
nsUrlClassifierHash::FromPlaintext(const nsACString& plainText,
                                   nsICryptoHash *hash)
{
  // From the protocol doc:
  // Each entry in the chunk is composed of the 128 most significant bits
  // of the SHA 256 hash of a suffix/prefix expression.

  nsresult rv = hash->Init(nsICryptoHash::SHA256);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hash->Update
          (reinterpret_cast<const PRUint8*>(plainText.BeginReading()),
           plainText.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString hashed;
  rv = hash->Finish(PR_FALSE, hashed);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(hashed.Length() >= KEY_LENGTH,
               "not enough characters in the hash");

  memcpy(buf, hashed.BeginReading(), KEY_LENGTH);

  return NS_OK;
}

void
nsUrlClassifierHash::Assign(const nsACString& str)
{
  NS_ASSERTION(str.Length() >= KEY_LENGTH,
               "string must be at least KEY_LENGTH characters long");
  memcpy(buf, str.BeginReading(), KEY_LENGTH);
}

// -------------------------------------------------------------------------
// Entry class implementation

// This class represents one entry in the classifier database.  It is a list
// of fragments and their associated chunks for a given key/table pair.
class nsUrlClassifierEntry
{
public:
  nsUrlClassifierEntry() : mId(0) {}
  ~nsUrlClassifierEntry() {}

  // Read an entry from a database statement
  PRBool ReadStatement(mozIStorageStatement* statement);

  // Prepare a statement to write this entry to the database
  nsresult BindStatement(mozIStorageStatement* statement);

  // Add a single fragment associated with a given chunk
  PRBool AddFragment(const nsUrlClassifierHash& hash, PRUint32 chunkNum);

  // Add all the fragments in a given entry to this entry
  PRBool Merge(const nsUrlClassifierEntry& entry);

  // Remove all fragments in a given entry from this entry
  PRBool SubtractFragments(const nsUrlClassifierEntry& entry);

  // Remove all fragments associated with a given chunk
  PRBool SubtractChunk(PRUint32 chunkNum);

  // Check if there is a fragment with this hash in the entry
  PRBool HasFragment(const nsUrlClassifierHash& hash);

  // Clear out the entry structure
  void Clear();

  PRBool IsEmpty() { return mFragments.Length() == 0; }

  nsUrlClassifierHash mKey;
  PRUint32 mId;
  PRUint32 mTableId;

private:
  // Add all the fragments from a database blob
  PRBool AddFragments(const PRUint8* blob, PRUint32 blobLength);

  // One hash/chunkID pair in the fragment
  struct Fragment {
    nsUrlClassifierHash hash;
    PRUint32 chunkNum;

    PRInt32 Diff(const Fragment& fragment) const {
      PRInt32 cmp = memcmp(hash.buf, fragment.hash.buf, sizeof(hash.buf));
      if (cmp != 0) return cmp;
      return chunkNum - fragment.chunkNum;
    }

    PRBool operator==(const Fragment& fragment) const {
      return (Diff(fragment) == 0);
    }

    PRBool operator<(const Fragment& fragment) const {
      return (Diff(fragment) < 0);
    }
  };

  nsTArray<Fragment> mFragments;
};

PRBool
nsUrlClassifierEntry::ReadStatement(mozIStorageStatement* statement)
{
  mId = statement->AsInt32(0);

  PRUint32 size;
  const PRUint8* blob = statement->AsSharedBlob(1, &size);
  if (!blob || (size != KEY_LENGTH))
    return PR_FALSE;
  memcpy(mKey.buf, blob, KEY_LENGTH);

  blob = statement->AsSharedBlob(2, &size);
  if (!AddFragments(blob, size))
    return PR_FALSE;

  mTableId = statement->AsInt32(3);

  return PR_TRUE;
}

nsresult
nsUrlClassifierEntry::BindStatement(mozIStorageStatement* statement)
{
  nsresult rv;

  if (mId == 0)
    rv = statement->BindNullParameter(0);
  else
    rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindBlobParameter(1, mKey.buf, KEY_LENGTH);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the entries as one big blob.
  // This results in a database that isn't portable between machines.
  rv = statement->BindBlobParameter
    (2, reinterpret_cast<PRUint8*>(mFragments.Elements()),
       mFragments.Length() * sizeof(Fragment));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(3, mTableId);
  NS_ENSURE_SUCCESS(rv, rv);

  return PR_TRUE;
}

PRBool
nsUrlClassifierEntry::AddFragment(const nsUrlClassifierHash& hash,
                                  PRUint32 chunkNum)
{
  Fragment* fragment = mFragments.AppendElement();
  if (!fragment)
    return PR_FALSE;

  fragment->hash = hash;
  fragment->chunkNum = chunkNum;

  return PR_TRUE;
}

PRBool
nsUrlClassifierEntry::AddFragments(const PRUint8* blob, PRUint32 blobLength)
{
  NS_ASSERTION(blobLength % sizeof(Fragment) == 0,
               "Fragment blob not the right length");
  Fragment* fragment = mFragments.AppendElements
    (reinterpret_cast<const Fragment*>(blob), blobLength / sizeof(Fragment));
  return (fragment != nsnull);
}

PRBool
nsUrlClassifierEntry::Merge(const nsUrlClassifierEntry& entry)
{
  Fragment* fragment = mFragments.AppendElements(entry.mFragments);
  return (fragment != nsnull);
}

PRBool
nsUrlClassifierEntry::SubtractFragments(const nsUrlClassifierEntry& entry)
{
  for (PRUint32 i = 0; i < entry.mFragments.Length(); i++) {
    for (PRUint32 j = 0; j < mFragments.Length(); j++) {
      if (mFragments[j].hash == entry.mFragments[i].hash) {
        mFragments.RemoveElementAt(j);
        break;
      }
    }
  }

  return PR_TRUE;
}

PRBool
nsUrlClassifierEntry::SubtractChunk(PRUint32 chunkNum)
{
  PRUint32 i = 0;
  while (i < mFragments.Length()) {
    if (mFragments[i].chunkNum == chunkNum)
      mFragments.RemoveElementAt(i);
    else
      i++;
  }

  return PR_TRUE;
}

PRBool
nsUrlClassifierEntry::HasFragment(const nsUrlClassifierHash& hash)
{
  for (PRUint32 i = 0; i < mFragments.Length(); i++) {
    const Fragment& fragment = mFragments[i];
    if (fragment.hash == hash)
      return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsUrlClassifierEntry::Clear()
{
  mId = 0;
  mFragments.Clear();
}

// -------------------------------------------------------------------------
// Actual worker implemenatation
class nsUrlClassifierDBServiceWorker : public nsIUrlClassifierDBServiceWorker
{
public:
  nsUrlClassifierDBServiceWorker();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE
  NS_DECL_NSIURLCLASSIFIERDBSERVICEWORKER

  // Initialize, called in the main thread
  nsresult Init();

  // Queue a lookup for the worker to perform, called in the main thread.
  nsresult QueueLookup(const nsACString& lookupKey,
                       nsIUrlClassifierCallback* callback);

private:
  // No subclassing
  ~nsUrlClassifierDBServiceWorker();

  // Disallow copy constructor
  nsUrlClassifierDBServiceWorker(nsUrlClassifierDBServiceWorker&);

  // Try to open the db, DATABASE_FILENAME.
  nsresult OpenDb();

  // Create table in the db if they don't exist.
  nsresult MaybeCreateTables(mozIStorageConnection* connection);

  nsresult GetTableName(PRUint32 tableId, nsACString& table);
  nsresult GetTableId(const nsACString& table, PRUint32* tableId);

  // Read the entry for a given key/table from the database
  nsresult ReadEntry(const nsUrlClassifierHash& key,
                     PRUint32 tableId,
                     nsUrlClassifierEntry& entry);

  // Read the entry with a given ID from the database
  nsresult ReadEntry(PRUint32 id, nsUrlClassifierEntry& entry);

  // Remove an entry from the database
  nsresult DeleteEntry(nsUrlClassifierEntry& entry);

  // Write an entry to the database
  nsresult WriteEntry(nsUrlClassifierEntry& entry);

  // Decompress a zlib'ed chunk (used for -exp tables)
  nsresult InflateChunk(nsACString& chunk);

  // Expand a chunk into its individual entries
  nsresult GetChunkEntries(const nsACString& table,
                           PRUint32 tableId,
                           PRUint32 chunkNum,
                           nsACString& chunk,
                           nsTArray<nsUrlClassifierEntry>& entries);

  // Expand a stringified chunk list into an array of ints.
  nsresult ParseChunkList(const nsACString& chunkStr,
                          nsTArray<PRUint32>& chunks);

  // Join an array of ints into a stringified chunk list.
  nsresult JoinChunkList(nsTArray<PRUint32>& chunks, nsCString& chunkStr);

  // List the add/subtract chunks that have been applied to a table
  nsresult GetChunkLists(PRUint32 tableId,
                         nsACString& addChunks,
                         nsACString& subChunks);

  // Set the list of add/subtract chunks that have been applied to a table
  nsresult SetChunkLists(PRUint32 tableId,
                         const nsACString& addChunks,
                         const nsACString& subChunks);

  // Add a list of entries to the database, merging with
  // existing entries as necessary
  nsresult AddChunk(PRUint32 tableId, PRUint32 chunkNum,
                    nsTArray<nsUrlClassifierEntry>& entries);

  // Expire an add chunk
  nsresult ExpireAdd(PRUint32 tableId, PRUint32 chunkNum);

  // Subtract a list of entries from the database
  nsresult SubChunk(PRUint32 tableId, PRUint32 chunkNum,
                    nsTArray<nsUrlClassifierEntry>& entries);

  // Expire a subtract chunk
  nsresult ExpireSub(PRUint32 tableId, PRUint32 chunkNum);

  // Handle line-oriented control information from a stream update
  nsresult ProcessResponseLines(PRBool* done);
  // Handle chunk data from a stream update
  nsresult ProcessChunk(PRBool* done);

  // Reset an in-progress update
  void ResetUpdate();

  // take a lookup string (www.hostname.com/path/to/resource.html) and
  // expand it into the set of fragments that should be searched for in an
  // entry
  nsresult GetLookupFragments(const nsCSubstring& spec,
                              nsTArray<nsUrlClassifierHash>& fragments);

  // Get the database key for a given URI.  This is the top three
  // domain components if they exist, otherwise the top two.
  //  hostname.com/foo/bar -> hostname
  //  mail.hostname.com/foo/bar -> mail.hostname.com
  //  www.mail.hostname.com/foo/bar -> mail.hostname.com
  nsresult GetKey(const nsACString& spec, nsUrlClassifierHash& hash);

  // Look for a given lookup string (www.hostname.com/path/to/resource.html)
  // in the entries at the given key.  Return the tableids found.
  nsresult CheckKey(const nsCSubstring& spec,
                    const nsUrlClassifierHash& key,
                    nsTArray<PRUint32>& tables);

  // Perform a classifier lookup for a given url.
  nsresult DoLookup(const nsACString& spec, nsIUrlClassifierCallback* c);

  // Handle any queued-up lookups.  We call this function during long-running
  // update operations to prevent lookups from blocking for too long.
  nsresult HandlePendingLookups();

  nsCOMPtr<nsIFile> mDBFile;

  nsCOMPtr<nsICryptoHash> mCryptoHash;

  // Holds a connection to the Db.  We lazily initialize this because it has
  // to be created in the background thread (currently mozStorageConnection
  // isn't thread safe).
  nsCOMPtr<mozIStorageConnection> mConnection;

  nsCOMPtr<mozIStorageStatement> mLookupStatement;
  nsCOMPtr<mozIStorageStatement> mLookupWithTableStatement;
  nsCOMPtr<mozIStorageStatement> mLookupWithIDStatement;

  nsCOMPtr<mozIStorageStatement> mUpdateStatement;
  nsCOMPtr<mozIStorageStatement> mDeleteStatement;

  nsCOMPtr<mozIStorageStatement> mAddChunkEntriesStatement;
  nsCOMPtr<mozIStorageStatement> mGetChunkEntriesStatement;
  nsCOMPtr<mozIStorageStatement> mDeleteChunkEntriesStatement;

  nsCOMPtr<mozIStorageStatement> mGetChunkListsStatement;
  nsCOMPtr<mozIStorageStatement> mSetChunkListsStatement;

  nsCOMPtr<mozIStorageStatement> mGetTablesStatement;
  nsCOMPtr<mozIStorageStatement> mGetTableIdStatement;
  nsCOMPtr<mozIStorageStatement> mGetTableNameStatement;
  nsCOMPtr<mozIStorageStatement> mInsertTableIdStatement;

  // We receive data in small chunks that may be broken in the middle of
  // a line.  So we save the last partial line here.
  nsCString mPendingStreamUpdate;

  PRInt32 mUpdateWait;

  enum {
    STATE_LINE,
    STATE_CHUNK
  } mState;

  enum {
    CHUNK_ADD,
    CHUNK_SUB
  } mChunkType;

  PRUint32 mChunkNum;
  PRUint32 mChunkLen;

  nsCString mUpdateTable;
  PRUint32 mUpdateTableId;

  nsresult mUpdateStatus;

  // Pending lookups are stored in a queue for processing.  The queue
  // is protected by mPendingLookupLock.
  PRLock* mPendingLookupLock;

  class PendingLookup {
  public:
    nsCString mKey;
    nsCOMPtr<nsIUrlClassifierCallback> mCallback;
  };

  // list of pending lookups
  nsTArray<PendingLookup> mPendingLookups;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUrlClassifierDBServiceWorker,
                              nsIUrlClassifierDBServiceWorker)

nsUrlClassifierDBServiceWorker::nsUrlClassifierDBServiceWorker()
  : mUpdateStatus(NS_OK)
  , mPendingLookupLock(nsnull)
{
}

nsUrlClassifierDBServiceWorker::~nsUrlClassifierDBServiceWorker()
{
  NS_ASSERTION(!mConnection,
               "Db connection not closed, leaking memory!  Call CloseDb "
               "to close the connection.");
  if (mPendingLookupLock)
    PR_DestroyLock(mPendingLookupLock);
}

nsresult
nsUrlClassifierDBServiceWorker::Init()
{
  // Compute database filename

  // Because we dump raw integers into the database, this database isn't
  // portable between machine types, so store it in the local profile dir.
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                       getter_AddRefs(mDBFile));
  if (NS_FAILED(rv)) return rv;

  rv = mDBFile->Append(NS_LITERAL_STRING(DATABASE_FILENAME));
  NS_ENSURE_SUCCESS(rv, rv);

  mPendingLookupLock = PR_NewLock();
  if (!mPendingLookupLock)
    return NS_ERROR_OUT_OF_MEMORY;

  ResetUpdate();

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::QueueLookup(const nsACString& spec,
                                            nsIUrlClassifierCallback* callback)
{
  nsAutoLock lock(mPendingLookupLock);

  PendingLookup* lookup = mPendingLookups.AppendElement();
  if (!lookup) return NS_ERROR_OUT_OF_MEMORY;

  lookup->mKey = spec;
  lookup->mCallback = callback;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::GetLookupFragments(const nsACString& spec,
                                                   nsTArray<nsUrlClassifierHash>& fragments)
{
  fragments.Clear();

  nsACString::const_iterator begin, end, iter;
  spec.BeginReading(begin);
  spec.EndReading(end);

  iter = begin;
  if (!FindCharInReadable('/', iter, end)) {
    return NS_OK;
  }

  const nsCSubstring& host = Substring(begin, iter++);
  nsCAutoString path;
  path.Assign(Substring(iter, end));

  /**
   * From the protocol doc:
   * For the hostname, the client will try at most 5 different strings.  They
   * are:
   * a) The exact hostname of the url
   * b) The 4 hostnames formed by starting with the last 5 components and
   *    successivly removing the leading component.  The top-level component
   *    can be skipped.
   */
  nsCStringArray hosts;
  hosts.AppendCString(host);

  host.BeginReading(begin);
  host.EndReading(end);
  int numComponents = 0;
  while (RFindInReadable(NS_LITERAL_CSTRING("."), begin, end) &&
         numComponents < MAX_HOST_COMPONENTS) {
    // don't bother checking toplevel domains
    if (++numComponents >= 2) {
      host.EndReading(iter);
      hosts.AppendCString(Substring(end, iter));
    }
    end = begin;
    host.BeginReading(begin);
  }

  /**
   * From the protocol doc:
   * For the path, the client will also try at most 6 different strings.
   * They are:
   * a) the exact path of the url, including query parameters
   * b) the exact path of the url, without query parameters
   * c) the 4 paths formed by starting at the root (/) and
   *    successively appending path components, including a trailing
   *    slash.  This behavior should only extend up to the next-to-last
   *    path component, that is, a trailing slash should never be
   *    appended that was not present in the original url.
   */
  nsCStringArray paths;
  paths.AppendCString(path);

  path.BeginReading(iter);
  path.EndReading(end);
  if (FindCharInReadable('?', iter, end)) {
    path.BeginReading(begin);
    path = Substring(begin, iter);
    paths.AppendCString(path);
  }

  numComponents = 0;
  path.BeginReading(begin);
  path.EndReading(end);
  iter = begin;
  while (FindCharInReadable('/', iter, end) &&
         numComponents < MAX_PATH_COMPONENTS) {
    iter++;
    paths.AppendCString(Substring(begin, iter));
    numComponents++;
  }

  for (int hostIndex = 0; hostIndex < hosts.Count(); hostIndex++) {
    for (int pathIndex = 0; pathIndex < paths.Count(); pathIndex++) {
      nsCAutoString key;
      key.Assign(*hosts[hostIndex]);
      key.Append('/');
      key.Append(*paths[pathIndex]);
      LOG(("Chking %s", key.get()));

      nsUrlClassifierHash* hash = fragments.AppendElement();
      if (!hash) return NS_ERROR_OUT_OF_MEMORY;
      hash->FromPlaintext(key, mCryptoHash);
    }
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::CheckKey(const nsACString& spec,
                                         const nsUrlClassifierHash& hash,
                                         nsTArray<PRUint32>& tables)
{
  mozStorageStatementScoper lookupScoper(mLookupStatement);

  nsresult rv = mLookupStatement->BindBlobParameter
    (0, hash.buf, KEY_LENGTH);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsUrlClassifierHash> fragments;
  PRBool haveFragments = PR_FALSE;

  PRBool exists;
  rv = mLookupStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  while (exists) {
    if (!haveFragments) {
      rv = GetLookupFragments(spec, fragments);
      NS_ENSURE_SUCCESS(rv, rv);
      haveFragments = PR_TRUE;
    }

    nsUrlClassifierEntry entry;
    if (!entry.ReadStatement(mLookupStatement))
      return NS_ERROR_FAILURE;

    for (PRUint32 i = 0; i < fragments.Length(); i++) {
      if (entry.HasFragment(fragments[i])) {
        tables.AppendElement(entry.mTableId);
        break;
      }
    }

    rv = mLookupStatement->ExecuteStep(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/**
 * Lookup up a key in the database is a two step process:
 *
 * a) First we look for any Entries in the database that might apply to this
 *    url.  For each URL there are one or two possible domain names to check:
 *    the two-part domain name (example.com) and the three-part name
 *    (www.example.com).  We check the database for both of these.
 * b) If we find any entries, we check the list of fragments for that entry
 *    against the possible subfragments of the URL as described in the
 *    "Simplified Regular Expression Lookup" section of the protocol doc.
 */
nsresult
nsUrlClassifierDBServiceWorker::DoLookup(const nsACString& spec,
                                         nsIUrlClassifierCallback* c)
{
  if (gShuttingDownThread) {
    c->HandleEvent(EmptyCString());
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    c->HandleEvent(EmptyCString());
    return NS_ERROR_FAILURE;
  }

#if defined(PR_LOGGING)
  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED()) {
    clockStart = PR_IntervalNow();
  }
#endif

  nsACString::const_iterator begin, end, iter;
  spec.BeginReading(begin);
  spec.EndReading(end);

  iter = begin;
  if (!FindCharInReadable('/', iter, end)) {
    return NS_OK;
  }

  const nsCSubstring& host = Substring(begin, iter++);
  nsCStringArray hostComponents;
  hostComponents.ParseString(PromiseFlatCString(host).get(), ".");

  if (hostComponents.Count() < 2) {
    // no host or toplevel host, this won't match anything in the db
    c->HandleEvent(EmptyCString());
    return NS_OK;
  }

  // First check with two domain components
  PRInt32 last = hostComponents.Count() - 1;
  nsCAutoString lookupHost;
  lookupHost.Assign(*hostComponents[last - 1]);
  lookupHost.Append(".");
  lookupHost.Append(*hostComponents[last]);
  lookupHost.Append("/");
  nsUrlClassifierHash hash;
  hash.FromPlaintext(lookupHost, mCryptoHash);

  // we ignore failures from CheckKey because we'd rather try to find
  // more results than fail.
  nsTArray<PRUint32> resultTables;
  CheckKey(spec, hash, resultTables);

  // Now check with three domain components
  if (hostComponents.Count() > 2) {
    nsCAutoString lookupHost2;
    lookupHost2.Assign(*hostComponents[last - 2]);
    lookupHost2.Append(".");
    lookupHost2.Append(lookupHost);
    hash.FromPlaintext(lookupHost2, mCryptoHash);

    CheckKey(spec, hash, resultTables);
  }

  nsCAutoString result;
  for (PRUint32 i = 0; i < resultTables.Length(); i++) {
    nsCAutoString tableName;
    GetTableName(resultTables[i], tableName);

    // ignore GetTableName failures - we want to try to get as many of the
    // matched tables as possible
    if (!result.IsEmpty()) {
      result.Append(',');
    }
    result.Append(tableName);
  }

#if defined(PR_LOGGING)
  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("query took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }
#endif

  c->HandleEvent(result);

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::HandlePendingLookups()
{
  nsAutoLock lock(mPendingLookupLock);
  while (mPendingLookups.Length() > 0) {
    PendingLookup lookup = mPendingLookups[0];
    mPendingLookups.RemoveElementAt(0);
    lock.unlock();

    DoLookup(lookup.mKey, lookup.mCallback);

    lock.lock();
  }

  return NS_OK;
}

// Lookup a key in the db.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Lookup(const nsACString& spec,
                                       nsIUrlClassifierCallback* c,
                                       PRBool needsProxy)
{
  return HandlePendingLookups();
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::GetTables(nsIUrlClassifierCallback* c)
{
  if (gShuttingDownThread)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  mozStorageStatementScoper scoper(mGetTablesStatement);

  nsCAutoString response;
  PRBool hasMore;
  while (NS_SUCCEEDED(rv = mGetTablesStatement->ExecuteStep(&hasMore)) &&
         hasMore) {
    nsCAutoString val;
    mGetTablesStatement->GetUTF8String(0, val);

    if (val.IsEmpty()) {
      continue;
    }

    response.Append(val);
    response.Append(';');

    mGetTablesStatement->GetUTF8String(1, val);

    if (!val.IsEmpty()) {
      response.Append("a:");
      response.Append(val);
    }

    mGetTablesStatement->GetUTF8String(2, val);
    if (!val.IsEmpty()) {
      response.Append("s:");
      response.Append(val);
    }

    response.Append('\n');
  }

  if (NS_FAILED(rv)) {
    response.Truncate();
  }

  c->HandleEvent(response);

  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::GetTableId(const nsACString& table,
                                           PRUint32* tableId)
{
  mozStorageStatementScoper findScoper(mGetTableIdStatement);

  nsresult rv = mGetTableIdStatement->BindUTF8StringParameter(0, table);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mGetTableIdStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (exists) {
    *tableId = mGetTableIdStatement->AsInt32(0);
    return NS_OK;
  }

  mozStorageStatementScoper insertScoper(mInsertTableIdStatement);
  rv = mInsertTableIdStatement->BindUTF8StringParameter(0, table);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInsertTableIdStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 rowId;
  rv = mConnection->GetLastInsertRowID(&rowId);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rowId > PR_UINT32_MAX)
    return NS_ERROR_FAILURE;

  *tableId = rowId;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::GetTableName(PRUint32 tableId,
                                             nsACString& tableName)
{
  mozStorageStatementScoper findScoper(mGetTableNameStatement);
  nsresult rv = mGetTableNameStatement->BindInt32Parameter(0, tableId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mGetTableNameStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) return NS_ERROR_FAILURE;

  return mGetTableNameStatement->GetUTF8String(0, tableName);
}

nsresult
nsUrlClassifierDBServiceWorker::InflateChunk(nsACString& chunk)
{
  nsCAutoString inflated;
  char buf[4096];

  const nsPromiseFlatCString& flat = PromiseFlatCString(chunk);

  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  stream.next_in = (Bytef*)flat.get();
  stream.avail_in = flat.Length();

  if (inflateInit(&stream) != Z_OK) {
    return NS_ERROR_FAILURE;
  }

  int code;
  do {
    stream.next_out = (Bytef*)buf;
    stream.avail_out = sizeof(buf);

    code = inflate(&stream, Z_NO_FLUSH);
    PRUint32 numRead = sizeof(buf) - stream.avail_out;

    if (code == Z_OK || code == Z_STREAM_END) {
      inflated.Append(buf, numRead);
    }
  } while (code == Z_OK);

  inflateEnd(&stream);

  if (code != Z_STREAM_END) {
    return NS_ERROR_FAILURE;
  }

  chunk = inflated;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ReadEntry(const nsUrlClassifierHash& hash,
                                          PRUint32 tableId,
                                          nsUrlClassifierEntry& entry)
{
  entry.Clear();

  mozStorageStatementScoper scoper(mLookupWithTableStatement);

  nsresult rv = mLookupWithTableStatement->BindBlobParameter
                  (0, hash.buf, KEY_LENGTH);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mLookupWithTableStatement->BindInt32Parameter(1, tableId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mLookupWithTableStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    if (!entry.ReadStatement(mLookupWithTableStatement))
      return NS_ERROR_FAILURE;
  } else {
    // New entry, initialize it
    entry.mKey = hash;
    entry.mTableId = tableId;
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ReadEntry(PRUint32 id,
                                          nsUrlClassifierEntry& entry)
{
  entry.Clear();
  entry.mId = id;

  mozStorageStatementScoper scoper(mLookupWithIDStatement);

  nsresult rv = mLookupWithIDStatement->BindInt32Parameter(0, id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mLookupWithIDStatement->BindInt32Parameter(0, id);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mLookupWithIDStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    if (!entry.ReadStatement(mLookupWithIDStatement))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::DeleteEntry(nsUrlClassifierEntry& entry)
{
  if (entry.mId == 0) {
    return NS_OK;
  }

  mozStorageStatementScoper scoper(mDeleteStatement);
  mDeleteStatement->BindInt32Parameter(0, entry.mId);
  nsresult rv = mDeleteStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  entry.mId = 0;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::WriteEntry(nsUrlClassifierEntry& entry)
{
  mozStorageStatementScoper scoper(mUpdateStatement);

  if (entry.IsEmpty()) {
    return DeleteEntry(entry);
  }

  PRBool newEntry = (entry.mId == 0);

  nsresult rv = entry.BindStatement(mUpdateStatement);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mUpdateStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  if (newEntry) {
    PRInt64 rowId;
    rv = mConnection->GetLastInsertRowID(&rowId);
    NS_ENSURE_SUCCESS(rv, rv);

    if (rowId > PR_UINT32_MAX) {
      return NS_ERROR_FAILURE;
    }

    entry.mId = rowId;
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::GetKey(const nsACString& spec,
                                       nsUrlClassifierHash& hash)
{
  nsACString::const_iterator begin, end, iter;
  spec.BeginReading(begin);
  spec.EndReading(end);

  iter = begin;
  if (!FindCharInReadable('/', iter, end)) {
    return NS_OK;
  }

  const nsCSubstring& host = Substring(begin, iter++);
  nsCStringArray hostComponents;
  hostComponents.ParseString(PromiseFlatCString(host).get(), ".");

  if (hostComponents.Count() < 2)
    return NS_ERROR_FAILURE;

  PRInt32 last = hostComponents.Count() - 1;
  nsCAutoString lookupHost;

  if (hostComponents.Count() > 2) {
    lookupHost.Append(*hostComponents[last - 2]);
    lookupHost.Append(".");
  }

  lookupHost.Append(*hostComponents[last - 1]);
  lookupHost.Append(".");
  lookupHost.Append(*hostComponents[last]);
  lookupHost.Append("/");

  return hash.FromPlaintext(lookupHost, mCryptoHash);
}

nsresult
nsUrlClassifierDBServiceWorker::GetChunkEntries(const nsACString& table,
                                                PRUint32 tableId,
                                                PRUint32 chunkNum,
                                                nsACString& chunk,
                                                nsTArray<nsUrlClassifierEntry>& entries)
{
  nsresult rv;
  if (StringEndsWith(table, NS_LITERAL_CSTRING("-exp"))) {
    // regexp tables need to be ungzipped
    rv = InflateChunk(chunk);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (StringEndsWith(table, NS_LITERAL_CSTRING("-sha128"))) {
    PRUint32 start = 0;
    while (start + KEY_LENGTH + 1 <= chunk.Length()) {
      nsUrlClassifierEntry* entry = entries.AppendElement();
      if (!entry) return NS_ERROR_OUT_OF_MEMORY;

      // first 16 bytes are the domain/key
      entry->mKey.Assign(Substring(chunk, start, KEY_LENGTH));

      start += KEY_LENGTH;
      // then there is a one-byte count of fragments
      PRUint8 numEntries = static_cast<PRUint8>(chunk[start]);
      start++;

      if (numEntries == 0) {
        // if there are no fragments, the domain itself is treated as a
        // fragment
        entry->AddFragment(entry->mKey, chunkNum);
      } else {
        if (start + (numEntries * KEY_LENGTH) >= chunk.Length()) {
          // there isn't as much data as they said there would be.
          return NS_ERROR_FAILURE;
        }

        for (PRUint8 i = 0; i < numEntries; i++) {
          nsUrlClassifierHash hash;
          hash.Assign(Substring(chunk, start, KEY_LENGTH));
          entry->AddFragment(hash, chunkNum);
          start += KEY_LENGTH;
        }
      }
    }
  } else {
    nsCStringArray lines;
    lines.ParseString(PromiseFlatCString(chunk).get(), "\n");

    // non-hashed tables need to be hashed
    for (PRInt32 i = 0; i < lines.Count(); i++) {
      nsUrlClassifierEntry* entry = entries.AppendElement();
      if (!entry) return NS_ERROR_OUT_OF_MEMORY;

      rv = GetKey(*lines[i], entry->mKey);
      NS_ENSURE_SUCCESS(rv, rv);

      entry->mTableId = tableId;
      nsUrlClassifierHash hash;
      hash.FromPlaintext(*lines[i], mCryptoHash);
      entry->AddFragment(hash, mChunkNum);
    }
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ParseChunkList(const nsACString& chunkStr,
                                               nsTArray<PRUint32>& chunks)
{
  LOG(("Parsing %s", PromiseFlatCString(chunkStr).get()));

  nsCStringArray elements;
  elements.ParseString(PromiseFlatCString(chunkStr).get() , ",");

  for (PRInt32 i = 0; i < elements.Count(); i++) {
    nsCString& element = *elements[i];

    PRUint32 first;
    PRUint32 last;
    if (PR_sscanf(element.get(), "%u-%u", &first, &last) == 2) {
      if (first > last) {
        PRUint32 tmp = first;
        first = last;
        last = tmp;
      }
      for (PRUint32 num = first; num <= last; num++) {
        chunks.AppendElement(num);
      }
    } else if (PR_sscanf(element.get(), "%u", &first) == 1) {
      chunks.AppendElement(first);
    }
  }

  LOG(("Got %d elements.", chunks.Length()));

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::JoinChunkList(nsTArray<PRUint32>& chunks,
                                              nsCString& chunkStr)
{
  chunkStr.Truncate();
  chunks.Sort();

  PRUint32 i = 0;
  while (i < chunks.Length()) {
    if (i != 0) {
      chunkStr.Append(',');
    }
    chunkStr.AppendInt(chunks[i]);

    PRUint32 first = i;
    PRUint32 last = first;
    i++;
    while (i < chunks.Length() && chunks[i] == chunks[i - 1] + 1) {
      last = chunks[i++];
    }

    if (last != first) {
      chunkStr.Append('-');
      chunkStr.AppendInt(last);
    }
  }

  return NS_OK;
}


nsresult
nsUrlClassifierDBServiceWorker::GetChunkLists(PRUint32 tableId,
                                              nsACString& addChunks,
                                              nsACString& subChunks)
{
  addChunks.Truncate();
  subChunks.Truncate();

  mozStorageStatementScoper scoper(mGetChunkListsStatement);

  nsresult rv = mGetChunkListsStatement->BindInt32Parameter(0, tableId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mGetChunkListsStatement->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasMore) {
    LOG(("Getting chunks for %d, found nothing", tableId));
    return NS_OK;
  }

  rv = mGetChunkListsStatement->GetUTF8String(0, addChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mGetChunkListsStatement->GetUTF8String(1, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Getting chunks for %d, got %s %s",
       tableId,
       PromiseFlatCString(addChunks).get(),
       PromiseFlatCString(subChunks).get()));

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::SetChunkLists(PRUint32 tableId,
                                              const nsACString& addChunks,
                                              const nsACString& subChunks)
{
  mozStorageStatementScoper scoper(mSetChunkListsStatement);

  mSetChunkListsStatement->BindUTF8StringParameter(0, addChunks);
  mSetChunkListsStatement->BindUTF8StringParameter(1, subChunks);
  mSetChunkListsStatement->BindInt32Parameter(2, tableId);
  nsresult rv = mSetChunkListsStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::AddChunk(PRUint32 tableId,
                                         PRUint32 chunkNum,
                                         nsTArray<nsUrlClassifierEntry>& entries)
{
#if defined(PR_LOGGING)
  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED()) {
    clockStart = PR_IntervalNow();
  }
#endif

  LOG(("Adding %d entries to chunk %d", entries.Length(), chunkNum));

  mozStorageTransaction transaction(mConnection, PR_FALSE);

  nsCAutoString addChunks;
  nsCAutoString subChunks;

  HandlePendingLookups();

  nsresult rv = GetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRUint32> adds;
  ParseChunkList(addChunks, adds);
  adds.AppendElement(chunkNum);
  JoinChunkList(adds, addChunks);
  rv = SetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRUint32> entryIDs;

  for (PRUint32 i = 0; i < entries.Length(); i++) {
    nsUrlClassifierEntry& thisEntry = entries[i];

    HandlePendingLookups();

    nsUrlClassifierEntry existingEntry;
    rv = ReadEntry(thisEntry.mKey, tableId, existingEntry);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!existingEntry.Merge(thisEntry))
      return NS_ERROR_FAILURE;

    HandlePendingLookups();

    rv = WriteEntry(existingEntry);
    NS_ENSURE_SUCCESS(rv, rv);

    entryIDs.AppendElement(existingEntry.mId);
  }

  mozStorageStatementScoper scoper(mAddChunkEntriesStatement);
  rv = mAddChunkEntriesStatement->BindInt32Parameter(0, chunkNum);
  NS_ENSURE_SUCCESS(rv, rv);

  mAddChunkEntriesStatement->BindInt32Parameter(1, tableId);
  NS_ENSURE_SUCCESS(rv, rv);

  mAddChunkEntriesStatement->BindBlobParameter
    (2,
     reinterpret_cast<PRUint8*>(entryIDs.Elements()),
     entryIDs.Length() * sizeof(PRUint32));

  HandlePendingLookups();

  rv = mAddChunkEntriesStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  HandlePendingLookups();

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(PR_LOGGING)
  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    printf("adding chunk %d took %dms\n", chunkNum,
           PR_IntervalToMilliseconds(clockEnd - clockStart));
  }
#endif

  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::ExpireAdd(PRUint32 tableId,
                                          PRUint32 chunkNum)
{
  mozStorageTransaction transaction(mConnection, PR_FALSE);

  LOG(("Expiring chunk %d\n", chunkNum));

  nsCAutoString addChunks;
  nsCAutoString subChunks;

  HandlePendingLookups();

  nsresult rv = GetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRUint32> adds;
  ParseChunkList(addChunks, adds);
  adds.RemoveElement(chunkNum);
  JoinChunkList(adds, addChunks);
  rv = SetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper getChunkEntriesScoper(mGetChunkEntriesStatement);

  rv = mGetChunkEntriesStatement->BindInt32Parameter(0, chunkNum);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mGetChunkEntriesStatement->BindInt32Parameter(1, tableId);
  NS_ENSURE_SUCCESS(rv, rv);

  HandlePendingLookups();

  PRBool exists;
  rv = mGetChunkEntriesStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  while (exists) {
    PRUint32 size;
    const PRUint8* blob = mGetChunkEntriesStatement->AsSharedBlob(0, &size);
    if (blob) {
      const PRUint32* entries = reinterpret_cast<const PRUint32*>(blob);
      for (PRUint32 i = 0; i < (size / sizeof(PRUint32)); i++) {
        HandlePendingLookups();

        nsUrlClassifierEntry entry;
        rv = ReadEntry(entries[i], entry);
        NS_ENSURE_SUCCESS(rv, rv);

        entry.SubtractChunk(chunkNum);

        HandlePendingLookups();

        rv = WriteEntry(entry);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    HandlePendingLookups();
    rv = mGetChunkEntriesStatement->ExecuteStep(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  HandlePendingLookups();

  mozStorageStatementScoper removeScoper(mDeleteChunkEntriesStatement);
  mDeleteChunkEntriesStatement->BindInt32Parameter(0, tableId);
  mDeleteChunkEntriesStatement->BindInt32Parameter(1, chunkNum);
  rv = mDeleteChunkEntriesStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  HandlePendingLookups();

  return transaction.Commit();
}

nsresult
nsUrlClassifierDBServiceWorker::SubChunk(PRUint32 tableId,
                                         PRUint32 chunkNum,
                                         nsTArray<nsUrlClassifierEntry>& entries)
{
  mozStorageTransaction transaction(mConnection, PR_FALSE);

  nsCAutoString addChunks;
  nsCAutoString subChunks;

  HandlePendingLookups();

  nsresult rv = GetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRUint32> subs;
  ParseChunkList(subChunks, subs);
  subs.AppendElement(chunkNum);
  JoinChunkList(subs, subChunks);
  rv = SetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < entries.Length(); i++) {
    nsUrlClassifierEntry& thisEntry = entries[i];

    HandlePendingLookups();

    nsUrlClassifierEntry existingEntry;
    rv = ReadEntry(thisEntry.mKey, tableId, existingEntry);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!existingEntry.SubtractFragments(thisEntry))
      return NS_ERROR_FAILURE;

    HandlePendingLookups();

    rv = WriteEntry(existingEntry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  HandlePendingLookups();

  return transaction.Commit();
}

nsresult
nsUrlClassifierDBServiceWorker::ExpireSub(PRUint32 tableId, PRUint32 chunkNum)
{
  mozStorageTransaction transaction(mConnection, PR_FALSE);

  nsCAutoString addChunks;
  nsCAutoString subChunks;

  HandlePendingLookups();

  nsresult rv = GetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRUint32> subs;
  ParseChunkList(subChunks, subs);
  subs.RemoveElement(chunkNum);
  JoinChunkList(subs, subChunks);
  rv = SetChunkLists(tableId, addChunks, subChunks);
  NS_ENSURE_SUCCESS(rv, rv);

  HandlePendingLookups();

  return transaction.Commit();
}

nsresult
nsUrlClassifierDBServiceWorker::ProcessChunk(PRBool* done)
{
  // wait until the chunk plus terminating \n has been read
  if (mPendingStreamUpdate.Length() <= static_cast<PRUint32>(mChunkLen)) {
    *done = PR_TRUE;
    return NS_OK;
  }

  if (mPendingStreamUpdate[mChunkLen] != '\n') {
    LOG(("Didn't get a terminating newline after the chunk, failing the update"));
    return NS_ERROR_FAILURE;
  }

  nsCAutoString chunk;
  chunk.Assign(Substring(mPendingStreamUpdate, 0, mChunkLen));
  mPendingStreamUpdate = Substring(mPendingStreamUpdate, mChunkLen);

  LOG(("Handling a chunk sized %d", chunk.Length()));

  nsTArray<nsUrlClassifierEntry> entries;
  GetChunkEntries(mUpdateTable, mUpdateTableId, mChunkNum, chunk, entries);

  nsresult rv;

  if (mChunkType == CHUNK_ADD) {
    rv = AddChunk(mUpdateTableId, mChunkNum, entries);
  } else {
    rv = SubChunk(mUpdateTableId, mChunkNum, entries);
  }

  // pop off the chunk and the trailing \n
  mPendingStreamUpdate = Substring(mPendingStreamUpdate, 1);

  mState = STATE_LINE;
  *done = PR_FALSE;

  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::ProcessResponseLines(PRBool* done)
{
  PRUint32 cur = 0;
  PRInt32 next;

  nsresult rv;
  // We will run to completion unless we find a chunk line
  *done = PR_TRUE;

  nsACString& updateString = mPendingStreamUpdate;

  while(cur < updateString.Length() &&
        (next = updateString.FindChar('\n', cur)) != kNotFound) {
    const nsCSubstring& line = Substring(updateString, cur, next - cur);
    cur = next + 1;

    LOG(("Processing %s\n", PromiseFlatCString(line).get()));

    if (StringBeginsWith(line, NS_LITERAL_CSTRING("n:"))) {
      if (PR_sscanf(PromiseFlatCString(line).get(), "n:%d",
                    &mUpdateWait) != 1) {
        LOG(("Error parsing n: field: %s", PromiseFlatCString(line).get()));
        mUpdateWait = 0;
      }
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("k:"))) {
      // XXX: pleaserekey
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("i:"))) {
      const nsCSubstring& data = Substring(line, 2);
      PRInt32 comma;
      if ((comma = data.FindChar(',')) == kNotFound) {
        mUpdateTable = data;
      } else {
        mUpdateTable = Substring(data, 0, comma);
        // The rest is the mac, which we don't support for now
      }
      GetTableId(mUpdateTable, &mUpdateTableId);
      LOG(("update table: '%s' (%d)", mUpdateTable.get(), mUpdateTableId));
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("a:")) ||
               StringBeginsWith(line, NS_LITERAL_CSTRING("s:"))) {
      mState = STATE_CHUNK;
      char command;
      if (PR_sscanf(PromiseFlatCString(line).get(),
                    "%c:%d:%d", &command, &mChunkNum, &mChunkLen) != 3 ||
          mChunkLen > MAX_CHUNK_SIZE) {
        return NS_ERROR_FAILURE;
      }
      mChunkType = (command == 'a') ? CHUNK_ADD : CHUNK_SUB;

      // Done parsing lines, move to chunk state now
      *done = PR_FALSE;
      break;
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("ad:"))) {
      PRUint32 chunkNum;
      if (PR_sscanf(PromiseFlatCString(line).get(), "ad:%u", &chunkNum) != 1) {
        return NS_ERROR_FAILURE;
      }
      rv = ExpireAdd(mUpdateTableId, chunkNum);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (StringBeginsWith(line, NS_LITERAL_CSTRING("sd:"))) {
      PRUint32 chunkNum;
      if (PR_sscanf(PromiseFlatCString(line).get(), "ad:%u", &chunkNum) != 1) {
        return NS_ERROR_FAILURE;
      }
      rv = ExpireSub(mUpdateTableId, chunkNum);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      LOG(("ignoring unknown line: '%s'", PromiseFlatCString(line).get()));
    }
  }

  mPendingStreamUpdate = Substring(updateString, cur);

  return NS_OK;
}

void
nsUrlClassifierDBServiceWorker::ResetUpdate()
{
  mUpdateWait = 0;
  mState = STATE_LINE;
  mChunkNum = 0;
  mChunkLen = 0;
  mUpdateStatus = NS_OK;

  mUpdateTable.Truncate();
  mPendingStreamUpdate.Truncate();
}

/**
 * Updating the database:
 *
 * The Update() method takes a series of chunks seperated with control data,
 * as described in
 * http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec
 *
 * It will iterate through the control data until it reaches a chunk.  By
 * the time it reaches a chunk, it should have received
 * a) the table to which this chunk applies
 * b) the type of chunk (add, delete, expire add, expire delete).
 * c) the chunk ID
 * d) the length of the chunk.
 *
 * For add and subtract chunks, it needs to read the chunk data (expires
 * don't have any data).  Chunk data is a list of URI fragments whose
 * encoding depends on the type of table (which is indicated by the end
 * of the table name):
 * a) tables ending with -exp are a zlib-compressed list of URI fragments
 *    separated by newlines.
 * b) tables ending with -sha128 have the form
 *    [domain][N][frag0]...[fragN]
 *       16    1   16        16
 *    If N is 0, the domain is reused as a fragment.
 * c) any other tables are assumed to be a plaintext list of URI fragments
 *    separated by newlines.
 *
 * Update() can be fed partial data;  It will accumulate data until there is
 * enough to act on.  Finish() should be called when there will be no more
 * data.
 */
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Update(const nsACString& chunk)
{
  if (gShuttingDownThread)
    return NS_ERROR_NOT_INITIALIZED;

  HandlePendingLookups();

  LOG(("Update from Stream."));
  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open database");
    return NS_ERROR_FAILURE;
  }

  // if something has gone wrong during this update, just throw it away
  if (NS_FAILED(mUpdateStatus)) {
    return mUpdateStatus;
  }

  LOG(("Got %s\n", PromiseFlatCString(chunk).get()));

  mPendingStreamUpdate.Append(chunk);

  PRBool done = PR_FALSE;
  while (!done) {
    if (mState == STATE_CHUNK) {
      rv = ProcessChunk(&done);
    } else {
      rv = ProcessResponseLines(&done);
    }
    if (NS_FAILED(rv)) {
      mUpdateStatus = rv;
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Finish(nsIUrlClassifierCallback* aSuccessCallback,
                                       nsIUrlClassifierCallback* aErrorCallback)
{
  nsCAutoString arg;
  if (NS_SUCCEEDED(mUpdateStatus)) {
    arg.AppendInt(mUpdateWait);
    aSuccessCallback->HandleEvent(arg);
  } else {
    arg.AppendInt(mUpdateStatus);
    aErrorCallback->HandleEvent(arg);
  }

  ResetUpdate();

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CancelStream()
{
  LOG(("CancelStream"));

  ResetUpdate();

  return NS_OK;
}

// Allows the main thread to delete the connection which may be in
// a background thread.
// XXX This could be turned into a single shutdown event so the logic
// is simpler in nsUrlClassifierDBService::Shutdown.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CloseDb()
{
  if (mConnection) {
    mLookupStatement = nsnull;
    mLookupWithTableStatement = nsnull;
    mLookupWithIDStatement = nsnull;

    mUpdateStatement = nsnull;
    mDeleteStatement = nsnull;

    mAddChunkEntriesStatement = nsnull;
    mGetChunkEntriesStatement = nsnull;
    mDeleteChunkEntriesStatement = nsnull;

    mGetChunkListsStatement = nsnull;
    mSetChunkListsStatement = nsnull;

    mGetTablesStatement = nsnull;
    mGetTableIdStatement = nsnull;
    mGetTableNameStatement = nsnull;
    mInsertTableIdStatement = nsnull;

    mConnection = nsnull;
    LOG(("urlclassifier db closed\n"));
  }

  mCryptoHash = nsnull;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::OpenDb()
{
  // Connection already open, don't do anything.
  if (mConnection)
    return NS_OK;

  LOG(("Opening db\n"));

  nsresult rv;
  // open the connection
  nsCOMPtr<mozIStorageService> storageService =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageConnection> connection;
  rv = storageService->OpenDatabase(mDBFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = mDBFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = storageService->OpenDatabase(mDBFile, getter_AddRefs(connection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous=OFF"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA page_size=4096"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA default_page_size=4096"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the table
  rv = MaybeCreateTables(connection);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT * FROM moz_classifier"
                        " WHERE domain=?1"),
     getter_AddRefs(mLookupStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT * FROM moz_classifier"
                        " WHERE domain=?1 AND table_id=?2"),
     getter_AddRefs(mLookupWithTableStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT * FROM moz_classifier"
                        " WHERE id=?1"),
     getter_AddRefs(mLookupWithIDStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("INSERT OR REPLACE INTO moz_classifier"
                        " VALUES (?1, ?2, ?3, ?4)"),
     getter_AddRefs(mUpdateStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
         (NS_LITERAL_CSTRING("DELETE FROM moz_classifier"
                             " WHERE id=?1"),
          getter_AddRefs(mDeleteStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("INSERT OR REPLACE INTO moz_chunks VALUES (?1, ?2, ?3)"),
     getter_AddRefs(mAddChunkEntriesStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT entries FROM moz_chunks"
                        " WHERE chunk_id = ?1 AND table_id = ?2"),
     getter_AddRefs(mGetChunkEntriesStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("DELETE FROM moz_chunks WHERE table_id=?1 AND chunk_id=?2"),
     getter_AddRefs(mDeleteChunkEntriesStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
         (NS_LITERAL_CSTRING("SELECT add_chunks, sub_chunks FROM moz_tables"
                             " WHERE id=?1"),
          getter_AddRefs(mGetChunkListsStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
         (NS_LITERAL_CSTRING("UPDATE moz_tables"
                             " SET add_chunks=?1, sub_chunks=?2"
                             " WHERE id=?3"),
          getter_AddRefs(mSetChunkListsStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
         (NS_LITERAL_CSTRING("SELECT name, add_chunks, sub_chunks"
                             " FROM moz_tables"),
          getter_AddRefs(mGetTablesStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT id FROM moz_tables"
                        " WHERE name = ?1"),
     getter_AddRefs(mGetTableIdStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("SELECT name FROM moz_tables"
                        " WHERE id = ?1"),
     getter_AddRefs(mGetTableNameStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->CreateStatement
    (NS_LITERAL_CSTRING("INSERT INTO moz_tables(id, name, add_chunks, sub_chunks)"
                        " VALUES (null, ?1, null, null)"),
     getter_AddRefs(mInsertTableIdStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  mConnection = connection;

  mCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::MaybeCreateTables(mozIStorageConnection* connection)
{
  LOG(("MaybeCreateTables\n"));

  nsresult rv = connection->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_classifier"
                       " (id INTEGER PRIMARY KEY,"
                       "  domain BLOB,"
                       "  data BLOB,"
                       "  table_id INTEGER)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("CREATE UNIQUE INDEX IF NOT EXISTS"
                       " moz_classifier_domain_index"
                       " ON moz_classifier(domain, table_id)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_tables"
                       " (id INTEGER PRIMARY KEY,"
                       "  name TEXT,"
                       "  add_chunks TEXT,"
                       "  sub_chunks TEXT);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_chunks"
                       " (chunk_id INTEGER,"
                       "  table_id INTEGER,"
                       "  entries BLOB)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = connection->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS moz_chunks_id"
                       " ON moz_chunks(chunk_id)"));
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

// -------------------------------------------------------------------------
// Helper class for nsIURIClassifier implementation, translates table names
// to nsIURIClassifier enums.

class nsUrlClassifierClassifyCallback : public nsIUrlClassifierCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  nsUrlClassifierClassifyCallback(nsIURIClassifierCallback *c)
    : mCallback(c)
    {}

private:
  nsCOMPtr<nsIURIClassifierCallback> mCallback;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUrlClassifierClassifyCallback,
                              nsIUrlClassifierCallback)

NS_IMETHODIMP
nsUrlClassifierClassifyCallback::HandleEvent(const nsACString& tables)
{
  // XXX: we should probably have the wardens tell the service which table
  // names match with which classification.  For now the table names give
  // enough information.
  nsresult response = NS_OK;

  nsACString::const_iterator begin, end;

  tables.BeginReading(begin);
  tables.EndReading(end);
  if (FindInReadable(NS_LITERAL_CSTRING("-malware-"), begin, end)) {
    response = NS_ERROR_MALWARE_URI;
  }

  mCallback->OnClassifyComplete(response);

  return NS_OK;
}


// -------------------------------------------------------------------------
// Proxy class implementation

NS_IMPL_THREADSAFE_ISUPPORTS3(nsUrlClassifierDBService,
                              nsIUrlClassifierDBService,
                              nsIURIClassifier,
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
 : mCheckMalware(CHECK_MALWARE_DEFAULT)
{
}

nsUrlClassifierDBService::~nsUrlClassifierDBService()
{
  sUrlClassifierDBService = nsnull;
}

nsresult
nsUrlClassifierDBService::Init()
{
  NS_ASSERTION(sizeof(nsUrlClassifierHash) == KEY_LENGTH,
               "nsUrlClassifierHash must be KEY_LENGTH bytes long!");

#if defined(PR_LOGGING)
  if (!gUrlClassifierDbServiceLog)
    gUrlClassifierDbServiceLog = PR_NewLogModule("UrlClassifierDbService");
#endif

  // Force the storage service to be created on the main thread.
  nsresult rv;
  nsCOMPtr<mozIStorageService> storageService =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Force PSM to be loaded on the main thread.
  nsCOMPtr<nsICryptoHash> hash =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Should we check document loads for malware URIs?
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if (prefs) {
    PRBool tmpbool;
    rv = prefs->GetBoolPref(CHECK_MALWARE_PREF, &tmpbool);
    mCheckMalware = NS_SUCCEEDED(rv) ? tmpbool : CHECK_MALWARE_DEFAULT;

    prefs->AddObserver(CHECK_MALWARE_PREF, this, PR_FALSE);
  }

  // Start the background thread.
  rv = NS_NewThread(&gDbBackgroundThread);
  if (NS_FAILED(rv))
    return rv;

  mWorker = new nsUrlClassifierDBServiceWorker();
  if (!mWorker)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = mWorker->Init();
  if (NS_FAILED(rv)) {
    mWorker = nsnull;
    return rv;
  }

  // Add an observer for shutdown
  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return NS_ERROR_FAILURE;

  observerService->AddObserver(this, "profile-before-change", PR_FALSE);
  observerService->AddObserver(this, "xpcom-shutdown-threads", PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::Classify(nsIURI *uri,
                                   nsIURIClassifierCallback* c,
                                   PRBool* result)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  if (!mCheckMalware) {
    *result = PR_FALSE;
    return NS_OK;
  }

  nsRefPtr<nsUrlClassifierClassifyCallback> callback =
    new nsUrlClassifierClassifyCallback(c);
  if (!callback) return NS_ERROR_OUT_OF_MEMORY;

  *result = PR_TRUE;
  return LookupURI(uri, callback, PR_TRUE);
}

NS_IMETHODIMP
nsUrlClassifierDBService::Lookup(const nsACString& spec,
                                 nsIUrlClassifierCallback* c,
                                 PRBool needsProxy)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  uri = NS_GetInnermostURI(uri);
  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  return LookupURI(uri, c, needsProxy);
}

nsresult
nsUrlClassifierDBService::LookupURI(nsIURI* uri,
                                    nsIUrlClassifierCallback* c,
                                    PRBool needsProxy)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsCAutoString key;
  // Canonicalize the url
  nsCOMPtr<nsIUrlClassifierUtils> utilsService =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);
  nsresult rv = utilsService->GetKeyForURI(uri, key);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback;
  if (needsProxy) {
    // The proxy callback uses the current thread.
    rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                              NS_GET_IID(nsIUrlClassifierCallback),
                              c,
                              NS_PROXY_ASYNC,
                              getter_AddRefs(proxyCallback));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    proxyCallback = c;
  }

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  // Queue this lookup and call the lookup function to flush the queue if
  // necessary.
  rv = mWorker->QueueLookup(key, proxyCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->Lookup(EmptyCString(), nsnull, PR_FALSE);
}

NS_IMETHODIMP
nsUrlClassifierDBService::GetTables(nsIUrlClassifierCallback* c)
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

  return proxy->GetTables(proxyCallback);
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
nsUrlClassifierDBService::Finish(nsIUrlClassifierCallback* aSuccessCallback,
                                 nsIUrlClassifierCallback* aErrorCallback)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxySuccessCallback;
  if (aSuccessCallback) {
    rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                              NS_GET_IID(nsIUrlClassifierCallback),
                              aSuccessCallback,
                              NS_PROXY_ASYNC,
                              getter_AddRefs(proxySuccessCallback));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIUrlClassifierCallback> proxyErrorCallback;
  if (aErrorCallback) {
    rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                              NS_GET_IID(nsIUrlClassifierCallback),
                              aErrorCallback,
                              NS_PROXY_ASYNC,
                              getter_AddRefs(proxyErrorCallback));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The actual worker uses the background thread.
  nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
  rv = NS_GetProxyForObject(gDbBackgroundThread,
                            NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                            mWorker,
                            NS_PROXY_ASYNC,
                            getter_AddRefs(proxy));
  NS_ENSURE_SUCCESS(rv, rv);

  return proxy->Finish(proxySuccessCallback, proxyErrorCallback);
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
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefs(do_QueryInterface(aSubject, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_LITERAL_STRING(CHECK_MALWARE_PREF).Equals(aData)) {
      PRBool tmpbool;
      rv = prefs->GetBoolPref(CHECK_MALWARE_PREF, &tmpbool);
      mCheckMalware = NS_SUCCEEDED(rv) ? tmpbool : CHECK_MALWARE_DEFAULT;
    }
  } else if (!strcmp(aTopic, "profile-before-change") ||
             !strcmp(aTopic, "xpcom-shutdown-threads")) {
    Shutdown();
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

// Join the background thread if it exists.
nsresult
nsUrlClassifierDBService::Shutdown()
{
  LOG(("shutting down db service\n"));

  if (!gDbBackgroundThread)
    return NS_OK;

  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->RemoveObserver(CHECK_MALWARE_PREF, this);
  }

  nsresult rv;
  // First close the db connection.
  if (mWorker) {
    nsCOMPtr<nsIUrlClassifierDBServiceWorker> proxy;
    rv = NS_GetProxyForObject(gDbBackgroundThread,
                              NS_GET_IID(nsIUrlClassifierDBServiceWorker),
                              mWorker,
                              NS_PROXY_ASYNC,
                              getter_AddRefs(proxy));
    if (NS_SUCCEEDED(rv)) {
      rv = proxy->CloseDb();
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to post close db event");
    }
  }
  LOG(("joining background thread"));

  gShuttingDownThread = PR_TRUE;
  gDbBackgroundThread->Shutdown();
  NS_RELEASE(gDbBackgroundThread);

  return NS_OK;
}
