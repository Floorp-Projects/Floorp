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
 *     IBM Corp.
 */


#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFileOS2.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include "nsEscape.h"

#include <ctype.h>    // needed for toupper
#include <string.h>

#include "nsXPIDLString.h"
#include "prproces.h"


static unsigned char* PR_CALLBACK
_mbschr( const unsigned char* stringToSearch, int charToSearchFor);
extern unsigned char*
_mbsrchr( const unsigned char* stringToSearch, int charToSearchFor);
static nsresult PR_CALLBACK
CreateDirectoryA( PSZ resolvedPath, PEAOP2 ppEABuf);
static int isleadbyte(int c);

#ifdef XP_OS2_VACPP
#include <direct.h>
#include "dirent.h"
#else
#include <unistd.h>
#endif
#include <io.h>


#ifdef XP_OS2
static nsresult ConvertOS2Error(int err)
#else
static nsresult ConvertWinError(DWORD winErr)
#endif
{
    nsresult rv;
    
    switch (err)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            rv = NS_ERROR_FILE_NOT_FOUND;
            break;
        case ERROR_ACCESS_DENIED:
        case ERROR_NOT_SAME_DEVICE:
            rv = NS_ERROR_FILE_ACCESS_DENIED;
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_INVALID_BLOCK:
        case ERROR_INVALID_HANDLE:
        case ERROR_ARENA_TRASHED:
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
        case ERROR_CURRENT_DIRECTORY:
            rv = NS_ERROR_FILE_DIR_NOT_EMPTY;
            break;
        case ERROR_WRITE_PROTECT:
            rv = NS_ERROR_FILE_READ_ONLY;
            break;
        case ERROR_HANDLE_DISK_FULL:
            rv = NS_ERROR_FILE_TOO_BIG;
            break;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
        case ERROR_CANNOT_MAKE:
            rv = NS_ERROR_FILE_ALREADY_EXISTS;
            break;
        case 0:
            rv = NS_OK;
        default:    
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}


static void
myLL_II2L(PRInt32 hi, PRInt32 lo, PRInt64 *result)
{
    PRInt64 a64, b64;  // probably could have been done with 
                       // only one PRInt64, but these are macros, 
                       // and I am a wimp.

    // put hi in the low bits of a64.
    LL_I2L(a64, hi);
    // now shift it to the upperbit and place it the result in result
    LL_SHL(b64, a64, 32);
    // now put the low bits on by adding them to the result.
    LL_ADD(*result, b64, lo);
}


static void
myLL_L2II(PRInt64 result, PRInt32 *hi, PRInt32 *lo )
{
    PRInt64 a64, b64;  // probably could have been done with 
                       // only one PRInt64, but these are macros, 
                       // and I am a wimp.
    
    // shift the hi word to the low word, then push it into a long.
    LL_SHR(a64, result, 32);
    LL_L2I(*hi, a64);

    // shift the low word to the hi word first, then shift it back.
    LL_SHL(b64, result, 32);
    LL_SHR(a64, b64, 32);
    LL_L2I(*lo, a64);
}


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
			
			nsMemory::Free(filepath);

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
            
            mNext = nsnull;
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

NS_IMPL_ISUPPORTS1(nsDirEnumerator, nsISimpleEnumerator)


nsLocalFile::nsLocalFile()
{
    NS_INIT_REFCNT();
      
#ifndef XP_OS2  
    mPersistFile = nsnull;
    mShellLink   = nsnull;
#endif
    mLastResolution = PR_FALSE;
    mFollowSymlinks = PR_FALSE;
    MakeDirty();
}

nsLocalFile::~nsLocalFile()
{
#ifndef XP_OS2  
    PRBool uninitCOM = PR_FALSE;
    if (mPersistFile || mShellLink)
    {
        uninitCOM = PR_TRUE;
    }
    // Release the pointer to the IPersistFile interface. 
    if (mPersistFile)
        mPersistFile->Release(); 
    
    // Release the pointer to the IShellLink interface. 
    if(mShellLink)
        mShellLink->Release();

    if (uninitCOM)
        CoUninitialize();
#endif
}

/* nsISupports interface implementation. */
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

// This function resets any cached information about the file.
void
nsLocalFile::MakeDirty()
{
    mDirty       = PR_TRUE;
}


//----------------------------------------------------------------------------------------
//
// ResolvePath
//  this function will walk the native path of |this| resolving any symbolic
//  links found.  The new resulting path will be placed into mResolvedPath.
//----------------------------------------------------------------------------------------

nsresult 
nsLocalFile::ResolvePath(const char* workingPath, PRBool resolveTerminal, char** resolvedPath)
{
    nsresult rv = NS_OK;
    
#ifndef XP_OS2

    if (strstr(workingPath, ".lnk") == nsnull)
        return NS_ERROR_FILE_INVALID_PATH;

    if (mPersistFile == nsnull || mShellLink == nsnull)
    {
        CoInitialize(NULL);  // FIX: we should probably move somewhere higher up during startup

        HRESULT hres; 

        // FIX.  This should be in a service.
        // Get a pointer to the IShellLink interface. 
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&mShellLink); 
        if (SUCCEEDED(hres)) 
        { 
            // Get a pointer to the IPersistFile interface. 
            hres = mShellLink->QueryInterface(IID_IPersistFile, (void**)&mPersistFile); 
        }
        
        if (mPersistFile == nsnull || mShellLink == nsnull)
        {
            return NS_ERROR_FILE_INVALID_PATH;
        }
    }
