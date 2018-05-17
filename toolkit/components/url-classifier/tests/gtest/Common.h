#include "HashStore.h"
#include "LookupCacheV4.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "gtest/gtest.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

namespace mozilla {
namespace safebrowsing {
    class Classifier;
}
}

typedef nsCString _Fragment;
typedef nsTArray<nsCString> _PrefixArray;

template<typename Function>
void RunTestInNewThread(Function&& aFunction);

// Synchronously apply updates by calling Classifier::AsyncApplyUpdates.
nsresult SyncApplyUpdates(Classifier* aClassifier,
                          nsTArray<TableUpdate*>* aUpdates);

// Return nsIFile with root directory - NS_APP_USER_PROFILE_50_DIR
// Sub-directories are passed in path argument.
already_AddRefed<nsIFile>
GetFile(const nsTArray<nsString>& path);

// ApplyUpdate will call |ApplyUpdates| of Classifier within a new thread
void ApplyUpdate(nsTArray<TableUpdate*>& updates);

void ApplyUpdate(TableUpdate* update);

// This function converts lexigraphic-sorted prefixes to a hashtable
// which key is prefix size and value is concatenated prefix string.
void PrefixArrayToPrefixStringMap(const nsTArray<nsCString>& prefixArray,
                                  PrefixStringMap& out);

nsresult PrefixArrayToAddPrefixArrayV2(const nsTArray<nsCString>& prefixArray,
                                       AddPrefixArray& out);

// Generate a hash prefix from string
nsCString GeneratePrefix(const nsCString& aFragment, uint8_t aLength);

// Create a LookupCacheV4 object with sepecified prefix array.
template<typename T>
RefPtr<T> SetupLookupCache(const _PrefixArray& prefixArray);
