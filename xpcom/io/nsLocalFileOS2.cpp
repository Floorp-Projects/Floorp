/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Henry Sobotka <sobotka@axess.com>
 * Implementation of nsIFile for OS/2. Freely adapted from the Win32 and
 * UNIX versions, as well as the outgoing nsFileSpecOS2.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/23/2000  IBM Corp.       Updated original version, which was prior to nsIFile drop, to the  latest 
 *                                     Win/Unix versions.
 */

#include "nsCOMPtr.h"
#include "nsIAllocator.h"
#include "nsIComponentManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsLocalFileOS2.h"
#include "prtypes.h"
#include "prlong.h"
#include "prmem.h"
#include "prio.h"
#include "prproces.h"
#include <sys/utime.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef XP_OS2_VACPP
#include <direct.h>
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define mkdir(a,b) mkdir(a)
#else
#include <unistd.h>
#endif


#define VALIDATE_STAT_CACHE()         \
  PR_BEGIN_MACRO                               \
  if (!mHaveStatCached) {                      \
      LoadStatCache();                           \
      if (!mHaveStatCached)                    \
         return NSRESULT_FOR_ERRNO();    \
  }                                                      \
  PR_END_MACRO

#define CHECK_mPath()                        \
    PR_BEGIN_MACRO                             \
    if (!mPath)                                      \
        return NS_ERROR_NOT_INITIALIZED; \
    PR_END_MACRO


class nsDirEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator() : mDir(nsnull) 
        {
            NS_INIT_REFCNT();
        }

        nsresult Init(nsILocalFile* parent) 
        {
            char* filepath;
            parent->GetTarget(&filepath);
        
            if (filepath == nsnull)
            {
                parent->GetPath(&filepath);
            }
            
            if (filepath == nsnull)
            {
                return NS_ERROR_OUT_OF_MEMORY;
            }

            mDir = PR_OpenDir(filepath);
            if (mDir == nsnull)    // not a directory?
                return NS_ERROR_FAILURE;

            nsAllocator::Free(filepath);
        
            mParent          = parent;    
            return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
            nsresult rv;
            if (mNext == nsnull && mDir) 
            {
                PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
                if (entry == nsnull) 
                {
                    // end of dir entries

                    PRStatus status = PR_CloseDir(mDir);
                    if (status != PR_SUCCESS)
                        return NS_ERROR_FAILURE;
                    mDir = nsnull;

                    *result = PR_FALSE;
                    return NS_OK;
                }

                nsCOMPtr<nsIFile> file;
                rv = mParent->Clone(getter_AddRefs(file));
                if (NS_FAILED(rv)) 
                    return rv;

                rv = file->Append(entry->name);
                if (NS_FAILED(rv)) 
                    return rv;

                // make sure the thing exists.  If it does, try the next one.
                PRBool exists;
                rv = file->Exists(&exists);
                if (NS_FAILED(rv) || !exists) 
                {
                    return HasMoreElements(result); 
                }
            
                mNext = do_QueryInterface(file);
            }
            *result = mNext != nsnull;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
            nsresult rv;
            PRBool hasMore;
            rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            *result = mNext;        // might return nsnull
            NS_IF_ADDREF(*result);
            
            mNext = null_nsCOMPtr();
            return NS_OK;
        }

        virtual ~nsDirEnumerator() 
        {
            if (mDir) 
            {
                PRStatus status = PR_CloseDir(mDir);
                NS_ASSERTION(status == PR_SUCCESS, "close failed");
            }
        }

    protected:
        PRDir*                  mDir;
        nsCOMPtr<nsILocalFile>  mParent;
        nsCOMPtr<nsILocalFile>  mNext;
};

NS_IMPL_ISUPPORTS(nsDirEnumerator, NS_GET_IID(nsISimpleEnumerator));


// ctor & dtor
// ===========
nsLocalFile::nsLocalFile() :
    mHaveStatCached(PR_FALSE)
{
    mPath = "";
    NS_INIT_REFCNT();
}