#endif
        
    
    // Get the native path for |this|
    char* filePath = (char*) nsMemory::Clone( workingPath, strlen(workingPath)+1 );

    if (filePath == nsnull)
        return NS_ERROR_NULL_POINTER;
    
    // We are going to walk the native file path
    // and stop at each slash.  For each partial
    // path (the string to the left of the slash)
    // we will check to see if it is a shortcut.
    // if it is, we will resolve it and continue
    // with that resolved path.
    
    // Get the first slash.          
    char* slash = strchr(filePath, '\\');
            
    if (slash == nsnull)
    {
        if (filePath[0] != nsnull && filePath[1] == ':' && filePath[2] == '\0')
        {
            // we have a drive letter and a colon (eg 'c:'
            // this is resolve already
            
            int filePathLen = strlen(filePath);
            char* rp = (char*) nsMemory::Alloc( filePathLen + 2 );
            if (!rp)
                return NS_ERROR_OUT_OF_MEMORY;
            memcpy( rp, filePath, filePathLen );
            rp[filePathLen] = '\\';
            rp[filePathLen+1] = 0;
            *resolvedPath = rp;

            nsMemory::Free(filePath);
            return NS_OK;
        }
        else
        {
            nsMemory::Free(filePath);
            return NS_ERROR_FILE_INVALID_PATH;
        }
    }
        

    // We really cant have just a drive letter as
    // a shortcut, so we will skip the first '\\'
    slash = strchr(++slash, '\\');
    
    while (slash || resolveTerminal)
    {
        // Change the slash into a null so that
        // we can use the partial path. It is is 
        // null, we know it is the terminal node.
        
        if (slash)
        {
            *slash = '\0';
        }
        else
        {
            if (resolveTerminal)
            {
                // this is our last time in this loop.
                // set loop condition to false

                resolveTerminal = PR_FALSE;
            }
            else
            {
                // something is wrong.  we should not have
                // both slash being null and resolveTerminal 
                // not set!
                nsMemory::Free(filePath);
                return NS_ERROR_NULL_POINTER;
            }
        }
#ifndef XP_OS2
      
        WORD wsz[MAX_PATH];     // TODO, Make this dynamically allocated.

        // check to see the file is a shortcut by the magic .lnk extension.
        size_t offset = strlen(filePath) - 4;
        if ((offset > 0) && (strncmp( (filePath + offset), ".lnk", 4) == 0))
        {
            MultiByteToWideChar(CP_ACP, 0, filePath, -1, wsz, MAX_PATH); 
        }        
        else
        {
            char linkStr[MAX_PATH];
            strcpy(linkStr, filePath);
            strcat(linkStr, ".lnk");

            // Ensure that the string is Unicode. 
            MultiByteToWideChar(CP_ACP, 0, linkStr, -1, wsz, MAX_PATH); 
        }        

        HRESULT hres; 
        
        // see if we can Load the path.
        hres = mPersistFile->Load(wsz, STGM_READ); 

        if (SUCCEEDED(hres)) 
        {
            // Resolve the link. 
            hres = mShellLink->Resolve(nsnull, SLR_NO_UI ); 
            if (SUCCEEDED(hres)) 
            { 
                WIN32_FIND_DATA wfd; 
                
                char *temp = (char*) nsMemory::Alloc( MAX_PATH );
                if (temp == nsnull)
                    return NS_ERROR_NULL_POINTER;
                
                // Get the path to the link target. 
                hres = mShellLink->GetPath( temp, MAX_PATH, &wfd, SLGP_UNCPRIORITY ); 

                if (SUCCEEDED(hres))
                {
                    // found a new path.
                    
                    // addend a slash on it since it does not come out of GetPath()
                    // with one only if it is a directory.  If it is not a directory
                    // and there is more to append, than we have a problem.
                    
                    struct stat st;
                    int statrv = stat(temp, &st);
                    
                    if (0 == statrv && (_S_IFDIR & st.st_mode))
                    {
                        strcat(temp, "\\");
                    }
                                       
                    if (slash)
                    {
                        // save where we left off.
                        char *carot= (temp + strlen(temp) -1 );

                        // append all the stuff that we have not done.
                        strcat(temp, ++slash);
                        
                        slash = carot;
                    }

                    nsMemory::Free(filePath);
                    filePath = temp;
            
                }
                else
                {
                    nsMemory::Free(temp);
                }
            }
            else
            {
                // could not resolve shortcut.  Return error;
                nsMemory::Free(filePath);
                return NS_ERROR_FILE_INVALID_PATH;
            }
        }
#endif
    
        if (slash)
        {
            *slash = '\\';
            ++slash;
            slash = strchr(slash, '\\');
        }
    }

    // kill any trailing seperator
    char* temp = filePath;
    int len = strlen(temp) - 1;
    if(temp[len] == '\\')
        temp[len] = '\0';
    
    *resolvedPath = filePath;
    return rv;
}

