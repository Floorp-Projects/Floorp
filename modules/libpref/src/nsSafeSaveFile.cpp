/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brian Nesse <bnesse@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSafeSaveFile.h"


nsSafeSaveFile::nsSafeSaveFile(nsIFile *aTargetFile, PRInt32 aNumBackupCopies)
 : mBackupNameLen(0),
   mBackupCount(aNumBackupCopies)
{
    nsCAutoString targetFileName;  
    const char *  temp;
    nsresult      rv;

    // determine if the target file currently exists
    aTargetFile->Exists(&mTargetFileExists);

    // if the target file doesn't exist this object does nothing
    if (!mTargetFileExists)
        return;

    // determine the actual filename (less the extension)
    rv = aTargetFile->GetNativeLeafName(targetFileName);
    if (NS_FAILED(rv)) // yikes! out of memory
        return;

    // keep a reference to the file that will be saved
    mTargetFile = aTargetFile;

    // determine the file name (less the extension)
    temp = strrchr(targetFileName.get(), '.');
    if (temp)
       mBackupNameLen = temp - targetFileName.get();
    else
       mBackupNameLen = targetFileName.Length();

    // save the name of the backup file and its length
    mBackupFileName = Substring(targetFileName, 0, mBackupNameLen) + NS_LITERAL_CSTRING(".bak");
    mBackupNameLen = mBackupFileName.Length();

    // create a new file object that points to our backup file... this isn't
    // absolutely necessary, but it does allow us to easily transistion to a
    // model where all .bak are stored in a single directory if so desired.
    rv = aTargetFile->Clone(getter_AddRefs(mBackupFile));
    if (NS_SUCCEEDED(rv))
        mBackupFile->SetNativeLeafName(mBackupFileName);
}

nsSafeSaveFile::~nsSafeSaveFile(void)
{
    // if the target file didn't exist nothing was backed up
    if (mTargetFileExists) {
        // if no backups desired, remove the backup file
        if (mBackupCount == 0) {
            mBackupFile->Remove(PR_FALSE);
        }
    }
}

nsresult nsSafeSaveFile::CreateBackup(PurgeBackupType aPurgeType)
{
    nsCOMPtr<nsIFile> backupParent;
    nsresult          rv, rv2;
    PRBool            bExists;

    // if the target file doesn't exist there is nothing to backup
    if (!mTargetFileExists)
        return NS_OK;

    // if a backup file currently exists... do the right thing
    mBackupFile->Exists(&bExists);
    if (bExists) {
        rv = ManageRedundantBackups();
        if (NS_FAILED(rv))
            return rv;
    }

    // Ugh, copy only takes a directory and a name, lets "unpackage" our target file...
    rv = mBackupFile->GetParent(getter_AddRefs(backupParent));
    if (NS_FAILED(rv))
        return rv;

    // and finally, copy the file (preserves file permissions)
    rv2 = NS_OK;
    do {
        rv = mTargetFile->CopyToNative(backupParent, mBackupFileName);
        if (NS_SUCCEEDED(rv))
            break;

        switch (rv) {
            case NS_ERROR_FILE_DISK_FULL:       // Mac
            case NS_ERROR_FILE_TOO_BIG:         // Windows
            case NS_ERROR_FILE_NO_DEVICE_SPACE: // Who knows...
                if (aPurgeType == kPurgeNone) {
                    return rv;
                } if (aPurgeType == kPurgeOne) {
                    aPurgeType = kPurgeNone;
                }
                rv2 = PurgeOldestRedundantBackup();
                break;

            default:
                return rv;
                break;
        }
    } while (rv2 == NS_OK);

    return rv;
}

nsresult nsSafeSaveFile::RestoreFromBackup(void)
{
    nsCOMPtr<nsIFile> parentDir;
    nsCAutoString     fileName;
    nsresult          rv;

    // if the target file didn't initially exist there is nothing to restore from
    if (!mTargetFileExists)
        return NS_ERROR_FILE_NOT_FOUND;

    rv = mTargetFile->GetNativeLeafName(fileName);
    if (NS_FAILED(rv)) // yikes! out of memory
        return rv;

    // Ugh, copy only takes a directory and a name, lets "unpackage" our target file...
    rv = mTargetFile->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv))
        return rv;

    // kill the target... it's bad anyway
    mTargetFile->Remove(PR_FALSE);

    // and finally, copy the file (preserves file permissions)
    rv = mBackupFile->CopyToNative(parentDir, fileName);
    return rv;
}

nsresult nsSafeSaveFile::ManageRedundantBackups(void)
{
    nsCOMPtr<nsIFile> backupFile;
    nsCAutoString     fileName;
    nsresult          rv;
    PRBool            bExists;

    rv = mBackupFile->Clone(getter_AddRefs(backupFile));
    if (NS_FAILED(rv)) // yikes! out of memory, probably best to not continue
        return rv;

    if (mBackupCount > 0) {
        // kill the (oldest) backup copy, if necessary
        fileName = mBackupFileName;
        if (mBackupCount > 1)
            fileName.AppendInt(mBackupCount - 1);
        backupFile->SetNativeLeafName(fileName);
    }

    // remove the file as determined by the logic above
    backupFile->Remove(PR_FALSE);

    // bump any redundant backups up one (i.e. bak -> bak1, bak1 -> bak2, etc.)
    if (mBackupCount > 1) {
        PRInt32 backupCount = mBackupCount;
        fileName = mBackupFileName;
        while (--backupCount > 0) {
            if (backupCount > 1)
                fileName.AppendInt(backupCount - 1);
            backupFile->SetNativeLeafName(fileName);
            backupFile->Exists(&bExists);
            if (bExists) {
                fileName.Truncate(mBackupNameLen);
                fileName.AppendInt(backupCount);
                // fail silently because it's not important enough to bail on the save for
                backupFile->MoveToNative(0, fileName);
            }
            fileName.Truncate(mBackupNameLen);
        };
    }
    return NS_OK;
}

nsresult nsSafeSaveFile::PurgeOldestRedundantBackup(void)
{
    nsCOMPtr<nsIFile> backupFile;
    nsCAutoString     fileName;
    nsresult          rv;

    rv = mBackupFile->Clone(getter_AddRefs(backupFile));
    if (NS_FAILED(rv)) // yikes! out of memory, probably best to not continue
        return rv;

    // if no redundant backups, nothing to delete
    if (mBackupCount <= 1)
        return NS_ERROR_FILE_NOT_FOUND;

    PRInt32 backupCount = mBackupCount;
    fileName = mBackupFileName;
    while (--backupCount > 0) {
        fileName.AppendInt(backupCount);
        backupFile->SetNativeLeafName(fileName);
        rv = backupFile->Remove(PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
            return NS_OK;
        }
        fileName.Truncate(mBackupNameLen);
    };

    return NS_ERROR_FILE_NOT_FOUND;
}

