/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Doug Turner <dougt@netscape.com>
 */


#include "nsCOMPtr.h"
#include "nsIAllocator.h"

#include "nsIFileImplWin.h"
#include "nsFileUtils.h"

#include "prtypes.h"
#include "prio.h"

#include <direct.h>
#include "windows.h"

// For older version (<6.0) of the VC Compiler
#if (_MSC_VER == 1100)
#define INITGUID
#include "objbase.h"
DEFINE_OLEGUID(IID_IPersistFile, 0x0000010BL, 0, 0);
#endif

#include "shlobj.h"
#include "shellapi.h"
#include "shlguid.h"


nsIFileImpl::nsIFileImpl()
{
    NS_INIT_REFCNT();
    CoInitialize(NULL);  // FIX: we should probably move somewhere higher up during startup
    makeDirty();
}

nsIFileImpl::~nsIFileImpl()
{
    CoUninitialize();
}

/* nsISupports interface implementation. */
NS_IMPL_ISUPPORTS1(nsIFileImpl, nsIFile)

NS_METHOD
nsIFileImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsIFileImpl* inst = new nsIFileImpl();
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


// This function resets any cached information about the file.
void
nsIFileImpl::makeDirty()
{
    mResolutionDirty = PR_TRUE;
    mStatDirty       = PR_TRUE;
}

//----------------------------------------------------------------------------------------
//
// resolveWorkingPath
//  this function will walk the native path of |this| resolving any symbolic
//  links found.  The new resulting path will be placed into mResolvedPath 
//  and mResolutionDirty will be set to true.
//----------------------------------------------------------------------------------------

nsresult 
nsIFileImpl::resolveWorkingPath()
{
    if (!mResolutionDirty)
        return NS_OK;
    
    nsresult rv = NS_OK;
    HRESULT hres; 
    IShellLink* psl; 
    
    // Get the native path for |this|
    
    char* filePath = (char*) nsAllocator::Clone( mWorkingPath, strlen(mWorkingPath)+1 );

    if (filePath == nsnull)
        return NS_ERROR_NULL_POINTER;

    // Get a pointer to the IShellLink interface. 
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl); 
    if (SUCCEEDED(hres)) 
    { 
        IPersistFile* ppf; 
        
        // Get a pointer to the IPersistFile interface. 
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf); 
        
        if (SUCCEEDED(hres)) 
        {
            char* slash = strchr(filePath, '\\');
            
            if (slash == nsnull)
                return NS_ERROR_NULL_POINTER;

            // skip the first '\\'
            slash = strchr(++slash, '\\');
            
            while (slash)
            {
                *slash = '\0';
                
                WORD wsz[MAX_PATH]; 

                if (strstr(filePath, ".lnk") == 0)
                {
                    char linkStr[MAX_PATH];
                    strcpy(linkStr, filePath);
                    strcat(linkStr, ".lnk");

                    // Ensure that the string is Unicode. 
                    MultiByteToWideChar(CP_ACP, 0, linkStr, -1, wsz, MAX_PATH); 
                }
                else
                {
                    // Ensure that the string is Unicode. 
                    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wsz, MAX_PATH); 
                }
                
                // Load the shortcut. 
                hres = ppf->Load(wsz, STGM_READ); 
                if (SUCCEEDED(hres)) 
                {
                    // Resolve the link. 
                    hres = psl->Resolve(nsnull, SLR_NO_UI ); 
                    if (SUCCEEDED(hres)) 
                    { 
                        char szGotPath[MAX_PATH]; 
                        WIN32_FIND_DATA wfd; 

                        // Get the path to the link target. 
                        hres = psl->GetPath( szGotPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY ); 

                        if (SUCCEEDED(hres))
                        {
                            char *temp = (char*) nsAllocator::Alloc( MAX_PATH );
                            char *carot;

                            strcpy(temp, szGotPath);
                            strcat(temp, "\\");
                            
                            // save where we left off.
                            carot = (temp + strlen(temp) -1 );

                            // append all the stuff that we have not done.
                            strcat(temp, ++slash);
                            
                            nsAllocator::Free(filePath);

                            filePath = temp;
                            slash = carot;
                        }
                    } 
                }
                *slash = '\\';
                ++slash;
                slash = strchr(slash, '\\');
                
                rv = NS_OK;
            }
            
            // Release the pointer to the IPersistFile interface. 
            ppf->Release(); 
        }
        // Release the pointer to the IShellLink interface. 
        psl->Release();
    }
    
    mResolvedPath.SetString(filePath);
    
    if(filePath)
        nsAllocator::Free(filePath);
    
    return rv;
}