nsresult
nsLocalFile::ResolveAndStat(PRBool resolveTerminal)
{
    if (!mDirty && mLastResolution == resolveTerminal)
    {
        return NS_OK;
    }
    mLastResolution = resolveTerminal;
    mResolvedPath.Assign(mWorkingPath);  //until we know better.

    // First we will see if the workingPath exists.  If it does, then we
    // can simply use that as the resolved path.  This simplification can
    // be done on windows cause its symlinks (shortcuts) use the .lnk
    // file extension.

    char temp[4];
    const char* workingFilePath = mWorkingPath.get();
    const char* nsprPath = workingFilePath;

    if (mWorkingPath.Length() == 2 && mWorkingPath.CharAt(1) == ':') {
        temp[0] = workingFilePath[0];
        temp[1] = workingFilePath[1];
        temp[2] = '\\';
        temp[3] = '\0';
        nsprPath = temp;
    }

    PRStatus status = PR_GetFileInfo64(nsprPath, &mFileInfo64);
    if ( status == PR_SUCCESS )
    {
        if (!resolveTerminal)
        {
		    mDirty = PR_FALSE;
            return NS_OK;
        }
       
        // check to see that this is shortcut, i.e., the leaf is ".lnk"
        // if the length < 4, then it's not a link.
            
        int pathLen = strlen(workingFilePath);
        const char* leaf = workingFilePath + pathLen - 4;
    
#ifdef XP_OS2
        if (pathLen < 4)
#else
        if (pathLen < 4 || (strcmp(leaf, ".lnk") != 0))
#endif
        {
            mDirty = PR_FALSE;
            return NS_OK;
        }
    }

#ifndef XP_OS2
    if (!mFollowSymlinks)
        return NS_ERROR_FILE_NOT_FOUND;  // if we are not resolving, we just give up here.
#endif

    nsresult result;

    // okay, something is wrong with the working path.  We will try to resolve it.

    char *resolvePath;
    
	result = ResolvePath(workingFilePath, resolveTerminal, &resolvePath);
    if (NS_FAILED(result))
       return NS_ERROR_FILE_NOT_FOUND;
    
	mResolvedPath.Assign(resolvePath);
    nsMemory::Free(resolvePath);

    status = PR_GetFileInfo64(mResolvedPath, &mFileInfo64);
    
    if ( status == PR_SUCCESS )
		mDirty = PR_FALSE;
    else
        result = NS_ERROR_FILE_NOT_FOUND;

	return result;
}

NS_IMETHODIMP  
nsLocalFile::Clone(nsIFile **file)
{
    nsresult rv;
    char * aFilePath;
    GetPath(&aFilePath);

    nsCOMPtr<nsILocalFile> localFile;

    rv = NS_NewLocalFile(aFilePath, mFollowSymlinks, getter_AddRefs(localFile));
    nsMemory::Free(aFilePath);
    
    if (NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)file);
    }
            
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::InitWithPath(const char *filePath)
{
    MakeDirty();
    NS_ENSURE_ARG(filePath);
    
    char* nativeFilePath = nsnull;
    
    // just do a sanity check.  if it has any forward slashes, it is not a Native path
    // on windows.  Also, it must have a colon at after the first char.
    if (filePath[0] == 0)
        return NS_ERROR_FAILURE;
    
    if ( (filePath[2] == 0) && (filePath[1] == ':') )
    {
        // C : //
        nativeFilePath = (char*) nsMemory::Alloc( 4 );
        if (!nativeFilePath)
            return NS_ERROR_OUT_OF_MEMORY;
        nativeFilePath[0] = filePath[0];
        nativeFilePath[1] = ':';
        nativeFilePath[2] = '\\';
        nativeFilePath[3] = 0;
    }
    // XXX is this an 'else'? Otherwise 'nativeFilePath' could leak.
    if ( ( (filePath[1] == ':')  && (strchr(filePath, '/') == 0) ) ||  // normal windows path
         ( (filePath[0] == '\\') && (filePath[1] == '\\') ) )  // netwerk path
    {
        // This is a native path
        if (nativeFilePath) nsCRT::free(nativeFilePath);
        nativeFilePath = (char*) nsMemory::Clone( filePath, strlen(filePath)+1 );
    }
    
    if (nativeFilePath == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // kill any trailing seperator
    char* temp = nativeFilePath;
    int len = strlen(temp) - 1;
    // Is '\' second charactor of DBCS?
#ifdef XP_OS2
    if(temp[len] == '\\' && !::isleadbyte(temp[len-1]))
#else
    if(temp[len] == '\\' && !::IsDBCSLeadByte(temp[len-1]))
#endif
        temp[len] = '\0';
    
    mWorkingPath.Assign(nativeFilePath);
    nsMemory::Free( nativeFilePath );

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = PR_Open(mResolvedPath, flags, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ErrorAccordingToNSPR();
}


NS_IMETHODIMP  
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = fopen(mResolvedPath, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}



NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = ResolveAndStat(PR_FALSE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;  
    
   // create nested directories to target
    unsigned char* slash = _mbschr((const unsigned char*) mResolvedPath.get(), '\\');
    // skip the first '\\'
    ++slash;
    slash = _mbschr(slash, '\\');

    while (slash)
    {
        *slash = '\0';

#ifdef XP_OS2
        rv = CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL);
        if (rv) {
            rv = ConvertOS2Error(rv);
#else
        if (!CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL)) {
            rv = ConvertWinError(GetLastError());
#endif
            if (rv != NS_ERROR_FILE_ALREADY_EXISTS) return rv;
        }
        *slash = '\\';
        ++slash;
        slash = _mbschr(slash, '\\');
    }


    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mResolvedPath, PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;
          
        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
#ifdef XP_OS2
        rv = CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL);
        if (rv) 
            return ConvertOS2Error(rv);
#else
        if (!CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL))
            return ConvertWinError(GetLastError());
#endif
        else 
            return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}
    
NS_IMETHODIMP  
nsLocalFile::Append(const char *node)
{
    // Append only one component. Check for subdirs.
    if (!node || (_mbschr((const unsigned char*) node, '\\') != nsnull))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    return AppendRelativePath(node);
}

