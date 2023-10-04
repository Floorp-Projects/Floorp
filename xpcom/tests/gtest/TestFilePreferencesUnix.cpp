#include "gtest/gtest.h"

#include "mozilla/FilePreferences.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "nsIDirectoryEnumerator.h"

using namespace mozilla;

const char kForbiddenPathsPref[] = "network.file.path_blacklist";

TEST(TestFilePreferencesUnix, Parsing)
{
#define kForbidden "/tmp/forbidden"
#define kForbiddenDir "/tmp/forbidden/"
#define kForbiddenFile "/tmp/forbidden/file"
#define kOther "/tmp/other"
#define kOtherDir "/tmp/other/"
#define kOtherFile "/tmp/other/file"
#define kAllowed "/tmp/allowed"

  // This is run on exit of this function to make sure we clear the pref
  // and that behaviour with the pref cleared is correct.
  auto cleanup = MakeScopeExit([&] {
    nsresult rv = Preferences::ClearUser(kForbiddenPathsPref);
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbidden)),
              true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbiddenDir)),
              true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbiddenFile)),
              true);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kAllowed)), true);
  });

  auto CheckPrefs = [](const nsACString& aPaths) {
    nsresult rv;
    rv = Preferences::SetCString(kForbiddenPathsPref, aPaths);
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbiddenDir)),
              false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbiddenDir)),
              false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbiddenFile)),
              false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kForbidden)),
              false);
    ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kAllowed)), true);
  };

  CheckPrefs(nsLiteralCString(kForbidden));
  CheckPrefs(nsLiteralCString(kForbidden "," kOther));
  ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kOtherFile)),
            false);
  CheckPrefs(nsLiteralCString(kForbidden "," kOther ","));
  ASSERT_EQ(FilePreferences::IsAllowedPath(nsLiteralCString(kOtherFile)),
            false);
}

TEST(TestFilePreferencesUnix, Simple)
{
  nsAutoCString tempPath;

  // This is the directory we will forbid
  nsCOMPtr<nsIFile> forbiddenDir;
  nsresult rv =
      NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(forbiddenDir));
  ASSERT_EQ(rv, NS_OK);
  rv = forbiddenDir->GetNativePath(tempPath);
  ASSERT_EQ(rv, NS_OK);
  rv = forbiddenDir->AppendNative("forbidden_dir"_ns);
  ASSERT_EQ(rv, NS_OK);

  // This is executed at exit to clean up after ourselves.
  auto cleanup = MakeScopeExit([&] {
    nsresult rv = Preferences::ClearUser(kForbiddenPathsPref);
    ASSERT_EQ(rv, NS_OK);
    FilePreferences::InitPrefs();

    rv = forbiddenDir->Remove(true);
    ASSERT_EQ(rv, NS_OK);
  });

  // Create the directory
  rv = forbiddenDir->Create(nsIFile::DIRECTORY_TYPE, 0666);
  ASSERT_EQ(rv, NS_OK);

  // This is the file we will try to access
  nsCOMPtr<nsIFile> forbiddenFile;
  rv = forbiddenDir->Clone(getter_AddRefs(forbiddenFile));
  ASSERT_EQ(rv, NS_OK);
  rv = forbiddenFile->AppendNative("test_file"_ns);

  // Create the file
  ASSERT_EQ(rv, NS_OK);
  rv = forbiddenFile->Create(nsIFile::NORMAL_FILE_TYPE, 0666);

  // Get the forbidden path
  nsAutoCString forbiddenPath;
  rv = forbiddenDir->GetNativePath(forbiddenPath);
  ASSERT_EQ(rv, NS_OK);

  // Set the pref and make sure it is enforced
  rv = Preferences::SetCString(kForbiddenPathsPref, forbiddenPath);
  ASSERT_EQ(rv, NS_OK);
  FilePreferences::InitPrefs();

  // Check that we can't access some of the file attributes
  int64_t size;
  rv = forbiddenFile->GetFileSize(&size);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  bool exists;
  rv = forbiddenFile->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that we can't enumerate the directory
  nsCOMPtr<nsIDirectoryEnumerator> dirEnumerator;
  rv = forbiddenDir->GetDirectoryEntries(getter_AddRefs(dirEnumerator));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  nsCOMPtr<nsIFile> newPath;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative("."_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative("forbidden_dir"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = newPath->AppendNative("test_file"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that ./ does not bypass the filter
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendRelativeNativePath("./forbidden_dir/file"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->Exists(&exists);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that ..  does not bypass the filter
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendRelativeNativePath("allowed/../forbidden_dir/file"_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_UNRECOGNIZED_PATH);

  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(newPath));
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative("allowed"_ns);
  ASSERT_EQ(rv, NS_OK);
  rv = newPath->AppendNative(".."_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_UNRECOGNIZED_PATH);

#if defined(XP_UNIX) && !defined(ANDROID)
  nsAutoCString homePath;
  NS_GetSpecialDirectory(NS_OS_HOME_DIR, getter_AddRefs(newPath));
  newPath->GetNativePath(homePath);

  newPath->InitWithNativePath("~"_ns);
  ASSERT_TRUE(newPath->NativePath().Equals(homePath));

  newPath->InitWithNativePath("~/foo"_ns);
  ASSERT_TRUE(newPath->NativePath().Equals(homePath + "/foo"_ns));

  nsLiteralCString homeBase =
#  ifdef XP_MACOSX
      "/Users"_ns;
#  else
      "/home"_ns;
#  endif

  newPath->InitWithNativePath("~foo"_ns);
  ASSERT_TRUE(newPath->NativePath().Equals(homeBase + "/foo"_ns));

  newPath->InitWithNativePath("~foo/bar"_ns);
  ASSERT_TRUE(newPath->NativePath().Equals(homeBase + "/foo/bar"_ns));
#endif

  nsAutoCString trickyPath(tempPath);
  trickyPath.AppendLiteral("/allowed/../forbidden_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that we can't construct a path that is functionally the same
  // as the forbidden one and bypasses the filter.
  trickyPath = tempPath;
  trickyPath.AppendLiteral("/./forbidden_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath = tempPath;
  trickyPath.AppendLiteral("//forbidden_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath.Truncate();
  trickyPath.AppendLiteral("//");
  trickyPath.Append(tempPath);
  trickyPath.AppendLiteral("/forbidden_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  trickyPath.Truncate();
  trickyPath.AppendLiteral("//");
  trickyPath.Append(tempPath);
  trickyPath.AppendLiteral("//forbidden_dir/file");
  rv = newPath->InitWithNativePath(trickyPath);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  // Check that if the forbidden string is a directory, we only block access
  // to subresources, not the directory itself.
  nsAutoCString forbiddenDirPath(forbiddenPath);
  forbiddenDirPath.Append("/");
  rv = Preferences::SetCString(kForbiddenPathsPref, forbiddenDirPath);
  ASSERT_EQ(rv, NS_OK);
  FilePreferences::InitPrefs();

  // This should work, since we only block subresources
  rv = forbiddenDir->Exists(&exists);
  ASSERT_EQ(rv, NS_OK);

  rv = forbiddenDir->GetDirectoryEntries(getter_AddRefs(dirEnumerator));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);
}
