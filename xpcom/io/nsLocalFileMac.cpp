/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Steve Dagley <sdagley@netscape.com>
 *     John R. McMullen
 */


#include "nsCOMPtr.h"
#include "nsIAllocator.h"

#include "nsLocalFileMac.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include "FullPath.h"
#include "FileCopy.h"
#include "MoreFilesExtras.h"

#include <Errors.h>
#include <Script.h>
#include <Processes.h>

// Simple func to map Mac OS errors into nsresults
static nsresult MacErrorMapper(OSErr inErr)
{
    nsresult outErr;
    
    switch (inErr)
    {
        case fnfErr:
            outErr = NS_ERROR_FILE_NOT_FOUND;
            break;

        case dupFNErr:
            outErr = NS_ERROR_FILE_ALREADY_EXISTS;
            break;
		
		// Can't find good map for some
        case bdNamErr:
            outErr = NS_ERROR_FAILURE;
            break;

        case noErr:
            outErr = NS_OK;

        default:    
            outErr = NS_ERROR_FAILURE;
    }
    return outErr;
}


static void myPLstrcpy(Str255 dst, const char* src)
{
	int srcLength = strlen(src);
	NS_ASSERTION(srcLength <= 255, "Oops, Str255 can't hold >255 chars");
	if (srcLength > 255)
		srcLength = 255;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

static void myPLstrncpy(Str255 dst, const char* src, int inMax)
{
	int srcLength = strlen(src);
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
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

                nsCOMPtr<nsILocalFile> file;

                rv = nsComponentManager::CreateInstance(NS_LOCAL_FILE_PROGID, 
                                                        nsnull, 
                                                        nsCOMTypeInfo<nsILocalFile>::GetIID(), 
                                                        getter_AddRefs(file));
                if (NS_FAILED(rv)) 
                    return rv;
        
                rv = file->InitWithFile(mParent);
                if (NS_FAILED(rv)) 
                    return rv;
            
                rv = file->AppendPath(entry->name);
                if (NS_FAILED(rv)) 
                    return rv;
            
                mNext = file;
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








nsLocalFile::nsLocalFile()
{
    NS_INIT_REFCNT();

    MakeDirty();
}

nsLocalFile::~nsLocalFile()
{
}

/* nsISupports interface implementation. */
NS_IMPL_ISUPPORTS2(nsLocalFile, nsILocalFile, nsIFile)
NS_METHOD
nsLocalFile::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

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
    mStatDirty       = PR_TRUE;
}


NS_IMETHODIMP  
nsLocalFile::InitWithKey(const char *fileKey)
{
    MakeDirty();
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::InitWithFile(nsIFile *file)
{
    MakeDirty();
    NS_ENSURE_ARG(file);

    // Until we have a version that extracts the FSSpec from the donor nsIFile just
    // grab the 
    char* aFilePath;
    file->GetPath(&aFilePath);

    // Be paranoid and see if we got a null string for the path
    if (aFilePath == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    mWorkingPath.SetString(aFilePath);
    nsAllocator::Free(aFilePath);
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::InitWithPath(const char *filePath)
{
    MakeDirty();
    NS_ENSURE_ARG(filePath);
    
    // Make sure there's a colon in the path and it is not the first character
    // so we know we got a full path, not a partial one
    if (strchr(filePath, ':') == 0 || *filePath == ':')
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // Just save the specified file path since we can't actually do anything
    // about turniung it into an FSSpec until the Create() method is called
    mWorkingPath.SetString(filePath);
    
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

	OSErr	err = noErr;
    char*	filePath = (char*) nsAllocator::Clone( mWorkingPath, strlen(mWorkingPath)+1 );
	size_t	inLength = strlen(filePath);

	if (inLength < 255)
	{
		Str255 pascalpath;
		myPLstrcpy(pascalpath, filePath);
		err = ::FSMakeFSSpec(0, 0, pascalpath, &mSpec);
	}
	else
		err = FSpLocationFromFullPath(inLength, filePath, &mSpec);

	if ((err == dirNFErr || err == bdNamErr))
	{
		const char* path = filePath;
		mSpec.vRefNum = 0;
		mSpec.parID = 0;

		do
		{
			// Locate the colon that terminates the node.
			// But if we've a partial path (starting with a colon), find the second one.
			const char* nextColon = strchr(path + (*path == ':'), ':');
			// Well, if there are no more colons, point to the end of the string.
			if (!nextColon)
				nextColon = path + strlen(path);

			// Make a pascal string out of this node.  Include initial
			// and final colon, if any!
			Str255 ppath;
			myPLstrncpy(ppath, path, nextColon - path + 1);
			
			// Use this string as a relative path using the directory created
			// on the previous round (or directory 0,0 on the first round).
			err = ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, ppath, &mSpec);

			// If this was the leaf node, then we are done.
			if (!*nextColon)
				break;

			// Since there's more to go, we have to get the directory ID, which becomes
			// the parID for the next round.
			if (err == noErr)
			{
				// The directory (or perhaps a file) exists. Find its dirID.
				long dirID;
				Boolean isDirectory;
				err = ::FSpGetDirectoryID(&mSpec, &dirID, &isDirectory);
				if (!isDirectory)
					err = dupFNErr; // oops! a file exists with that name.
				if (err != noErr)
					break;			// bail if we've got an error
				mSpec.parID = dirID;
			}
			else if (err == fnfErr)
			{
				// If we got "file not found", then we need to create a directory.
				err = ::FSpDirCreate(&mSpec, smCurrentScript, &mSpec.parID);
				// For some reason, this usually returns fnfErr, even though it works.
				if (err == fnfErr)
					err = noErr;
			}
			if (err != noErr)
				break;
			path = nextColon; // next round
		} while (true);
	}
	
	if (err != noErr)
		return (MacErrorMapper(err));

	switch (type)
	{
	    case NORMAL_FILE_TYPE:
	    	// We really should use some sort of meaningful file type/creator but where
	    	// do we get the info from?
		    err = ::FSpCreate(&mSpec, '????', '????', smCurrentScript);
	        return (MacErrorMapper(err));
	        break;

    	case DIRECTORY_TYPE:
		    err = ::FSpDirCreate(&mSpec, smCurrentScript, &mSpec.parID);
			// For some reason, this usually returns fnfErr, even though it works.
			if (err == fnfErr)
				err = noErr;
	        return (MacErrorMapper(err));
	        break;
	    
	    default:
	    	// For now just fall out of the switch into the default return NS_ERROR_FILE_UNKNOWN_TYPE
	    	break;

    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}
    
NS_IMETHODIMP  
nsLocalFile::AppendPath(const char *node)
{
    if ( (node == nsnull) || (*node == '/') || strchr(node, '\\') )
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    MakeDirty();

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
nsLocalFile::Normalize()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::GetLeafName(char * *aLeafName)
{
    NS_ENSURE_ARG_POINTER(aLeafName);

    const char* temp = mWorkingPath.GetBuffer();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = strrchr(temp, '\\');
    
    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    *aLeafName = (char*) nsAllocator::Clone(leaf, strlen(leaf)+1);
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::GetPath(char **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = (char*) nsAllocator::Clone(mWorkingPath, strlen(mWorkingPath)+1);
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
    nsCString destPath = inFilePath;
    nsAllocator::Free(inFilePath);

    destPath.Append("\\");

    if (newName == nsnull)
    {
        char *aFileName;
        sourceFile->GetLeafName(&aFileName);
        destPath.Append(aFileName);
        nsAllocator::Free(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }

           
    if (followSymlinks)
    {
       rv = sourceFile->GetTarget(&filePath);
    }
    else
    {
        rv = sourceFile->GetPath(&filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    //int copyOK;

    if (!move)
    {
        //copyOK = CopyFile(filePath, destPath, PR_TRUE);
    }
    else
    {
        //copyOK = MoveFile(filePath, destPath);
    }
    
    //if (!copyOK)  // CopyFile and MoveFile returns non-zero if succeeds (backward if you ask me).
    
    nsAllocator::Free(filePath);

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *newParentDir, const char *newName, PRBool followSymlinks, PRBool move)
{
    NS_ENSURE_ARG(newParentDir);
	nsresult rv = NS_OK;
	
    // check to see if this exists, otherwise return an error.
    PRBool sourceExists;
    Exists(&sourceExists);
    if (sourceExists == PR_FALSE)
    {
    	return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
    }

    // make sure destination exists and is a directory.  Create it if not there.
    PRBool parentExists;
    newParentDir->Exists(&parentExists);
    if (parentExists == PR_FALSE)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0);
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

                    nsCOMPtr<nsILocalFile> realDest;

                    rv = nsComponentManager::CreateInstance(NS_LOCAL_FILE_PROGID, 
                                                            nsnull, 
                                                            nsCOMTypeInfo<nsILocalFile>::GetIID(), 
                                                            getter_AddRefs(realDest));
                    if (NS_FAILED(rv)) 
                        return rv;

                    rv = realDest->InitWithPath(target);
                    
                    nsAllocator::Free(target);

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
    if (!isDir)
    {
        rv = CopySingleFile(this, newParentDir, newName, followSymlinks, move);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        // create a new target destination in the new parentDir;
        nsCOMPtr<nsILocalFile> target;

        rv = nsComponentManager::CreateInstance(NS_LOCAL_FILE_PROGID, 
                                                nsnull, 
                                                nsCOMTypeInfo<nsILocalFile>::GetIID(), 
                                                getter_AddRefs(target));
        if (NS_FAILED(rv)) 
            return rv;

        rv = target->InitWithFile(newParentDir);
        if (NS_FAILED(rv)) 
            return rv;
        
        char *allocatedNewName;
        if (!newName)
        {
            GetLeafName(&allocatedNewName);// this should be the leaf name of the 
        }
        else
        {
            allocatedNewName = (char*) nsAllocator::Clone( newName, strlen(newName)+1 );
        }
        
        rv = target->AppendPath(allocatedNewName);
        if (NS_FAILED(rv)) 
            return rv;

        nsAllocator::Free(allocatedNewName);

        target->Create(DIRECTORY_TYPE, 0);
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
        if (newName == nsnull)
        {
            char *aFileName;
            GetLeafName(&aFileName);
            InitWithFile(newParentDir);
            AppendPath(aFileName); 
            nsAllocator::Free(aFileName);
        }
        else
        {
            InitWithFile(newParentDir);
            AppendPath(newName);
        }
        MakeDirty();
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
nsLocalFile::Spawn(const char *args)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;


    return NS_ERROR_FILE_EXECUTION_FAILED;
}

NS_IMETHODIMP  
nsLocalFile::Delete(PRBool recursive)
{
    MakeDirty();
    
    PRBool isDir;
    
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    const char *filePath = mResolvedPath.GetBuffer();

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
        //rmdir(filePath);  // todo: save return value?
    }
    else
    {
        //remove(filePath); // todo: save return value?
    }
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDate(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    aLastModificationDate->hi = 0;
    aLastModificationDate->lo = 0;
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDate(PRInt64 aLastModificationDate)
{
    MakeDirty();

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModificationDateOfLink(PRInt64 *aLastModificationDate)
{
    NS_ENSURE_ARG(aLastModificationDate);
    
    aLastModificationDate->hi = 0;
    aLastModificationDate->lo = 0;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLastModificationDateOfLink(PRInt64 aLastModificationDate)
{
    MakeDirty();

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
    
    aFileSize->hi = 0;
    aFileSize->lo = 0;

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{

    return NS_OK;
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
        
	PRInt64 space64Bits;

	LL_I2L(space64Bits , LONG_MAX);

	XVolumeParam	pb;
	pb.ioCompletion = nsnull;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = nsnull;
	pb.ioVRefNum = mSpec.vRefNum;
	
	OSErr err = ::PBXGetVolInfoSync(&pb);
	
	if (err == noErr)
	{
		space64Bits.lo = pb.ioVFreeBytes.lo;
		space64Bits.hi = pb.ioVFreeBytes.hi;
	}
		
	*aDiskSpaceAvailable = space64Bits;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCString parentPath = mWorkingPath;

    PRInt32 offset = parentPath.RFindChar('\\');
    if (offset == -1)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    parentPath.Truncate(offset);

    const char* filePath = parentPath.GetBuffer();

    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    file->AddRef();
    file->InitWithPath(filePath);
    *aParent = file;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
            
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsWritable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
       
    return NS_OK;


}

NS_IMETHODIMP  
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsExecutable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsHidden(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsSpecial(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
    
    nsAllocator::Free(inFilePath);
    nsAllocator::Free(filePath);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsContainedIn(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsLocalFile::GetTarget(char **_retval)
{   
    NS_ENSURE_ARG(_retval);
    *_retval = nsnull;
    
    PRBool symLink;
    
    nsresult rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    if (!symLink)
    {
        return NS_ERROR_FILE_INVALID_PATH;
    }
        
    *_retval = (char*) nsAllocator::Clone( mResolvedPath, strlen(mResolvedPath)+1 );
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