NS_IMETHODIMP  
nsLocalFile::AppendRelativePath(const char *node)
{
    // Cannot start with a / or have .. or have / anywhere
    if (!node || strchr(node, '/'))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    MakeDirty();
    mWorkingPath.Append("\\");
    mWorkingPath.Append(node);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLeafName(char * *aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);

    const char* temp = mWorkingPath.get();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');
    
    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    *aLeafName = (char*) nsMemory::Clone(leaf, strlen(leaf)+1);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLeafName(const char * aLeafName)
{
    MakeDirty();
    
    const unsigned char* temp = (const unsigned char*) mWorkingPath.get();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr(temp, '\\') - temp);
    if (offset)
    {
        mWorkingPath.Truncate(offset+1);
    }
    mWorkingPath.Append(aLeafName);

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetPath(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = (char*) nsMemory::Clone(mWorkingPath.get(), strlen(mWorkingPath)+1);
    return NS_OK;
}

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent, const char * newName, PRBool followSymlinks, PRBool move)
{
    nsresult rv;
    char* filePath;

    // get the path that we are going to copy to.
    // Since windows does not know how to auto
    // resolve shortcust, we must work with the
    // target.
    char* inFilePath;
    destParent->GetTarget(&inFilePath);  
    nsCString destPath(inFilePath);
    nsMemory::Free(inFilePath);

    destPath.Append("\\");

    if (newName == nsnull)
    {
        char *aFileName;
        sourceFile->GetLeafName(&aFileName);
        destPath.Append(aFileName);
        nsMemory::Free(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }

           
    if (followSymlinks)
    {
        rv = sourceFile->GetTarget(&filePath);
        if (!filePath)
            rv = sourceFile->GetPath(&filePath);
    }
    else
    {
        rv = sourceFile->GetPath(&filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    APIRET rc = NO_ERROR;

    if( move )
    {
        rc = DosMove(filePath, (PSZ)NS_CONST_CAST(char*, destPath.get()));
    }

    if (!move || rc == ERROR_NOT_SAME_DEVICE) {
        /* will get an error if the destination and source files aren't on the
         * same drive.  "MoveFile()" on Windows will go ahead and move the
         * file without error, so we need to do the same   IBM-AKR
         */
        rc = DosCopy(filePath, (PSZ)NS_CONST_CAST(char*, destPath.get()), DCPY_EXISTING);
        /* WSOD2 HACK */
        if (rc == 65) { // NETWORK_ACCESS_DENIED
          CHAR         achProgram[CCHMAXPATH];  // buffer for program name, parameters
          RESULTCODES  rescResults;             // buffer for results of dosexecpgm

          strcpy(achProgram, "CMD.EXE  /C ");
          strcat(achProgram, """COPY ");
          strcat(achProgram, filePath);
          strcat(achProgram, " ");
          strcat(achProgram, (PSZ)NS_CONST_CAST(char*, destPath.get()));
          strcat(achProgram, """");
          achProgram[strlen(achProgram) + 1] = '\0';
          achProgram[7] = '\0';
          DosExecPgm(NULL, 0,
                     EXEC_SYNC, achProgram, (PSZ)NULL,
                     &rescResults, achProgram);
          rc = 0; // Assume it worked

        } /* rc == 65 */
        /* moving the file is supposed to act like a rename, so delete the
         * original file if we got this far without error
         */
        if( move && (rc == NO_ERROR) )
        {
          DosDelete( filePath );
        }
    } /* !move or ERROR */
    
    if (rc)
        rv = ConvertOS2Error(rc);
    
    nsMemory::Free(filePath);

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const char *newName, PRBool followSymlinks, PRBool move)
{
    nsCOMPtr<nsIFile> newParentDir = aParentDir;
    // check to see if this exists, otherwise return an error.
    // we will check this by resolving.  If the user wants us
    // to follow links, then we are talking about the target,
    // hence we can use the |followSymlinks| parameter.
    nsresult rv  = ResolveAndStat(followSymlinks);
    if (NS_FAILED(rv))
        return rv;

    if (!newParentDir)
    {
        // no parent was specified.  We must rename.
        
        if (!newName)
            return NS_ERROR_INVALID_ARG;

        move = PR_TRUE;
        
        rv = GetParent(getter_AddRefs(newParentDir));
        if (NS_FAILED(rv))
            return rv;
    }

    if (!newParentDir)
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
    
    // make sure it exists and is a directory.  Create it if not there.
    PRBool exists;
    newParentDir->Exists(&exists);
    if (exists == PR_FALSE)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        PRBool isDir;
        newParentDir->IsDirectory(&isDir);
        if (isDir == PR_FALSE)
        {
            if (followSymlinks)
            {
                PRBool isLink;
                newParentDir->IsSymlink(&isLink);
                if (isLink)
                {
                    char* target;
                    newParentDir->GetTarget(&target);

                    nsCOMPtr<nsILocalFile> realDest = new nsLocalFile();
                    if (realDest == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = realDest->InitWithPath(target);
                    
                    nsMemory::Free(target);

                    if (NS_FAILED(rv)) 
                        return rv;
                    
                    return CopyMove(realDest, newName, followSymlinks, move);
                }
            }
            else
            {                
                return NS_ERROR_FILE_DESTINATION_NOT_DIR;
            }
        }
    }

    // check to see if we are a directory, if so enumerate it.

    PRBool isDir;
    IsDirectory(&isDir);
    PRBool isSymlink;
    IsSymlink(&isSymlink);

    if (!isDir || (isSymlink && !followSymlinks))
    {
        rv = CopySingleFile(this, newParentDir, newName, followSymlinks, move);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        // create a new target destination in the new parentDir;
        nsCOMPtr<nsIFile> target;
        rv = newParentDir->Clone(getter_AddRefs(target));
        
        if (NS_FAILED(rv)) 
            return rv;
        
        char *allocatedNewName;
        if (!newName)
        {
            PRBool isLink;
            IsSymlink(&isLink);
            if (isLink)
            {
                char* temp;
                GetTarget(&temp);
                const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');
                if (leaf[0] == '\\')
                    leaf++;
                allocatedNewName = (char*) nsMemory::Clone( leaf, strlen(leaf)+1 );
            }
            else
            {
                GetLeafName(&allocatedNewName);// this should be the leaf name of the 
            }
        }
        else
        {
            allocatedNewName = (char*) nsMemory::Clone( newName, strlen(newName)+1 );
        }
        
        rv = target->Append(allocatedNewName);
        if (NS_FAILED(rv)) 
            return rv;

        nsMemory::Free(allocatedNewName);

        target->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
        
        nsDirEnumerator* dirEnum = new nsDirEnumerator();
        if (!dirEnum)
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
            PRBool isDir, isLink;
            
            file->IsDirectory(&isDir);
            file->IsSymlink(&isLink);

            if (move)
            {
                if (followSymlinks)
                    rv = NS_ERROR_FAILURE;
                else
                    rv = file->MoveTo(target, nsnull);
            }
            else
            {   
                if (followSymlinks)
                    rv = file->CopyToFollowingLinks(target, nsnull);
                else
                    rv = file->CopyTo(target, nsnull);
            }
                    
            iterator->HasMoreElements(&more);
        }
    }
    

    // If we moved, we want to adjust this.
    if (move)
    {
        MakeDirty();
        
        char* newParentPath;
        newParentDir->GetPath(&newParentPath);
        
        if (newParentPath == nsnull)
            return NS_ERROR_FAILURE;

        if (newName == nsnull)
        {
            char *aFileName;
            GetLeafName(&aFileName);
            
            InitWithPath(newParentPath);
            Append(aFileName); 

            nsMemory::Free(aFileName);
        }
        else
        {
            InitWithPath(newParentPath);
            Append(newName);
        }
        
        nsMemory::Free(newParentPath);
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::CopyTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::MoveTo(nsIFile *newParentDir, const char *newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_TRUE);
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
//    char **my_argv = NULL;
    
//    my_argv = (char **)nsMemory::Alloc(sizeof(char *) * (count + 2) );
//    if (!my_argv) {
//        return NS_ERROR_OUT_OF_MEMORY;
//    }

    // copy the args
    PRUint32 i;
    ULONG paramLen = 0;
    for (i=0; i < count; i++) {
//        my_argv[i+1] = (char *)args[i];
        paramLen += strlen((char *)args[i]) + 1;
    }
    // we need to set argv[0] to the program name.
//    my_argv[0] = mResolvedPath;

    // Build a single string with all of the parameters
    char *pszInputs = NULL;
    pszInputs = (char *)nsMemory::Alloc( paramLen );

    strcpy(pszInputs, (char *)args[0]);
    for (i=1; i < count; i++) {
        strcat(pszInputs, " ");
        strcat(pszInputs, (char *)args[i]);
    }
    
    // null terminate the array
//    my_argv[count+1] = NULL;

// Need to use DosStartSession to launch a program of another type
//    rv = PR_CreateProcessDetached(mResolvedPath, my_argv, NULL, NULL);
    STARTDATA sd;
    ULONG sid;
    PID pid;
    memset(&sd, 0, sizeof(STARTDATA));
    sd.Length = 24;
    sd.PgmName = NS_CONST_CAST(char*, mResolvedPath.get());
    sd.PgmInputs = pszInputs;
    APIRET rc = DosStartSession(&sd, &sid, &pid);

     // free up our argv
//     nsMemory::Free(my_argv);
     nsMemory::Free(pszInputs);

//     if (PR_SUCCESS == rv)
     if (rc == NO_ERROR || rc == ERROR_SMG_START_IN_BACKGROUND)
         return NS_OK;
     else
         return NS_ERROR_FILE_EXECUTION_FAILED;
}

NS_IMETHODIMP  
nsLocalFile::Load(PRLibrary * *_retval)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;
    
    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

    *_retval =  PR_LoadLibrary(mResolvedPath);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP  
nsLocalFile::Remove(PRBool recursive)
{
    PRBool isDir;
    
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.get();

    if (isDir)
    {
        if (recursive)
        {
            nsDirEnumerator* dirEnum = new nsDirEnumerator();
            if (dirEnum == nsnull)
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
    
                file->Remove(recursive);
                
                iterator->HasMoreElements(&more);
            }
        }
#ifdef XP_OS2_VACPP
        rmdir((char *) filePath);  // todo: save return value?
#else
        rmdir(filePath);  // todo: save return value?
#endif
    }
    else
    {
        remove(filePath); // todo: save return value?
    }
    
    MakeDirty();
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDate(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    *aLastModificationDate = 0;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModificationDate = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    *aLastModificationDate = 0;

    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModificationDate = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRInt64 aLastModificationDate)
{
    return nsLocalFile::SetModDate(aLastModificationDate, PR_TRUE);
}


NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRInt64 aLastModificationDate)
{
    return nsLocalFile::SetModDate(aLastModificationDate, PR_FALSE);
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModificationDate, PRBool resolveTerminal)
{
    nsresult rv = ResolveAndStat(resolveTerminal);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();
    
#ifndef XP_OS2
    HANDLE file = CreateFile(  filePath,          // pointer to name of the file
                               GENERIC_WRITE,     // access (write) mode
                               0,                 // share mode
                               NULL,              // pointer to security attributes
                               OPEN_EXISTING,     // how to create
                               0,                 // file attributes 
                               NULL);

    MakeDirty();

    if (!file)
    {
        return ConvertWinError(GetLastError());
    }
#endif

#ifdef XP_OS2
    PRExplodedTime pret;
    FILESTATUS3 pathInfo;

    rv = DosQueryPathInfo(filePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));

    if (NS_FAILED(rv))
    {
       rv = ConvertOS2Error(rv);
       return rv;
    }
    
    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModificationDate * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    pathInfo.fdateLastWrite.year      = pret.tm_year;    
    pathInfo.fdateLastWrite.month    = pret.tm_month + 1; // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
// ???? OS2TODO    st.wDayOfWeek       = pret.tm_wday;    
    pathInfo.fdateLastWrite.day        = pret.tm_mday;    
    pathInfo.ftimeLastWrite.hours      = pret.tm_hour;
    pathInfo.ftimeLastWrite.minutes   = pret.tm_min;    
    pathInfo.ftimeLastWrite.twosecs   = pret.tm_sec / 2;   // adjust for twosecs?
// ??? OS2TODO    st.wMilliseconds    = pret.tm_usec/1000;

    rv = DosSetPathInfo(filePath, 
                                 FIL_STANDARD,             // Level 1 info
                                 &pathInfo, 
                                 sizeof(pathInfo),
                                 0UL);
                               
    if (NS_FAILED(rv))
       return rv;

    MakeDirty();
#else  

    FILETIME lft, ft;
    SYSTEMTIME st;
    PRExplodedTime pret;
    
    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModificationDate * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    st.wYear            = pret.tm_year;    
    st.wMonth           = pret.tm_month + 1; // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
    st.wDayOfWeek       = pret.tm_wday;    
    st.wDay             = pret.tm_mday;    
    st.wHour            = pret.tm_hour;
    st.wMinute          = pret.tm_min;    
    st.wSecond          = pret.tm_sec;
    st.wMilliseconds    = pret.tm_usec/1000;

    if ( 0 == SystemTimeToFileTime(&st, &lft) )
    {
        rv = ConvertWinError(GetLastError());
    }
    else if ( 0 == LocalFileTimeToFileTime(&lft, &ft) )
    {
        rv = ConvertWinError(GetLastError());
    }
    else if ( 0 == SetFileTime(file, NULL, &ft, &ft) )
    {
        // could not set time
        rv = ConvertWinError(GetLastError());
    }

    CloseHandle( file );
#endif
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();


    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();
    if( chmod(filePath, aPermissions) == -1 )
        return NS_ERROR_FAILURE;
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();
    if( chmod(filePath, aPermissions) == -1 )
        return NS_ERROR_FAILURE;
        
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    

    *aFileSize = mFileInfo64.size;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{

#ifndef XP_OS2
    DWORD status;
    HANDLE hFile;
#endif

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();


#ifdef XP_OS2   
    APIRET rc;
    HFILE hFile;
    ULONG actionTaken;

    rc = DosOpen(filePath,
                       &hFile,
                       &actionTaken,
                       0,                 
                       FILE_NORMAL,
                       OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                       OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                       NULL);
                                 
    if (rc != NO_ERROR)
    {
        MakeDirty();
        return NS_ERROR_FAILURE;
    }
#else
    // Leave it to Microsoft to open an existing file with a function
    // named "CreateFile".
    hFile = CreateFile(filePath,
                       GENERIC_WRITE, 
                       FILE_SHARE_READ, 
                       NULL, 
                       OPEN_EXISTING, 
                       FILE_ATTRIBUTE_NORMAL, 
                       NULL); 

    if (hFile == INVALID_HANDLE_VALUE)
    {
        MakeDirty();
        return NS_ERROR_FAILURE;
    }
#endif   
    
    // Seek to new, desired end of file
    PRInt32 hi, lo;
    myLL_L2II(aFileSize, &hi, &lo );

#ifdef XP_OS2
    rc = DosSetFileSize(hFile, lo);
    if (rc == NO_ERROR) 
       DosClose(hFile);
    else
       goto error; 
#else   
    status = SetFilePointer(hFile, lo, NULL, FILE_BEGIN);
    if (status == 0xffffffff)
        goto error;

    // Truncate file at current cursor position
    if (!SetEndOfFile(hFile))
        goto error;

    if (!CloseHandle(hFile))
        return NS_ERROR_FAILURE;
#endif

    MakeDirty();
    return NS_OK;

 error:
    MakeDirty();
#ifdef XP_OS2
    DosClose(hFile);
#else   
    CloseHandle(hFile);
#endif
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP  
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    *aFileSize = mFileInfo64.size;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);
    
    ResolveAndStat(PR_FALSE);
    
#ifdef XP_OS2
    ULONG ulDriveNo = toupper(mResolvedPath.CharAt(0)) + 1 - 'A';
    FSALLOCATE fsAllocate;
    APIRET rv = DosQueryFSInfo(ulDriveNo,
                                            FSIL_ALLOC,
                                            &fsAllocate,
                                            sizeof(fsAllocate));

    if (NS_FAILED(rv)) 
        return rv;

    *aDiskSpaceAvailable = (PRInt64)(fsAllocate.cUnitAvail  *
                                                    fsAllocate.cSectorUnit *
                                                    (ULONG)fsAllocate.cbSector);
#else   
    const char *filePath = mResolvedPath.get();

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

    LL_D2L(*aDiskSpaceAvailable, nBytes);
#endif

    return NS_OK;

}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCString parentPath = mWorkingPath;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr((const unsigned char *) parentPath.get(), '\\')
                     - (const unsigned char *) parentPath.get());
    if (offset < 0)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    parentPath.Truncate(offset);

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv =  NS_NewLocalFile(parentPath.get(), mFollowSymlinks, getter_AddRefs(localFile));
    
    if(NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aParent);
    }
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    MakeDirty();
    nsresult rv = ResolveAndStat( PR_TRUE );
    
    if (NS_SUCCEEDED(rv))
        *_retval = PR_TRUE;
    else 
        *_retval = PR_FALSE;
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsWritable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
   
    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;  
    
    const char *workingFilePath = mWorkingPath.get();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    *_retval =  !((pathInfo.attrFile & FILE_READONLY)  != 0); 
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval = !((word & FILE_ATTRIBUTE_READONLY) != 0); 
#endif

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat( PR_TRUE );
    if (NS_FAILED(rv))
        return rv;

    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsExecutable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;


    nsresult rv = ResolveAndStat( PR_TRUE );
    if (NS_FAILED(rv))
        return rv;

    char* path = nsnull;
    PRBool symLink;
    
    rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;
    
    if (symLink)
        GetTarget(&path);
    else
        GetPath(&path);

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) path, '\\');

    // XXX On Windows NT / 2000, it should use "PATHEXT" environment value