nsLocalFile::~nsLocalFile()
{
}


// nsISupports interface implementation
// ====================================
NS_IMPL_THREADSAFE_ISUPPORTS2(nsLocalFile, nsILocalFile, nsIFile)

NS_METHOD
nsLocalFile::nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_NO_AGGREGATION(outer);

    nsLocalFile* inst = new nsLocalFile();
    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv))
    {
        delete inst;
        return rv;
    }
    return NS_OK;
}

// Interface methods implementation
// ================================
NS_IMETHODIMP
nsLocalFile::Clone(nsIFile **file)
{
    nsresult rv;
    char * aFilePath;
    GetPath(&aFilePath);

    nsCOMPtr<nsILocalFile> localFile;

    rv = NS_NewLocalFile(aFilePath, getter_AddRefs(localFile));
    nsAllocator::Free(aFilePath);
    
    if (NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)file);
    }
            
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::InitWithPath(const char *filePath)
{
    SetNoStatCache();
    NS_ENSURE_ARG(filePath);
    nsresult rv = NS_OK;
    char* pathString = nsnull;
    char* nativeFilePath = nsnull;

 //  Do a sanity check.  if it has any forward slashes, it is not a Native path
 //  Also, it can only  have a colon at the first char.

    if ( ( (filePath[0] != 0) && (filePath[1] == ':') && (strchr(filePath, '/') == 0) ) ||  
         ( (filePath[0] == '\\') && (filePath[1] == '\\') ) )  // netwerk path
    {
        // This is a native path
        if(filePath[1] == ':')
        {
           rv = CheckDrive(filePath);
           if (NS_FAILED(rv)) 
              return NS_ERROR_FILE_INVALID_PATH; 
        }
        nativeFilePath = (char*) nsAllocator::Clone( filePath, strlen(filePath)+1 );
    }
    
    if (nativeFilePath == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // kill any trailing seperator
    char* temp = nativeFilePath;
    int len = strlen(temp) - 1;
    if(temp[len] == '\\')
        temp[len] = '\0';

    mPath.SetString(nativeFilePath);
    nsAllocator::Free( nativeFilePath );
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    CHECK_mPath();

    *_retval = PR_Open(mPath, flags, mode);
    
    if (*_retval)
        return NS_OK;
    else
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

NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    CHECK_mPath();

    // Return if file exists
    if (access(mPath, F_OK) == 0)
        return NS_ERROR_FILE_ALREADY_EXISTS;

    // Check type
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    char* segment = (char*)PR_Malloc(strlen(mPath));
    char* tail = strchr(mPath, '\\');
    char* leaf = strrchr(mPath, '\\');
    size_t len = strlen(mPath);

#if DEBUG_sobotka
    printf("In nsLocalFile::Create: mPath = %s\n", mPath);
    printf("tail = %s\n", tail);
    printf("leaf = %s\n", leaf);
#endif

    // Walk path if not a leaf
    if (stricmp(tail, leaf) || tail != nsnull) {

        // Absolute path
        if (mPath[2] == '\\' && mPath[1] == ':') {
            ++tail;
            tail = strchr(tail, '\\');
        }

        // LAN path
        if (mPath[0] == '\\' && mPath[1] == '\\') {
            tail += 2;
            tail = strchr(tail, '\\');
        }

        if (tail != NULL) {
            size_t idx = len - strlen(tail);

            while (stricmp(segment, mPath)) {
                memcpy(segment, mPath, idx);
                if (access(segment, F_OK) != 0)
                    mkdir(segment, 0);
                
                ++tail;
                tail = strchr(tail, '\\');

                if (tail == NULL)
                    break;
                else
                    idx = len - strlen(tail);
            }
        }
    }

    // Now just create the directory or file
    int ret;
    if (type == DIRECTORY_TYPE) {
        ret = mkdir(mPath, attributes);
        if (!ret)
            return NS_OK;
        else
            return NSRESULT_FOR_RETURN(ret);
    }
    else {
        ret = creat(mPath, attributes);
        if (ret != -1) {
            close(ret);
            return NS_OK;
        }
        else
            return NSRESULT_FOR_RETURN(ret);
    }
}
    
NS_IMETHODIMP  
nsLocalFile::Append(const char *node)
{
    if ( (node == nsnull)           || 
         (*node == '/')             || 
         (strstr(node, "..") != nsnull) ||
         (strchr(node, '\\') != nsnull) ||
         (strchr(node, '/')  != nsnull) )
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    mPath.Append("\\");
    mPath.Append(node);
    return NS_OK;
#if 0
    NS_ENSURE_ARG(fragment);
    CHECK_mPath();
    char * newPath = (char *)nsAllocator::Alloc(strlen(mPath) +
                                                strlen(fragment) + 2);
    if (!newPath)
        return NS_ERROR_OUT_OF_MEMORY;
    strcpy(newPath, mPath);
    strcat(newPath, "\\");

    // flip slashes
    char *c;
    char *temp =  (char *)nsAllocator::Alloc(strlen(fragment) + 1);
    strcpy(temp, fragment);
    for(c = temp; *c; c++)
        if( *c == '/') *c = '\\';

    strcat(newPath, temp);
    mPath = newPath;
    nsAllocator::Free(newPath);
    return NS_OK;
#endif
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLeafName(char **aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);

    const char* temp = mPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = strrchr(temp, '\\');
    
    // if mPath is just a node without any slashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    *aLeafName = (char*)nsAllocator::Clone(leaf, strlen(leaf) + 1);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLeafName(const char * aLeafName)
{
    const char* temp = mPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = strrchr(temp, '\\');
    
    PRInt32 offset = mPath.RFindChar('\\');
    if (offset)
    {
        mPath.Truncate(offset+1);
    }
    mPath.Append(aLeafName);

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPath(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = (char*)nsAllocator::Clone(mPath, strlen(mPath) + 1);
    return NS_OK;
}

nsresult
nsLocalFile::CopyMove(nsIFile *newParentDir, const char *newName, PRBool move)
{
    CHECK_mPath();
    NS_ENSURE_ARG(newParentDir);

    int rv;
    PRBool exists;
    newParentDir->Exists(&exists);
    if (exists == PR_FALSE) {
        rv = newParentDir->Create(DIRECTORY_TYPE,  S_IREAD | S_IWRITE);
        if (NS_FAILED(rv))
            return rv;
    }

    nsCString newPath;
    newParentDir->GetPath((char**)&newPath);
    if (newPath[strlen(newPath - 1)] != '\\')
        newPath.Append("\\");
    newPath.Append(newName);

    rv = DosCopy((PSZ)mPath, (PSZ)newPath, DCPY_EXISTING);
    if (NS_FAILED(rv))
        return rv;

    if(move) {
        rv = remove(mPath);
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::CopyTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::MoveTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE);
}

NS_IMETHODIMP
nsLocalFile::Spawn(const char **args, PRUint32 count)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    // make sure that when we allocate we have 1 greater than the
    // count since we need to null terminate the list for the argv to
    // pass into PR_CreateProcessDetached
    char **my_argv = NULL;
    
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
    my_argv[0] = mPath;
    
    // null terminate the array
    my_argv[count+1] = NULL;
    rv = PR_CreateProcessDetached(mPath, my_argv, NULL, NULL);

     // free up our argv
     nsAllocator::Free(my_argv);

     if (PR_SUCCESS == rv)
         return NS_OK;
     else
         return NS_ERROR_FILE_EXECUTION_FAILED;
}

NS_IMETHODIMP  
nsLocalFile::Load(PRLibrary **_retval)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;
    
    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

    *_retval =  PR_LoadLibrary(mPath);
    
    if (*_retval)
        return NS_OK;
    else
        return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP  
nsLocalFile::Delete(PRBool recursive)
{
    PRBool isDir;
    
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mPath.GetBuffer();

    if (isDir)
    {
        if (recursive)
        {
            nsDirEnumerator* dirEnum = new nsDirEnumerator();
            if (dirEnum)
                return NS_ERROR_OUT_OF_MEMORY;
        
            rv = dirEnum->Init(this);

            nsCOMPtr<nsISimpleEnumerator> iterator = do_QueryInterface(dirEnum);
        
            PRBool more;
            iterator->HasMoreElements(&more);
            while (more)
            {
                nsCOMPtr<nsISupports> item;
                nsCOMPtr<nsIFile> file;
                iterator->GetNext(getter_AddRefs(item));
                file = do_QueryInterface(item);
    
                file->Delete(recursive);
                
                iterator->HasMoreElements(&more);
            }
        }
        rmdir(filePath);  // todo: save return value?
    }
    else
    {
        remove(filePath); // todo: save return value?
    }
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDate(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG_POINTER(aLastModificationDate);
    CHECK_mPath();
    LL_UI2L(*aLastModificationDate, (PRUint32)mStatCache.st_mtime);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRInt64 aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    int result;
    if (aLastModificationDate) {
        VALIDATE_STAT_CACHE();
        struct utimbuf ut;
        ut.actime = mStatCache.st_atime;
        LL_L2UI(ut.modtime, aLastModificationDate);
        result = utime((const char*)mPath, &ut);
    } else {
        result = utime((const char*)mPath, NULL);
    }
    SetNoStatCache();
    return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRInt64 *aLastModificationDate)
{
    return (GetLastModificationDate(aLastModificationDate));
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRInt64 aLastModificationDate)
{
    return (SetLastModificationDate(aLastModificationDate));
}

/* Permission flags for st_mode; emx's <sys/stat.h> already defines them.
 * Though these permissions are meaningless on OS/2, in a network context
 * a file can end up a system where these bits do serve a purpose. Since
 * setting and getting is easy, might as well provide the facility.
 */
#ifdef XP_OS2_VACPP
#define S_IRWXU 00700
#define S_IRWXG 00070
#define S_IRWXO 00007
#endif

#define NORMALIZE_PERMS(mode)  ((mode)& (S_IRWXU | S_IRWXG | S_IRWXO))

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG_POINTER(aPermissions);
    VALIDATE_STAT_CACHE();
    *aPermissions = NORMALIZE_PERMS(mStatCache.st_mode);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return (GetPermissions(aPermissionsOfLink));
}


NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    NS_ENSURE_ARG(aPermissions);
    SetNoStatCache();
    return NSRESULT_FOR_RETURN(chmod(mPath, aPermissions));
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return (SetPermissions(aPermissions));
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG_POINTER(aFileSize);
    VALIDATE_STAT_CACHE();
    *aFileSize = (PRUint32)mStatCache.st_size;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG_POINTER(aFileSize);
    return (GetFileSize(aFileSize));
}

NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    NS_ENSURE_ARG_POINTER(aFileSize);
    CHECK_mPath();

    int fh = open(mPath, O_RDWR);
    if (fh == -1)
        return NSRESULT_FOR_ERRNO();
   
    PRInt32 newSize;
    LL_L2I(newSize, aFileSize);

    nsresult rv = chsize(fh, newSize);
    if (NS_FAILED(rv)) 
        return rv;
    else
        close(fh);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG_POINTER(aDiskSpaceAvailable);
    CHECK_mPath();

    ULONG ulDriveNo = toupper(mPath[0]) + 1 - 'A';
    FSALLOCATE fsAllocate = { 0 };
    APIRET rv = DosQueryFSInfo(ulDriveNo, FSIL_ALLOC,
                               &fsAllocate, sizeof(fsAllocate));

    if (NS_FAILED(rv)) 
        return rv;
    else
        *aDiskSpaceAvailable = (PRInt64)(fsAllocate.cUnitAvail  *
                                         fsAllocate.cSectorUnit *
                                         fsAllocate.cbSector);

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCString parentPath = mPath;

    PRInt32 offset = parentPath.RFindChar('\\');
    if (offset == -1)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    parentPath.Truncate(offset);

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv =  NS_NewLocalFile(parentPath.GetBuffer(), getter_AddRefs(localFile));
    
    if(NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aParent);
    }
    return rv;
}

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
    NS_ENSURE_ARG(_retval);
    CHECK_mPath();

    char *ext = strrchr(mPath, '.');

    if (!(stricmp(ext, ".exe")) ||
        !(stricmp(ext, ".cmd")) ||
        !(stricmp(ext, ".com")) ||
        !(stricmp(ext, ".bat")))
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = S_ISDIR(mStatCache.st_mode);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = S_ISREG(mStatCache.st_mode);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsHidden(PRBool *_retval)
{
    CHECK_mPath();
    NS_ENSURE_ARG(_retval);

    HFILE fh = 0;
    ULONG openAct = 0;
    FILESTATUS3 fs3 = {{0}};

    APIRET rv = DosOpen((PSZ)mPath, &fh, &openAct, 0L, 0L,
                        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                        OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READONLY | OPEN_SHARE_DENYWRITE,
                        0L);
    if (NS_FAILED(rv)) 
        return rv;
    else
        rv = DosQueryPathInfo(mPath, FIL_STANDARD, &fs3, sizeof(fs3));

    if (NS_FAILED(rv)) 
        return rv;
    else
        *_retval = fs3.attrFile & FILE_HIDDEN ? PR_TRUE : PR_FALSE;

    rv = DosClose(fh);

    if (NS_FAILED(rv)) 
        return rv;
    else
        return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_FALSE;             // No symlinks on OS/2
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = !(S_ISREG(mStatCache.st_mode) || S_ISDIR(mStatCache.st_mode));
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(inFile);
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_FALSE;

    char* inFilePath;
    inFile->GetPath(&inFilePath);
    
    char* filePath;
    GetPath(&filePath);

    if (strcmp(inFilePath, filePath) == 0)
        *_retval = PR_TRUE;
    
    nsAllocator::Free(inFilePath);
    nsAllocator::Free(filePath);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    *_retval = PR_FALSE;
       
    char* myFilePath;
    if ( NS_FAILED(GetTarget(&myFilePath)))
        GetPath(&myFilePath);
    
    PRInt32 myFilePathLen = strlen(myFilePath);
    
    char* inFilePath;
    if ( NS_FAILED(inFile->GetTarget(&inFilePath)))
        inFile->GetPath(&inFilePath);

    if ( strncmp( myFilePath, inFilePath, myFilePathLen) == 0)
    {
        // now make sure that the |inFile|'s path has a trailing
        // separator.

        if (inFilePath[myFilePathLen] == '\\')
        {
            *_retval = PR_TRUE;
        }

    }
        
    nsAllocator::Free(inFilePath);
    nsAllocator::Free(myFilePath);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{   
    NS_ENSURE_ARG_POINTER(_retval);
    VALIDATE_STAT_CACHE();
    *_retval = (char*)nsAllocator::Clone(mPath, strlen(mPath) + 1);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator **entries)
{
    NS_ENSURE_ARG_POINTER(entries);
    PRBool isDir;

    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) 
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(dirEnum);
        return rv;
    }
    
    *entries = dirEnum;
    return NS_OK;
}

nsresult 
NS_NewLocalFile(const char* path, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    nsresult rv = file->InitWithPath(path);
    if (NS_FAILED(rv)) {
        NS_RELEASE(file);
        return rv;
    }
    *result = file;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CheckDrive(const char* inPath) {
    
    ULONG ulDriveNo = 0;
    ULONG ulDriveMap = 0;
    APIRET rv = DosQueryCurrentDisk(&ulDriveNo, &ulDriveMap);
    if (NS_FAILED(rv)) 
        return NS_ERROR_FAILURE;

    // See if specified drive exists
    ulDriveNo = toupper(inPath[0]) + 1 - 'A';
    if (!((ulDriveMap << (31 - ulDriveNo)) >> 31))
        return NS_ERROR_FILE_INVALID_PATH;

    return NS_OK;
}
