#include "LookupCache.h"
#include "LookupCacheV4.h"
#include "HashStore.h"
#include "gtest/gtest.h"
#include "nsAppDirectoryServiceDefs.h"

namespace mozilla {
namespace safebrowsing {

class PerProviderDirectoryTestUtils {
public:
  template<typename T>
  static nsIFile* InspectStoreDirectory(const T& aT)
  {
    return aT.mStoreDirectory;
  }
};

} // end of namespace safebrowsing
} // end of namespace mozilla

using namespace mozilla;
using namespace mozilla::safebrowsing;

template<typename T>
void VerifyPrivateStorePath(T* target,
                            const nsCString& aTableName,
                            const nsCString& aProvider,
                            nsCOMPtr<nsIFile> aRootDir,
                            bool aUsePerProviderStore)
{
  nsString rootStorePath;
  nsresult rv = aRootDir->GetPath(rootStorePath);
  EXPECT_EQ(rv, NS_OK);

  nsIFile* privateStoreDirectory =
    PerProviderDirectoryTestUtils::InspectStoreDirectory(*target);

  nsString privateStorePath;
  rv = privateStoreDirectory->GetPath(privateStorePath);
  ASSERT_EQ(rv, NS_OK);

  nsString expectedPrivateStorePath = rootStorePath;

  if (aUsePerProviderStore) {
    // Use API to append "provider" to the root directoy path
    nsCOMPtr<nsIFile> expectedPrivateStoreDir;
    rv = aRootDir->Clone(getter_AddRefs(expectedPrivateStoreDir));
    ASSERT_EQ(rv, NS_OK);

    expectedPrivateStoreDir->AppendNative(aProvider);
    rv = expectedPrivateStoreDir->GetPath(expectedPrivateStorePath);
    ASSERT_EQ(rv, NS_OK);
  }

  printf("table: %s\nprovider: %s\nroot path: %s\nprivate path: %s\n\n",
         aTableName.get(),
         aProvider.get(),
         NS_ConvertUTF16toUTF8(rootStorePath).get(),
         NS_ConvertUTF16toUTF8(privateStorePath).get());

  ASSERT_TRUE(privateStorePath == expectedPrivateStorePath);
}

TEST(UrlClassifierPerProviderDirectory, LookupCache)
{
  RunTestInNewThread([] () -> void {
    nsCOMPtr<nsIFile> rootDir;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(rootDir));

    // For V2 tables (NOT ending with '-proto'), root directory should be
    // used as the private store.
    {
      nsAutoCString table("goog-phish-shavar");
      nsAutoCString provider("google");
      RefPtr<LookupCacheV2> lc = new LookupCacheV2(table, provider, rootDir);
      VerifyPrivateStorePath<LookupCacheV2>(lc, table, provider, rootDir, false);
    }

    // For V4 tables, if provider is found, use per-provider subdirectory;
    // If not found, use root directory.
    {
      nsAutoCString table("goog-noprovider-proto");
      nsAutoCString provider("");
      RefPtr<LookupCacheV4> lc = new LookupCacheV4(table, provider, rootDir);
      VerifyPrivateStorePath<LookupCacheV4>(lc, table, provider, rootDir, false);
    }
    {
      nsAutoCString table("goog-phish-proto");
      nsAutoCString provider("google4");
      RefPtr<LookupCacheV4> lc = new LookupCacheV4(table, provider, rootDir);
      VerifyPrivateStorePath<LookupCacheV4>(lc, table, provider, rootDir, true);
    }
  });
}

TEST(UrlClassifierPerProviderDirectory, HashStore)
{
  RunTestInNewThread([] () -> void {
    nsCOMPtr<nsIFile> rootDir;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(rootDir));

    // For V2 tables (NOT ending with '-proto'), root directory should be
    // used as the private store.
    {
      nsAutoCString table("goog-phish-shavar");
      nsAutoCString provider("google");
      HashStore hs(table, provider, rootDir);
      VerifyPrivateStorePath(&hs, table, provider, rootDir, false);
    }
    // For V4 tables, if provider is found, use per-provider subdirectory;
    // If not found, use root directory.
    {
      nsAutoCString table("goog-noprovider-proto");
      nsAutoCString provider("");
      HashStore hs(table, provider, rootDir);
      VerifyPrivateStorePath(&hs, table, provider, rootDir, false);
    }
    {
      nsAutoCString table("goog-phish-proto");
      nsAutoCString provider("google4");
      HashStore hs(table, provider, rootDir);
      VerifyPrivateStorePath(&hs, table, provider, rootDir, true);
    }
  });
}
