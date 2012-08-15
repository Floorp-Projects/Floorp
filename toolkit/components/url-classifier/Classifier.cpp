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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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

#include "Classifier.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIRandomGenerator.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsIFile.h"
#include "nsAutoPtr.h"
#include "mozilla/Telemetry.h"
#include "prlog.h"

// NSPR_LOG_MODULES=UrlClassifierDbService:5
extern PRLogModuleInfo *gUrlClassifierDbServiceLog;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gUrlClassifierDbServiceLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUrlClassifierDbServiceLog, 4)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

namespace mozilla {
namespace safebrowsing {

Classifier::Classifier()
  : mFreshTime(45 * 60)
  , mPerClientRandomize(true)
{
}

Classifier::~Classifier()
{
  Close();
}

/*
 * Generate a unique 32-bit key for this user, which we will
 * use to rehash all prefixes. This ensures that different users
 * will get hash collisions on different prefixes, which in turn
 * avoids that "unlucky" URLs get mysterious slowdowns, and that
 * the servers get spammed if any such URL should get slashdotted.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=669407#c10
 */
nsresult
Classifier::InitKey()
{
  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(NS_LITERAL_CSTRING("classifier.hashkey"));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = storeFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    // generate and store key
    nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1");
    NS_ENSURE_STATE(rg);

    PRUint8 *temp;
    nsresult rv = rg->GenerateRandomBytes(sizeof(mHashKey), &temp);
    NS_ENSURE_SUCCESS(rv, rv);
    memcpy(&mHashKey, temp, sizeof(mHashKey));
    NS_Free(temp);

    nsCOMPtr<nsIOutputStream> out;
    rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out), storeFile,
                                       -1, -1, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 written;
    rv = out->Write(reinterpret_cast<char*>(&mHashKey), sizeof(PRUint32), &written);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISafeOutputStream> safeOut = do_QueryInterface(out);
    rv = safeOut->Finish();
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Initialized classifier, key = %X", mHashKey));
  } else {
    // read key
    nsCOMPtr<nsIInputStream> inputStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), storeFile,
                                    -1, -1, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(inputStream);
    nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    void *buffer = &mHashKey;
    rv = NS_ReadInputStreamToBuffer(inputStream,
                                    &buffer,
                                    sizeof(PRUint32));
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Loaded classifier key = %X", mHashKey));
  }

  return NS_OK;
}

