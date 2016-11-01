#include "HashStore.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "gtest/gtest.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

template<typename Function>
void RunTestInNewThread(Function&& aFunction);

// Return nsIFile with root directory - NS_APP_USER_PROFILE_50_DIR
// Sub-directories are passed in path argument.
already_AddRefed<nsIFile>
GetFile(const nsTArray<nsString>& path);

// ApplyUpdate will call |ApplyUpdates| of Classifier within a new thread
void ApplyUpdate(nsTArray<TableUpdate*>& updates);

void ApplyUpdate(TableUpdate* update);

