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

#include "LookupCache.h"
#include "HashStore.h"
#include "nsISeekableStream.h"
#include "mozilla/Telemetry.h"
#include "prlog.h"
#include "prprf.h"

// We act as the main entry point for all the real lookups,
// so note that those are not done to the actual HashStore.
// The latter solely exists to store the data needed to handle
// the updates from the protocol.

// This module has its own store, which stores the Completions,
// mostly caching lookups that have happened over the net.
// The prefixes are cached/checked by looking them up in the
// PrefixSet.

// Data format for the ".cache" files:
//    uint32 magic           Identify the file type
//    uint32 version         Version identifier for file format
//    uint32 numCompletions  Amount of completions stored
//    0...numCompletions     256-bit Completions

// Name of the lookupcomplete cache
#define CACHE_SUFFIX ".cache"

// Name of the persistent PrefixSet storage
#define PREFIXSET_SUFFIX  ".pset"

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

const uint32 LOOKUPCACHE_MAGIC = 0x1231af3e;
const uint32 CURRENT_VERSION = 1;

LookupCache::LookupCache(const nsACString& aTableName, nsIFile* aStoreDir,
                         bool aPerClientRandomize)
  : mPrimed(false)
  , mPerClientRandomize(aPerClientRandomize)
  , mTableName(aTableName)
  , mStoreDirectory(aStoreDir)
{
}