nsresult
Classifier::Open(nsIFile& aCacheDirectory)
{
  nsresult rv;

  mCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure the safebrowsing directory exists.
  rv = aCacheDirectory.Clone(getter_AddRefs(mStoreDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStoreDirectory->AppendNative(NS_LITERAL_CSTRING("safebrowsing"));
  NS_ENSURE_SUCCESS(rv, rv);

  bool storeExists;
  rv = mStoreDirectory->Exists(&storeExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!storeExists) {
    rv = mStoreDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    bool storeIsDir;
    rv = mStoreDirectory->IsDirectory(&storeIsDir);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!storeIsDir)
      return NS_ERROR_FILE_DESTINATION_NOT_DIR;
  }

  rv = InitKey();
  if (NS_FAILED(rv)) {
    // Without a usable key the database is useless
    Reset();
    return NS_ERROR_FAILURE;
  }

  mTableFreshness.Init();
  // XXX: Disk IO potentially on the main thread during startup
  RegenActiveTables();

  return NS_OK;
}

nsresult
Classifier::Close()
{
  DropStores();

  return NS_OK;
}

nsresult
Classifier::Reset()
{
  DropStores();

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mStoreDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsIFile> file;
    rv = entries->GetNext(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mTableFreshness.Clear();
  RegenActiveTables();

  return NS_OK;
}

void
Classifier::TableRequest(nsACString& aResult)
{
  nsTArray<nsCString> tables;
  ActiveTables(tables);
  for (uint32 i = 0; i < tables.Length(); i++) {
    nsAutoPtr<HashStore> store(new HashStore(tables[i], mStoreDirectory));
    if (!store)
      continue;

    nsresult rv = store->Open();
    if (NS_FAILED(rv))
      continue;

    aResult.Append(store->TableName());
    aResult.Append(";");

    ChunkSet &adds = store->AddChunks();
    ChunkSet &subs = store->SubChunks();

    if (adds.Length() > 0) {
      aResult.Append("a:");
      nsCAutoString addList;
      adds.Serialize(addList);
      aResult.Append(addList);
    }

    if (subs.Length() > 0) {
      if (adds.Length() > 0)
        aResult.Append(':');
      aResult.Append("s:");
      nsCAutoString subList;
      subs.Serialize(subList);
      aResult.Append(subList);
    }

    aResult.Append('\n');
  }
}

nsresult
Classifier::Check(const nsACString& aSpec, LookupResultArray& aResults)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CL_CHECK_TIME> timer;

  // Get the set of fragments to look up.
  nsTArray<nsCString> fragments;
  nsresult rv = LookupCache::GetLookupFragments(aSpec, &fragments);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCString> activeTables;
  ActiveTables(activeTables);

  nsTArray<LookupCache*> cacheArray;
  for (PRUint32 i = 0; i < activeTables.Length(); i++) {
    LookupCache *cache = GetLookupCache(activeTables[i]);
    if (cache) {
      cacheArray.AppendElement(cache);
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // Now check each lookup fragment against the entries in the DB.
  for (PRUint32 i = 0; i < fragments.Length(); i++) {
    Completion lookupHash;
    lookupHash.FromPlaintext(fragments[i], mCryptoHash);

    // Get list of host keys to look up
    Completion hostKey;
    rv = LookupCache::GetKey(fragments[i], &hostKey, mCryptoHash);
    if (NS_FAILED(rv)) {
      // Local host on the network
      continue;
    }

#if DEBUG && defined(PR_LOGGING)
    if (LOG_ENABLED()) {
      nsCAutoString checking;
      lookupHash.ToString(checking);
      LOG(("Checking %s (%X)", checking.get(), lookupHash.ToUint32()));
    }
#endif
    for (PRUint32 i = 0; i < cacheArray.Length(); i++) {
      LookupCache *cache = cacheArray[i];
      bool has, complete;
      Prefix codedPrefix;
      rv = cache->Has(lookupHash, hostKey, mHashKey,
                      &has, &complete, &codedPrefix);
      NS_ENSURE_SUCCESS(rv, rv);
      if (has) {
        LookupResult *result = aResults.AppendElement();
        if (!result)
          return NS_ERROR_OUT_OF_MEMORY;

        PRInt64 age;
        bool found = mTableFreshness.Get(cache->TableName(), &age);
        if (!found) {
          age = 24 * 60 * 60; // just a large number
        } else {
          PRInt64 now = (PR_Now() / PR_USEC_PER_SEC);
          age = now - age;
        }

        LOG(("Found a result in %s: %s (Age: %Lds)",
             cache->TableName().get(),
             complete ? "complete." : "Not complete.",
             age));

        result->hash.complete = lookupHash;
        result->mCodedPrefix = codedPrefix;
        result->mComplete = complete;
        result->mFresh = (age < mFreshTime);
        result->mTableName.Assign(cache->TableName());
      }
    }

  }

  return NS_OK;
}

nsresult
Classifier::ApplyUpdates(nsTArray<TableUpdate*>* aUpdates)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CL_UPDATE_TIME> timer;

#if defined(PR_LOGGING)
  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED() || true) {
    clockStart = PR_IntervalNow();
  }
#endif

  LOG(("Applying table updates."));

  nsresult rv;

  for (uint32 i = 0; i < aUpdates->Length(); i++) {
    // Previous ApplyTableUpdates() may have consumed this update..
    if ((*aUpdates)[i]) {
      // Run all updates for one table
      rv = ApplyTableUpdates(aUpdates, aUpdates->ElementAt(i)->TableName());
      if (NS_FAILED(rv)) {
        Reset();
        return rv;
      }
    }
  }
  aUpdates->Clear();
  RegenActiveTables();
  LOG(("Done applying updates."));

#if defined(PR_LOGGING)
  if (LOG_ENABLED() || true) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("update took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }
#endif

  return NS_OK;
}

nsresult
Classifier::MarkSpoiled(nsTArray<nsCString>& aTables)
{
  for (uint32 i = 0; i < aTables.Length(); i++) {
    LOG(("Spoiling table: %s", aTables[i].get()));
    // Spoil this table by marking it as no known freshness
    mTableFreshness.Remove(aTables[i]);
  }
  return NS_OK;
}

void
Classifier::DropStores()
{
  for (uint32 i = 0; i < mHashStores.Length(); i++) {
    delete mHashStores[i];
  }
  mHashStores.Clear();
  for (uint32 i = 0; i < mLookupCaches.Length(); i++) {
    delete mLookupCaches[i];
  }
  mLookupCaches.Clear();
}

nsresult
Classifier::RegenActiveTables()
{
  mActiveTablesCache.Clear();

  nsTArray<nsCString> foundTables;
  ScanStoreDir(foundTables);

  for (uint32 i = 0; i < foundTables.Length(); i++) {
    nsAutoPtr<HashStore> store(new HashStore(nsCString(foundTables[i]), mStoreDirectory));
    if (!store)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = store->Open();
    if (NS_FAILED(rv))
      continue;

    LookupCache *lookupCache = GetLookupCache(store->TableName());
    if (!lookupCache) {
      continue;
    }

    const ChunkSet &adds = store->AddChunks();
    const ChunkSet &subs = store->SubChunks();

    if (adds.Length() == 0 && subs.Length() == 0)
      continue;

    LOG(("Active table: %s", store->TableName().get()));
    mActiveTablesCache.AppendElement(store->TableName());
  }

  return NS_OK;
}

nsresult
Classifier::ScanStoreDir(nsTArray<nsCString>& aTables)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mStoreDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsIFile> file;
    rv = entries->GetNext(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString suffix(NS_LITERAL_CSTRING(".sbstore"));

    PRInt32 dot = leafName.RFind(suffix, 0);
    if (dot != -1) {
      leafName.Cut(dot, suffix.Length());
      aTables.AppendElement(leafName);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Classifier::ActiveTables(nsTArray<nsCString>& aTables)
{
  aTables = mActiveTablesCache;
  return NS_OK;
}

/*
 * This will consume+delete updates from the passed nsTArray.
*/
nsresult
Classifier::ApplyTableUpdates(nsTArray<TableUpdate*>* aUpdates,
                              const nsACString& aTable)
{
  LOG(("Classifier::ApplyTableUpdates(%s)",
       PromiseFlatCString(aTable).get()));

  nsAutoPtr<HashStore> store(new HashStore(aTable, mStoreDirectory));

  if (!store)
    return NS_ERROR_FAILURE;

  // take the quick exit if there is no valid update for us
  // (common case)
  uint32 validupdates = 0;

  for (uint32 i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(store->TableName()))
      continue;
    if (update->Empty()) {
      aUpdates->ElementAt(i) = nullptr;
      delete update;
      continue;
    }
    validupdates++;
  }

  if (!validupdates) {
    return NS_OK;
  }

  nsresult rv = store->Open();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store->BeginUpdate();
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the part of the store that is (only) in the cache
  LookupCache *prefixSet = GetLookupCache(store->TableName());
  if (!prefixSet) {
    return NS_ERROR_FAILURE;
  }
  nsTArray<PRUint32> AddPrefixHashes;
  rv = prefixSet->GetPrefixes(&AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = store->AugmentAdds(AddPrefixHashes);
  NS_ENSURE_SUCCESS(rv, rv);
  AddPrefixHashes.Clear();

  uint32 applied = 0;
  bool updateFreshness = false;

  for (uint32 i = 0; i < aUpdates->Length(); i++) {
    TableUpdate *update = aUpdates->ElementAt(i);
    if (!update || !update->TableName().Equals(store->TableName()))
      continue;

    rv = store->ApplyUpdate(*update);
    NS_ENSURE_SUCCESS(rv, rv);

    applied++;

    LOG(("Applied update to table %s:", store->TableName().get()));
    LOG(("  %d add chunks", update->AddChunks().Length()));
    LOG(("  %d add prefixes", update->AddPrefixes().Length()));
    LOG(("  %d add completions", update->AddCompletes().Length()));
    LOG(("  %d sub chunks", update->SubChunks().Length()));
    LOG(("  %d sub prefixes", update->SubPrefixes().Length()));
    LOG(("  %d sub completions", update->SubCompletes().Length()));
    LOG(("  %d add expirations", update->AddExpirations().Length()));
    LOG(("  %d sub expirations", update->SubExpirations().Length()));

    if (!update->IsLocalUpdate()) {
      updateFreshness = true;
      LOG(("Remote update, updating freshness"));
    }

    aUpdates->ElementAt(i) = nullptr;
    delete update;
  }

  LOG(("Applied %d update(s) to %s.", applied, store->TableName().get()));

  rv = store->Rebuild();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Table %s now has:", store->TableName().get()));
  LOG(("  %d add chunks", store->AddChunks().Length()));
  LOG(("  %d add prefixes", store->AddPrefixes().Length()));
  LOG(("  %d add completions", store->AddCompletes().Length()));
  LOG(("  %d sub chunks", store->SubChunks().Length()));
  LOG(("  %d sub prefixes", store->SubPrefixes().Length()));
  LOG(("  %d sub completions", store->SubCompletes().Length()));

  rv = store->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point the store is updated and written out to disk, but
  // the data is still in memory.  Build our quick-lookup table here.
  rv = prefixSet->Build(store->AddPrefixes(), store->AddCompletes());
  NS_ENSURE_SUCCESS(rv, rv);
#if defined(DEBUG) && defined(PR_LOGGING)
  prefixSet->Dump();
#endif
  rv = prefixSet->WriteFile();
  NS_ENSURE_SUCCESS(rv, rv);

  // This will drop all the temporary storage used during the update.
  rv = store->FinishUpdate();
  NS_ENSURE_SUCCESS(rv, rv);

  if (updateFreshness) {
    PRInt64 now = (PR_Now() / PR_USEC_PER_SEC);
    LOG(("Successfully updated %s", store->TableName().get()));
    mTableFreshness.Put(store->TableName(), now);
  }

  return NS_OK;
}

LookupCache *
Classifier::GetLookupCache(const nsACString& aTable)
{
  for (uint32 i = 0; i < mLookupCaches.Length(); i++) {
    if (mLookupCaches[i]->TableName().Equals(aTable)) {
      return mLookupCaches[i];
    }
  }

  LookupCache *cache = new LookupCache(aTable, mStoreDirectory,
                                       mPerClientRandomize);
  nsresult rv = cache->Init();
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  rv = cache->Open();
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_FILE_CORRUPTED) {
      Reset();
    }
    return nullptr;
  }
  mLookupCaches.AppendElement(cache);
  return cache;
}

nsresult
Classifier::ReadNoiseEntries(const Prefix& aPrefix,
                             const nsACString& aTableName,
                             PRInt32 aCount,
                             PrefixArray* aNoiseEntries)
{
  LookupCache *cache = GetLookupCache(aTableName);
  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<PRUint32> prefixes;
  nsresult rv = cache->GetPrefixes(&prefixes);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 idx = prefixes.BinaryIndexOf(aPrefix.ToUint32());

  if (idx == nsTArray<PRUint32>::NoIndex) {
    NS_WARNING("Could not find prefix in PrefixSet during noise lookup");
    return NS_ERROR_FAILURE;
  }

  idx -= idx % aCount;

  for (PRInt32 i = 0; (i < aCount) && ((idx+i) < prefixes.Length()); i++) {
    Prefix newPref;
    newPref.FromUint32(prefixes[idx+i]);
    aNoiseEntries->AppendElement(newPref);
  }

  return NS_OK;
}

}
}
