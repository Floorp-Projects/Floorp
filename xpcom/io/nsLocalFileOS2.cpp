/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com>
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFile.h"
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
CreateDirectoryA( PSZ path, PEAOP2 ppEABuf);
static int isleadbyte(int c);

#include <unistd.h>
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

    private:
        ~nsDirEnumerator() 
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
    MakeDirty();
}

nsLocalFile::nsLocalFile(const nsLocalFile& other)
  : mDirty(other.mDirty)
  , mWorkingPath(other.mWorkingPath)
  , mFileInfo64(other.mFileInfo64)
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

nsresult
nsLocalFile::Stat()
{
    if (!mDirty)
        return NS_OK;

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
        return NS_OK;

    return NS_ERROR_FILE_NOT_FOUND;
}

NS_IMETHODIMP  
nsLocalFile::Clone(nsIFile **file)
{
    // Just copy-construct ourselves
    *file = new nsLocalFile(*this);
    if (!*file)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*file);
    
    return NS_OK;
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
    nsresult rv = Stat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = PR_Open(mWorkingPath.get(), flags, mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ErrorAccordingToNSPR();
}


NS_IMETHODIMP  
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = Stat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv; 
   
    *_retval = fopen(mWorkingPath.get(), mode);
    
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}



NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = Stat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;  
    
   // create nested directories to target
    unsigned char* slash = _mbschr((const unsigned char*) mWorkingPath.get(), '\\');


    if (slash)
    {
        // skip the first '\\'
        ++slash;
        slash = _mbschr(slash, '\\');
    
        while (slash)
        {
            *slash = '\0';
            
                rv = CreateDirectoryA(NS_CONST_CAST(char*, mWorkingPath.get()), NULL);
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
        PRFileDesc* file = PR_Open(mWorkingPath.get(), PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;
          
        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
        rv = CreateDirectoryA(NS_CONST_CAST(char*, mWorkingPath.get()), NULL);
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
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent, const nsACString &newName, PRBool move)
{
    nsresult rv;
    nsCAutoString filePath;

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
           
    rv = sourceFile->GetNativePath(filePath);
    
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
nsLocalFile::CopyMove(nsIFile *aParentDir, const nsACString &newName, PRBool move)
{
    nsCOMPtr<nsIFile> newParentDir = aParentDir;

    nsresult rv  = Stat();
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
            return NS_ERROR_FILE_DESTINATION_NOT_DIR;
        }
    }

    // check to see if we are a directory, if so enumerate it.

    PRBool isDir;
    IsDirectory(&isDir);

    if (!isDir)
    {
        rv = CopySingleFile(this, newParentDir, newName, move);
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
            GetNativeLeafName(allocatedNewName);// this should be the leaf name of the 
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

            if (move)
            {
                rv = file->MoveToNative(target, nsCString());
            }
            else
            {   
                rv = file->CopyToNative(target, nsCString());
            }
                    
            iterator->HasMoreElements(&more);
        }
        // we've finished moving all the children of this directory
        // in the new directory.  so now delete the directory
        // note, we don't need to do a recursive delete.
        // MoveTo() is recursive.  At this point,
        // we've already moved the children of the current folder 
        // to the new location.  nothing should be left in the folder.
        if (move)
        {
          rv = Remove(PR_FALSE);
          NS_ENSURE_SUCCESS(rv,rv);
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
    return CopyMove(newParentDir, newName, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE);
}

NS_IMETHODIMP  
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE);
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

    *_retval =  PR_LoadLibrary(mWorkingPath.get());
    
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

    const char *filePath = mWorkingPath.get();

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
        rv = rmdir(filePath) == -1 ? NSRESULT_FOR_ERRNO() : NS_OK;
    }
    else
    {
        rv = remove(filePath) == -1 ? NSRESULT_FOR_ERRNO() : NS_OK;
    }
    
    MakeDirty();
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
    
    *aLastModifiedTime = 0;

    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    
    // microseconds -> milliseconds
    *aLastModifiedTime = mFileInfo64.modifyTime / PR_USEC_PER_MSEC;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModifiedTime)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    return nsLocalFile::SetModDate(aLastModifiedTime);
}


NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTime)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModifiedTime)
{
    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mWorkingPath.get();
    
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
    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mWorkingPath.get();


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
    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mWorkingPath.get();
    if( chmod(filePath, aPermissions) == -1 )
        return NS_ERROR_FAILURE;
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = 0;

    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    

    *aFileSize = mFileInfo64.size;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{

    nsresult rv = Stat();
    
    if (NS_FAILED(rv))
        return rv;
    
    const char *filePath = mWorkingPath.get();


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
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);
    
    ULONG ulDriveNo = toupper(mWorkingPath.CharAt(0)) + 1 - 'A';
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
    nsresult rv =  NS_NewNativeLocalFile(parentPath, PR_TRUE, getter_AddRefs(localFile));
    
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
    *_retval = NS_SUCCEEDED(Stat());

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsWritable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
   
    nsresult rv = Stat();
    
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

    nsresult rv = Stat();
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

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString path;
    GetNativeTarget(path);

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

    nsresult rv = Stat();
    
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

    nsresult rv = Stat();
    
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

    nsresult rv = Stat();
    
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

    nsresult rv = Stat();
    
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

    if ( strnicmp( myFilePath.get(), inFilePath.get(), myFilePathLen) == 0)
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
    _retval = mWorkingPath;
    return NS_OK;
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
#define OPEN_DEFAULT       0
#define OPEN_CONTENTS      1
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
  WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
  WinOpenObject( hobject, OPEN_CONTENTS, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
  HOBJECT hobject = WinQueryObject(mWorkingPath.get());
  WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
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
CreateDirectoryA( PSZ path, PEAOP2 ppEABuf)
{
   APIRET rc;
   nsresult rv;
   FILESTATUS3 pathInfo;

   rc = DosCreateDir( path, ppEABuf );  
   if (rc != NO_ERROR) { 
      rv = ConvertOS2Error(rc);

      // Check if directory already exists and if so, reflect that in the return value
      rc = DosQueryPathInfo(path, 
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
    nsCAutoString buf;
    nsresult rv = NS_CopyUnicodeToNative(path, buf);
    if (NS_FAILED(rv)) {
        *result = nsnull;
        return rv;
    }
    return NS_NewNativeLocalFile(buf, followLinks, result);
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