nsresult
LookupCache::Init()
{
  mPrefixSet = new nsUrlClassifierPrefixSet();
  nsresult rv = mPrefixSet->Init(mTableName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

LookupCache::~LookupCache()
{
}

nsresult
LookupCache::Open()
{
  nsCOMPtr<nsIFile> storeFile;

  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(CACHE_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewLocalFileInputStream(getter_AddRefs(mInputStream), storeFile,
                                  PR_RDONLY);

  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    Reset();
    return rv;
  }

  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    Clear();
    UpdateHeader();
    return NS_OK;
  }

  rv = ReadHeader();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("ReadCompletions"));
  rv = ReadCompletions();
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Loading PrefixSet"));
  rv = LoadPrefixSet();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
LookupCache::Reset()
{
  LOG(("LookupCache resetting"));

  nsCOMPtr<nsIFile> storeFile;
  nsCOMPtr<nsIFile> prefixsetFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mStoreDirectory->Clone(getter_AddRefs(prefixsetFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(CACHE_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefixsetFile->AppendNative(mTableName + NS_LITERAL_CSTRING(PREFIXSET_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = storeFile->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefixsetFile->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  Clear();

  return NS_OK;
}


nsresult
LookupCache::Build(AddPrefixArray& aAddPrefixes,
                   AddCompleteArray& aAddCompletes)
{
  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LC_COMPLETIONS,
                        static_cast<uint32_t>(aAddCompletes.Length()));

  mCompletions.Clear();
  mCompletions.SetCapacity(aAddCompletes.Length());
  for (uint32 i = 0; i < aAddCompletes.Length(); i++) {
    mCompletions.AppendElement(aAddCompletes[i].CompleteHash());
  }
  aAddCompletes.Clear();
  mCompletions.Sort();

  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LC_PREFIXES,
                        static_cast<uint32_t>(aAddPrefixes.Length()));

  nsresult rv = ConstructPrefixSet(aAddPrefixes);
  NS_ENSURE_SUCCESS(rv, rv);
  mPrimed = true;

  return NS_OK;
}

#if defined(DEBUG) && defined(PR_LOGGING)
void
LookupCache::Dump()
{
  if (!LOG_ENABLED())
    return;

  for (uint32 i = 0; i < mCompletions.Length(); i++) {
    nsAutoCString str;
    mCompletions[i].ToString(str);
    LOG(("Completion: %s", str.get()));
  }
}
#endif

nsresult
LookupCache::Has(const Completion& aCompletion,
                 const Completion& aHostkey,
                 const uint32_t aHashKey,
                 bool* aHas, bool* aComplete,
                 Prefix* aOrigPrefix)
{
  *aHas = *aComplete = false;

  // check completion store first
  if (mCompletions.BinaryIndexOf(aCompletion) != nsTArray<Completion>::NoIndex) {
    LOG(("Complete in %s", mTableName.get()));
    *aComplete = true;
    *aHas = true;
    return NS_OK;
  }

  uint32_t prefix = aCompletion.ToUint32();
  uint32_t hostkey = aHostkey.ToUint32();
  uint32_t codedkey;
  nsresult rv = KeyedHash(prefix, hostkey, aHashKey, &codedkey, !mPerClientRandomize);
  NS_ENSURE_SUCCESS(rv, rv);

  Prefix codedPrefix;
  codedPrefix.FromUint32(codedkey);
  *aOrigPrefix = codedPrefix;

  bool found;
  rv = mPrefixSet->Contains(codedkey, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Probe in %s: %X, found %d", mTableName.get(), prefix, found));

  if (found) {
    *aHas = true;
  }

  return NS_OK;
}

nsresult
LookupCache::WriteFile()
{
  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(CACHE_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  // Need to close the inputstream here *before* rewriting its file.
  // Windows will fail if we don't.
  if (mInputStream) {
    rv = mInputStream->Close();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out), storeFile,
                                       PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateHeader();
  LOG(("Writing %d completions", mHeader.numCompletions));

  uint32_t written;
  rv = out->Write(reinterpret_cast<char*>(&mHeader), sizeof(mHeader), &written);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteTArray(out, mCompletions);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISafeOutputStream> safeOut = do_QueryInterface(out);
  rv = safeOut->Finish();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureSizeConsistent();
  NS_ENSURE_SUCCESS(rv, rv);

  // Reopen the file now that we've rewritten it.
  rv = NS_NewLocalFileInputStream(getter_AddRefs(mInputStream), storeFile,
                                  PR_RDONLY);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> psFile;
  rv = mStoreDirectory->Clone(getter_AddRefs(psFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = psFile->AppendNative(mTableName + NS_LITERAL_CSTRING(PREFIXSET_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrefixSet->StoreToFile(psFile);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to store the prefixset");

  return NS_OK;
}

void
LookupCache::Clear()
{
  mCompletions.Clear();
  mPrefixSet->SetPrefixes(nullptr, 0);
  mPrimed = false;
}

void
LookupCache::UpdateHeader()
{
  mHeader.magic = LOOKUPCACHE_MAGIC;
  mHeader.version = CURRENT_VERSION;
  mHeader.numCompletions = mCompletions.Length();
}

nsresult
LookupCache::EnsureSizeConsistent()
{
  nsCOMPtr<nsIFile> storeFile;
  nsresult rv = mStoreDirectory->Clone(getter_AddRefs(storeFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storeFile->AppendNative(mTableName + NS_LITERAL_CSTRING(CACHE_SUFFIX));
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t fileSize;
  rv = storeFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (fileSize < 0) {
    return NS_ERROR_FAILURE;
  }

  int64_t expectedSize = sizeof(mHeader)
                        + mHeader.numCompletions*sizeof(Completion);
  if (expectedSize != fileSize) {
    NS_WARNING("File length does not match. Probably corrupted.");
    Reset();
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

nsresult
LookupCache::ReadHeader()
{
  if (!mInputStream) {
    Clear();
    UpdateHeader();
    return NS_OK;
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);
  nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  void *buffer = &mHeader;
  rv = NS_ReadInputStreamToBuffer(mInputStream,
                                  &buffer,
                                  sizeof(Header));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mHeader.magic != LOOKUPCACHE_MAGIC || mHeader.version != CURRENT_VERSION) {
    NS_WARNING("Unexpected header data in the store.");
    Reset();
    return NS_ERROR_FILE_CORRUPTED;
  }
  LOG(("%d completions present", mHeader.numCompletions));

  rv = EnsureSizeConsistent();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
LookupCache::ReadCompletions()
{
  if (!mHeader.numCompletions) {
    mCompletions.Clear();
    return NS_OK;
  }

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);
  nsresult rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, sizeof(Header));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadTArray(mInputStream, &mCompletions, mHeader.numCompletions);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Read %d completions", mCompletions.Length()));

  return NS_OK;
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
LookupCache::GetKey(const nsACString& aSpec,
                    Completion* aHash,
                    nsCOMPtr<nsICryptoHash>& aCryptoHash)
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
    nsAutoCString key;
    key.Assign(host);
    key.Append("/");
    return aHash->FromPlaintext(key, aCryptoHash);
  }

  nsTArray<nsCString> hostComponents;
  ParseString(PromiseFlatCString(host), '.', hostComponents);

  if (hostComponents.Length() < 2)
    return NS_ERROR_FAILURE;

  int32_t last = int32_t(hostComponents.Length()) - 1;
  nsAutoCString lookupHost;

  if (hostComponents.Length() > 2) {
    lookupHost.Append(hostComponents[last - 2]);
    lookupHost.Append(".");
  }

  lookupHost.Append(hostComponents[last - 1]);
  lookupHost.Append(".");
  lookupHost.Append(hostComponents[last]);
  lookupHost.Append("/");

  return aHash->FromPlaintext(lookupHost, aCryptoHash);
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
      LOG(("Chking %s", key.get()));

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

/* We have both a prefix and a domain. Drop the domain, but
   hash the domain, the prefix and a random value together,
   ensuring any collisions happens at a different points for
   different users.
*/
/* static */ nsresult LookupCache::KeyedHash(uint32_t aPref, uint32_t aHostKey,
                                             uint32_t aUserKey, uint32_t* aOut,
                                             bool aPassthrough)
{
  /* Do not do any processing in passthrough mode. */
  if (aPassthrough) {
    *aOut = aPref;
    return NS_OK;
  }

  /* This is a reimplementation of MurmurHash3 32-bit
     based on the public domain C++ sources.
     http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
     for nblocks = 2
  */
  uint32_t c1 = 0xCC9E2D51;
  uint32_t c2 = 0x1B873593;
  uint32_t c3 = 0xE6546B64;
  uint32_t c4 = 0x85EBCA6B;
  uint32_t c5 = 0xC2B2AE35;
  uint32_t h1 = aPref; // seed
  uint32_t k1;
  uint32_t karr[2];

  karr[0] = aHostKey;
  karr[1] = aUserKey;

  for (uint32_t i = 0; i < 2; i++) {
    k1 = karr[i];
    k1 *= c1;
    k1 = (k1 << 15) | (k1 >> (32-15));
    k1 *= c2;

    h1 ^= k1;
    h1 = (h1 << 13) | (h1 >> (32-13));
    h1 *= 5;
    h1 += c3;
  }

  h1 ^= 2; // len
  // fmix
  h1 ^= h1 >> 16;
  h1 *= c4;
  h1 ^= h1 >> 13;
  h1 *= c5;
  h1 ^= h1 >> 16;

  *aOut = h1;

  return NS_OK;
}

bool LookupCache::IsPrimed()
{
  return mPrimed;
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
LookupCache::ConstructPrefixSet(AddPrefixArray& aAddPrefixes)
{
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_PS_CONSTRUCT_TIME> timer;

  nsTArray<uint32_t> array;
  array.SetCapacity(aAddPrefixes.Length());

  for (uint32 i = 0; i < aAddPrefixes.Length(); i++) {
    array.AppendElement(aAddPrefixes[i].PrefixHash().ToUint32());
  }
  aAddPrefixes.Clear();

#ifdef DEBUG
  // PrefixSet requires sorted order
  EnsureSorted(&array);
#endif

  // construct new one, replace old entries
  nsresult rv = mPrefixSet->SetPrefixes(array.Elements(), array.Length());
  if (NS_FAILED(rv)) {
    goto error_bailout;
  }

#ifdef DEBUG
  uint32_t size;
  size = mPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
  LOG(("SB tree done, size = %d bytes\n", size));
#endif

  mPrimed = true;

  return NS_OK;

 error_bailout:
  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_PS_FAILURE, 1);
  return rv;
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
    rv = mPrefixSet->LoadFromFile(psFile);
  }
  if (!exists || NS_FAILED(rv)) {
    LOG(("no (usable) stored PrefixSet found"));
  } else {
    mPrimed = true;
  }

#ifdef DEBUG
  if (mPrimed) {
    uint32_t size = mPrefixSet->SizeOfIncludingThis(moz_malloc_size_of);
    LOG(("SB tree done, size = %d bytes\n", size));
  }
#endif

  return NS_OK;
}

nsresult
LookupCache::GetPrefixes(nsTArray<uint32_t>* aAddPrefixes)
{
  if (!mPrimed) {
    // This can happen if its a new table, so no error.
    LOG(("GetPrefixes from empty LookupCache"));
    return NS_OK;
  }
  uint32_t cnt;
  uint32_t *arr;
  nsresult rv = mPrefixSet->GetPrefixes(&cnt, &arr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!aAddPrefixes->AppendElements(arr, cnt))
    return NS_ERROR_FAILURE;
  nsMemory::Free(arr);
  return NS_OK;
}


}
}