nsresult
nsIFileImpl::bufferedStat(struct stat *st)
{
    if (!mStatDirty)
    {
        *st = mBuffered_st;
        return mBuffered_stat_rv;
    }
    
    nsresult rv = resolveWorkingPath();
    
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.GetBuffer();

    mBuffered_stat_rv = stat(filePath, &mBuffered_st);
    *st = mBuffered_st;
    return rv;
}

NS_IMETHODIMP  
nsIFileImpl::InitWithKey(const char *fileKey)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::InitWithFile(nsIFile *file)
{
    NS_ENSURE_ARG(file);

    // TODO:  how do we get to the |file|'s working    XXXXXX
    //        directory so that we do not expose
    //        symlinked directories.

    char* aFilePath;
    file->GetPath(nsIFile::NATIVE_PATH, &aFilePath);

    if (aFilePath == nsnull)
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;

    mWorkingPath.SetString(aFilePath);

    nsAllocator::Free(aFilePath);
    
    makeDirty();

    return NS_OK;
}

NS_IMETHODIMP  
nsIFileImpl::InitWithPath(PRUint32 pathType, const char *filePath)
{
    NS_ENSURE_ARG(filePath);

    makeDirty();

    char* nativeFilePath = nsnull;
    char* temp;

    if (pathType == nsIFile::UNIX_PATH)
    {
        if (*filePath != '/')
	        return NS_ERROR_FILE_INVALID_PATH;

        // Since it was an absolute path, check for the drive letter
        // format looks like:  /d|/
        //                     ^
        //                     |
        //                    filePath
		char* colonPointer = ((char*)(filePath)) + 2;  // this case is okay since I am not changing
		if (strstr(filePath, "|/") == colonPointer)    // filePath
        {
            // allocate new string
	        nativeFilePath = (char*) nsAllocator::Clone( filePath+1, strlen(filePath+1)+1 );
            nativeFilePath[1] = ':';
        }
        else
        {
            // there is not a drive letter specifier.  we need to preserve the first slash.
            nativeFilePath = (char*) nsAllocator::Clone( filePath, strlen(filePath)+1 );
        }
    
        // Convert '/' to '\'.
        temp = nativeFilePath;
        do
        {
            if (*temp == '/')
                *temp = '\\';
        }
        while (*temp++);
    }
    else if (pathType == nsIFile::NATIVE_PATH  || pathType == nsIFile::NSPR_PATH)  //TODO does NSPR take any other kind of path?
    {
        // just do a sanity check.  if it has any forward slashes, it is not a Native path
        // on windows.  Also, it must have a colon at after the first char.

        if ((strchr(filePath, '/') == 0) && filePath[1] == ':')
        {
            // This is a native path
            nativeFilePath = (char*) nsAllocator::Clone( filePath, strlen(filePath)+1 );
        }
    }

    // kill any trailing seperator
    if(nativeFilePath)
    {
        temp = nativeFilePath;
        int len = strlen(temp) - 1;
        if(temp[len] == '\\')
            temp[len] = '\0';
    }

    if (nativeFilePath == nsnull)
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;
    
    mWorkingPath.SetString(nativeFilePath);
    
    nsAllocator::Free( nativeFilePath );

    return NS_OK;
}


NS_IMETHODIMP  
nsIFileImpl::Create(PRUint32 type, PRUint32 attributes)
{
    nsresult rv = resolveWorkingPath();

    if (NS_FAILED(rv))
        return rv;
        
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;


   // create nested directories to target
    char* slash = strchr(mResolvedPath, '\\');
    // skip the first '\\'
    ++slash;
    slash = strchr(slash, '\\');

    while (slash)
    {
        *slash = '\0';

        int result = mkdir(mResolvedPath);  // todo: pass back the result

        *slash = '\\';
        ++slash;
        slash = strchr(slash, '\\');
    }


    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mResolvedPath, PR_RDONLY | PR_CREATE_FILE | PR_APPEND, attributes);
        if (file) PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
	    int result = mkdir(mResolvedPath);  // todo: pass back the result
        return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}
    
