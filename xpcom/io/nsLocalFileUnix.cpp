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
 *     Christopher Blizzard <blizzard@mozilla.org>
 *     Jason Eager <jce2@po.cwru.edu>
 *     Stuart Parmenter <pavlov@netscape.com>
 */

/*
 * Implementation of nsIFile for ``Unixy'' systems.
 */

/* we're going to need some autoconf loving, I can just tell */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utime.h>
#include <dirent.h>
#include <ctype.h>
#ifdef XP_BEOS
#include <Path.h>
#include <Entry.h>
#endif

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsLocalFileUnix.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "prproces.h"

// we need these for statfs()

#ifdef HAVE_SYS_STATVFS_H
#if defined(__osf__) && defined(__DECCXX)
    extern "C" int statvfs(const char *, struct statvfs *);
#endif
#include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_STATVFS
#define STATFS statvfs
#else
#define STATFS statfs
#endif

// On some platforms file/directory name comparisons need to
// be case-blind.
#if defined(VMS)
#define FILE_STRCMP strcasecmp
#define FILE_STRNCMP strncasecmp
#else
#define FILE_STRCMP strcmp
#define FILE_STRNCMP strncmp
#endif

#define VALIDATE_STAT_CACHE()                   \
  PR_BEGIN_MACRO                                \
  if (!mHaveCachedStat) {                       \
      FillStatCache();                          \
      if (!mHaveCachedStat)                     \
         return NSRESULT_FOR_ERRNO();           \
  }                                             \
  PR_END_MACRO

#define CHECK_mPath()				\
  PR_BEGIN_MACRO				\
    if (!(const char *)mPath)			\
        return NS_ERROR_NOT_INITIALIZED;	\
  PR_END_MACRO

#define mLL_II2L(hi, lo, res)                   \
  LL_I2L(res, hi);                              \
  LL_SHL(res, res, 32);                         \
  LL_ADD(res, res, lo);

#define mLL_L2II(a64, hi, lo)                   \
  PR_BEGIN_MACRO                                \
    PRInt64 tmp;                                \
    LL_SHR(tmp, a64, 32);                       \
    LL_L2I(hi, tmp);                            \
    LL_SHL(tmp, a64, 32);                       \
    LL_SHR(tmp, tmp, 32);                       \
    LL_L2I(lo, tmp);                            \
  PR_END_MACRO

/* directory enumerator */
class NS_COM
nsDirEnumeratorUnix : public nsISimpleEnumerator
{
 public:
    nsDirEnumeratorUnix();
    virtual ~nsDirEnumeratorUnix();

    // nsISupports interface
    NS_DECL_ISUPPORTS
    
    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    NS_IMETHOD Init(nsIFile *parent, PRBool ignored);
 protected:
    NS_IMETHOD GetNextEntry();

    DIR *mDir;
    struct dirent *mEntry;
    nsXPIDLCString mParentPath;
};

nsDirEnumeratorUnix::nsDirEnumeratorUnix() :
  mDir(nsnull), mEntry(nsnull)
{
    NS_INIT_REFCNT();
}

nsDirEnumeratorUnix::~nsDirEnumeratorUnix()
{
    if (mDir)
        closedir(mDir);
}

NS_IMPL_ISUPPORTS1(nsDirEnumeratorUnix, nsISimpleEnumerator)

NS_IMETHODIMP
nsDirEnumeratorUnix::Init(nsIFile *parent, PRBool resolveSymlinks /*ignored*/)
{
    nsXPIDLCString dirPath;
    if (NS_FAILED(parent->GetPath(getter_Copies(dirPath))) ||
        (const char *)dirPath == 0)
        return NS_ERROR_FILE_INVALID_PATH;

    if (NS_FAILED(parent->GetPath(getter_Copies(mParentPath))))
        return NS_ERROR_FAILURE;

    mDir = opendir(dirPath);
    if (!mDir)
        return NSRESULT_FOR_ERRNO();
    return GetNextEntry();
}

NS_IMETHODIMP
nsDirEnumeratorUnix::HasMoreElements(PRBool *result)
{
    *result = mDir && mEntry;
    return NS_OK;
}

