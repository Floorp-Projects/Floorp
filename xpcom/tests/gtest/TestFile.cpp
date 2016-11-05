/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prsystem.h"

#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

#include "gtest/gtest.h"

static bool VerifyResult(nsresult aRV, const char* aMsg)
{
  bool failed = NS_FAILED(aRV);
  EXPECT_FALSE(failed) << aMsg << " rv=" << std::hex << (unsigned int)aRV;
  return !failed;
}

static already_AddRefed<nsIFile> NewFile(nsIFile* aBase)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  VerifyResult(rv, "Creating nsIFile");
  rv = file->InitWithFile(aBase);
  VerifyResult(rv, "InitWithFile");
  return file.forget();
}

static nsCString FixName(const char* aName)
{
  nsCString name;
  for (uint32_t i = 0; aName[i]; ++i) {
    char ch = aName[i];
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
static bool TestInvalidFileName(nsIFile* aBase, const char* aName)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (NS_SUCCEEDED(rv)) {
    EXPECT_FALSE(NS_SUCCEEDED(rv)) << "AppendNative with invalid filename " << name.get();
    return false;
  }

  return true;
}

// Test nsIFile::Create, verifying that the file exists and did not exist before,
// and leaving it there for future tests
static bool TestCreate(nsIFile* aBase, const char* aName, int32_t aType, int32_t aPerm)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;
  EXPECT_FALSE(exists) << "File "<< name.get() << " already exists";
  if (exists) {
    return false;
  }

  rv = file->Create(aType, aPerm);
  if (!VerifyResult(rv, "Create"))
    return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_TRUE(exists) << "File " << name.get() << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::CreateUnique, verifying that the new file exists and if it existed before,
// the new file has a different name.
// The new file is left in place.
static bool TestCreateUnique(nsIFile* aBase, const char* aName, int32_t aType, int32_t aPerm)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool existsBefore;
  rv = file->Exists(&existsBefore);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;

  rv = file->CreateUnique(aType, aPerm);
  if (!VerifyResult(rv, "Create"))
    return false;

  bool existsAfter;
  rv = file->Exists(&existsAfter);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_TRUE(existsAfter) << "File " << name.get() << " was not created";
  if (!existsAfter) {
    return false;
  }

  if (existsBefore) {
    nsAutoCString leafName;
    rv = file->GetNativeLeafName(leafName);
    if (!VerifyResult(rv, "GetNativeLeafName"))
      return false;
    EXPECT_FALSE(leafName.Equals(name)) << "File " << name.get() << " was not given a new name by CreateUnique";
    if (leafName.Equals(name)) {
      return false;
    }
  }

  return true;
}

// Test nsIFile::OpenNSPRFileDesc with DELETE_ON_CLOSE, verifying that the file exists
// and did not exist before, and leaving it there for future tests
static bool TestDeleteOnClose(nsIFile* aBase, const char* aName, int32_t aFlags, int32_t aPerm)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " already exists";
  if (exists) {
    return false;
  }

  PRFileDesc* fileDesc;
  rv = file->OpenNSPRFileDesc(aFlags | nsIFile::DELETE_ON_CLOSE, aPerm, &fileDesc);
  if (!VerifyResult(rv, "OpenNSPRFileDesc"))
    return false;
  PRStatus status = PR_Close(fileDesc);
  EXPECT_EQ(status, PR_SUCCESS) << "File " << name.get() << " could not be closed";
  if (status != PR_SUCCESS) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not removed on close";
  if (exists) {
    return false;
  }

  return true;
}

// Test nsIFile::Remove, verifying that the file does not exist and did before
static bool TestRemove(nsIFile* aBase, const char* aName, bool aRecursive)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  rv = file->Remove(aRecursive);
  if (!VerifyResult(rv, "Remove"))
    return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not removed";
  if (exists) {
    return false;
  }

  return true;
}

// Test nsIFile::MoveToNative, verifying that the file did not exist at the new location
// before and does afterward, and that it does not exist at the old location anymore
static bool TestMove(nsIFile* aBase, nsIFile* aDestDir, const char* aName, const char* aNewName)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  nsCOMPtr<nsIFile> newFile = NewFile(file);
  nsCString newName = FixName(aNewName);
  rv = newFile->MoveToNative(aDestDir, newName);
  if (!VerifyResult(rv, "MoveToNative"))
    return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_FALSE(exists) << "File " << name.get() << " was not moved";
  if (exists) {
    return false;
  }

  file = NewFile(aDestDir);
  if (!file)
    return false;
  rv = file->AppendNative(newName);
  if (!VerifyResult(rv, "AppendNative"))
    return false;
  bool equal;
  rv = file->Equals(newFile, &equal);
  if (!VerifyResult(rv, "Equals"))
    return false;
  EXPECT_TRUE(equal) << "File object was not updated to destination";
  if (!equal) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (new after)"))
    return false;
  EXPECT_TRUE(exists) << "Destination file " << newName.get() << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::CopyToNative, verifying that the file did not exist at the new location