NS_IMETHODIMP  
nsIFileImpl::AppendPath(const char *node)
{
    if ( (node == nsnull) || (*node == '/') || strchr(node, '\\') )
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;

    makeDirty();

    // We only can append relative unix styles strings.

    // Convert '\' to '/'
	nsString path(node);

    char* nodeCString = path.ToNewCString();    
    char* temp = nodeCString;
    for (; *temp; temp++)
    {
        if (*temp == '/')
            *temp = '\\';
    }

    // kill any trailing seperator
    if(nodeCString)
    {
        temp = nodeCString;
        int len = strlen(temp) - 1;
        if(temp[len] == '\\')
            temp[len] = '\0';
    }
    
    mWorkingPath.Append("\\");
    mWorkingPath.Append(nodeCString);
    
    Recycle(nodeCString);

    return NS_OK;
}

NS_IMETHODIMP  
nsIFileImpl::GetFileName(char * *aFileName)
{
    NS_ENSURE_ARG_POINTER(aFileName);

    const char* temp = mWorkingPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;

    const char* leaf = strrchr(temp, '\\');
    
    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    *aFileName = (char*) nsAllocator::Clone(leaf, strlen(leaf)+1);
    return NS_OK;
}


NS_IMETHODIMP  
nsIFileImpl::GetPath(PRUint32 pathType, char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;

    nsresult rv = resolveWorkingPath();
    
    if (NS_FAILED(rv)) return rv;

    const char *filePath = mResolvedPath.GetBuffer();
    char* canonicalPath = (char*) nsAllocator::Clone(filePath, strlen(filePath)+1);
    
    if (canonicalPath == nsnull)
        return NS_ERROR_NULL_POINTER;

    if (pathType == nsIFile::UNIX_PATH)
    {
        // Convert the drive-letter separator, if present
	    nsString path;
        path.SetString("/");

	    char* cp = (char*)canonicalPath + 1;
	
        if (strstr(cp, ":\\") == cp)
		    *cp = '|';    // absolute path
        else
            path.SetString("\0"); // relative path
	
	    // Convert '\' to '/'
	    for (; *cp; cp++)
        {
            if (*cp == '\\')
                *cp = '/';
        }

        path.Append(canonicalPath);
        
        cp = path.ToNewCString();
        
        *_retval = (char*) nsAllocator::Clone(cp, strlen(cp)+1);

        delete [] cp;
    
    }
    else if (pathType == nsIFile::NATIVE_PATH  || pathType == nsIFile::NSPR_PATH)  //TODO does NSPR take any other kind of path?
    {
        *_retval = (char*) nsAllocator::Clone(canonicalPath, strlen(canonicalPath)+1);
    }
    
    nsAllocator::Free((char*)canonicalPath);

    if (*_retval == nsnull)
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;

    return NS_OK;
}