NS_IMETHODIMP
nsDirEnumeratorUnix::GetNext(nsISupports **_retval)
{
    nsresult rv;
    if (!mDir || !mEntry) {
        *_retval = nsnull;
        return NS_OK;
    }

    nsCOMPtr<nsILocalFile> file = new nsLocalFile();
    if (!file)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = file->InitWithPath(mParentPath)) ||
        NS_FAILED(rv = file->Append(mEntry->d_name))) {
        return rv;
    }
    *_retval = file;
    NS_ADDREF(*_retval);
    return GetNextEntry();
}

NS_IMETHODIMP
nsDirEnumeratorUnix::GetNextEntry()
{
    do {
        errno = 0;
        mEntry = readdir(mDir);

        /* end of dir or error */
        if (!mEntry)
            return NSRESULT_FOR_ERRNO();

        /* keep going past "." and ".." */
    } while (mEntry->d_name[0] == '.' && 
             (mEntry->d_name[1] == '\0' || /* .\0 */
              (mEntry->d_name[1] == '.' &&
               mEntry->d_name[2] == '\0'))); /* ..\0 */
    return NS_OK;
}

nsLocalFile::nsLocalFile() :
    mHaveCachedStat(PR_FALSE)
{
    mPath = "";
    NS_INIT_REFCNT();
}

nsLocalFile::~nsLocalFile()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLocalFile, nsILocalFile, nsIFile)

