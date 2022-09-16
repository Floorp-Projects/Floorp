/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prsystem.h"

#include "mozilla/gtest/MozAssertions.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsILocalFileWin.h"
#include "nsString.h"

#define MAX_PATH 260

#include "gtest/gtest.h"

static void CanInitWith(const char* aPath, bool aShouldWork) {
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = file->InitWithNativePath(nsDependentCString(aPath));
  bool success = aShouldWork ? NS_SUCCEEDED(rv) : NS_FAILED(rv);
  EXPECT_TRUE(success) << "'" << aPath << "' rv=" << std::hex
                       << (unsigned int)rv;
}

static void CanAppend(const char* aRoot, const char* aPath, bool aShouldWork) {
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  file->InitWithNativePath(nsDependentCString(aRoot));
  nsAutoCString basePath;
  file->GetNativeTarget(basePath);

  nsresult rv = file->AppendNative(nsDependentCString(aPath));
  bool success = aShouldWork ? NS_SUCCEEDED(rv) : NS_FAILED(rv);
  EXPECT_TRUE(success) << "'" << basePath.get() << "' + '" << aPath
                       << "' rv=" << std::hex << (unsigned int)rv;
}

static void CanSetLeafName(const char* aRoot, const char* aPath,
                           bool aShouldWork) {
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  file->InitWithNativePath(nsDependentCString(aRoot));
  nsAutoCString basePath;
  file->GetNativeTarget(basePath);

  nsresult rv =
      file->SetLeafName(NS_ConvertUTF8toUTF16(nsDependentCString(aPath)));
  bool success = aShouldWork ? NS_SUCCEEDED(rv) : NS_FAILED(rv);
  EXPECT_TRUE(success) << "'" << basePath.get() << "' set leaf to '" << aPath
                       << "' rv=" << std::hex << (unsigned int)rv;
}

TEST(TestFileNTFSSpecialPaths, PlainPaths)
{
  CanInitWith("C:\\", true);
  CanInitWith("C:\\foo", true);
  CanInitWith("C:\\bar\\foo", true);
  CanInitWith("C:\\bar\\foo\\", true);

  CanAppend("C:\\", "foo", true);
  CanAppend("C:\\", "bar", true);
  CanAppend("C:\\bar", "foo", true);

  CanSetLeafName("C:\\a", "foo", true);
  CanSetLeafName("C:\\a", "bar", true);
}

TEST(TestFileNTFSSpecialPaths, AllowedSpecialChars)
{
  CanInitWith("C:\\$foo", true);
  CanInitWith("C:\\bar\\$foo", true);
  CanInitWith("C:\\foo:Zone.Identifier", true);
  CanInitWith("C:\\$foo:Zone.Identifier", true);
  CanInitWith("C:\\bar\\$foo:Zone.Identifier", true);

  CanAppend("C:\\", "$foo", true);
  CanAppend("C:\\bar\\", "$foo", true);
  CanAppend("C:\\", "foo:Zone.Identifier", true);
  CanAppend("C:\\", "$foo:Zone.Identifier", true);
  CanAppend("C:\\bar\\", "$foo:Zone.Identifier", true);

  CanSetLeafName("C:\\a", "$foo", true);
  CanSetLeafName("C:\\a", "foo:Zone.Identifier", true);
  CanSetLeafName("C:\\a", "$foo:Zone.Identifier", true);
}