#ifdef XP_OS2
    if ( (strstr(leaf, ".bat") != nsnull) ||
         (strstr(leaf, ".exe") != nsnull) ||
         (strstr(leaf, ".cmd") != nsnull) ||
         (strstr(leaf, ".com") != nsnull) ) {
#else
    if ( (strstr(leaf, ".bat") != nsnull) ||
         (strstr(leaf, ".exe") != nsnull) ) {
#endif
        *_retval = PR_TRUE;
    } else {
        *_retval = PR_FALSE;
    }
    
    nsMemory::Free(path);
    
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_DIRECTORY); 

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_FILE); 
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *workingFilePath = mWorkingPath.get();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    *_retval =  ((pathInfo.attrFile & FILE_HIDDEN)  != 0); 
    
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval =  ((word & FILE_ATTRIBUTE_HIDDEN)  != 0); 
#endif

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

#ifndef XP_OS2   // No Symlinks on OS/2
    char* path;
    int   pathLen;
    
    GetPath(&path);
    pathLen = strlen(path);
    
    const char* leaf = path + pathLen - 4;
    
    if ( (strcmp(leaf, ".lnk") == 0)) 
    {
        *_retval = PR_TRUE;
    }
    
    nsMemory::Free(path);
#endif
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *workingFilePath = mWorkingPath.get();

