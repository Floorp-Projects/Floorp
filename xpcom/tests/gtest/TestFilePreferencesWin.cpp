#include "gtest/gtest.h"

#include "mozilla/FilePreferences.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsXPCOMCID.h"

TEST(FilePreferencesWin, Normalization)
{
  nsAutoString normalized;

  mozilla::FilePreferences::testing::NormalizePath(u"foo"_ns, normalized);
  ASSERT_TRUE(normalized == u"foo"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\foo"_ns, normalized);
  ASSERT_TRUE(normalized == u"\\foo"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo"_ns, normalized);
  ASSERT_TRUE(normalized == u"\\\\foo"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"foo\\some"_ns, normalized);
  ASSERT_TRUE(normalized == u"foo\\some"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\.\\foo"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\."_ns, normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\.\\"_ns, normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\.\\."_ns, normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\bar"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\bar\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\."_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\bar\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\.\\"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\bar\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\..\\"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\.."_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\foo\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\..\\bar\\..\\"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\..\\bar"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\bar"_ns);

  mozilla::FilePreferences::testing::NormalizePath(u"\\\\foo\\bar\\..\\..\\"_ns,
                                                   normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  mozilla::FilePreferences::testing::NormalizePath(
      u"\\\\foo\\bar\\.\\..\\.\\..\\"_ns, normalized);
  ASSERT_TRUE(normalized == u"\\\\"_ns);

  bool result;

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\.."_ns,
                                                            normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\..\\"_ns,
                                                            normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\.\\..\\"_ns,
                                                            normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      u"\\\\foo\\\\bar"_ns, normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(
      u"\\\\foo\\bar\\..\\..\\..\\..\\"_ns, normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\\\"_ns,
                                                            normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\.\\\\"_ns,
                                                            normalized);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::testing::NormalizePath(u"\\\\..\\\\"_ns,
                                                            normalized);
  ASSERT_FALSE(result);
}

TEST(FilePreferencesWin, AccessUNC)
{
  nsCOMPtr<nsIFile> lf = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  nsresult rv;

  mozilla::FilePreferences::testing::SetBlockUNCPaths(false);

  rv = lf->InitWithPath(u"\\\\nice\\..\\evil\\share"_ns);
  ASSERT_EQ(rv, NS_OK);

  mozilla::FilePreferences::testing::SetBlockUNCPaths(true);

  rv = lf->InitWithPath(u"\\\\nice\\..\\evil\\share"_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  mozilla::FilePreferences::testing::AddDirectoryToAllowlist(u"\\\\nice"_ns);

  rv = lf->InitWithPath(u"\\\\nice\\share"_ns);
  ASSERT_EQ(rv, NS_OK);

  rv = lf->InitWithPath(u"\\\\nice\\..\\evil\\share"_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);
}

TEST(FilePreferencesWin, AccessDOSDevicePath)
{
  const auto devicePathSpecifier = u"\\\\?\\"_ns;

  nsCOMPtr<nsIFile> lf = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  nsresult rv;

  mozilla::FilePreferences::testing::SetBlockUNCPaths(true);

  rv = lf->InitWithPath(devicePathSpecifier + u"evil\\z:\\share"_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = lf->InitWithPath(devicePathSpecifier + u"UNC\\evil\\share"_ns);
  ASSERT_EQ(rv, NS_ERROR_FILE_ACCESS_DENIED);

  rv = lf->InitWithPath(devicePathSpecifier + u"C:\\"_ns);
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
      u"\\\\UNC\\path"_ns);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      u"\\single\\backslash"_ns);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      u"C:relative"_ns);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      u"\\\\?\\C:\\"_ns);
  ASSERT_FALSE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      u"C:\\"_ns);
  ASSERT_TRUE(result);

  result = mozilla::FilePreferences::StartsWithDiskDesignatorAndBackslash(
      u"c:\\"_ns);
  ASSERT_TRUE(result);
}
