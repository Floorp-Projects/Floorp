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
#include "prmem.h"

// Definitions
#define BACKUP_FILE_EXTENSION ".bak"


nsSafeSaveFile::nsSafeSaveFile(nsIFile *aTargetFile, PRInt32 aNumBackupCopies)
 : mTargetNameLen(0),
   mBackupCount(aNumBackupCopies)
{
    nsCAutoString tempFileName;  
    char *        temp;
    nsresult      rv;

    // determine the actual filename (less the extension)
    rv = aTargetFile->GetLeafName(getter_Copies(mTargetFileName));
    if (NS_FAILED(rv)) // yikes! out of memory
        return;

    temp = strrchr(mTargetFileName, '.');
    if (temp)
       mTargetNameLen = temp - mTargetFileName;
    else
       mTargetNameLen = strlen(mTargetFileName);

    // create a new file object that points to the temp file
    tempFileName.Assign(mTargetFileName, mTargetNameLen);
    tempFileName += ".tmp";
    rv = aTargetFile->Clone(getter_AddRefs(mTempFile));
    if (NS_SUCCEEDED(rv))
        mTempFile->SetLeafName(tempFileName.get());
}

void nsSafeSaveFile::CleanupFailedSave(void)
{
    NS_ASSERTION(mTempFile, "Must create a temp file first!");

    // the save failed, remove the temp file
    mTempFile->Remove(PR_FALSE);
}

nsresult nsSafeSaveFile::GetSaveFile(nsIFile **_retval)
{
    if (!mTargetFileName || !mTempFile)
        return(NS_ERROR_OUT_OF_MEMORY);

    *_retval = mTempFile;
    NS_ADDREF(*_retval);

    return(NS_OK);
}

nsresult nsSafeSaveFile::PostProcessSave(void)
{
    nsCOMPtr<nsIFile> backupFile;
    nsCAutoString     fileName;
    nsresult          rv;
    PRBool            bExists;

    NS_ASSERTION(mTempFile, "Must create a temp file first!");

    rv = mTempFile->Clone(getter_AddRefs(backupFile));
    if (NS_FAILED(rv)) // yikes! out of memory, probably best to not continue
        return rv;

    if (mBackupCount > 0) {
        // kill the (oldest) backup copy, if necessary
        fileName.Assign(mTargetFileName, mTargetNameLen);
        fileName += BACKUP_FILE_EXTENSION;
        if (mBackupCount > 1)
            fileName.AppendInt(mBackupCount - 1);
        backupFile->SetLeafName(fileName.get());
    } else {
        // no backups desired, delete the previous save
        backupFile->SetLeafName(mTargetFileName);
    }

    // remove the file as determined by the logic above
    backupFile->Remove(PR_FALSE);

    // now manage the backup copies
    if (mBackupCount > 0) {
        PRInt32 backupCount = mBackupCount;
        fileName.Assign(mTargetFileName, mTargetNameLen);
        fileName += BACKUP_FILE_EXTENSION;
        while (--backupCount > 0) {
            // bump all of the redundant backups up one (i.e. bak -> bak1, bak1 -> bak2, etc.)
            if (backupCount > 1)
                fileName.AppendInt(backupCount - 1);
            backupFile->SetLeafName(fileName.get());
            backupFile->Exists(&bExists);
            if (bExists) {
                fileName.Truncate(mTargetNameLen + (sizeof(BACKUP_FILE_EXTENSION) - 1));
                fileName.AppendInt(backupCount);
                // fail silently because it's not important enough to bail on the save for
                backupFile->MoveTo(0, fileName.get());
            }
            fileName.Truncate(mTargetNameLen + (sizeof(BACKUP_FILE_EXTENSION) - 1));
        };

        // rename the previous save to .bak (i.e. <filename.js> to <filename.bak>)
        backupFile->SetLeafName(mTargetFileName);
        rv = backupFile->MoveTo(0, fileName.get());
        if (NS_FAILED(rv))
            return rv;
    }

    // finally rename the temp file to the original name (i.e. <filename.tmp> to <filename.js>)
    rv = mTempFile->MoveTo(0, mTargetFileName);
    return rv;
}

nsresult nsSafeSaveFile::PurgeOldestBackup(void)
{
    nsCOMPtr<nsIFile> backupFile;
    nsCAutoString     fileName;
    nsresult          rv;

    NS_ASSERTION(mTempFile, "Must create a temp file first!");

    rv = mTempFile->Clone(getter_AddRefs(backupFile));
    if (NS_FAILED(rv)) // yikes! out of memory, probably best to not continue
        return rv;

    // can't delete what you're not creating
    if (mBackupCount == 0)
        return NS_ERROR_FILE_NOT_FOUND;

    PRInt32 backupCount = mBackupCount;
    fileName.Assign(mTargetFileName, mTargetNameLen);
    fileName += BACKUP_FILE_EXTENSION;
    while (--backupCount >= 0) {
        if (backupCount)
            fileName.AppendInt(backupCount);
        backupFile->SetLeafName(fileName.get());
        rv = backupFile->Remove(PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
            return NS_OK;
        }
        fileName.Truncate(mTargetNameLen + (sizeof(BACKUP_FILE_EXTENSION) - 1));
    };

    return NS_ERROR_FILE_NOT_FOUND;
}

