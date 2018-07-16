#include "gtest/gtest.h"

#include "mozilla/FilePreferences.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "nsISimpleEnumerator.h"

using namespace mozilla;

TEST(TestFilePreferencesUnix, Parsing)
{
  #define kBlacklisted "/tmp/blacklisted"
  #define kBlacklistedDir "/tmp/blacklisted/"
  #define kBlacklistedFile "/tmp/blacklisted/file"
  #define kOther "/tmp/other"
  #define kOtherDir "/tmp/other/"
  #define kOtherFile "/tmp/other/file"
  #define kAllowed "/tmp/allowed"

  // This is run on exit of this function to make sure we clear the pref
  // and that behaviour with the pref cleared is correct.
  auto cleanup = MakeScopeExit([&] {
    nsresult rv = Preferences::ClearUser("network.file.path_blacklist");
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklisted)), true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklistedDir)), true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklistedFile)), true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kAllowed)), true);
  });

  auto CheckPrefs = [](const nsACString& aPaths)
  {
    nsresult rv;
    rv = Preferences::SetCString("network.file.path_blacklist", aPaths);
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklistedDir)), false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklistedDir)), false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklistedFile)), false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kBlacklisted)), false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kAllowed)), true);
  };

  CheckPrefs(NS_LITERAL_CSTRING(kBlacklisted));
  CheckPrefs(NS_LITERAL_CSTRING(kBlacklisted "," kOther));
  ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kOtherFile)), false);
  CheckPrefs(NS_LITERAL_CSTRING(kBlacklisted "," kOther ","));
  ASSERT_EQ(FilePreferences::IsAllowedPath(NS_LITERAL_CSTRING(kOtherFile)), false);
}

TEST(TestFilePreferencesUnix, Simple)
{
  nsAutoCString tempPath;

  // This is the directory we will blacklist
  nsCOMPtr<nsIFile> blacklistedDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(blacklistedDir));
  ASSERT_EQ(rv, NS_OK);
  rv = blacklistedDir->GetNativePath(tempPath);
  ASSERT_EQ(rv, NS_OK);
  rv = blacklistedDir->AppendNative(NS_LITERAL_CSTRING("blacklisted_dir"));
  ASSERT_EQ(rv, NS_OK);

  // This is executed at exit to clean up after ourselves.
  auto cleanup = MakeScopeExit([&] {
    nsresult rv = Preferences::ClearUser("network.file.path_blacklist");
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();

    rv = blacklistedDir->Remove(true);
    ASSERT_EQ(rv, NS_OK);
  });

  // Create the directory
  rv = blacklistedDir->Create(nsIFile::DIRECTORY_TYPE, 0666);
  ASSERT_EQ(rv, NS_OK);

  // This is the file we will try to access
  nsCOMPtr<nsIFile> blacklistedFile;
  rv = blacklistedDir->Clone(getter_AddRefs(blacklistedFile));
  ASSERT_EQ(rv, NS_OK);
  rv = blacklistedFile->AppendNative(NS_LITERAL_CSTRING("test_file"));

  // Create the file
  ASSERT_EQ(rv, NS_OK);
  rv = blacklistedFile->Create(nsIFile::NORMAL_FILE_TYPE, 0666);

  // Get the path for the blacklist
  nsAutoCString blackListPath;
  rv = blacklistedDir->GetNativePath(blackListPath);
  ASSERT_EQ(rv, NS_OK);

  // Set the pref and make sure it is enforced
  rv = Preferences::SetCString("network.file.path_blacklist", blackListPath);
  ASSERT_EQ(rv, NS_OK);
  FilePreferences::InitPrefs();

  // Check that we can't access some of the file attributes
  int64_t size;
  rv = blacklistedFile->GetFileSize(&size);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  bool exists;
  rv = blacklistedFile->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that we can't enumerate the directory
  nsCOMPtr<nsISimpleEnumerator> dirEnumerator;
  rv = blacklistedDir->GetDirectoryEntries(getter_AddRefs(dirEnumerator));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  nsCOMPtr<nsIFile> newPath;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(NS_LITERAL_CSTRING("."));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(NS_LITERAL_CSTRING("blacklisted_dir"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = newPath->AppendNative(NS_LITERAL_CSTRING("test_file"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that ./ does not bypass the filter
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendRelativeNativePath(NS_LITERAL_CSTRING("./blacklisted_dir/file"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that ..  does not bypass the filter
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendRelativeNativePath(NS_LITERAL_CSTRING("allowed/../blacklisted_dir/file"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(NS_LITERAL_CSTRING("allowed"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(NS_LITERAL_CSTRING(".."));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(NS_LITERAL_CSTRING("blacklisted_dir"));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  nsAutoCString trickyPath(tempPath);
  trickyPath.AppendLiteral("/allowed/../blacklisted_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that we can't construct a path that is functionally the same
  // as the blacklisted one and bypasses the filter.
  trickyPath = tempPath;
  trickyPath.AppendLiteral("/./blacklisted_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath = tempPath;
  trickyPath.AppendLiteral("//blacklisted_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath.Truncate();
  trickyPath.AppendLiteral("//");
  trickyPath.Append(tempPath);
  trickyPath.AppendLiteral("/blacklisted_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath.Truncate();
  trickyPath.AppendLiteral("//");
  trickyPath.Append(tempPath);
  trickyPath.AppendLiteral("//blacklisted_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that if the blacklisted string is a directory, we only block access
  // to subresources, not the directory itself.
  nsAutoCString blacklistDirPath(blackListPath);
  blacklistDirPath.Append("/");
  rv = Preferences::SetCString("network.file.path_blacklist", blacklistDirPath);
  ASSERT_EQ(rv, NS_OK);
  FilePreferences::InitPrefs();

  // This should work, since we only block subresources
  rv = blacklistedDir->Exists(&exists);
  ASSERT_EQ(rv, NS_OK);

  rv = blacklistedDir->GetDirectoryEntries(getter_AddRefs(dirEnumerator));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);
}
