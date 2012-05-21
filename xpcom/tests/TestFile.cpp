/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prsystem.h"

#include "TestHarness.h"

#include "nsILocalFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

static const char* gFunction = "main";

static bool VerifyResult(nsresult aRV, const char* aMsg)
{
    if (NS_FAILED(aRV)) {
        fail("%s %s, rv=%x", gFunction, aMsg, aRV);
        return false;
    }
    return true;
}

static already_AddRefed<nsILocalFile> NewFile(nsIFile* aBase)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> file =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    VerifyResult(rv, "Creating nsILocalFile");
    nsCOMPtr<nsILocalFile> localBase = do_QueryInterface(aBase);
    if (!localBase) {
        fail("%s Base directory not a local file", gFunction);
        return nsnull;
    }
    rv = file->InitWithFile(localBase);
    VerifyResult(rv, "InitWithFile");
    return file.forget();
}

static nsCString FixName(const char* aName)
{
    nsCString name;
    for (PRUint32 i = 0; aName[i]; ++i) {
        char ch = aName[i];
       // PR_GetPathSeparator returns the wrong value on Mac so don't use it
#if defined(XP_WIN) || defined(XP_OS2)
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
    gFunction = "TestInvalidFileName";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
    if (!file)
        return false;

    nsCString name = FixName(aName);
    nsresult rv = file->AppendNative(name);
    if (NS_SUCCEEDED(rv)) {
        fail("%s AppendNative with invalid filename %s", gFunction, name.get());
        return false;
    }

    return true;
}

// Test nsIFile::Create, verifying that the file exists and did not exist before,
// and leaving it there for future tests
static bool TestCreate(nsIFile* aBase, const char* aName, PRInt32 aType, PRInt32 aPerm)
{
    gFunction = "TestCreate";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (exists) {
        fail("%s File %s already exists", gFunction, name.get());
        return false;
    }

    rv = file->Create(aType, aPerm);  
    if (!VerifyResult(rv, "Create"))
        return false;

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (after)"))
        return false;
    if (!exists) {
        fail("%s File %s was not created", gFunction, name.get());
        return false;
    }

    return true;
}

// Test nsIFile::CreateUnique, verifying that the new file exists and if it existed before,
// the new file has a different name.
// The new file is left in place.
static bool TestCreateUnique(nsIFile* aBase, const char* aName, PRInt32 aType, PRInt32 aPerm)
{
    gFunction = "TestCreateUnique";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (!existsAfter) {
        fail("%s File %s was not created", gFunction, name.get());
        return false;
    }

    if (existsBefore) {
        nsCAutoString leafName;
        rv = file->GetNativeLeafName(leafName);
        if (!VerifyResult(rv, "GetNativeLeafName"))
            return false;
        if (leafName.Equals(name)) {
            fail("%s File %s was not given a new name by CreateUnique", gFunction, name.get());
            return false;
        }
    }

    return true;
}

// Test nsILocalFile::OpenNSPRFileDesc with DELETE_ON_CLOSE, verifying that the file exists
// and did not exist before, and leaving it there for future tests
static bool TestDeleteOnClose(nsIFile* aBase, const char* aName, PRInt32 aFlags, PRInt32 aPerm)
{
    gFunction = "TestDeleteOnClose";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (exists) {
        fail("%s File %s already exists", gFunction, name.get());
        return false;
    }

    PRFileDesc* fileDesc;
    rv = file->OpenNSPRFileDesc(aFlags | nsILocalFile::DELETE_ON_CLOSE, aPerm, &fileDesc);  
    if (!VerifyResult(rv, "OpenNSPRFileDesc"))
        return false;
    PRStatus status = PR_Close(fileDesc);
    if (status != PR_SUCCESS) {
        fail("%s File %s could not be closed", gFunction, name.get());
        return false;
    }

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (after)"))
        return false;
    if (exists) {
        fail("%s File %s was not removed on close!", gFunction, name.get());
        return false;
    }

    return true;
}

// Test nsIFile::Remove, verifying that the file does not exist and did before
static bool TestRemove(nsIFile* aBase, const char* aName, bool aRecursive)
{
    gFunction = "TestDelete";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (!exists) {
        fail("%s File %s does not exist", gFunction, name.get());
        return false;
    }

    rv = file->Remove(aRecursive);  
    if (!VerifyResult(rv, "Remove"))
        return false;

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (after)"))
        return false;
    if (exists) {
        fail("%s File %s was not removed", gFunction, name.get());
        return false;
    }

    return true;
}

// Test nsIFile::MoveToNative, verifying that the file did not exist at the new location
// before and does afterward, and that it does not exist at the old location anymore
static bool TestMove(nsIFile* aBase, nsIFile* aDestDir, const char* aName, const char* aNewName)
{
    gFunction = "TestMove";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (!exists) {
        fail("%s File %s does not exist", gFunction, name.get());
        return false;
    }

    nsCOMPtr<nsILocalFile> newFile = NewFile(file);
    nsCString newName = FixName(aNewName);
    rv = newFile->MoveToNative(aDestDir, newName);
    if (!VerifyResult(rv, "MoveToNative"))
        return false;

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (after)"))
        return false;
    if (exists) {
        fail("%s File %s was not moved", gFunction, name.get());
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
    if (!equal) {
        fail("%s file object was not updated to destination", gFunction);
        return false;
    }

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (new after)"))
        return false;
    if (!exists) {
        fail("%s Destination file %s was not created", gFunction, newName.get());
        return false;
    }

    return true;
}

// Test nsIFile::CopyToNative, verifying that the file did not exist at the new location
// before and does afterward, and that it does exist at the old location too
static bool TestCopy(nsIFile* aBase, nsIFile* aDestDir, const char* aName, const char* aNewName)
{
    gFunction = "TestCopy";
    nsCOMPtr<nsILocalFile> file = NewFile(aBase);
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
    if (!exists) {
        fail("%s File %s does not exist", gFunction, name.get());
        return false;
    }

    nsCOMPtr<nsILocalFile> newFile = NewFile(file);
    nsCString newName = FixName(aNewName);
    rv = newFile->CopyToNative(aDestDir, newName);
    if (!VerifyResult(rv, "MoveToNative"))
        return false;
    bool equal;
    rv = file->Equals(newFile, &equal);
    if (!VerifyResult(rv, "Equals"))
        return false;
    if (!equal) {
        fail("%s file object updated unexpectedly", gFunction);
        return false;
    }

    rv = file->Exists(&exists);
    if (!VerifyResult(rv, "Exists (after)"))
        return false;
    if (!exists) {
        fail("%s File %s was removed", gFunction, name.get());
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
    if (!exists) {
        fail("%s Destination file %s was not created", gFunction, newName.get());
        return false;
    }

    return true;
}

// Test nsIFile::GetParent
static bool TestParent(nsIFile* aBase, nsIFile* aStart)
{
    gFunction = "TestParent";
    nsCOMPtr<nsILocalFile> file = NewFile(aStart);
    if (!file)
        return false;

    nsCOMPtr<nsIFile> parent;
    nsresult rv = file->GetParent(getter_AddRefs(parent));
    VerifyResult(rv, "GetParent");

    bool equal;
    rv = parent->Equals(aBase, &equal);
    VerifyResult(rv, "Equals");
    if (!equal) {
        fail("%s Incorrect parent", gFunction);
        return false;
    }

    return true;
}

// Test nsIFile::Normalize and native path setting/getting
static bool TestNormalizeNativePath(nsIFile* aBase, nsIFile* aStart)
{
    gFunction = "TestNormalizeNativePath";
    nsCOMPtr<nsILocalFile> file = NewFile(aStart);
    if (!file)
        return false;

    nsCAutoString path;
    nsresult rv = file->GetNativePath(path);
    VerifyResult(rv, "GetNativePath");
    path.Append(FixName("/./.."));
    rv = file->InitWithNativePath(path);
    VerifyResult(rv, "InitWithNativePath");
    rv = file->Normalize();
    VerifyResult(rv, "Normalize");
    rv = file->GetNativePath(path);
    VerifyResult(rv, "GetNativePath (after normalization)");

    nsCAutoString basePath;
    rv = aBase->GetNativePath(basePath);
    VerifyResult(rv, "GetNativePath (base)");

    if (!path.Equals(basePath)) {
        fail("%s Incorrect normalization");
        return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    ScopedXPCOM xpcom("nsLocalFile");
    if (xpcom.failed())
        return 1;

    nsCOMPtr<nsIFile> base;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
    if (!VerifyResult(rv, "Getting temp directory"))
        return 1;
    rv = base->AppendNative(nsDependentCString("mozfiletests"));
    if (!VerifyResult(rv, "Appending mozfiletests to temp directory name"))
        return 1;
    // Remove the directory in case tests failed and left it behind.
    // don't check result since it might not be there
    base->Remove(true);

    // Now create the working directory we're going to use
    rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (!VerifyResult(rv, "Creating temp directory"))
        return 1;
    // Now we can safely normalize the path
    rv = base->Normalize();
    if (!VerifyResult(rv, "Normalizing temp directory name"))
        return 1;

    // Initialize subdir object for later use
    nsCOMPtr<nsILocalFile> subdir = NewFile(base);
    if (!subdir)
        return 1;
    rv = subdir->AppendNative(nsDependentCString("subdir"));
    if (!VerifyResult(rv, "Appending 'subdir' to test dir name"))
        return 1;

    passed("Setup");

    // Test path parsing
    if (TestInvalidFileName(base, "a/b")) {
        passed("AppendNative with invalid file name");
    }
    if (TestParent(base, subdir)) {
        passed("GetParent");
    }

    // Test file creation
    if (TestCreate(base, "file.txt", nsIFile::NORMAL_FILE_TYPE, 0600)) {
        passed("Create file");
    }
    if (TestRemove(base, "file.txt", false)) {
        passed("Remove file");
    }

    // Test directory creation
    if (TestCreate(base, "subdir", nsIFile::DIRECTORY_TYPE, 0700)) {
        passed("Create directory");
    }

    // Test move and copy in the base directory
    if (TestCreate(base, "file.txt", nsIFile::NORMAL_FILE_TYPE, 0600) &&
        TestMove(base, base, "file.txt", "file2.txt")) {
        passed("MoveTo rename file");
    }
    if (TestCopy(base, base, "file2.txt", "file3.txt")) {
        passed("CopyTo copy file");
    }
    // Test moving across directories
    if (TestMove(base, subdir, "file2.txt", "file2.txt")) {
        passed("MoveTo move file");
    }
    // Test moving across directories and renaming at the same time
    if (TestMove(subdir, base, "file2.txt", "file4.txt")) {
        passed("MoveTo move and rename file");
    }
    // Test copying across directoreis
    if (TestCopy(base, subdir, "file4.txt", "file5.txt")) {
        passed("CopyTo copy file across directories");
    }

    // Run normalization tests while the directory exists
    if (TestNormalizeNativePath(base, subdir)) {
        passed("Normalize with native paths");
    }

    // Test recursive directory removal
    if (TestRemove(base, "subdir", true)) {
        passed("Remove directory");
    }

    if (TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600) &&
        TestCreateUnique(base, "foo", nsIFile::NORMAL_FILE_TYPE, 0600)) {
        passed("CreateUnique file");
    }
    if (TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700) &&
        TestCreateUnique(base, "bar.xx", nsIFile::DIRECTORY_TYPE, 0700)) {
        passed("CreateUnique directory");
    }

    if (TestDeleteOnClose(base, "file7.txt", PR_RDWR | PR_CREATE_FILE, 0600)) {
        passed("OpenNSPRFileDesc DELETE_ON_CLOSE");
    }

    gFunction = "main";
    // Clean up temporary stuff
    rv = base->Remove(true);
    VerifyResult(rv, "Cleaning up temp directory");

    return gFailCount > 0;
}
