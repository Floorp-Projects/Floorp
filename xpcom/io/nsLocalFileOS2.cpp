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
#include "nsNativeCharsetUtils.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include <ctype.h>    // needed for toupper
#include <string.h>

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "prproces.h"
#include "prthread.h"


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


static nsresult ConvertOS2Error(int err)
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
        }

        nsresult Init(nsILocalFile* parent) 
        {
            nsCAutoString filepath;
            parent->GetNativeTarget(filepath);
        
            if (filepath.IsEmpty())
            {
                parent->GetNativePath(filepath);
            }
            
            if (filepath.IsEmpty())
            {
                return NS_ERROR_UNEXPECTED;
            }

            mDir = PR_OpenDir(filepath.get());
            if (mDir == nsnull)    // not a directory?
                return NS_ERROR_FAILURE;

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
                
                rv = file->AppendNative(nsDependentCString(entry->name));
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
    mLastResolution = PR_FALSE;
    mFollowSymlinks = PR_FALSE;
    MakeDirty();
}

nsLocalFile::~nsLocalFile()
{
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
    
        if (slash)
        {
            *slash = '\\';
            ++slash;
            slash = strchr(slash, '\\');
        }
    }

    // kill any trailing separator
    char* temp = filePath;
    int len = strlen(temp) - 1;
    if(temp[len] == '\\')
        temp[len] = '\0';
    
    *resolvedPath = filePath;
    return NS_OK;
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

    DosError(FERR_DISABLEHARDERR);
    PRStatus status = PR_GetFileInfo64(nsprPath, &mFileInfo64);
    DosError(FERR_ENABLEHARDERR);
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
    
        if (pathLen < 4)
        {
            mDirty = PR_FALSE;
            return NS_OK;
        }
    }

    nsresult result;

    // okay, something is wrong with the working path.  We will try to resolve it.

    char *resolvePath;
    
	result = ResolvePath(workingFilePath, resolveTerminal, &resolvePath);
    if (NS_FAILED(result))
       return NS_ERROR_FILE_NOT_FOUND;
    
	mResolvedPath.Assign(resolvePath);
    nsMemory::Free(resolvePath);

    DosError(FERR_DISABLEHARDERR);
    status = PR_GetFileInfo64(mResolvedPath.get(), &mFileInfo64);
    DosError(FERR_ENABLEHARDERR);
    
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

    nsCOMPtr<nsILocalFile> localFile;

    rv = NS_NewNativeLocalFile(mWorkingPath, mFollowSymlinks, getter_AddRefs(localFile));
    
    if (NS_SUCCEEDED(rv) && localFile)
    {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)file);
    }
            
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::InitWithNativePath(const nsACString &filePath)
{
    MakeDirty();

    nsACString::const_iterator begin, end;
    filePath.BeginReading(begin);
    filePath.EndReading(end);

    // input string must not be empty
    if (begin == end)
        return NS_ERROR_FAILURE;

    char firstChar = *begin;
    char secondChar = *(++begin);
    
    // just do a sanity check.  if it has any forward slashes, it is not a Native path
    // on windows.  Also, it must have a colon at after the first char.
    
    char *path = nsnull;
    PRInt32 pathLen = 0;

    if ( ( (secondChar == ':') && !FindCharInReadable('/', begin, end) ) ||  // normal path
         ( (firstChar == '\\') && (secondChar == '\\') ) )  // network path
    {
        // This is a native path
        path = ToNewCString(filePath);
        pathLen = filePath.Length();
    }

    if (path == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // kill any trailing '\' provided it isn't the second char of DBCS
    PRInt32 len = pathLen - 1;
    if (path[len] == '\\' && !::isleadbyte(path[len-1]))
    {
        path[len] = '\0';
        pathLen = len;
    }
    
    mWorkingPath.Adopt(path, pathLen);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    nsresult rv = ResolveAndStat(PR_TRUE);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = PR_Open(mResolvedPath.get(), flags, mode);
    
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
   
    *_retval = fopen(mResolvedPath.get(), mode);
    
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


    if (slash)
    {
        // skip the first '\\'
        ++slash;
        slash = _mbschr(slash, '\\');
    
        while (slash)
        {
            *slash = '\0';
            
                rv = CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL);
                if (rv) {
                    rv = ConvertOS2Error(rv);
                    if (rv != NS_ERROR_FILE_ALREADY_EXISTS) return rv;
                }
                *slash = '\\';
                ++slash;
                slash = _mbschr(slash, '\\');
        }
    }

    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mResolvedPath.get(), PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;
          
        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
        rv = CreateDirectoryA(NS_CONST_CAST(char*, mResolvedPath.get()), NULL);
        if (rv) 
            return ConvertOS2Error(rv);
        else 
            return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}
    