TEST(TestFileNTFSSpecialPaths, ForbiddenAttributes)
{
  CanInitWith("C:\\:$MFT", false);
  CanInitWith("C:\\:$mft", false);
  CanInitWith("C:\\:$foo", false);
  // nsLocalFileWin strips the trailing slash so this should also fail:
  CanInitWith("C:\\:$MFT\\", false);
  CanInitWith("C:\\:$mft\\", false);
  CanInitWith("C:\\:$foo\\", false);

  // We just block these everywhere, not just at the root:
  CanInitWith("C:\\bar\\:$mft", false);
  CanInitWith("C:\\bar\\:$mft\\", false);
  CanInitWith("C:\\bar\\:$foo", false);
  CanInitWith("C:\\bar\\:$foo\\", false);

  // Now do the same for appending.
  CanAppend("C:\\", ":$MFT", false);
  CanAppend("C:\\", ":$mft", false);
  CanAppend("C:\\", ":$foo", false);
  // nsLocalFileWin strips the trailing slash so this should also fail:
  CanAppend("C:\\", ":$MFT\\", false);
  CanAppend("C:\\", ":$mft\\", false);
  CanAppend("C:\\", ":$foo\\", false);

  // We just block these everywhere, not just at the root:
  CanAppend("C:\\bar\\", ":$mft", false);
  CanAppend("C:\\bar\\", ":$mft\\", false);
  CanAppend("C:\\bar\\", ":$foo", false);
  CanAppend("C:\\bar\\", ":$foo\\", false);

  // And the same thing for leaf names:
  CanSetLeafName("C:\\a", ":$MFT", false);
  CanSetLeafName("C:\\a", ":$mft", false);
  CanSetLeafName("C:\\a", ":$foo", false);

  CanSetLeafName("C:\\a", ":$MFT\\", false);
  CanSetLeafName("C:\\a", ":$mft\\", false);
  CanSetLeafName("C:\\a", ":$foo\\", false);

  CanSetLeafName("C:\\bar\\foo", ":$mft", false);
  CanSetLeafName("C:\\bar\\foo", ":$mft\\", false);
  CanSetLeafName("C:\\bar\\foo", ":$foo", false);
  CanSetLeafName("C:\\bar\\foo", ":$foo\\", false);
}

TEST(TestFileNTFSSpecialPaths, ForbiddenMetaFiles)
{
  CanInitWith("C:\\$MFT", false);
  CanInitWith("C:\\$mft", false);
  CanInitWith("C:\\$bitmap", false);

  CanAppend("C:\\", "$MFT", false);
  CanAppend("C:\\", "$mft", false);
  CanAppend("C:\\", "$bitmap", false);

  CanSetLeafName("C:\\a", "$MFT", false);
  CanSetLeafName("C:\\a", "$mft", false);
  CanSetLeafName("C:\\a", "$bitmap", false);

  // nsLocalFileWin strips the trailing slash so this should also fail:
  CanInitWith("C:\\$MFT\\", false);
  CanInitWith("C:\\$mft\\", false);
  CanInitWith("C:\\$bitmap\\", false);

  CanAppend("C:\\", "$MFT\\", false);
  CanAppend("C:\\", "$mft\\", false);
  CanAppend("C:\\", "$bitmap\\", false);

  CanSetLeafName("C:\\a", "$MFT\\", false);
  CanSetLeafName("C:\\a", "$mft\\", false);
  CanSetLeafName("C:\\a", "$bitmap\\", false);

  // Shouldn't be able to bypass this by asking for ADS stuff:
  CanInitWith("C:\\$MFT:Zone.Identifier", false);
  CanInitWith("C:\\$mft:Zone.Identifier", false);
  CanInitWith("C:\\$bitmap:Zone.Identifier", false);

  CanAppend("C:\\", "$MFT:Zone.Identifier", false);
  CanAppend("C:\\", "$mft:Zone.Identifier", false);
  CanAppend("C:\\", "$bitmap:Zone.Identifier", false);

  CanSetLeafName("C:\\a", "$MFT:Zone.Identifier", false);
  CanSetLeafName("C:\\a", "$mft:Zone.Identifier", false);
  CanSetLeafName("C:\\a", "$bitmap:Zone.Identifier", false);
}

