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
