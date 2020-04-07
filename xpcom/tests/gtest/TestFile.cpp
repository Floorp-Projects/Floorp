/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prsystem.h"

#include "nsIFile.h"
#ifdef XP_WIN
#  include "nsILocalFileWin.h"
#endif
#include "nsComponentManagerUtils.h"
#include "nsString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"

#include "gtest/gtest.h"

#ifdef XP_WIN
bool gTestWithPrefix_Win = false;
#endif

static bool VerifyResult(nsresult aRV, const char* aMsg) {
  bool failed = NS_FAILED(aRV);
  EXPECT_FALSE(failed) << aMsg << " rv=" << std::hex << (unsigned int)aRV;
  return !failed;
}

#ifdef XP_WIN
static void SetUseDOSDevicePathSyntax(nsIFile* aFile) {
  if (gTestWithPrefix_Win) {
    nsresult rv;
    nsCOMPtr<nsILocalFileWin> winFile = do_QueryInterface(aFile, &rv);
    VerifyResult(rv, "Querying nsILocalFileWin");

    MOZ_ASSERT(winFile);
    winFile->SetUseDOSDevicePathSyntax(true);
  }
}
#endif

static already_AddRefed<nsIFile> NewFile(nsIFile* aBase) {
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  VerifyResult(rv, "Creating nsIFile");
  rv = file->InitWithFile(aBase);
  VerifyResult(rv, "InitWithFile");

#ifdef XP_WIN
  SetUseDOSDevicePathSyntax(file);
#endif

  return file.forget();
}

template <typename char_type>
static nsTString<char_type> FixName(const char_type* aName) {
  nsTString<char_type> name;
  for (uint32_t i = 0; aName[i]; ++i) {
    char_type ch = aName[i];
    // PR_GetPathSeparator returns the wrong value on Mac so don't use it
#if defined(XP_WIN)
    if (ch == '/') {
      ch = '\\';
    }
#endif
    name.Append(ch);
  }
  return name;
}

// Test nsIFile::AppendNative, verifying that aName is not a valid file name
static bool TestInvalidFileName(nsIFile* aBase, const char* aName) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (NS_SUCCEEDED(rv)) {
    EXPECT_FALSE(NS_SUCCEEDED(rv))
        << "AppendNative with invalid filename " << name.get();
    return false;
  }

  return true;
}

// Test nsIFile::Create, verifying that the file exists and did not exist
// before, and leaving it there for future tests
static bool TestCreate(nsIFile* aBase, const char* aName, int32_t aType,
                       int32_t aPerm) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)")) return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " already exists";
  if (exists) {
    return false;
  }

  rv = file->Create(aType, aPerm);
  if (!VerifyResult(rv, "Create")) return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_TRUE(exists) << "File " << name.get() << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::CreateUnique, verifying that the new file exists and if it
// existed before, the new file has a different name. The new file is left in
// place.
static bool TestCreateUnique(nsIFile* aBase, const char* aName, int32_t aType,
                             int32_t aPerm) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool existsBefore;
  rv = file->Exists(&existsBefore);
  if (!VerifyResult(rv, "Exists (before)")) return false;

  rv = file->CreateUnique(aType, aPerm);
  if (!VerifyResult(rv, "Create")) return false;

  bool existsAfter;
  rv = file->Exists(&existsAfter);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_TRUE(existsAfter) << "File " << name.get() << " was not created";
  if (!existsAfter) {
    return false;
  }

  if (existsBefore) {
    nsAutoCString leafName;
    rv = file->GetNativeLeafName(leafName);
    if (!VerifyResult(rv, "GetNativeLeafName")) return false;
    EXPECT_FALSE(leafName.Equals(name))
        << "File " << name.get() << " was not given a new name by CreateUnique";
    if (leafName.Equals(name)) {
      return false;
    }
  }

  return true;
}

// Test nsIFile::OpenNSPRFileDesc with DELETE_ON_CLOSE, verifying that the file
// exists and did not exist before, and leaving it there for future tests
static bool TestDeleteOnClose(nsIFile* aBase, const char* aName, int32_t aFlags,
                              int32_t aPerm) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)")) return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " already exists";
  if (exists) {
    return false;
  }

  PRFileDesc* fileDesc;
  rv = file->OpenNSPRFileDesc(aFlags | nsIFile::DELETE_ON_CLOSE, aPerm,
                              &fileDesc);
  if (!VerifyResult(rv, "OpenNSPRFileDesc")) return false;
  PRStatus status = PR_Close(fileDesc);
  EXPECT_EQ(status, PR_SUCCESS)
      << "File " << name.get() << " could not be closed";
  if (status != PR_SUCCESS) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not removed on close";
  if (exists) {
    return false;
  }

  return true;
}

