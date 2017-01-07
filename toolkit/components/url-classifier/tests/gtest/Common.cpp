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

template<typename Function>
void RunTestInNewThread(Function&& aFunction) {
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(mozilla::Forward<Function>(aFunction));
  nsCOMPtr<nsIThread> testingThread;
  nsresult rv =
    NS_NewNamedThread("Testing Thread", getter_AddRefs(testingThread), r);
  ASSERT_EQ(rv, NS_OK);
  testingThread->Shutdown();
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

  RunTestInNewThread([&] () -> void {
    classifier->ApplyUpdates(&updates);
  });
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

