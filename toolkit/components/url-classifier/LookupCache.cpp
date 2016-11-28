//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCache.h"
#include "HashStore.h"
#include "nsISeekableStream.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "Classifier.h"

// We act as the main entry point for all the real lookups,
// so note that those are not done to the actual HashStore.
// The latter solely exists to store the data needed to handle
// the updates from the protocol.

// This module provides a front for PrefixSet, mUpdateCompletions,
// and mGetHashCache, which together contain everything needed to
// provide a classification as long as the data is up to date.

// PrefixSet stores and provides lookups for 4-byte prefixes.
// mUpdateCompletions contains 32-byte completions which were
// contained in updates. They are retrieved from HashStore/.sbtore
// on startup.
// mGetHashCache contains 32-byte completions which were
// returned from the gethash server. They are not serialized,
// only cached until the next update.

// Name of the persistent PrefixSet storage
#define PREFIXSET_SUFFIX  ".pset"

// MOZ_LOG=UrlClassifierDbService:5
extern mozilla::LazyLogModule gUrlClassifierDbServiceLog;
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

namespace mozilla {
namespace safebrowsing {

const int LookupCacheV2::VER = 2;

LookupCache::LookupCache(const nsACString& aTableName,
                         const nsACString& aProvider,
                         nsIFile* aRootStoreDir)
  : mPrimed(false)
  , mTableName(aTableName)
  , mProvider(aProvider)
  , mRootStoreDirectory(aRootStoreDir)
{
  UpdateRootDirHandle(mRootStoreDirectory);
}

nsresult
LookupCache::Open()
{
  LOG(("Loading PrefixSet"));
  nsresult rv = LoadPrefixSet();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
LookupCache::UpdateRootDirHandle(nsIFile* aNewRootStoreDirectory)
{
  nsresult rv;

  if (aNewRootStoreDirectory != mRootStoreDirectory) {
    rv = aNewRootStoreDirectory->Clone(getter_AddRefs(mRootStoreDirectory));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = Classifier::GetPrivateStoreDirectory(mRootStoreDirectory,
                                            mTableName,
                                            mProvider,
                                            getter_AddRefs(mStoreDirectory));

  if (NS_FAILED(rv)) {
    LOG(("Failed to get private store directory for %s", mTableName.get()));
    mStoreDirectory = mRootStoreDirectory;
  }

  if (LOG_ENABLED()) {
    nsString path;
    mStoreDirectory->GetPath(path);
    LOG(("Private store directory for %s is %s", mTableName.get(),
                                                 NS_ConvertUTF16toUTF8(path).get()));
  }

  return rv;
}

nsresult
LookupCache::Reset()
{
  LOG(("LookupCache resetting"));

  nsCOMPtr<nsIFile> prefixsetFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(prefixsetFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefixsetFile->AppendNative(mTableName + NS_LITERAL_CSTRING(PREFIXSET_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefixsetFile->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  ClearAll();

  return NS_OK;
}

nsresult
LookupCache::AddCompletionsToCache(AddCompleteArray& aAddCompletes)
{
  for (uint32_t i = 0; i < aAddCompletes.Length(); i++) {
    if (mGetHashCache.BinaryIndexOf(aAddCompletes[i].CompleteHash()) == mGetHashCache.NoIndex) {
      mGetHashCache.AppendElement(aAddCompletes[i].CompleteHash());
    }
  }
  mGetHashCache.Sort();

  return NS_OK;
}

#if defined(DEBUG)
void
LookupCache::DumpCache()
{
  if (!LOG_ENABLED())
    return;

  for (uint32_t i = 0; i < mGetHashCache.Length(); i++) {
    nsAutoCString str;
    mGetHashCache[i].ToHexString(str);
    LOG(("Caches: %s", str.get()));
  }
}
#endif

nsresult
LookupCache::WriteFile()
{
  if (nsUrlClassifierDBService::ShutdownHasStarted()) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIFile> psFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(psFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = psFile->AppendNative(mTableName + NS_LITERAL_CSTRING(PREFIXSET_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = StoreToFile(psFile);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "failed to store the prefixset");

  return NS_OK;
}

void
LookupCache::ClearAll()
{
  ClearCache();
  ClearPrefixes();
  mPrimed = false;
}

void
LookupCache::ClearCache()
{
  mGetHashCache.Clear();
}

/* static */ bool
LookupCache::IsCanonicalizedIP(const nsACString& aHost)
{
  // The canonicalization process will have left IP addresses in dotted
  // decimal with no surprises.
  uint32_t i1, i2, i3, i4;
  char c;
  if (PR_sscanf(PromiseFlatCString(aHost).get(), "%u.%u.%u.%u%c",
                &i1, &i2, &i3, &i4, &c) == 4) {
    return (i1 <= 0xFF && i2 <= 0xFF && i3 <= 0xFF && i4 <= 0xFF);
  }

  return false;
}

/* static */ nsresult
LookupCache::GetLookupFragments(const nsACString& aSpec,
                                nsTArray<nsCString>* aFragments)

{
  aFragments->Clear();

  nsACString::const_iterator begin, end, iter;
  aSpec.BeginReading(begin);
  aSpec.EndReading(end);

  iter = begin;
  if (!FindCharInReadable('/', iter, end)) {
    return NS_OK;
  }

  const nsCSubstring& host = Substring(begin, iter++);
  nsAutoCString path;
  path.Assign(Substring(iter, end));

  /**
   * From the protocol doc:
   * For the hostname, the client will try at most 5 different strings.  They
   * are:
   * a) The exact hostname of the url
   * b) The 4 hostnames formed by starting with the last 5 components and
   *    successivly removing the leading component.  The top-level component
   *    can be skipped. This is not done if the hostname is a numerical IP.
   */
  nsTArray<nsCString> hosts;
  hosts.AppendElement(host);

  if (!IsCanonicalizedIP(host)) {
    host.BeginReading(begin);
    host.EndReading(end);
    int numHostComponents = 0;
    while (RFindInReadable(NS_LITERAL_CSTRING("."), begin, end) &&
           numHostComponents < MAX_HOST_COMPONENTS) {
      // don't bother checking toplevel domains
      if (++numHostComponents >= 2) {
        host.EndReading(iter);
        hosts.AppendElement(Substring(end, iter));
      }
      end = begin;
      host.BeginReading(begin);
    }
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
  nsTArray<nsCString> paths;
  nsAutoCString pathToAdd;

  path.BeginReading(begin);
  path.EndReading(end);
  iter = begin;
  if (FindCharInReadable('?', iter, end)) {
    pathToAdd = Substring(begin, iter);
    paths.AppendElement(pathToAdd);
    end = iter;
  }

  int numPathComponents = 1;
  iter = begin;
  while (FindCharInReadable('/', iter, end) &&
         numPathComponents < MAX_PATH_COMPONENTS) {
    iter++;
    pathToAdd.Assign(Substring(begin, iter));
    paths.AppendElement(pathToAdd);
    numPathComponents++;
  }

  // If we haven't already done so, add the full path
  if (!pathToAdd.Equals(path)) {
    paths.AppendElement(path);
  }
  // Check an empty path (for whole-domain blacklist entries)
  paths.AppendElement(EmptyCString());

  for (uint32_t hostIndex = 0; hostIndex < hosts.Length(); hostIndex++) {
    for (uint32_t pathIndex = 0; pathIndex < paths.Length(); pathIndex++) {
      nsCString key;
      key.Assign(hosts[hostIndex]);
      key.Append('/');
      key.Append(paths[pathIndex]);
      LOG(("Checking fragment %s", key.get()));

      aFragments->AppendElement(key);
    }
  }

  return NS_OK;
}

/* static */ nsresult
LookupCache::GetHostKeys(const nsACString& aSpec,
                         nsTArray<nsCString>* aHostKeys)
{
  nsACString::const_iterator begin, end, iter;
  aSpec.BeginReading(begin);
  aSpec.EndReading(end);

  iter = begin;
  if (!FindCharInReadable('/', iter, end)) {
    return NS_OK;
  }

  const nsCSubstring& host = Substring(begin, iter);

  if (IsCanonicalizedIP(host)) {
    nsCString *key = aHostKeys->AppendElement();
    if (!key)
      return NS_ERROR_OUT_OF_MEMORY;

    key->Assign(host);
    key->Append("/");
    return NS_OK;
  }

  nsTArray<nsCString> hostComponents;
  ParseString(PromiseFlatCString(host), '.', hostComponents);

  if (hostComponents.Length() < 2) {
    // no host or toplevel host, this won't match anything in the db
    return NS_OK;
  }

  // First check with two domain components
  int32_t last = int32_t(hostComponents.Length()) - 1;
  nsCString *lookupHost = aHostKeys->AppendElement();
  if (!lookupHost)
    return NS_ERROR_OUT_OF_MEMORY;

  lookupHost->Assign(hostComponents[last - 1]);
  lookupHost->Append(".");
  lookupHost->Append(hostComponents[last]);
  lookupHost->Append("/");

  // Now check with three domain components
  if (hostComponents.Length() > 2) {
    nsCString *lookupHost2 = aHostKeys->AppendElement();
    if (!lookupHost2)
      return NS_ERROR_OUT_OF_MEMORY;
    lookupHost2->Assign(hostComponents[last - 2]);
    lookupHost2->Append(".");
    lookupHost2->Append(*lookupHost);
  }

  return NS_OK;
}

nsresult
LookupCache::LoadPrefixSet()
{
  nsCOMPtr<nsIFile> psFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(psFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = psFile->AppendNative(mTableName + NS_LITERAL_CSTRING(PREFIXSET_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = psFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    LOG(("stored PrefixSet exists, loading from disk"));
    rv = LoadFromFile(psFile);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_FILE_CORRUPTED) {
        Reset();
      }
      return rv;
    }
    mPrimed = true;
  } else {
    LOG(("no (usable) stored PrefixSet found"));
  }

#ifdef DEBUG
  if (mPrimed) {
    uint32_t size = SizeOfPrefixSet();
    LOG(("SB tree done, size = %d bytes\n", size));
  }
#endif

  return NS_OK;
}

nsresult
LookupCacheV2::Init()
{
  mPrefixSet = new nsUrlClassifierPrefixSet();
  nsresult rv = mPrefixSet->Init(mTableName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
LookupCacheV2::Open()
{
  nsresult rv = LookupCache::Open();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Reading Completions"));
  rv = ReadCompletions();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
LookupCacheV2::ClearAll()
{
  LookupCache::ClearAll();
  mUpdateCompletions.Clear();
}

nsresult
LookupCacheV2::Has(const Completion& aCompletion,
                   bool* aHas, bool* aComplete)
{
  *aHas = *aComplete = false;

  uint32_t prefix = aCompletion.ToUint32();

  bool found;
  nsresult rv = mPrefixSet->Contains(prefix, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Probe in %s: %X, found %d", mTableName.get(), prefix, found));

  if (found) {
    *aHas = true;
  }

  if ((mGetHashCache.BinaryIndexOf(aCompletion) != nsTArray<Completion>::NoIndex) ||
      (mUpdateCompletions.BinaryIndexOf(aCompletion) != nsTArray<Completion>::NoIndex)) {
    LOG(("Complete in %s", mTableName.get()));
    *aComplete = true;
    *aHas = true;
  }

  return NS_OK;
}

nsresult
LookupCacheV2::Build(AddPrefixArray& aAddPrefixes,
                     AddCompleteArray& aAddCompletes)
{
  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LC_COMPLETIONS,
                        static_cast<uint32_t>(aAddCompletes.Length()));

  mUpdateCompletions.Clear();
  mUpdateCompletions.SetCapacity(aAddCompletes.Length());
  for (uint32_t i = 0; i < aAddCompletes.Length(); i++) {
    mUpdateCompletions.AppendElement(aAddCompletes[i].CompleteHash());
  }
  aAddCompletes.Clear();
  mUpdateCompletions.Sort();

  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LC_PREFIXES,
                        static_cast<uint32_t>(aAddPrefixes.Length()));

  nsresult rv = ConstructPrefixSet(aAddPrefixes);
  NS_ENSURE_SUCCESS(rv, rv);
  mPrimed = true;

  return NS_OK;
}

nsresult
LookupCacheV2::GetPrefixes(FallibleTArray<uint32_t>& aAddPrefixes)
{
  if (!mPrimed) {
    // This can happen if its a new table, so no error.
    LOG(("GetPrefixes from empty LookupCache"));
    return NS_OK;
  }
  return mPrefixSet->GetPrefixesNative(aAddPrefixes);
}

nsresult
LookupCacheV2::ReadCompletions()
{
  HashStore store(mTableName, mProvider, mRootStoreDirectory);

  nsresult rv = store.Open();
  NS_ENSURE_SUCCESS(rv, rv);

  mUpdateCompletions.Clear();

  const AddCompleteArray& addComplete = store.AddCompletes();
  for (uint32_t i = 0; i < addComplete.Length(); i++) {
    mUpdateCompletions.AppendElement(addComplete[i].complete);
  }

  return NS_OK;
}

nsresult
LookupCacheV2::ClearPrefixes()
{
  return mPrefixSet->SetPrefixes(nullptr, 0);
}

nsresult
LookupCacheV2::StoreToFile(nsIFile* aFile)
{
  return mPrefixSet->StoreToFile(aFile);
}

nsresult
LookupCacheV2::LoadFromFile(nsIFile* aFile)
{
  return mPrefixSet->LoadFromFile(aFile);
}

size_t
LookupCacheV2::SizeOfPrefixSet()
{
  return mPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
}

#ifdef DEBUG
template <class T>
static void EnsureSorted(T* aArray)
{
  typename T::elem_type* start = aArray->Elements();
  typename T::elem_type* end = aArray->Elements() + aArray->Length();
  typename T::elem_type* iter = start;
  typename T::elem_type* previous = start;

  while (iter != end) {
    previous = iter;
    ++iter;
    if (iter != end) {
      MOZ_ASSERT(*previous <= *iter);
    }
  }
  return;
}
#endif

nsresult
LookupCacheV2::ConstructPrefixSet(AddPrefixArray& aAddPrefixes)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_CONSTRUCT_TIME> timer;

  nsTArray<uint32_t> array;
  if (!array.SetCapacity(aAddPrefixes.Length(), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aAddPrefixes.Length(); i++) {
    array.AppendElement(aAddPrefixes[i].PrefixHash().ToUint32());
  }
  aAddPrefixes.Clear();

#ifdef DEBUG
  // PrefixSet requires sorted order
  EnsureSorted(&array);
#endif

  // construct new one, replace old entries
  nsresult rv = mPrefixSet->SetPrefixes(array.Elements(), array.Length());
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  uint32_t size;
  size = mPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
  LOG(("SB tree done, size = %d bytes\n", size));
#endif

  mPrimed = true;

  return NS_OK;
}

#if defined(DEBUG)
void
LookupCacheV2::DumpCompletions()
{
  if (!LOG_ENABLED())
    return;

  for (uint32_t i = 0; i < mUpdateCompletions.Length(); i++) {
    nsAutoCString str;
    mUpdateCompletions[i].ToHexString(str);
    LOG(("Update: %s", str.get()));
  }
}
#endif

} // namespace safebrowsing
} // namespace mozilla