// Test nsIFile::Remove, verifying that the file does not exist and did before
static bool TestRemove(nsIFile* aBase, const char* aName, bool aRecursive) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)")) return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  rv = file->Remove(aRecursive);
  if (!VerifyResult(rv, "Remove")) return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not removed";
  if (exists) {
    return false;
  }

  return true;
}

// Test nsIFile::MoveToNative, verifying that the file did not exist at the new
// location before and does afterward, and that it does not exist at the old
// location anymore
static bool TestMove(nsIFile* aBase, nsIFile* aDestDir, const char* aName,
                     const char* aNewName) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)")) return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  nsCOMPtr<nsIFile> newFile = NewFile(file);
  nsCString newName = FixName(aNewName);
  rv = newFile->MoveToNative(aDestDir, newName);
  if (!VerifyResult(rv, "MoveToNative")) return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not moved";
  if (exists) {
    return false;
  }

  file = NewFile(aDestDir);
  if (!file) return false;
  rv = file->AppendNative(newName);
  if (!VerifyResult(rv, "AppendNative")) return false;
  bool equal;
  rv = file->Equals(newFile, &equal);
  if (!VerifyResult(rv, "Equals")) return false;
  EXPECT_TRUE(equal) << "File object was not updated to destination";
  if (!equal) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (new after)")) return false;
  EXPECT_TRUE(exists) << "Destination file " << newName.get()
                      << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::CopyToNative, verifying that the file did not exist at the new
// location before and does afterward, and that it does exist at the old
// location too
static bool TestCopy(nsIFile* aBase, nsIFile* aDestDir, const char* aName,
                     const char* aNewName) {
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file) return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative")) return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)")) return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  nsCOMPtr<nsIFile> newFile = NewFile(file);
  nsCString newName = FixName(aNewName);
  rv = newFile->CopyToNative(aDestDir, newName);
  if (!VerifyResult(rv, "MoveToNative")) return false;
  bool equal;
  rv = file->Equals(newFile, &equal);
  if (!VerifyResult(rv, "Equals")) return false;
  EXPECT_TRUE(equal) << "File object updated unexpectedly";
  if (!equal) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)")) return false;
  EXPECT_TRUE(exists) << "File " << name.get() << " was removed";
  if (!exists) {
    return false;
  }

  file = NewFile(aDestDir);
  if (!file) return false;
  rv = file->AppendNative(newName);
  if (!VerifyResult(rv, "AppendNative")) return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (new after)")) return false;
  EXPECT_TRUE(exists) << "Destination file " << newName.get()
                      << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::GetParent
static bool TestParent(nsIFile* aBase, nsIFile* aStart) {
  nsCOMPtr<nsIFile> file = NewFile(aStart);
  if (!file) return false;

  nsCOMPtr<nsIFile> parent;
  nsresult rv = file->GetParent(getter_AddRefs(parent));
  VerifyResult(rv, "GetParent");

  bool equal;
  rv = parent->Equals(aBase, &equal);
  VerifyResult(rv, "Equals");
  EXPECT_TRUE(equal) << "Incorrect parent";
  if (!equal) {
    return false;
  }

  return true;
}

// Test nsIFile::Normalize and native path setting/getting
static bool TestNormalizeNativePath(nsIFile* aBase, nsIFile* aStart) {
  nsCOMPtr<nsIFile> file = NewFile(aStart);
  if (!file) return false;

  auto path = file->NativePath();
#ifdef XP_WIN
  path.Append(FixName(u"/./.."));
  nsresult rv = file->InitWithPath(path);
  VerifyResult(rv, "InitWithPath");
#else
  path.Append(FixName("/./.."));
  nsresult rv = file->InitWithNativePath(path);
  VerifyResult(rv, "InitWithNativePath");
#endif
  rv = file->Normalize();
  VerifyResult(rv, "Normalize");
  path = file->NativePath();

  auto basePath = aBase->NativePath();
  VerifyResult(rv, "GetNativePath (base)");

  EXPECT_TRUE(path.Equals(basePath))
      << "Incorrect normalization: " << file->HumanReadablePath().get() << " - "
      << aBase->HumanReadablePath().get();
  if (!path.Equals(basePath)) {
    return false;
  }

  return true;
}

