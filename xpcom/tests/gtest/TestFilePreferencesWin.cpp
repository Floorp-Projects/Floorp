#include "gtest/gtest.h"

#include "mozilla/FilePreferences.h"
#include "nsIFile.h"
#include "nsXPCOMCID.h"

TEST(FilePreferencesWin, Normalization)
{
  nsAutoString normalized;

  mozilla::FilePreferences::testing::NormalizePath(NS_LITERAL_STRING("foo"),
                                                   normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("foo"));

  mozilla::FilePreferences::testing::NormalizePath(NS_LITERAL_STRING("\\foo"),
                                                   normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\foo"));

  mozilla::FilePreferences::testing::NormalizePath(NS_LITERAL_STRING("\\\\foo"),
                                                   normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("foo\\some"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("foo\\some"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\.\\foo"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo"));

  mozilla::FilePreferences::testing::NormalizePath(NS_LITERAL_STRING("\\\\."),
                                                   normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  mozilla::FilePreferences::testing::NormalizePath(NS_LITERAL_STRING("\\\\.\\"),
                                                   normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\.\\."), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\bar"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\bar\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\."), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\bar\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\.\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\bar\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\..\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\.."), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\foo\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\..\\bar\\..\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\..\\bar"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\bar"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\..\\..\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\.\\..\\.\\..\\"), normalized);
  ASSERT_TRUE(normalized == NS_LITERAL_STRING("\\\\"));

  bool result;

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\.."), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\..\\"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\.\\..\\"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\\\bar"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\foo\\bar\\..\\..\\..\\..\\"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\\\"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\.\\\\"), normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      NS_LITERAL_STRING("\\\\..\\\\"), normalized);
  ASSERT_FALSE(result);
}

TEST(FilePreferencesWin, AccessUNC)
{
  nsCOMPtr<nsIFile> lf = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  nsresult rv;

  mozilla::FilePreferences::testing::SetBlockUNCPaths(false);

  rv = lf->InitWithPath(NS_LITERAL_STRING("\\\\nice\\..\\evil\\share"));
  ASSERT_EQ(rv, NS_OK);

  mozilla::FilePreferences::testing::SetBlockUNCPaths(true);

  rv = lf->InitWithPath(NS_LITERAL_STRING("\\\\nice\\..\\evil\\share"));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  mozilla::FilePreferences::testing::AddDirectoryToWhitelist(
      NS_LITERAL_STRING("\\\\nice"));

  rv = lf->InitWithPath(NS_LITERAL_STRING("\\\\nice\\share"));
  ASSERT_EQ(rv, NS_OK);

  rv = lf->InitWithPath(NS_LITERAL_STRING("\\\\nice\\..\\evil\\share"));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);
}

TEST(FilePreferencesWin, AccessDOSDevicePath)
{
  const auto devicePathSpecifier = NS_LITERAL_STRING("\\\\?\\");

  nsCOMPtr<nsIFile> lf = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  nsresult rv;

  mozilla::FilePreferences::testing::SetBlockUNCPaths(true);

  rv = lf->InitWithPath(devicePathSpecifier +
                        NS_LITERAL_STRING("evil\\z:\\share"));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = lf->InitWithPath(devicePathSpecifier +
                        NS_LITERAL_STRING("UNC\\evil\\share"));
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = lf->InitWithPath(devicePathSpecifier + NS_LITERAL_STRING("C:\\"));
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIFile> base;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  ASSERT_EQ(rv, NS_OK);

  nsAutoString path;
  rv = base->GetPath(path);
  ASSERT_EQ(rv, NS_OK);

  rv = lf->InitWithPath(devicePathSpecifier + path);
  ASSERT_EQ(rv, NS_OK);
}

TEST(FilePreferencesWin, StartsWithDiskDesignatorAndBackslash)
{
  bool result;

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("\\\\UNC\\path"));
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("\\single\\backslash"));
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("C:relative"));
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("\\\\?\\C:\\"));
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("C:\\"));
  ASSERT_TRUE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      NS_LITERAL_STRING("c:\\"));
  ASSERT_TRUE(result);
}