TEST(TestFileNTFSSpecialPaths, ForbiddenMetaFilesOtherRoots)
{
  // Should still block them for UNC and volume roots
  CanInitWith("\\\\LOCALHOST\\C$\\$MFT", false);
  CanInitWith("\\\\?\\Volume{1234567}\\$MFT", false);

  CanAppend("\\\\LOCALHOST\\", "C$\\$MFT", false);
  CanAppend("\\\\LOCALHOST\\C$\\", "$MFT", false);
  CanAppend("\\\\?\\Volume{1234567}\\", "$MFT", false);
  CanAppend("\\\\Blah\\", "Volume{1234567}\\$MFT", false);

  CanSetLeafName("\\\\LOCALHOST\\C$", "C$\\$MFT", false);
  CanSetLeafName("\\\\LOCALHOST\\C$\\foo", "$MFT", false);
  CanSetLeafName("\\\\?\\Volume{1234567}\\foo", "$MFT", false);
  CanSetLeafName("\\\\Blah\\foo", "Volume{1234567}\\$MFT", false);

  // Root detection should cope with un-normalized paths:
  CanInitWith("C:\\foo\\..\\$MFT", false);
  CanInitWith("C:\\foo\\..\\$mft\\", false);
  CanInitWith("\\\\LOCALHOST\\C$\\blah\\..\\$MFT", false);
  CanInitWith("\\\\?\\Volume{13455635}\\blah\\..\\$MFT", false);
  // As well as different or duplicated separators:
  CanInitWith("C:\\foo\\..\\\\$MFT\\", false);
  CanInitWith("\\\\?\\Volume{1234567}/$MFT", false);
  CanInitWith("\\\\LOCALHOST\\C$/blah//../$MFT", false);

  // There are no "append" equivalents for the preceding set of tests,
  // because append does not allow '..' to be used as a relative path
  // component, nor does it allow forward slashes:
  CanAppend("C:\\foo", "..\\", false);
  CanAppend("C:\\foo", "bar/baz", false);

  // But this is (strangely) allowed for SetLeafName. Yes, really.
  CanSetLeafName("C:\\foo\\bar", "..\\$MFT", false);
  CanSetLeafName("C:\\foo\\bar", "..\\$mft\\", false);
  CanSetLeafName("\\\\LOCALHOST\\C$\\bl", "ah\\..\\$MFT", false);
  CanSetLeafName("\\\\?\\Volume{13455635}\\bla", "ah\\..\\$MFT", false);

  CanSetLeafName("C:\\foo\\bar", "..\\\\$MFT\\", false);
  CanSetLeafName("\\\\?\\Volume{1234567}\\bar", "/$MFT", false);
  CanSetLeafName("\\\\LOCALHOST\\C$/blah/", "\\../$MFT", false);
}

TEST(TestFileNTFSSpecialPaths, NotQuiteMetaFiles)
{
  // These files should not be blocked away from the root:
  CanInitWith("C:\\bar\\$bitmap", true);
  CanInitWith("C:\\bar\\$mft", true);

  // Same for append:
  CanAppend("C:\\bar\\", "$bitmap", true);
  CanAppend("C:\\bar\\", "$mft", true);

  // And SetLeafName:
  CanSetLeafName("C:\\bar\\foo", "$bitmap", true);
  CanSetLeafName("C:\\bar\\foo", "$mft", true);

  // And we shouldn't block on substring matches:
  CanInitWith("C:\\$MFT stocks", true);
  CanAppend("C:\\", "$MFT stocks", true);
  CanSetLeafName("C:\\", "$MFT stocks", true);
}

TEST(TestFileNTFSSpecialPaths, Normalization)
{
  // First determine the working directory:
  wchar_t workingDir[MAX_PATH];
  if (nullptr == _wgetcwd(workingDir, MAX_PATH - 1)) {
    EXPECT_FALSE(true) << "Getting working directory failed.";
    return;
  }

  nsString normalizedPath(workingDir);
  // Need at least 3 chars for the root, at least 2 more to get another subdir
  // in there. This test will fail if cwd is the root of a drive.
  if (normalizedPath.Length() < 5 ||
      !mozilla::IsAsciiAlpha(normalizedPath.First()) ||
      normalizedPath.CharAt(1) != L':' || normalizedPath.CharAt(2) != L'\\') {
    EXPECT_FALSE(true) << "Working directory not long enough?!";
    return;
  }

  // Copy the drive and colon, but NOT the backslash.
  nsAutoString startingFilePath(Substring(normalizedPath, 0, 2));
  normalizedPath.Cut(0, 3);

  // Then determine the number of path components in cwd:
  nsAString::const_iterator begin, end;
  normalizedPath.BeginReading(begin);
  normalizedPath.EndReading(end);
  if (!FindCharInReadable(L'\\', begin, end)) {
    EXPECT_FALSE(true) << "Working directory was at a root";
    return;
  }
  auto numberOfComponentsAboveRoot = 1;
  while (FindCharInReadable(L'\\', begin, end)) {
    begin++;
    numberOfComponentsAboveRoot++;
  }

  // Then set up a file with that many `..\` components:
  startingFilePath.SetCapacity(3 + numberOfComponentsAboveRoot * 3 + 9);
  while (numberOfComponentsAboveRoot--) {
    startingFilePath.AppendLiteral(u"..\\");
  }
  startingFilePath.AppendLiteral(u"$mft");

  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  // This should fail immediately, rather than waiting for a call to
  // nsIFile::Normalize, because normalization doesn't happen reliably,
  // and where it does happen consumers often don't check for errors.
  nsresult rv = file->InitWithPath(startingFilePath);
  EXPECT_NS_FAILED(rv) << " from normalizing '"
                       << NS_ConvertUTF16toUTF8(startingFilePath).get()
                       << "' rv=" << std::hex << (unsigned int)rv;
}