nsresult
nsLocalFile::nsLocalFileConstructor(nsISupports *outer, const nsIID &aIID, void **aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_NO_AGGREGATION(outer);
    
    *aInstancePtr = 0;

    nsCOMPtr<nsIFile> inst = new nsLocalFile();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    return inst->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsLocalFile::Clone(nsIFile **file)
{
    nsresult rv;
    NS_ENSURE_ARG(file);
    
    *file = nsnull;

    nsCOMPtr<nsILocalFile> localFile = new nsLocalFile();
    if (localFile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    rv = localFile->InitWithPath(mPath);
    
    if (NS_FAILED(rv)) 
        return rv;

    *file = localFile;
    NS_ADDREF(*file);
    
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithPath(const char *filePath)
{
    NS_ENSURE_ARG(filePath);
    
    int   len  = strlen(filePath);
    char* name = (char*) nsMemory::Clone( filePath, len+1 );
    if (name[len-1] == '/') {
        if (len != 1)
            name[len-1] = '\0';
    }
    
    mPath = name;
    
    nsMemory::Free(name);
    
    InvalidateCache();
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CreateAllAncestors(PRUint32 permissions)
{
    /* <jband> I promise to play nice */
    char *buffer = NS_CONST_CAST(char *, (const char *)mPath),
        *ptr = buffer;

#ifdef DEBUG_NSIFILE
    fprintf(stderr, "nsIFile: before: %s\n", buffer);
#endif

    while ((ptr = strchr(ptr + 1, '/'))) {
        /*
         * Sequences of '/' are equivalent to a single '/'.
         */
        if (ptr[1] == '/')
            continue;

        /*
         * If the path has a trailing slash, don't make the last component here,
         * because we'll get EEXISTS in Create when we try to build the final
         * component again, and it's easier to condition the logic here than
         * there.
         */
        if (!ptr[1])
            break;
        /* Temporarily NUL-terminate here */
        *ptr = '\0';
#ifdef DEBUG_NSIFILE
        fprintf(stderr, "nsIFile: mkdir(\"%s\")\n", buffer);
#endif
        int result = mkdir(buffer, permissions);
        /* Put the / back before we (maybe) return */
        *ptr = '/';

        /*
         * We could get EEXISTS for an existing file -- not directory --
         * with the name of one of our ancestors, but that's OK: we'll get
         * ENOTDIR when we try to make the next component in the path,
         * either here on back in Create, and error out appropriately.
         */
        if (result == -1 && errno != EEXIST)
            return NSRESULT_FOR_ERRNO();
    }

#ifdef DEBUG_NSIFILE
    fprintf(stderr, "nsIFile: after: %s\n", buffer);
#endif
            
    return NS_OK;

}


NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    CHECK_mPath();
   
    *_retval = PR_Open(mPath, flags, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP  
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    CHECK_mPath(); 
   
    *_retval = fopen(mPath, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}
static int exclusive_create(const char * path, mode_t mode) {
  return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, mode);
}
static int exclusive_mkdir(const char * path, mode_t mode) {
  return mkdir(path, mode);
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
        type == NORMAL_FILE_TYPE ? exclusive_create : exclusive_mkdir;

    result = creationFunc((const char *)mPath, permissions);

    if (result == -1 && errno == ENOENT) {
        /*
         * If we failed because of missing ancestor components, try to create
         * them and then retry the original creation.
         * 
         * Ancestor directories get the same permissions as the file we're
         * creating, with the X bit set for each of (user,group,other) with
         * an R bit in the original permissions.  If you want to do anything
         * fancy like setgid or sticky bits, do it by hand.
         */
        int dirperm = permissions;
        if (permissions & S_IRUSR)
            dirperm |= S_IXUSR;
        if (permissions & S_IRGRP)
            dirperm |= S_IXGRP;
        if (permissions & S_IROTH)
            dirperm |= S_IXOTH;

#ifdef DEBUG_NSIFILE
        fprintf(stderr, "nsIFile: perm = %o, dirperm = %o\n", permissions, 
                dirperm);
#endif

        if (NS_FAILED(CreateAllAncestors(dirperm)))
            return NS_ERROR_FAILURE;

#ifdef DEBUG_NSIFILE
        fprintf(stderr, "nsIFile: Create(\"%s\") again\n", (const char *)mPath);
#endif
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
nsLocalFile::Append(const char *fragment)
{
    NS_ENSURE_ARG(fragment);
    CHECK_mPath();

    // only one component of path can be appended
    if (strstr(fragment, "/") != nsnull)
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    return AppendRelativePath(fragment);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const char *fragment)
{
    NS_ENSURE_ARG(fragment);
    CHECK_mPath();

    // Cannot start with a '/' and no .. allowed in the fragment
    if ((*fragment == '/') || (strstr(fragment, "..") != nsnull))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    char * newPath = (char *)nsMemory::Alloc(strlen(mPath) +
                                                strlen(fragment) + 2);
    if (!newPath)
        return NS_ERROR_OUT_OF_MEMORY;
    strcpy(newPath, mPath);
    strcat(newPath, "/");
    strcat(newPath, fragment);
    mPath = newPath;
    InvalidateCache();
    nsMemory::Free(newPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    char  resolved_path[PATH_MAX];
    char *resolved_path_ptr = NULL;
    CHECK_mPath();
#ifdef XP_BEOS
    BEntry be_e((const char*)mPath, true);
    BPath be_p;
    status_t err;
    if ((err = be_e.GetPath(&be_p)) == B_OK) {
        resolved_path_ptr = be_p.Path();
    };
#else
    resolved_path_ptr = realpath((const char *)mPath, resolved_path);
#endif
    // if there is an error, the return is null.
    if (resolved_path_ptr == NULL) {
        return NSRESULT_FOR_ERRNO();
    }
    // nsXPIDLCString will copy.
    mPath = resolved_path;
    return NS_OK;
}

nsresult
nsLocalFile::GetLeafNameRaw(const char **_retval)
{
    CHECK_mPath();
    const char *leafName = strrchr((const char *)mPath, '/');
    if (!leafName)
	return NS_ERROR_FILE_INVALID_PATH;
    *_retval = leafName+1;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(char **aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);
    nsresult rv;
    const char *leafName;
    if (NS_FAILED(rv = GetLeafNameRaw(&leafName)))
	return rv;
    
    *aLeafName = nsCRT::strdup(leafName);
    if (!*aLeafName)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::SetLeafName(const char *aLeafName)
{
    NS_ENSURE_ARG(aLeafName);
    CHECK_mPath();

    nsresult rv;
    char *leafName;
    if (NS_FAILED(rv = GetLeafNameRaw((const char**)&leafName)))
	return rv;
    char* newPath = (char *)nsMemory::Alloc(strlen(mPath) +
                                               strlen(aLeafName) + 2);
    *leafName = 0;
    
    strcpy(newPath, mPath);
    strcat(newPath, "/");
    strcat(newPath, aLeafName);
    mPath = newPath;
    InvalidateCache();
    nsMemory::Free(newPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPath(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    if (!(const char *)mPath) {
	*_retval = nsnull;
	return NS_OK;
    }

    *_retval = nsCRT::strdup((const char *)mPath);
    if (!*_retval)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *newParent, const char *newName)
{
    nsresult rv;

    // check to make sure that we have a new parent
    // or have a new name
    if (newParent == nsnull && newName == nsnull)
        return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;

    // check to make sure that this has been initialized properly
    CHECK_mPath();

    // check to see if we are a directory or if we are a file
    PRBool isDirectory;
    IsDirectory(&isDirectory);

    if (isDirectory)
    {
        // XXX
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    else
    {
        // ok, we're a file so this should be easy as pie.
        // check to see if our target exists
        PRBool targetExists;
        newParent->Exists(&targetExists);
        // check to see if we need to create it
        if (targetExists == PR_FALSE)
        {
            // XXX create the new directory with some permissions
            rv = newParent->Create(DIRECTORY_TYPE, 0755);
            if (NS_FAILED(rv))
                return rv;
        }
        // make sure that the target is actually a directory
        PRBool targetIsDirectory;
        newParent->IsDirectory(&targetIsDirectory);
        if (targetIsDirectory == PR_FALSE)
            return NS_ERROR_FILE_DESTINATION_NOT_DIR;
        // ok, we got this far.  create the name of the new file.
        nsXPIDLCString leafName;
        nsXPIDLCString newPath;
        const char *tmpConstChar;
        char       *tmpChar;
        // get the leaf name of the new file if a name isn't supplied.
        if (newName == nsnull)
        {
            if (NS_FAILED(rv = GetLeafNameRaw(&tmpConstChar)))
                return rv;
            leafName = tmpConstChar;
        }
        else 
        {
            leafName = newName;
        }
        // get the path name
        if (NS_FAILED(rv = newParent->GetPath(&tmpChar)))
            return rv;

        // this is the full path to the dir of the new file
        newPath = tmpChar;
        nsMemory::Free(tmpChar);

        // create the final name
        char *newPathName;
        newPathName = (char *)nsMemory::Alloc(strlen(newPath) + strlen(leafName) + 2);
        if (!newPathName)
            return NS_ERROR_OUT_OF_MEMORY;
        
        strcpy(newPathName, newPath);
        strcat(newPathName, "/");
        strcat(newPathName, leafName);

#ifdef DEBUG_blizzard
        printf("nsLocalFile::CopyTo() %s -> %s\n", (const char *)mPath, newPathName);
#endif

        nsXPIDLCString newPathNameAuto;
        newPathNameAuto = newPathName;
        nsMemory::Free(newPathName);

        // actually create the file.

        nsLocalFile *newFile = new nsLocalFile();
        newFile->AddRef(); // we own this.
        if (newFile == nsnull)
        {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        rv = newFile->InitWithPath(newPathNameAuto);

        if (NS_FAILED(rv))
        {
            NS_RELEASE(newFile);
            return rv;
        }

        // get the old permissions
        PRUint32 myPerms;
        GetPermissions(&myPerms);
        // create the new file with the same permissions
        rv = newFile->Create(NORMAL_FILE_TYPE, myPerms);
        if (NS_FAILED(rv))
        {
            NS_RELEASE(newFile);
            return rv;
        }

        // open the new file.

        PRInt32     openFlags = PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE;
        PRInt32     modeFlags = myPerms;
        PRFileDesc *newFD = nsnull;

        rv = newFile->OpenNSPRFileDesc(openFlags, modeFlags, &newFD);
        if (NS_FAILED(rv))
        {
            NS_RELEASE(newFile);
            return rv;
        }
        if (newFD == nsnull)
        {
            NS_RELEASE(newFile);
            NSRESULT_FOR_ERRNO();
        }

        // open the old file, too

        openFlags = PR_RDONLY;
        PRFileDesc *oldFD = nsnull;

        rv = OpenNSPRFileDesc(openFlags, modeFlags, &oldFD);
        // make sure to clean up properly
        if (NS_FAILED(rv))
        {
            NS_RELEASE(newFile);
            PR_Close(newFD);
            return rv;
        }
        if (newFD == nsnull)
        {
            NS_RELEASE(newFile);
            PR_Close(newFD);
            NSRESULT_FOR_ERRNO();
        }

        // get the length of the file
        
        PRStatus status;
        PRFileInfo fileInfo;
        status = PR_GetFileInfo(mPath, &fileInfo);
        if (status != PR_SUCCESS)
        {
            NS_RELEASE(newFile);
            PR_Close(newFD);
            NSRESULT_FOR_ERRNO();
        }
        
        // get the size of the file.

        PROffset32 fileSize;
        fileSize = fileInfo.size;
        PRInt32 bytesRead = 0;
        PRInt32 bytesWritten = 0;
        PRInt32 totalRead = 0;
        PRInt32 totalWritten = 0;

        char buf[BUFSIZ];

        while(1)
        {
            bytesRead = PR_Read(oldFD, &buf, BUFSIZ);
            if (bytesRead == 0)
                break;
            if (bytesRead == -1)
                return NS_ERROR_FAILURE;
            totalRead += bytesRead;
            bytesWritten = PR_Write(newFD, &buf, bytesRead);
            if (bytesWritten == -1)
                return NS_ERROR_FAILURE;
            totalWritten += bytesWritten;
        } 

#ifdef DEBUG_blizzard
        printf("read %d bytes, wrote %d bytes\n",
               totalRead, totalWritten);
#endif
        
        // close the files
        PR_Close(newFD);
        PR_Close(oldFD);

        // free our resources
        NS_RELEASE(newFile);

    }
           
    return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *newParent, const char *newName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *newParent, const char *newName)
{
    nsresult rv = CopyTo(newParent, newName);
    if (NS_SUCCEEDED(rv))
      rv = Delete(PR_TRUE);
    return rv;
}

NS_IMETHODIMP
nsLocalFile::Delete(PRBool recursive)
{
    VALIDATE_STAT_CACHE();
    PRBool isDir = S_ISDIR(mCachedStat.st_mode);

    /* XXX ?
     * if (!isDir && recursive)
     *     return NS_ERROR_INVALID_ARG;
     */
    InvalidateCache();

    if (isDir) {
        if (recursive) {
            nsDirEnumeratorUnix *dir = new nsDirEnumeratorUnix();
            if (!dir)
                return NS_ERROR_OUT_OF_MEMORY;

            nsresult rv = dir->Init(this, PR_FALSE);
            if (NS_FAILED(rv)) {
                delete dir;
                return rv;
            }

            nsCOMPtr<nsISimpleEnumerator> iterator;
            iterator = do_QueryInterface(dir, &rv);
            if (NS_FAILED(rv)) {
                delete dir;
                return rv;
            }

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
nsLocalFile::GetLastModificationDate(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    CHECK_mPath();
    PRFileInfo64 info;
    if (PR_GetFileInfo64(mPath, &info) != PR_SUCCESS) {
        return NSRESULT_FOR_ERRNO();
    }
    // PRTime is a 64 bit value
    // microseconds -> milliseconds
    *aLastModificationDate = info.modifyTime / PR_USEC_PER_MSEC;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRInt64 aLastModificationDate)
{
    int result;
    if (aLastModificationDate) {
        VALIDATE_STAT_CACHE();
        struct utimbuf ut;
        ut.actime = mCachedStat.st_atime;

        // convert PRTime microsecs to unix seconds since the epoch
        double dTime;
        LL_L2D(dTime, aLastModificationDate);
        ut.modtime = (time_t)( (PRUint32)(dTime / PR_MSEC_PER_SEC) );
        result = utime(mPath, &ut);
    } else {
        result = utime(mPath, NULL);
    }
    InvalidateCache();
    return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRInt64 *aLastModificationDateOfLink)
{
    NS_ENSURE_ARG(aLastModificationDateOfLink);
    struct stat sbuf;
    if (lstat(mPath, &sbuf) == -1)
        return NSRESULT_FOR_ERRNO();
    mLL_II2L(0, (PRUint32)sbuf.st_mtime, *aLastModificationDateOfLink);

    // lstat returns st_mtime in seconds!
    *aLastModificationDateOfLink *= PR_MSEC_PER_SEC;

    return NS_OK;
}

/*
 * utime(2) may or may not dereference symlinks, joy.
 */
NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRInt64 aLastModificationDateOfLink)
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
    VALIDATE_STAT_CACHE();
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
    InvalidateCache();
    return NSRESULT_FOR_RETURN(chmod(mPath, aPermissions));
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return SetPermissions(aPermissions);
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG_POINTER(aFileSize);
    VALIDATE_STAT_CACHE();
    
	/* XXX autoconf for and use stat64 if available */
    if( S_ISDIR(mCachedStat.st_mode) )
        {
		mLL_II2L(0, (PRUint32)0, *aFileSize);
        }
    else	
        {
		mLL_II2L(0, (PRUint32)mCachedStat.st_size, *aFileSize);
        }
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    PRInt32 hi, lo;
    mLL_L2II(aFileSize, hi, lo);
    /* XXX truncate64? */
    InvalidateCache();
    if (truncate((const char *)mPath, (off_t)lo) == -1)
        return NSRESULT_FOR_ERRNO();
   return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    struct stat sbuf;
    if (lstat(mPath, &sbuf) == -1)
        return NSRESULT_FOR_ERRNO();
    /* XXX autoconf for and use lstat64 if available */
    mLL_II2L(0, (PRInt32)sbuf.st_size, *aFileSize);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG_POINTER(aDiskSpaceAvailable);

    PRInt64 bytesFree = 0;

    //These systems have the operations necessary to check disk space.
    
#if defined(HAVE_SYS_STATFS_H) || defined(HAVE_SYS_STATVFS_H)

    // check to make sure that mPath is properly initialized
    CHECK_mPath();
    
    struct STATFS fs_buf;

    // Members of the STATFS struct that you should know about:
    // f_bsize = block size on disk.
    // f_bavail = number of free blocks available to a non-superuser.
    // f_bfree = number of total free blocks in file system.

    if (STATFS(mPath, &fs_buf) < 0)
    // The call to STATFS failed.
    {
#ifdef DEBUG
        printf("ERROR: GetDiskSpaceAvailable: STATFS call FAILED. \n");
#endif
        return NS_ERROR_FAILURE;
    }
#ifdef DEBUG_DISK_SPACE
    printf("DiskSpaceAvailable: %d bytes\n", 
       fs_buf.f_bsize * (fs_buf.f_bavail - 1));
#endif

    // The number of Bytes free = The number of free blocks available to
    // a non-superuser, minus one as a fudge factor, multiplied by the size
    // of the beforementioned blocks.

    LL_I2L( bytesFree, (fs_buf.f_bsize * (fs_buf.f_bavail - 1) ) );
    *aDiskSpaceAvailable = bytesFree;
    return NS_OK;

#else 
    /*
    ** This platform doesn't have statfs or statvfs.
    ** I'm sure that there's a way to check for free disk space
    ** on platforms that don't have statfs 
    ** (I'm SURE they have df, for example).
    ** 
    ** Until we figure out how to do that, lets be honest 
    ** and say that this command isn't implemented
    ** properly for these platforms yet.
    */
#ifdef DEBUG
    printf("ERROR: GetDiskSpaceAvailable: Not implemented for plaforms without statfs.\n");
#endif
    return NS_ERROR_NOT_IMPLEMENTED;

#endif /* HAVE_SYS_STATFS_H or HAVE_SYS_STATVFS_H */
    
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile **aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;

    CHECK_mPath();

    nsCString parentPath(NS_STATIC_CAST(const char*, mPath));

    // check to see whether or not we need to cut off any trailing
    // slashes

    PRInt32 offset = parentPath.RFindChar('/');
    PRInt32 length = parentPath.Length();

    // eat of trailing slash marks
    while ((length > 1) && offset && (length == (offset + 1))) {
        parentPath.Truncate(offset);
        offset = parentPath.RFindChar('/');
        length = parentPath.Length();
    }

    if (offset == -1)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // for the case where we are at '/'
    if (offset == 0)
        offset++;
    parentPath.Truncate(offset);

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv =  NS_NewLocalFile(parentPath.GetBuffer(), PR_TRUE, getter_AddRefs(localFile));
    
    if(NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aParent);
    }
    return rv;
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
    return NS_OK;
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
    VALIDATE_STAT_CACHE();
    *_retval = S_ISDIR(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = S_ISREG(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    nsresult rv;
    const char *leafName;
    if (NS_FAILED(rv = GetLeafNameRaw(&leafName)))
	return rv;
    *_retval = (leafName[0] == '.');
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = S_ISLNK(mCachedStat.st_mode);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = !(S_ISLNK(mCachedStat.st_mode) || S_ISREG(mCachedStat.st_mode) ||
		 S_ISDIR(mCachedStat.st_mode));
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_FALSE;
    
    nsresult rv;
    nsXPIDLCString myPath, inPath;
    
    if (NS_FAILED(rv = GetPath(getter_Copies(myPath))))
        return rv;
    if (NS_FAILED(rv = inFile->GetPath(getter_Copies(inPath))))
        return rv;
    *_retval = !FILE_STRCMP(inPath, myPath);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG_POINTER(_retval);
   
    nsXPIDLCString inPath;
    nsresult rv;
    
    *_retval = PR_FALSE;

    if (NS_FAILED(rv = inFile->GetPath(getter_Copies(inPath))))
        return rv;
    
    ssize_t len = strlen(mPath);

    if ( FILE_STRNCMP( mPath, inPath, len) == 0)
    {
        // now make sure that the |inFile|'s path has a trailing
        // separator.

        if (inPath[len] == '/')
        {
            *_retval = PR_TRUE;
        }

    }

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    if (!S_ISLNK(mCachedStat.st_mode))
        return NS_ERROR_FILE_INVALID_PATH;

    PRInt64 targetSize64;
    if (NS_FAILED(GetFileSizeOfLink(&targetSize64)))
        return NS_ERROR_FAILURE;

    PRInt32 hi, lo;
    mLL_L2II(targetSize64, hi, lo);
    char *target = (char *)nsMemory::Alloc(lo);
    if (!target)
        return NS_ERROR_OUT_OF_MEMORY;

    int result = readlink(mPath, target, (size_t)lo);
    if (!result) {
        *_retval = target;
        return NS_OK;
    }
    nsMemory::Free(target);
    return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::Spawn(const char **args, PRUint32 count)
{
    CHECK_mPath();

    // status from our create process
    PRStatus rv = PR_FAILURE;

    // this is the argv that will hold the args that 
    char ** my_argv = NULL;

    // make sure that when we allocate we have 1 greater than the
    // count since we need to null terminate the list for the argv to
    // pass into PR_CreateProcessDetached
    my_argv = (char **)malloc(sizeof(char *) * (count + 2) );
    if (!my_argv) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    // copy the args
    PRUint32 i;
    for (i=0; i < count; i++) {
        my_argv[i+1] = (char *)args[i];
    }
    // we need to set argv[0] to the program name.
    my_argv[0] = (char *)(const char *)mPath;
    
    // null terminate the array
    my_argv[count+1] = NULL;

    rv = PR_CreateProcessDetached(mPath, my_argv, NULL, NULL);

    // free up our argv
    free(my_argv);

    if (PR_SUCCESS == rv)
        return NS_OK;
    else  
        return NS_ERROR_FILE_EXECUTION_FAILED;
}


/* attribute PRBool followLinks; */
NS_IMETHODIMP 
nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
    *aFollowLinks = PR_TRUE;
    return NS_OK;
}
NS_IMETHODIMP 
nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator **entries)
{
    nsDirEnumeratorUnix *dir = new nsDirEnumeratorUnix();
    if (!dir)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = dir->Init(this, PR_FALSE);
    if (NS_FAILED(rv)) {
        delete dir;
        return rv;
    }

    /* QI needed? */
    return dir->QueryInterface(NS_GET_IID(nsISimpleEnumerator),
                               (void **)entries);
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_LoadLibrary(mPath);
    if (!*_retval)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(char * *aPersistentDescriptor)
{
    NS_ENSURE_ARG_POINTER(aPersistentDescriptor);
    return GetPath(aPersistentDescriptor);
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const char * aPersistentDescriptor)
{
   NS_ENSURE_ARG(aPersistentDescriptor); 
   return InitWithPath(aPersistentDescriptor);   
}

nsresult 
NS_NewLocalFile(const char* path, PRBool followSymlinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    if (path) {
        nsresult rv = file->InitWithPath(path);
        if (NS_FAILED(rv)) {
            NS_RELEASE(file);
            return rv;
        }
    }
    *result = file;
    return NS_OK;
}
