#include "Common.h"
#include "HashStore.h"
#include "Classifier.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsTArray.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsUrlClassifierUtils.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

#define GTEST_SAFEBROWSING_DIR NS_LITERAL_CSTRING("safebrowsing")
#define GTEST_TABLE NS_LITERAL_CSTRING("gtest-malware-proto")

template<typename Function>
void RunTestInNewThread(Function&& aFunction) {
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "RunTestInNewThread", mozilla::Forward<Function>(aFunction));
  nsCOMPtr<nsIThread> testingThread;
  nsresult rv =
    NS_NewNamedThread("Testing Thread", getter_AddRefs(testingThread), r);
  ASSERT_EQ(rv, NS_OK);
  testingThread->Shutdown();
}

nsresult SyncApplyUpdates(Classifier* aClassifier,
                          nsTArray<TableUpdate*>* aUpdates)
{
  // We need to spin a new thread specifically because the callback
  // will be on the caller thread. If we call Classifier::AsyncApplyUpdates
  // and wait on the same thread, this function will never return.

  nsresult ret = NS_ERROR_FAILURE;
  bool done = false;
  auto onUpdateComplete = [&done, &ret](nsresult rv) {
    // We are on the "ApplyUpdate" thread. Post an event to main thread
    // so that we can avoid busy waiting on the main thread.
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("SyncApplyUpdates", [&done, &ret, rv] {
        ret = rv;
        done = true;
      });
    NS_DispatchToMainThread(r);
  };

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("SyncApplyUpdates", [&]() {
    nsresult rv = aClassifier->AsyncApplyUpdates(aUpdates, onUpdateComplete);
    if (NS_FAILED(rv)) {
      onUpdateComplete(rv);
    }
  });

  nsCOMPtr<nsIThread> testingThread;
  NS_NewNamedThread("ApplyUpdates", getter_AddRefs(testingThread));
  if (!testingThread) {
    return NS_ERROR_FAILURE;
  }

  testingThread->Dispatch(r, NS_DISPATCH_NORMAL);

  // NS_NewCheckSummedOutputStream in HashStore::WriteFile
  // will synchronously init NS_CRYPTO_HASH_CONTRACTID on
  // the main thread. As a result we have to keep processing
  // pending event until |done| becomes true. If there's no
  // more pending event, what we only can do is wait.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return done; }));

  return ret;
}

already_AddRefed<nsIFile>
GetFile(const nsTArray<nsString>& path)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  for (uint32_t i = 0; i < path.Length(); i++) {
    file->Append(path[i]);
  }
  return file.forget();
}

void ApplyUpdate(nsTArray<TableUpdate*>& updates)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  UniquePtr<Classifier> classifier(new Classifier());
  classifier->Open(*file);

  {
    // Force nsIUrlClassifierUtils loading on main thread
    // because nsIUrlClassifierDBService will not run in advance
    // in gtest.
    nsresult rv;
    nsCOMPtr<nsIUrlClassifierUtils> dummy =
      do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID, &rv);
      ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  SyncApplyUpdates(classifier.get(), &updates);
}

void ApplyUpdate(TableUpdate* update)
{
  nsTArray<TableUpdate*> updates = { update };
  ApplyUpdate(updates);
}

void
PrefixArrayToPrefixStringMap(const nsTArray<nsCString>& prefixArray,
                             PrefixStringMap& out)
{
  out.Clear();

  for (uint32_t i = 0; i < prefixArray.Length(); i++) {
    const nsCString& prefix = prefixArray[i];
    nsCString* prefixString = out.LookupOrAdd(prefix.Length());
    prefixString->Append(prefix.BeginReading(), prefix.Length());
  }
}

nsresult
PrefixArrayToAddPrefixArrayV2(const nsTArray<nsCString>& prefixArray,
                              AddPrefixArray& out)
{
  out.Clear();

  for (size_t i = 0; i < prefixArray.Length(); i++) {
    // Create prefix hash from string
    Prefix hash;
    static_assert(sizeof(hash.buf) == PREFIX_SIZE, "Prefix must be 4 bytes length");
    memcpy(hash.buf, prefixArray[i].BeginReading(), PREFIX_SIZE);

    AddPrefix *add = out.AppendElement(fallible);
    if (!add) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    add->addChunk = i;
    add->prefix = hash;
  }

  return NS_OK;
}

nsCString
GeneratePrefix(const nsCString& aFragment, uint8_t aLength)
{
  Completion complete;
  nsCOMPtr<nsICryptoHash> cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  complete.FromPlaintext(aFragment, cryptoHash);

  nsCString hash;
  hash.Assign((const char *)complete.buf, aLength);
  return hash;
}

static nsresult
BuildCache(LookupCacheV2* cache, const _PrefixArray& prefixArray)
{
  AddPrefixArray prefixes;
  AddCompleteArray completions;
  nsresult rv = PrefixArrayToAddPrefixArrayV2(prefixArray, prefixes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  EntrySort(prefixes);
  return cache->Build(prefixes, completions);
}

static nsresult
BuildCache(LookupCacheV4* cache, const _PrefixArray& prefixArray)
{
  PrefixStringMap map;
  PrefixArrayToPrefixStringMap(prefixArray, map);
  return cache->Build(map);
}

template<typename T>
UniquePtr<T>
SetupLookupCache(const _PrefixArray& prefixArray)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));

  file->AppendNative(GTEST_SAFEBROWSING_DIR);

  UniquePtr<T> cache = MakeUnique<T>(GTEST_TABLE, EmptyCString(), file);
  nsresult rv = cache->Init();
  EXPECT_EQ(rv, NS_OK);

  rv = BuildCache(cache.get(), prefixArray);
  EXPECT_EQ(rv, NS_OK);

  return Move(cache);
}