#ifdef XP_OS2
    APIRET rc;
    FILESTATUS3 pathInfo;

    rc = DosQueryPathInfo(workingFilePath, 
                                    FIL_STANDARD,             // Level 1 info
                                    &pathInfo, 
                                    sizeof(pathInfo));
                               
    if (rc != NO_ERROR) 
    {
       rc = ConvertOS2Error(rc);
       return rc;
    } 

    *_retval =  ((pathInfo.attrFile & FILE_SYSTEM)  != 0); 
#else  
    DWORD word = GetFileAttributes(workingFilePath);

    *_retval = ((word & FILE_ATTRIBUTE_SYSTEM)  != 0); 
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    char* inFilePath;
    inFile->GetPath(&inFilePath);
    
    char* filePath;
    GetPath(&filePath);

    if (strcmp(inFilePath, filePath) == 0)
        *_retval = PR_TRUE;
    
    nsMemory::Free(inFilePath);
    nsMemory::Free(filePath);

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
        
    nsMemory::Free(inFilePath);
    nsMemory::Free(myFilePath);

    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{   
    NS_ENSURE_ARG(_retval);
    *_retval = nsnull;
#if STRICT_FAKE_SYMLINKS    
    PRBool symLink;
    
    nsresult rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    if (!symLink)
    {
        return NS_ERROR_FILE_INVALID_PATH;
    }
#endif
    ResolveAndStat(PR_TRUE);
 
    *_retval = (char*) nsMemory::Clone( mResolvedPath.get(), strlen(mResolvedPath)+1 );
    return NS_OK;
}