nsresult
nsIFileImpl::copymove(nsIFile *newParentDir, const char *newName, PRBool followSymlinks, PRBool move)
{
    NS_ENSURE_ARG(newParentDir);
    
    PRBool exists;
    nsresult rv = IsExists(&exists);
    if (NS_FAILED(rv))
        return rv;

    if (!exists)
        return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;

    const char *filePath = mResolvedPath.GetBuffer();

    newParentDir->IsExists(&exists);
    if (exists == PR_FALSE)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0644);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        PRBool isDir;
        newParentDir->IsDirectory(&isDir);
        if (isDir == PR_FALSE)
            return NS_ERROR_FILE_DESTINATION_NOT_DIR;
    }        

    // get the path that we are going to copy to.
    char* inFilePath;
    newParentDir->GetPath(nsIFile::NATIVE_PATH, &inFilePath);  //todo this needs to return a resolved path?
    nsCString inFileCString = inFilePath;
    nsAllocator::Free(inFilePath);

    inFileCString.Append("\\");

    if (newName == nsnull)
    {
        char *aFileName;
        GetFileName(&aFileName);
        inFileCString.Append(aFileName);
        nsAllocator::Free(aFileName);
    }
    else
    {
        inFileCString.Append(newName);
    }
    
    // check to see if we are a directory, if so enumerate it.
    PRBool isDir;
    IsDirectory(&isDir);
    if (isDir)
    {
        nsCOMPtr<nsIDirectoryEnumerator> iterator;
        NS_NewDirectoryEnumerator(this, followSymlinks, getter_AddRefs(iterator));
            
        PRBool more;
        iterator->HasMoreElements(&more);
        while (more)
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIFile> file;
            iterator->GetNext(getter_AddRefs(item));
            file = do_QueryInterface(item);

            char* filePath;
            file->GetPath(nsIFile::NATIVE_PATH, &filePath);
            
            
            int copyOK;
            
            if (!move)
                copyOK = CopyFile(filePath, inFileCString, PR_TRUE);
            else
                copyOK = MoveFile(filePath, inFileCString);

            nsAllocator::Free(filePath);

            // CopyFile returns non-zero if succeeds
            if (!copyOK)
                return NS_ERROR_FILE_COPY_OR_MOVE_FAILED;;
        
            iterator->HasMoreElements(&more);
        }
    }
    else
    {
        char* filePath;
        newParentDir->GetPath(nsIFile::NATIVE_PATH, &filePath);

        int copyOK;
        
        if (!move)
            copyOK = CopyFile(filePath, inFileCString, PR_TRUE);
        else
            copyOK = MoveFile(filePath, inFileCString);
    
        nsAllocator::Free(filePath);

        // CopyFile returns non-zero if succeeds
        if (!copyOK)
            return NS_ERROR_FILE_COPY_OR_MOVE_FAILED;;
    }

    // If we moved, we want to adjust this.
    if (move)
    {
        if (newName == nsnull)
        {
            char *aFileName;
            GetFileName(&aFileName);
            InitWithFile(newParentDir);
            AppendPath(aFileName); 
            nsAllocator::Free(aFileName);
        }
        else
        {
            InitWithFile(newParentDir);
            AppendPath(newName);
        }
        makeDirty();
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsIFileImpl::CopyTo(nsIFile *newParentDir, const char *newName)
{
    return copymove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP  
nsIFileImpl::CopyToFollowingLinks(nsIFile *newParentDir, const char *newName)
{
    return copymove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP  
nsIFileImpl::MoveTo(nsIFile *newParentDir, const char *newName)
{
    return copymove(newParentDir, newName, PR_FALSE, PR_TRUE);
}

NS_IMETHODIMP  
nsIFileImpl::MoveToFollowingLinks(nsIFile *newParentDir, const char *newName)
{
    return copymove(newParentDir, newName, PR_TRUE, PR_TRUE);
}

NS_IMETHODIMP  
nsIFileImpl::Execute(const char *args)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    nsCString fileNameWithArgs(mResolvedPath);

    if(args)
    {
        fileNameWithArgs.Append(" ");
        fileNameWithArgs.Append(args);
    }

    int execResult = WinExec( fileNameWithArgs, SW_NORMAL );     
    if (execResult > 31)
        return NS_OK;

    return NS_ERROR_FILE_EXECUTION_FAILED;
}

NS_IMETHODIMP  
nsIFileImpl::Delete(PRBool recursive)
{
    makeDirty();
    
    PRBool isDir;
    
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.GetBuffer();

    if (isDir)
    {
        if (recursive)
        {
            nsCOMPtr<nsIDirectoryEnumerator> iterator;
            NS_NewDirectoryEnumerator(this, false, getter_AddRefs(iterator));
            
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
nsIFileImpl::GetLastModificationDate(PRUint32 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::SetLastModificationDate(PRUint32 aLastModificationDate)
{
    makeDirty();
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::GetLastModificationDateOfLink(PRUint32 *aLastModificationDateOfLink)
{
    NS_ENSURE_ARG(aLastModificationDateOfLink);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::SetLastModificationDateOfLink(PRUint32 aLastModificationDateOfLink)
{
    makeDirty();
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::GetPermissions(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG(aPermissions);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    NS_ENSURE_ARG(aPermissionsOfLink);
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP  
nsIFileImpl::GetFileSize(PRUint32 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    struct stat st;
    nsresult rv = bufferedStat( &st );
    
    if (NS_FAILED(rv))
        return rv;
    
    if (0 == mBuffered_stat_rv)
        *aFileSize = (PRUint32)st.st_size;

    return NS_OK;
}

NS_IMETHODIMP  
nsIFileImpl::GetFileSizeOfLink(PRUint32 *aFileSizeOfLink)
{
    /* Link files on windows are not auto resolved. */
    return GetFileSize(aFileSizeOfLink);
}

NS_IMETHODIMP  
nsIFileImpl::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);
    
    nsresult rv = resolveWorkingPath();
    
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.GetBuffer();

    PRInt64 int64;
    
    LL_I2L(int64 , LONG_MAX);

    char aDrive[_MAX_DRIVE + 2];
	_splitpath( (const char*)filePath, aDrive, NULL, NULL, NULL);
	strcat(aDrive, "\\");

    // Check disk space
    DWORD dwSecPerClus, dwBytesPerSec, dwFreeClus, dwTotalClus;
    ULARGE_INTEGER liFreeBytesAvailableToCaller, liTotalNumberOfBytes, liTotalNumberOfFreeBytes;
    double nBytes = 0;

    BOOL (WINAPI* getDiskFreeSpaceExA)(LPCTSTR lpDirectoryName, 
                                       PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                       PULARGE_INTEGER lpTotalNumberOfBytes,    
                                       PULARGE_INTEGER lpTotalNumberOfFreeBytes) = NULL;

    HINSTANCE hInst = LoadLibrary("KERNEL32.DLL");
    NS_ASSERTION(hInst != NULL, "COULD NOT LOAD KERNEL32.DLL");
    if (hInst != NULL)
    {
        getDiskFreeSpaceExA =  (BOOL (WINAPI*)(LPCTSTR lpDirectoryName, 
                                               PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                               PULARGE_INTEGER lpTotalNumberOfBytes,    
                                               PULARGE_INTEGER lpTotalNumberOfFreeBytes)) 
        GetProcAddress(hInst, "GetDiskFreeSpaceExA");
        FreeLibrary(hInst);
    }

    if (getDiskFreeSpaceExA && (*getDiskFreeSpaceExA)(aDrive,
                                                      &liFreeBytesAvailableToCaller, 
                                                      &liTotalNumberOfBytes,  
                                                      &liTotalNumberOfFreeBytes))
    {
        nBytes = (double)(signed __int64)liFreeBytesAvailableToCaller.QuadPart;
    }
    else if ( GetDiskFreeSpace(aDrive, &dwSecPerClus, &dwBytesPerSec, &dwFreeClus, &dwTotalClus))
    {
        nBytes = (double)dwFreeClus*(double)dwSecPerClus*(double) dwBytesPerSec;
    }
    return (PRInt64)nBytes;


}

NS_IMETHODIMP  
nsIFileImpl::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCString parentPath = mWorkingPath;

    PRInt32 offset = parentPath.RFindChar('\\');
    if (offset == -1)
        return NS_ERROR_FILE_UNRECONGNIZED_PATH;

    parentPath.Truncate(offset);

    const char* filePath = parentPath.GetBuffer();

    nsIFileImpl* file = new nsIFileImpl();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    file->AddRef();
    file->InitWithPath(nsIFile::NATIVE_PATH, filePath);
    *aParent = file;

    return NS_OK;
}
NS_IMETHODIMP  
nsIFileImpl::IsExists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    
    struct stat st;
    nsresult rv = bufferedStat( &st );
    
    if (0 == mBuffered_stat_rv)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
    
    return rv;
}

NS_IMETHODIMP  
nsIFileImpl::IsWriteable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP  
nsIFileImpl::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsIFileImpl::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    struct stat st;
    nsresult rv = bufferedStat( &st );

    if (0 == mBuffered_stat_rv && (_S_IFDIR & st.st_mode))
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;

    return rv;
}

NS_IMETHODIMP  
nsIFileImpl::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    struct stat st;
    nsresult rv = bufferedStat( &st );

    if (0 == mBuffered_stat_rv && (_S_IFREG & st.st_mode))
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;

    return rv;
}

NS_IMETHODIMP  
nsIFileImpl::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    nsresult rv = resolveWorkingPath();
    const char* in = mWorkingPath.GetBuffer();

    DWORD attr = GetFileAttributes(in);
    if (FILE_ATTRIBUTE_HIDDEN & attr)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;

    return rv;
}

NS_IMETHODIMP  
nsIFileImpl::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

// just check for the .lnk extension

    *_retval = PR_FALSE;
    if (mWorkingPath.Find(".lnk", PR_FALSE, (mWorkingPath.Length() - 4)))
        *_retval = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsIFileImpl::IsEqual(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);

    *_retval = PR_FALSE;

    char* inFilePath;
    inFile->GetPath(nsIFile::NATIVE_PATH, &inFilePath);
    
    char* filePath;
    GetPath(nsIFile::NATIVE_PATH, &filePath);

    if (strcmp(inFilePath, filePath) == 0)
        *_retval = PR_TRUE;
    
    nsAllocator::Free(inFilePath);
    nsAllocator::Free(filePath);

    return NS_OK;
}

NS_IMETHODIMP
nsIFileImpl::IsContainedIn(nsIFile *inFile, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}