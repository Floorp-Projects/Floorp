/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Mike Shaver <shaver@mozilla.org>
 */

/*
 * Implementation of nsIFile for ``Unixy'' systems.
 */

/* we're going to need some autoconf loving, I can just tell */
#include <sys/types.h>
/* XXXautoconf for glibc */
#define __USE_BSD
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utime.h>

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsFileUtils.h"
#include "nsIAllocator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsLocalFileUnix.h"

#define FILL_STAT_CACHE()                       \
  PR_BEGIN_MACRO                                \
  if (!mHaveCachedStat) {                       \
      fillStatCache();                          \
      if (!mHaveCachedStat)                     \
         return NSRESULT_FOR_ERRNO();           \
  }                                             \
  PR_END_MACRO

#define CHECK_mPath()				\
  PR_BEGIN_MACRO				\
    if (!(const char *)mPath)			\
        return NS_ERROR_NOT_INITIALIZED;	\
  PR_END_MACRO

nsLocalFile::nsLocalFile() :
    mHaveCachedStat(PR_FALSE)
{
    mPath = "";
    NS_INIT_REFCNT();
}

nsLocalFile::~nsLocalFile()
{
}

NS_IMPL_ISUPPORTS1(nsLocalFile, nsIFile);

nsresult
nsLocalFile::Create(nsISupports *outer, const nsIID &aIID, void **aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);
    
    *aInstancePtr = 0;

    nsCOMPtr<nsIFile> inst = new nsLocalFile();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    return inst->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsLocalFile::InitWithKey(const char *fileKey)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::InitWithFile(nsIFile *file)
{
    NS_ENSURE_ARG(file);
    
    invalidateCache();
    if (NS_FAILED(file->GetPath(nsIFile::NATIVE_PATH, getter_Copies(mPath))))
        return NS_ERROR_FAILURE;
    
    if ((const char *)mPath == 0)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithPath(PRUint32 pathType, const char *filePath)
{
    NS_ENSURE_ARG(filePath);
    NS_ASSERTION(pathType == NATIVE_PATH ||
                 pathType == UNIX_PATH ||
                 pathType == NSPR_PATH, "unrecognized path type");

    /* NATIVE_PATH == UNIX_PATH == NSPR_PATH for us */
    mPath = filePath;
    invalidateCache();
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::createAllParentDirectories(PRUint32 permissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::Create(PRUint32 type, PRUint32 permissions)
{
    CHECK_mPath();
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    int result;
    /* use creat(2) for NORMAL_FILE, mkdir(2) for DIRECTORY */
    int (*creationFunc)(const char *, mode_t) =
        type == NORMAL_FILE_TYPE ? creat : mkdir;

    result = creationFunc((const char *)mPath, permissions);

    if (result == -1 && errno == ENOENT) {
        /*
         * if we failed because of missing parent components, try to create them
         * and then retry the original creation.
         */
        if (NS_FAILED(createAllParentDirectories(permissions)))
            return NS_ERROR_FAILURE;
        result = creationFunc((const char *)mPath, permissions);
    }

    /* creat(2) leaves the file open */
    if (result >= 0 && type == NORMAL_FILE_TYPE) {
	close(result);
	return NS_OK;
    }

    return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP
nsLocalFile::AppendPath(const char *fragment)
{
    NS_ENSURE_ARG(fragment);
    CHECK_mPath();
    char * newPath = (char *)nsAllocator::Alloc(strlen(mPath) +
                                                strlen(fragment) + 2);
    if (!newPath)
        return NS_ERROR_OUT_OF_MEMORY;
    strcpy(newPath, mPath);
    strcat(newPath, "/");
    strcat(newPath, fragment);
    mPath = newPath;
    invalidateCache();
    nsAllocator::Free(newPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsLocalFile::getLeafNameRaw(const char **_retval)
{
    CHECK_mPath();
    char *leafName = strrchr((const char *)mPath, '/');
    if (!leafName)
	return NS_ERROR_FILE_INVALID_PATH;
    *_retval = ++leafName;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(char **aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);
    nsresult rv;
    const char *leafName;
    if (NS_FAILED(rv = getLeafNameRaw(&leafName)))
	return rv;
    
    *aLeafName = nsCRT::strdup(leafName);
    if (!*aLeafName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPath(PRUint32 pathType, char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    if (!(const char *)mPath) {
	*_retval = nsnull;
	return NS_OK;
    }

    NS_ASSERTION(pathType == NATIVE_PATH ||
                 pathType == UNIX_PATH ||
                 pathType == NSPR_PATH, "unrecognized path type");
    *_retval = nsCRT::strdup((const char *)mPath);
    if (!*_retval)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *newParent, const char *newName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *newParent, const char *newName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *newParent, const char *newName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::MoveToFollowingLinks(nsIFile *newParent, const char *newName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::Execute(const char *args)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::Delete(PRBool recursive)
{
    FILL_STAT_CACHE();
    PRBool isDir = S_ISDIR(mCachedStat.st_mode);

    /* XXX ?
     * if (!isDir && recursive)
     *     return NS_ERROR_INVALID_ARG;
     */
    invalidateCache();

    if (isDir) {
        if (recursive) {
            nsCOMPtr<nsIDirectoryEnumerator> iterator;
            nsresult rv = NS_NewDirectoryEnumerator(this, PR_FALSE,
                                                    getter_AddRefs(iterator));
            if (NS_FAILED(rv))
                return rv;
            PRBool more;
            rv = iterator->HasMoreElements(&more);
            while (NS_SUCCEEDED(rv) && more) {
                nsCOMPtr<nsISupports> item;
                nsCOMPtr<nsIFile> file;
                rv = iterator->GetNext(getter_AddRefs(item));
                if (NS_FAILED(rv))
                    return NS_ERROR_FAILURE;
                file = do_QueryInterface(item, &rv);
                if (NS_FAILED(rv))
                    return NS_ERROR_FAILURE;
                if (NS_FAILED(rv = file->Delete(recursive)))
                    return rv;
                rv = iterator->HasMoreElements(&more);
            }
        }

        if (rmdir(mPath) == -1)
            return NSRESULT_FOR_ERRNO();
    } else {
        if (unlink(mPath) == -1)
            return NSRESULT_FOR_ERRNO();
    }

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDate(PRUint32 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    FILL_STAT_CACHE();
    *aLastModificationDate = (PRUint32)mCachedStat.st_mtime;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRUint32 aLastModificationDate)
{
    int result;
    if (aLastModificationDate) {
        FILL_STAT_CACHE();
        struct utimbuf ut;
        ut.actime = mCachedStat.st_atime;
        ut.modtime = (time_t)aLastModificationDate;
        result = utime(mPath, &ut);
    } else {
        result = utime(mPath, NULL);
    }
    invalidateCache();
    return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRUint32 *aLastModificationDateOfLink)
{
    NS_ENSURE_ARG(aLastModificationDateOfLink);
    struct stat sbuf;
    if (lstat(mPath, &sbuf) == -1)
        return NSRESULT_FOR_ERRNO();
    *aLastModificationDateOfLink = (PRUint32)sbuf.st_mtime;
    return NS_OK;
}

/*
 * utime(2) may or may not dereference symlinks, joy.
 */
NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRUint32 aLastModificationDateOfLink)
{
    return SetLastModificationDate(aLastModificationDateOfLink);
}

/*
 * only send back permissions bits: maybe we want to send back the whole
 * mode_t to permit checks against other file types?
 */
#define NORMALIZE_PERMS(mode)  ((mode)& (S_IRWXU | S_IRWXG | S_IRWXO))

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG(aPermissions);
    FILL_STAT_CACHE();
    *aPermissions = NORMALIZE_PERMS(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    NS_ENSURE_ARG(aPermissionsOfLink);
    struct stat sbuf;
    if (lstat(mPath, &sbuf) == -1)
        return NSRESULT_FOR_ERRNO();
    *aPermissionsOfLink = NORMALIZE_PERMS(sbuf.st_mode);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    invalidateCache();
    return NSRESULT_FOR_RETURN(chmod(mPath, aPermissions));
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return SetPermissions(aPermissions);
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(PRUint32 *aFileSize)
{
    NS_ENSURE_ARG_POINTER(aFileSize);
    FILL_STAT_CACHE();
    if (sizeof(off_t) > 4 && mCachedStat.st_size > (off_t)0xffffffff)
        *aFileSize = 0xffffffff; // return error code?
    else
        *aFileSize = (PRUint32)mCachedStat.st_size;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRUint32 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    struct stat sbuf;
    if (lstat(mPath, &sbuf) == -1)
        return NSRESULT_FOR_ERRNO();
    if (sizeof(off_t) > 4 && mCachedStat.st_size > (off_t)0xffffffff)
        *aFileSize = 0xffffffff; // return error code?
    else
        *aFileSize = (PRUint32)sbuf.st_size;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG_POINTER(aDiskSpaceAvailable);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile **aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * The results of Exists, isWritable and isReadable are not cached.
 */

NS_IMETHODIMP
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    PRBool accessOK;
    *_retval = accessOK = (access(mPath, F_OK) == 0);
    if (accessOK || errno == EACCES)
        return NS_OK;
    return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsWritable(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    PRBool accessOK;
    *_retval = accessOK = (access(mPath, W_OK) == 0);
    if (accessOK || errno == EACCES)
        return NS_OK;
    return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    PRBool accessOK;
    *_retval = accessOK = (access(mPath, R_OK) == 0);
    if (accessOK || errno == EACCES)
        return NS_OK;
    return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsExecutable(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    PRBool accessOK;
    *_retval = accessOK = (access(mPath, X_OK) == 0);
    if (accessOK || errno == EACCES)
        return NS_OK;
    return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    FILL_STAT_CACHE();
    *_retval = S_ISDIR(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    FILL_STAT_CACHE();
    *_retval = S_ISREG(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    nsresult rv;
    const char *leafName;
    if (NS_FAILED(rv = getLeafNameRaw(&leafName)))
	return rv;
    *_retval = (leafName[0] == '.');
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    FILL_STAT_CACHE();
    *_retval = S_ISLNK(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    FILL_STAT_CACHE();
    *_retval = !(S_ISLNK(mCachedStat.st_mode) || S_ISREG(mCachedStat.st_mode) ||
		 S_ISDIR(mCachedStat.st_mode));
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG_POINTER(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::IsContainedIn(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG_POINTER(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::Truncate(PRUint32 aLength)
{
    return NSRESULT_FOR_RETURN(truncate(mPath, (off_t)aLength));
}

NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    FILL_STAT_CACHE();
    if (!S_ISLNK(mCachedStat.st_mode))
        return NS_ERROR_FILE_INVALID_PATH;

    PRUint32 targetSize;
    if (NS_FAILED(GetFileSizeOfLink(&targetSize)))
        return NS_ERROR_FAILURE;

    char *target = (char *)nsAllocator::Alloc(targetSize);
    if (!target)
        return NS_ERROR_OUT_OF_MEMORY;

    int result = readlink(mPath, target, (size_t)targetSize);
    if (!result) {
        *_retval = target;
        return NS_OK;
    }
    nsAllocator::Free(target);
    return NSRESULT_FOR_ERRNO();
}