/* attribute PRBool followLinks; */
NS_IMETHODIMP 
nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
    *aFollowLinks = mFollowSymlinks;
    return NS_OK;
}
NS_IMETHODIMP 
nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
    MakeDirty();
    mFollowSymlinks = aFollowLinks;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator * *entries)
{
    nsresult rv;
    
    *entries = nsnull;

    PRBool isDir;
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) 
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(this);
    if (NS_FAILED(rv)) 
    {
        NS_RELEASE(dirEnum);
        return rv;
    }
    
    *entries = dirEnum;
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::GetURL(char * *aURL)
{
    NS_ENSURE_ARG_POINTER(aURL);
    *aURL = nsnull;

    nsresult rv;
    char* ePath = nsnull;
    nsCAutoString escPath;

    rv = GetPath(&ePath);
    if (NS_SUCCEEDED(rv)) {

        // Replace \ with / to convert to an url
        char* s = ePath;
        while (*s)
        {
            // We need to call IsDBCSLeadByte because
            // Japanese windows can have 0x5C in the sencond byte 
            // of a Japanese character, for example 0x8F 0x5C is
            // one Japanese character
#ifdef XP_OS2
            if(::isleadbyte(*s) && *(s+1) != nsnull)
#else
            if(::IsDBCSLeadByte(*s) && *(s+1) != nsnull)
#endif
            {
                s++;
            }
            else if (*s == '\\') {
                *s = '/';
            }
            s++;
        }

        // Escape the path with the directory mask
        rv = nsStdEscape(ePath, esc_Directory+esc_Forced, escPath);
        if (NS_SUCCEEDED(rv)) {
            escPath.Insert("file:///", 0);

            PRBool dir;
            rv = IsDirectory(&dir);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Cannot tell if this is a directory");
            if (NS_SUCCEEDED(rv) && dir && escPath[escPath.Length() - 1] != '/') {
                // make sure we have a trailing slash
                escPath += "/";
            }
            *aURL = nsCRT::strdup((const char *)escPath);
            rv = *aURL ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }    
    }
    CRTFREEIF(ePath);
    return rv;
}