static void SetupAndTestFunctions(const nsAString& aDirName,
                                  bool aTestCreateUnique, bool aTestNormalize) {
  nsCOMPtr<nsIFile> base;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  ASSERT_TRUE(VerifyResult(rv, "Getting temp directory"));

#ifdef XP_WIN
  SetUseDOSDevicePathSyntax(base);
#endif

  rv = base->Append(aDirName);
  ASSERT_TRUE(
      VerifyResult(rv, nsPrintfCString("Appending %s to temp directory name",
                                       NS_ConvertUTF16toUTF8(aDirName).get())
                           .get()));

  // Remove the directory in case tests failed and left it behind.
  // don't check result since it might not be there
  base->Remove(true);

  // Now create the working directory we're going to use
  rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_TRUE(VerifyResult(rv, "Creating temp directory"));

  // Now we can safely normalize the path
  if (aTestNormalize) {
    rv = base->Normalize();
    ASSERT_TRUE(VerifyResult(rv, "Normalizing temp directory name"));
  }

  // Initialize subdir object for later use
  nsCOMPtr<nsIFile> subdir = NewFile(base);
  ASSERT_TRUE(subdir);

  rv = subdir->AppendNative(nsDependentCString("subdir"));
  ASSERT_TRUE(VerifyResult(rv, "Appending 'subdir' to test dir name"));

  // ---------------
  // End setup code.
  // ---------------

  // Test leafName
  nsString leafName;
  rv = base->GetLeafName(leafName);
  ASSERT_TRUE(VerifyResult(rv, "Getting leafName"));
  ASSERT_TRUE(leafName.Equals(aDirName));

  // Test path parsing
  ASSERT_TRUE(TestInvalidFileName(base, "a/b"));
  ASSERT_TRUE(TestParent(base, subdir));

  // Test file creation
  ASSERT_TRUE(TestCreate(base, "file.txt", nsIFile::NORMAL_FILE_TYPE, 0600));
  ASSERT_TRUE(TestRemove(base, "file.txt", false));

  // Test directory creation
  ASSERT_TRUE(TestCreate(base, "subdir", nsIFile::DIRECTORY_TYPE, 0700));

  // Test move and copy in the base directory
  ASSERT_TRUE(TestCreate(base, "file.txt", nsIFile::NORMAL_FILE_TYPE, 0600));
  ASSERT_TRUE(TestMove(base, base, "file.txt", "file2.txt"));
  ASSERT_TRUE(TestCopy(base, base, "file2.txt", "file3.txt"));

  // Test moving across directories
  ASSERT_TRUE(TestMove(base, subdir, "file2.txt", "file2.txt"));

  // Test moving across directories and renaming at the same time
  ASSERT_TRUE(TestMove(subdir, base, "file2.txt", "file4.txt"));

  // Test copying across directoreis
  ASSERT_TRUE(TestCopy(base, subdir, "file4.txt", "file5.txt"));

  if (aTestNormalize) {
    // Run normalization tests while the directory exists
    ASSERT_TRUE(TestNormalizeNativePath(base, subdir));
  }

  // Test recursive directory removal
  ASSERT_TRUE(TestRemove(base, "subdir", true));

  if (aTestCreateUnique) {
    ASSERT_TRUE(TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600));
    ASSERT_TRUE(TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600));
    ASSERT_TRUE(
        TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700));
    ASSERT_TRUE(
        TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700));
  }

  ASSERT_TRUE(
      TestDeleteOnClose(base, "file7.txt", PR_RDWR | PR_CREATE_FILE, 0600));

  // Clean up temporary stuff
  rv = base->Remove(true);
  VerifyResult(rv, "Cleaning up temp directory");
}

TEST(TestFile, Unprefixed)
{
#ifdef XP_WIN
  gTestWithPrefix_Win = false;
#endif

  SetupAndTestFunctions(NS_LITERAL_STRING("mozfiletests"),
                        /* aTestCreateUnique */ true,
                        /* aTestNormalize */ true);

#ifdef XP_WIN
  gTestWithPrefix_Win = true;
#endif
}

// This simulates what QM_NewLocalFile does (NS_NewLocalFiles and then
// SetUseDOSDevicePathSyntax if it's on Windows for NewFile)
TEST(TestFile, PrefixedOnWin)
{
  SetupAndTestFunctions(NS_LITERAL_STRING("mozfiletests"),
                        /* aTestCreateUnique */ true,
                        /* aTestNormalize */ true);
}

TEST(TestFile, PrefixedOnWin_PathExceedsMaxPath)
{
  // We want to verify if the prefix would allow as to create a file with over
  // 260 char for its path. However, on Windows, the maximum length of filename
  // is 255. Given the base file path and we are going append some other file
  // to the current base file, let's assume the file path will exceed 260 so
  // that we are able to verify if the prefix works or not.
  nsString dirName;
  dirName.AssignLiteral("mozfiletests");
  for (uint32_t i = 255 - dirName.Length(); i > 0; --i) {
    dirName.AppendLiteral("a");
  }

  // Bypass the test for CreateUnique because there is a check for the max
  // length of the root on all platforms.
  SetupAndTestFunctions(dirName, /* aTestCreateUnique */ false,
                        /* aTestNormalize */ true);
}

TEST(TestFile, PrefixedOnWin_ComponentEndsWithPeriod)
{
  // Bypass the normalization for this because it would strip the trailing
  // period.
  SetupAndTestFunctions(NS_LITERAL_STRING("mozfiletests."),
                        /* aTestCreateUnique */ true,
                        /* aTestNormalize */ false);
}