NS_IMETHODIMP  
nsLocalFile::AppendNative(const nsACString &node)
{
    if (node.IsEmpty())
        return NS_OK;

    // Append only one component. Check for subdirs.
    // XXX can we avoid the PromiseFlatCString call?
    if (_mbschr((const unsigned char*) PromiseFlatCString(node).get(), '\\') != nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    return AppendRelativeNativePath(node);
}

NS_IMETHODIMP  
nsLocalFile::AppendRelativeNativePath(const nsACString &node)
{
    // Cannot start with a / or have .. or have / anywhere
    nsACString::const_iterator begin, end;
    node.BeginReading(begin);
    node.EndReading(end);
    if (node.IsEmpty() || FindCharInReadable('/', begin, end))
    {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    MakeDirty();
    mWorkingPath.Append(NS_LITERAL_CSTRING("\\") + node);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetNativeLeafName(nsACString &aLeafName)
{
    aLeafName.Truncate();

    const char* temp = mWorkingPath.get();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');
    
    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    aLeafName.Assign(leaf);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetNativeLeafName(const nsACString &aLeafName)
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
nsLocalFile::GetNativePath(nsACString &_retval)
{
    _retval = mWorkingPath;
    return NS_OK;
}

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent, const nsACString &newName, PRBool followSymlinks, PRBool move)
{
    nsresult rv;
    nsCAutoString filePath;

    // get the path that we are going to copy to.
    // Since windows does not know how to auto
    // resolve shortcust, we must work with the
    // target.
    nsCAutoString destPath;
    destParent->GetNativeTarget(destPath);  

    destPath.Append("\\");

    if (newName.IsEmpty())
    {
        nsCAutoString aFileName;
        sourceFile->GetNativeLeafName(aFileName);
        destPath.Append(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }

           
    if (followSymlinks)
    {
        rv = sourceFile->GetNativeTarget(filePath);
        if (filePath.IsEmpty())
            rv = sourceFile->GetNativePath(filePath);
    }
    else
    {
        rv = sourceFile->GetNativePath(filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    APIRET rc = NO_ERROR;

    if( move )
    {
        rc = DosMove(filePath.get(), (PSZ)NS_CONST_CAST(char*, destPath.get()));
    }

    if (!move || rc == ERROR_NOT_SAME_DEVICE || rc == ERROR_ACCESS_DENIED) {
        /* will get an error if the destination and source files aren't on the
         * same drive.  "MoveFile()" on Windows will go ahead and move the
         * file without error, so we need to do the same   IBM-AKR
         */
        do {
          rc = DosCopy(filePath.get(), (PSZ)NS_CONST_CAST(char*, destPath.get()), DCPY_EXISTING);
          if (rc == ERROR_TOO_MANY_OPEN_FILES) {
              ULONG CurMaxFH = 0;
              LONG ReqCount = 20;
              APIRET rc2;
              rc2 = DosSetRelMaxFH(&ReqCount, &CurMaxFH);
              if (rc2 != NO_ERROR) {
                break;
              }
          }
        } while (rc == ERROR_TOO_MANY_OPEN_FILES);

        /* WSOD2 HACK */
        if (rc == 65) { // NETWORK_ACCESS_DENIED
          CHAR         achProgram[CCHMAXPATH];  // buffer for program name, parameters
          RESULTCODES  rescResults;             // buffer for results of dosexecpgm

          strcpy(achProgram, "CMD.EXE  /C ");
          strcat(achProgram, """COPY ");
          strcat(achProgram, filePath.get());
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
          DosDelete( filePath.get() );
        }
    } /* !move or ERROR */
    
    if (rc)
        rv = ConvertOS2Error(rc);

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const nsACString &newName, PRBool followSymlinks, PRBool move)
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
        
        if (newName.IsEmpty())
            return NS_ERROR_INVALID_ARG;

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
                    nsCAutoString target;
                    newParentDir->GetNativeTarget(target);

                    nsCOMPtr<nsILocalFile> realDest = new nsLocalFile();
                    if (realDest == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = realDest->InitWithNativePath(target);

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
        
        nsCAutoString allocatedNewName;
        if (newName.IsEmpty())
        {
            PRBool isLink;
            IsSymlink(&isLink);
            if (isLink)
            {
                nsCAutoString temp;
                GetNativeTarget(temp);
                const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp.get(), '\\');
                if (leaf[0] == '\\')
                    leaf++;
                allocatedNewName = leaf;
            }
            else
            {
                GetNativeLeafName(allocatedNewName);// this should be the leaf name of the 
            }
        }
        else
        {
            allocatedNewName = newName;
        }
        
        rv = target->AppendNative(allocatedNewName);
        if (NS_FAILED(rv)) 
            return rv;

        allocatedNewName.Truncate();

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
                    rv = file->MoveToNative(target, nsCString());
            }
            else
            {   
                if (followSymlinks)
                    rv = file->CopyToFollowingLinksNative(target, nsCString());
                else
                    rv = file->CopyToNative(target, nsCString());
            }
                    
            iterator->HasMoreElements(&more);
        }
    }
    

    // If we moved, we want to adjust this.
    if (move)
    {
        MakeDirty();
        
        nsCAutoString newParentPath;
        newParentDir->GetNativePath(newParentPath);
        
        if (newParentPath.IsEmpty())
            return NS_ERROR_FAILURE;

        if (newName.IsEmpty())
        {
            nsCAutoString aFileName;
            GetNativeLeafName(aFileName);
            
            InitWithNativePath(newParentPath);
            AppendNative(aFileName); 
        }
        else
        {
            InitWithNativePath(newParentPath);
            AppendNative(newName);
        }
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_TRUE);
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

    *_retval =  PR_LoadLibrary(mResolvedPath.get());
    
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
        rv = rmdir((char *) filePath);  // todo: save return value?
#else
        rv = rmdir(filePath);  // todo: save return value?
#endif
    }
    else
    {
        rv = remove(filePath); // todo: save return value?
    }
    
    MakeDirty();
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
    
    *aLastModifiedTime = 0;

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModifiedTime = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
    
    *aLastModifiedTime = 0;

    nsresult rv = ResolveAndStat(PR_FALSE);
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModifiedTime = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    return nsLocalFile::SetModDate(aLastModifiedTime, PR_TRUE);
}


NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTime)
{
    return nsLocalFile::SetModDate(aLastModifiedTime, PR_FALSE);
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModifiedTime, PRBool resolveTerminal)
{
    nsresult rv = ResolveAndStat(resolveTerminal);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();
    
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
    PR_ExplodeTime(aLastModifiedTime * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    /* fdateLastWrite.year is based off of 1980 */
    if (pret.tm_year >= 1980)
      pathInfo.fdateLastWrite.year      = pret.tm_year-1980;
    else
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

    nsresult rv = ResolveAndStat(PR_TRUE);
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mResolvedPath.get();


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
   
    // Seek to new, desired end of file
    PRInt32 hi, lo;
    myLL_L2II(aFileSize, &hi, &lo );

    rc = DosSetFileSize(hFile, lo);
    if (rc == NO_ERROR) 
       DosClose(hFile);
    else
       goto error; 

    MakeDirty();
    return NS_OK;

 error:
    MakeDirty();
    DosClose(hFile);
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
    
    ULONG ulDriveNo = toupper(mResolvedPath.CharAt(0)) + 1 - 'A';
    FSALLOCATE fsAllocate;
    APIRET rc = DosQueryFSInfo(ulDriveNo,
                               FSIL_ALLOC,
                               &fsAllocate,
                               sizeof(fsAllocate));

    if (rc != NO_ERROR)
       return NS_ERROR_FAILURE;

    *aDiskSpaceAvailable = fsAllocate.cUnitAvail;
    *aDiskSpaceAvailable *= fsAllocate.cSectorUnit;
    *aDiskSpaceAvailable *= fsAllocate.cbSector;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCAutoString parentPath(mWorkingPath);

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr((const unsigned char *) parentPath.get(), '\\')
                     - (const unsigned char *) parentPath.get());
    if (offset < 0)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    parentPath.Truncate(offset);

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv =  NS_NewNativeLocalFile(parentPath, mFollowSymlinks, getter_AddRefs(localFile));
    
    if(NS_SUCCEEDED(rv) && localFile)
    {
        return CallQueryInterface(localFile, aParent);
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

    nsCAutoString path;
    PRBool symLink;
    
    rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;
    
    if (symLink)
        GetNativeTarget(path);
    else
        GetNativePath(path);

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) path.get(), '\\');

    if ( (strstr(leaf, ".bat") != nsnull) ||
         (strstr(leaf, ".exe") != nsnull) ||
         (strstr(leaf, ".cmd") != nsnull) ||
         (strstr(leaf, ".com") != nsnull) ) {
        *_retval = PR_TRUE;
    } else {
        *_retval = PR_FALSE;
    }
    
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
    
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    // No Symlinks on OS/2
    *_retval = PR_FALSE;

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

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsCAutoString inFilePath;
    inFile->GetNativePath(inFilePath);
    
    *_retval = inFilePath.Equals(mWorkingPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    *_retval = PR_FALSE;
       
    nsCAutoString myFilePath;
    if ( NS_FAILED(GetNativeTarget(myFilePath)))
        GetNativePath(myFilePath);
    
    PRInt32 myFilePathLen = myFilePath.Length();
    
    nsCAutoString inFilePath;
    if ( NS_FAILED(inFile->GetNativeTarget(inFilePath)))
        inFile->GetNativePath(inFilePath);

    if ( strncmp( myFilePath.get(), inFilePath.get(), myFilePathLen) == 0)
    {
        // now make sure that the |inFile|'s path has a trailing
        // separator.

        if (inFilePath[myFilePathLen] == '\\')
        {
            *_retval = PR_TRUE;
        }

    }
        
    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{   
    _retval.Truncate();
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
 
    _retval = mResolvedPath;
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

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
    return GetNativePath(aPersistentDescriptor);
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
   return InitWithNativePath(aPersistentDescriptor);   
}

#ifndef OPEN_DEFAULT
#define OPEN_DEFAULT 0
#endif


NS_IMETHODIMP
nsLocalFile::Reveal()
{
  PRBool isDirectory = PR_FALSE;
  nsCAutoString path;

  IsDirectory(&isDirectory);
  if (isDirectory)
  {
    GetNativePath(path);  
  }
  else
  {
    nsCOMPtr<nsIFile> parent;
    GetParent(getter_AddRefs(parent));
    if (parent)
      parent->GetNativePath(path);  
  }

  HOBJECT hobject = WinQueryObject(path.get());
  WinOpenObject( hobject, OPEN_DEFAULT, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
  HOBJECT hobject = WinQueryObject(mWorkingPath.get());
  WinOpenObject( hobject, OPEN_DEFAULT, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}

nsresult 
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    if (!path.IsEmpty()) {
        nsresult rv = file->InitWithNativePath(path);
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

NS_IMETHODIMP
nsLocalFile::InitWithPath(const nsAString &filePath)
{
    if (filePath.IsEmpty())
        return InitWithNativePath(nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(filePath, tmp);

    if (NS_SUCCEEDED(rv))
        return InitWithNativePath(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::Append(const nsAString &node)
{
    if (node.IsEmpty())
        return NS_OK;

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);

    if (NS_SUCCEEDED(rv))
        return AppendNative(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const nsAString &node)
{
    if (node.IsEmpty())
        return NS_OK;

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);

    if (NS_SUCCEEDED(rv))
        return AppendRelativeNativePath(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeLeafName(tmp);

    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, aLeafName);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
    if (aLeafName.IsEmpty())
        return SetNativeLeafName(nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(aLeafName, tmp);

    if (NS_SUCCEEDED(rv))
        return SetNativeLeafName(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetPath(nsAString &_retval)
{
    return NS_CopyNativeToUnicode(mWorkingPath, _retval);
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);

    if (NS_SUCCEEDED(rv))
        return CopyToNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToFollowingLinksNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);

    if (NS_SUCCEEDED(rv))
        return CopyToFollowingLinksNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return MoveToNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);

    if (NS_SUCCEEDED(rv))
        return MoveToNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetTarget(nsAString &_retval)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeTarget(tmp);

    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, _retval);

    return rv;
}

nsresult
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    if (path.IsEmpty())
        return NS_NewNativeLocalFile(nsCString(), followLinks, result);

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(path, tmp);

    if (NS_SUCCEEDED(rv))
        return NS_NewNativeLocalFile(tmp, followLinks, result);

    return NS_OK;
}

//----------------------------------------------------------------------------
// global init/shutdown
//----------------------------------------------------------------------------

void
nsLocalFile::GlobalInit()
{
}

void
nsLocalFile::GlobalShutdown()
{
}