NS_IMETHODIMP nsLocalFile::SetURL(const char * aURL)
{
    NS_ENSURE_ARG(aURL);
    nsresult rv;
    
    nsXPIDLCString host, directory, fileBaseName, fileExtension;
    
    rv = ParseURL(aURL, getter_Copies(host), getter_Copies(directory),
                  getter_Copies(fileBaseName), getter_Copies(fileExtension));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString path;
    nsCAutoString component;

    if (host)
    {
        // We can end up with a host when given: file://C|/ instead of file:///
        if (strlen((const char *)host) == 2 && ((const char *)host)[1] == '|')
        {
            path += host;
            path.SetCharAt(':', 1);
        }
    }
    if (directory)
    {
        nsStdEscape(directory, esc_Directory, component);
		if (!host && component.Length() > 2 && component.CharAt(2) == '|')
			component.SetCharAt(':', 2);
		component.ReplaceChar('/', '\\');
        path += component;
    }    
    if (fileBaseName)
    {
        nsStdEscape(fileBaseName, esc_FileBaseName, component);
        path += component;
    }
    if (fileExtension)
    {
        nsStdEscape(fileExtension, esc_FileExtension, component);
        path += '.';
        path += component;
    }
    
    nsUnescape((char*)path.get());

    // remove leading '\'
    if (path.CharAt(0) == '\\')
        path.Cut(0, 1);

    rv = InitWithPath(path);
    
    return rv;
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

#ifndef OPEN_DEFAULT
#define OPEN_DEFAULT 0
#endif


NS_IMETHODIMP
nsLocalFile::Reveal()
{
  PRBool isDirectory = PR_FALSE;
  nsXPIDLCString path;

  IsDirectory(&isDirectory);
  if (isDirectory)
  {
    GetPath(getter_Copies(path));  
  }
  else
  {
    nsCOMPtr<nsIFile> parent;
    GetParent(getter_AddRefs(parent));
    if (parent)
      parent->GetPath(getter_Copies(path));  
  }

  HOBJECT hobject = WinQueryObject(path);
  WinOpenObject( hobject, OPEN_DEFAULT, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
  nsXPIDLCString platformPath;

  GetPath(getter_Copies(platformPath));  

  HOBJECT hobject = WinQueryObject(platformPath);
  WinOpenObject( hobject, OPEN_DEFAULT, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}

nsresult 
NS_NewLocalFile(const char* path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

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

// Locates the first occurrence of charToSearchFor in the stringToSearch
static unsigned char* PR_CALLBACK
_mbschr( const unsigned char* stringToSearch, int charToSearchFor)
{
   const unsigned char* p = stringToSearch;
   do {
       if (*p == charToSearchFor)
           break;
       p  = (const unsigned char*)WinNextChar(0,0,0,(char*)p);
   } while (*p); /* enddo */
   // Result is p or NULL
   return *p ? (unsigned char*)p : NULL;
}

// Locates last occurence of charToSearchFor in the stringToSearch
extern unsigned char*
_mbsrchr( const unsigned char* stringToSearch, int charToSearchFor)
{
   int length = strlen((const char*)stringToSearch);
   const unsigned char* p = stringToSearch+length;
   do {
       if (*p == charToSearchFor)
           break;
       p  = (const unsigned char*)WinPrevChar(0,0,0,(char*)stringToSearch,(char*)p);
   } while (p > stringToSearch); /* enddo */
   // Result is p or NULL
   return (*p == charToSearchFor) ? (unsigned char*)p : NULL;
}

// Implement equivalent of Win32 CreateDirectoryA
static nsresult PR_CALLBACK
CreateDirectoryA( PSZ resolvedPath, PEAOP2 ppEABuf)
{
   APIRET rc;
   nsresult rv;
   FILESTATUS3 pathInfo;

   rc = DosCreateDir( resolvedPath, ppEABuf );  
   if (rc != NO_ERROR) { 
      rv = ConvertOS2Error(rc);

      // Check if directory already exists and if so, reflect that in the return value
      rc = DosQueryPathInfo(resolvedPath, 
                                      FIL_STANDARD,             // Level 1 info
                                      &pathInfo, 
                                      sizeof(pathInfo));
      if (rc == NO_ERROR) 
         rv = ERROR_FILE_EXISTS;
   } 
   else 
      rv = rc; 

   return rv;
}

static int isleadbyte(int c)
{
  static BOOL bDBCSFilled=FALSE;
  static BYTE DBCSInfo[12] = { 0 };  /* According to the Control Program Guide&Ref,
                                             12 bytes is sufficient */
  BYTE *curr;
  BOOL retval = FALSE;

  if( !bDBCSFilled ) {
    COUNTRYCODE ctrycodeInfo = { 0 };
    APIRET rc = NO_ERROR;
    ctrycodeInfo.country = 0;     /* Current Country */
    ctrycodeInfo.codepage = 0;    /* Current Codepage */

    rc = DosQueryDBCSEnv( sizeof( DBCSInfo ),
                          &ctrycodeInfo,
                          DBCSInfo );
    if( rc != NO_ERROR ) {
      /* we had an error, do something? */
      return FALSE;
    }
    bDBCSFilled=TRUE;
  }

  curr = DBCSInfo;
  /* DBCSInfo returned by DosQueryDBCSEnv is terminated with two '0' bytes in a row */
  while(( *curr != 0 ) && ( *(curr+1) != 0)) {
    if(( c >= *curr ) && ( c <= *(curr+1) )) {
      retval=TRUE;
      break;
    }
    curr+=2;
  }

  return retval;
}