// before and does afterward, and that it does exist at the old location too
static bool TestCopy(nsIFile* aBase, nsIFile* aDestDir, const char* aName, const char* aNewName)
{
  nsCOMPtr<nsIFile> file = NewFile(aBase);
  if (!file)
    return false;

  nsCString name = FixName(aName);
  nsresult rv = file->AppendNative(name);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  bool exists;
  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (before)"))
    return false;
  EXPECT_TRUE(exists);
  if (!exists) {
    return false;
  }

  nsCOMPtr<nsIFile> newFile = NewFile(file);
  nsCString newName = FixName(aNewName);
  rv = newFile->CopyToNative(aDestDir, newName);
  if (!VerifyResult(rv, "MoveToNative"))
    return false;
  bool equal;
  rv = file->Equals(newFile, &equal);
  if (!VerifyResult(rv, "Equals"))
    return false;
  EXPECT_TRUE(equal) << "File object updated unexpectedly";
  if (!equal) {
    return false;
  }

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (after)"))
    return false;
  EXPECT_TRUE(exists) << "File " << name.get() << " was removed";
  if (!exists) {
    return false;
  }

  file = NewFile(aDestDir);
  if (!file)
    return false;
  rv = file->AppendNative(newName);
  if (!VerifyResult(rv, "AppendNative"))
    return false;

  rv = file->Exists(&exists);
  if (!VerifyResult(rv, "Exists (new after)"))
    return false;
  EXPECT_TRUE(exists) << "Destination file " << newName.get() << " was not created";
  if (!exists) {
    return false;
  }

  return true;
}

// Test nsIFile::GetParent
static bool TestParent(nsIFile* aBase, nsIFile* aStart)
{
  nsCOMPtr<nsIFile> file = NewFile(aStart);
  if (!file)
    return false;

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
static bool TestNormalizeNativePath(nsIFile* aBase, nsIFile* aStart)
{
  nsCOMPtr<nsIFile> file = NewFile(aStart);
  if (!file)
    return false;

  nsAutoCString path;
  nsresult rv = file->GetNativePath(path);
  VerifyResult(rv, "GetNativePath");
  path.Append(FixName("/./.."));
  rv = file->InitWithNativePath(path);
  VerifyResult(rv, "InitWithNativePath");
  rv = file->Normalize();
  VerifyResult(rv, "Normalize");
  rv = file->GetNativePath(path);
  VerifyResult(rv, "GetNativePath (after normalization)");

  nsAutoCString basePath;
  rv = aBase->GetNativePath(basePath);
  VerifyResult(rv, "GetNativePath (base)");

  EXPECT_TRUE(path.Equals(basePath)) << "Incorrect normalization: " << path.get() << " - " << basePath.get();
  if (!path.Equals(basePath)) {
    return false;
  }

  return true;
}

TEST(TestFile, Tests)
{
  nsCOMPtr<nsIFile> base;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  ASSERT_TRUE(VerifyResult(rv, "Getting temp directory"));

  rv = base->AppendNative(nsDependentCString("mozfiletests"));
  ASSERT_TRUE(VerifyResult(rv, "Appending mozfiletests to temp directory name"));

  // Remove the directory in case tests failed and left it behind.
  // don't check result since it might not be there
  base->Remove(true);

  // Now create the working directory we're going to use
  rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_TRUE(VerifyResult(rv, "Creating temp directory"));

  // Now we can safely normalize the path
  rv = base->Normalize();
  ASSERT_TRUE(VerifyResult(rv, "Normalizing temp directory name"));

  // Initialize subdir object for later use
  nsCOMPtr<nsIFile> subdir = NewFile(base);
  ASSERT_TRUE(subdir);

  rv = subdir->AppendNative(nsDependentCString("subdir"));
  ASSERT_TRUE(VerifyResult(rv, "Appending 'subdir' to test dir name"));

  // ---------------
  // End setup code.
  // ---------------

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

  // Run normalization tests while the directory exists
  ASSERT_TRUE(TestNormalizeNativePath(base, subdir));

  // Test recursive directory removal
  ASSERT_TRUE(TestRemove(base, "subdir", true));

  ASSERT_TRUE(TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600));
  ASSERT_TRUE(TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600));
  ASSERT_TRUE(TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700));
  ASSERT_TRUE(TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700));

  ASSERT_TRUE(TestDeleteOnClose(base, "file7.txt", PR_RDWR | PR_CREATE_FILE, 0600));

  // Clean up temporary stuff
  rv = base->Remove(true);
  VerifyResult(rv, "Cleaning up temp directory");
}
