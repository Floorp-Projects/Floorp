/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */ 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *  Jungshik Shin <jshin@mailaps.org>
 *  Asaf Romano <mozilla.mano@sent.com>
 *  Mark Mentovai <mark@moxienet.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLocalFile.h"
#include "nsDirectoryServiceDefs.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDirectoryEnumerator.h"
#include "nsISimpleEnumerator.h"
#include "nsITimelineService.h"
#include "nsVoidArray.h"

#include "plbase64.h"
#include "prmem.h"
#include "nsCRT.h"
#include "nsHashKeys.h"

#include "MoreFilesX.h"
#include "FSCopyObject.h"
#include "nsAutoBuffer.h"
#include "nsTraceRefcntImpl.h"

// Mac Includes
#include <Carbon/Carbon.h>

// Unix Includes
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#if !defined(MAC_OS_X_VERSION_10_4) || MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_4
#define GetAliasSizeFromRecord(aliasRecord) aliasRecord.aliasSize
#else
#define GetAliasSizeFromRecord(aliasRecord) GetAliasSizeFromPtr(&aliasRecord)
#endif

#define CHECK_mBaseRef()                        \
    PR_BEGIN_MACRO                              \
        if (!mBaseRef)                          \
            return NS_ERROR_NOT_INITIALIZED;    \
    PR_END_MACRO

//*****************************************************************************
//  Static Function Prototypes
//*****************************************************************************

static nsresult MacErrorMapper(OSErr inErr);
static OSErr FindRunningAppBySignature(OSType aAppSig, ProcessSerialNumber& outPsn);
static void CopyUTF8toUTF16NFC(const nsACString& aSrc, nsAString& aResult);

//*****************************************************************************
//  Local Helper Classes
//*****************************************************************************

#pragma mark -
#pragma mark [FSRef operator==]

bool operator==(const FSRef& lhs, const FSRef& rhs)
{
  return (::FSCompareFSRefs(&lhs, &rhs) == noErr);
}

#pragma mark -
#pragma mark [StFollowLinksState]

class StFollowLinksState
{
  public:
    StFollowLinksState(nsLocalFile& aFile) :
        mFile(aFile)
    {
        mFile.GetFollowLinks(&mSavedState);
    }

    StFollowLinksState(nsLocalFile& aFile, PRBool followLinksState) :
        mFile(aFile)
    {
        mFile.GetFollowLinks(&mSavedState);
        mFile.SetFollowLinks(followLinksState);
    }

    ~StFollowLinksState()
    {
        mFile.SetFollowLinks(mSavedState);
    }
    
  private:
    nsLocalFile& mFile;
    PRBool mSavedState;
};

#pragma mark -
#pragma mark [nsDirEnumerator]

class nsDirEnumerator : public nsISimpleEnumerator,
                        public nsIDirectoryEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator() :
          mIterator(nsnull),
          mFSRefsArray(nsnull),
          mArrayCnt(0), mArrayIndex(0)
        {
        }

        nsresult Init(nsILocalFileMac* parent) 
        {
          NS_ENSURE_ARG(parent);
          
          OSErr err;
          nsresult rv;
          FSRef parentRef;
          
          rv = parent->GetFSRef(&parentRef);
          if (NS_FAILED(rv))
            return rv;
          
          mFSRefsArray = (FSRef *)nsMemory::Alloc(sizeof(FSRef)
                                                  * kRequestCountPerIteration);
          if (!mFSRefsArray)
            return NS_ERROR_OUT_OF_MEMORY;
          
          err = ::FSOpenIterator(&parentRef, kFSIterateFlat, &mIterator);
          if (err != noErr)
            return MacErrorMapper(err);
                              
          return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
          if (mNext == nsnull) {
            if (mArrayIndex >= mArrayCnt) {
              ItemCount actualCnt;
              OSErr err = ::FSGetCatalogInfoBulk(mIterator,
                                           kRequestCountPerIteration,
                                           &actualCnt,
                                           nsnull,
                                           kFSCatInfoNone,
                                           nsnull,
                                           mFSRefsArray,
                                           nsnull,
                                           nsnull);
            
              if (err == noErr || err == errFSNoMoreItems) {
                mArrayCnt = actualCnt;
                mArrayIndex = 0;
              }
            }
            if (mArrayIndex < mArrayCnt) {
              nsLocalFile *newFile = new nsLocalFile;
              if (!newFile)
                return NS_ERROR_OUT_OF_MEMORY;
              FSRef fsRef = mFSRefsArray[mArrayIndex];
              if (NS_FAILED(newFile->InitWithFSRef(&fsRef)))
                return NS_ERROR_FAILURE;
              mArrayIndex++;
              mNext = newFile;
            } 
          }
          *result = mNext != nsnull;
          if (!*result)
            Close();
          return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
            NS_ENSURE_ARG_POINTER(result);
            *result = nsnull;

            nsresult rv;
            PRBool hasMore;
            rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            *result = mNext;        // might return nsnull
            NS_IF_ADDREF(*result);

            mNext = nsnull;
            return NS_OK;
        }

        NS_IMETHOD GetNextFile(nsIFile **result)
        {
            *result = nsnull;
            PRBool hasMore = PR_FALSE;
            nsresult rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv) || !hasMore)
                return rv;
            *result = mNext;
            NS_IF_ADDREF(*result);
            mNext = nsnull;
            return NS_OK;
        }

        NS_IMETHOD Close()
        {
          if (mIterator) {
            ::FSCloseIterator(mIterator);
            mIterator = nsnull;
          }
          if (mFSRefsArray) {
            nsMemory::Free(mFSRefsArray);
            mFSRefsArray = nsnull;
          }
          return NS_OK;
        }

    private:
        ~nsDirEnumerator() 
        {
          Close();
        }

    protected:
        // According to Apple doc, request the number of objects
        // per call that will fit in 4 VM pages.
        enum {
          kRequestCountPerIteration = ((4096 * 4) / sizeof(FSRef))
        };
        
        nsCOMPtr<nsILocalFileMac>   mNext;
        
        FSIterator              mIterator;
        FSRef                   *mFSRefsArray;
        PRInt32                 mArrayCnt, mArrayIndex;
};

NS_IMPL_ISUPPORTS2(nsDirEnumerator, nsISimpleEnumerator, nsIDirectoryEnumerator)

#pragma mark -
#pragma mark [StAEDesc]

class StAEDesc: public AEDesc
{
public:
    StAEDesc()
    {
      descriptorType = typeNull;
      dataHandle = nil;
    }
              
    ~StAEDesc()
    {
      ::AEDisposeDesc(this);
    }
};

#define FILENAME_BUFFER_SIZE 512

//*****************************************************************************
//  nsLocalFile
//*****************************************************************************

const char      nsLocalFile::kPathSepChar = '/';
const PRUnichar nsLocalFile::kPathSepUnichar = '/';

// The HFS+ epoch is Jan. 1, 1904 GMT - differs from HFS in which times were local
// The NSPR epoch is Jan. 1, 1970 GMT
// 2082844800 is the difference in seconds between those dates
const PRInt64   nsLocalFile::kJanuaryFirst1970Seconds = 2082844800LL;

#pragma mark -
#pragma mark [CTORs/DTOR]

nsLocalFile::nsLocalFile() :
  mBaseRef(nsnull),
  mTargetRef(nsnull),
  mCachedFSRefValid(PR_FALSE),
  mFollowLinks(PR_TRUE),
  mFollowLinksDirty(PR_TRUE)
{
}

nsLocalFile::nsLocalFile(const nsLocalFile& src) :
  mBaseRef(src.mBaseRef),
  mTargetRef(src.mTargetRef),
  mCachedFSRef(src.mCachedFSRef),
  mCachedFSRefValid(src.mCachedFSRefValid),
  mFollowLinks(src.mFollowLinks),
  mFollowLinksDirty(src.mFollowLinksDirty)
{
  // A CFURLRef is immutable so no need to copy, just retain.
  if (mBaseRef)
    ::CFRetain(mBaseRef);
  if (mTargetRef)
    ::CFRetain(mTargetRef);
}

nsLocalFile::~nsLocalFile()
{
  if (mBaseRef)
    ::CFRelease(mBaseRef);
  if (mTargetRef)
    ::CFRelease(mTargetRef);
}


//*****************************************************************************
//  nsLocalFile::nsISupports
//*****************************************************************************
#pragma mark -
#pragma mark [nsISupports]

NS_IMPL_THREADSAFE_ISUPPORTS4(nsLocalFile,
                              nsILocalFileMac,
                              nsILocalFile,
                              nsIFile,
                              nsIHashable)
                              
NS_METHOD nsLocalFile::nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
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


//*****************************************************************************
//  nsLocalFile::nsIFile
//*****************************************************************************
#pragma mark -
#pragma mark [nsIFile]

/* void append (in AString node); */
NS_IMETHODIMP nsLocalFile::Append(const nsAString& aNode)
{
  return AppendNative(NS_ConvertUTF16toUTF8(aNode));
}

/* [noscript] void appendNative (in ACString node); */
NS_IMETHODIMP nsLocalFile::AppendNative(const nsACString& aNode)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  nsACString::const_iterator start, end;
  aNode.BeginReading(start);
  aNode.EndReading(end);
  if (FindCharInReadable(kPathSepChar, start, end))
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;

  CFStringRef nodeStrRef = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                  PromiseFlatCString(aNode).get(),
                                  kCFStringEncodingUTF8);
  if (nodeStrRef) {
    CFURLRef newRef = ::CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
                                  mBaseRef, nodeStrRef, PR_FALSE);
    ::CFRelease(nodeStrRef);
    if (newRef) {
      SetBaseRef(newRef);
      ::CFRelease(newRef);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

/* void normalize (); */
NS_IMETHODIMP nsLocalFile::Normalize()
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  // CFURL doesn't doesn't seem to resolve paths containing relative
  // components, so we'll nick the stdlib code from nsLocalFileUnix
  UInt8 path[PATH_MAX] = "";
  Boolean success;
  success = ::CFURLGetFileSystemRepresentation(mBaseRef, true, path, PATH_MAX);
  if (!success)
    return NS_ERROR_FAILURE;

  char resolved_path[PATH_MAX] = "";
  char *resolved_path_ptr = nsnull;
  resolved_path_ptr = realpath((char*)path, resolved_path);

  // if there is an error, the return is null.
  if (!resolved_path_ptr)
      return NSRESULT_FOR_ERRNO();

  // Need to know whether we're a directory to create a new CFURLRef
  PRBool isDirectory;
  nsresult rv = IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_ERROR_FAILURE;
  CFStringRef pathStrRef =
    ::CFStringCreateWithCString(kCFAllocatorDefault,
                                resolved_path,
                                kCFStringEncodingUTF8);
  if (pathStrRef) {
    CFURLRef newURLRef =
      ::CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathStrRef,
                                      kCFURLPOSIXPathStyle, isDirectory);
    if (newURLRef) {
      SetBaseRef(newURLRef);
      ::CFRelease(newURLRef);
      rv = NS_OK;
    }
    ::CFRelease(pathStrRef);
  }

  return rv;
}

/* void create (in unsigned long type, in unsigned long permissions); */
NS_IMETHODIMP nsLocalFile::Create(PRUint32 type, PRUint32 permissions)
{
  if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
    return NS_ERROR_FILE_UNKNOWN_TYPE;

  // Check we are correctly initialized.
  CHECK_mBaseRef();
  
  nsStringArray nonExtantNodes;
  CFURLRef pathURLRef = mBaseRef;
  FSRef pathFSRef;
  CFStringRef leafStrRef = nsnull;
  nsAutoBuffer<UniChar, FILENAME_BUFFER_SIZE> buffer;
  Boolean success;
  
  // Work backwards through the path to find the last node which
  // exists. Place the nodes which don't exist in an array and we'll
  // create those below.
  while ((success = ::CFURLGetFSRef(pathURLRef, &pathFSRef)) == false) {
    leafStrRef = ::CFURLCopyLastPathComponent(pathURLRef);
    if (!leafStrRef)
      break;
    CFIndex leafLen = ::CFStringGetLength(leafStrRef);
    if (!buffer.EnsureElemCapacity(leafLen + 1))
      break;
    ::CFStringGetCharacters(leafStrRef, CFRangeMake(0, leafLen), buffer.get());
    buffer.get()[leafLen] = '\0';
    nonExtantNodes.AppendString(nsString(nsDependentString(buffer.get())));
    ::CFRelease(leafStrRef);
    leafStrRef = nsnull;
    
    // Get the parent of the leaf for the next go round
    CFURLRef parent = ::CFURLCreateCopyDeletingLastPathComponent(NULL, pathURLRef);
    if (!parent)
      break;
    if (pathURLRef != mBaseRef)
      ::CFRelease(pathURLRef);
    pathURLRef = parent;
  }
  if (pathURLRef != mBaseRef)
    ::CFRelease(pathURLRef);
  if (leafStrRef != nsnull)
    ::CFRelease(leafStrRef);
  if (!success)
    return NS_ERROR_FAILURE;
  PRInt32 nodesToCreate = nonExtantNodes.Count();
  if (nodesToCreate == 0)
    return NS_ERROR_FILE_ALREADY_EXISTS;
  
  OSErr err;    
  nsAutoString nextNodeName;
  for (PRInt32 i = nodesToCreate - 1; i > 0; i--) {
    nonExtantNodes.StringAt(i, nextNodeName);
    err = ::FSCreateDirectoryUnicode(&pathFSRef,
                                      nextNodeName.Length(),
                                      (const UniChar *)nextNodeName.get(),
                                      kFSCatInfoNone,
                                      nsnull, &pathFSRef, nsnull, nsnull);
    if (err != noErr)
      return MacErrorMapper(err);
  }
  nonExtantNodes.StringAt(0, nextNodeName);
  if (type == NORMAL_FILE_TYPE) {
    err = ::FSCreateFileUnicode(&pathFSRef,
                                nextNodeName.Length(),
                                (const UniChar *)nextNodeName.get(),
                                kFSCatInfoNone,
                                nsnull, nsnull, nsnull);
  }
  else {
    err = ::FSCreateDirectoryUnicode(&pathFSRef,
                                    nextNodeName.Length(),
                                    (const UniChar *)nextNodeName.get(),
                                    kFSCatInfoNone,
                                    nsnull, nsnull, nsnull, nsnull);
  }
            
  return MacErrorMapper(err);
}

/* attribute AString leafName; */
NS_IMETHODIMP nsLocalFile::GetLeafName(nsAString& aLeafName)
{
  nsCAutoString nativeString;
  nsresult rv = GetNativeLeafName(nativeString);
  if (NS_FAILED(rv))
    return rv;
  CopyUTF8toUTF16NFC(nativeString, aLeafName);
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetLeafName(const nsAString& aLeafName)
{
  return SetNativeLeafName(NS_ConvertUTF16toUTF8(aLeafName));
}

/* [noscript] attribute ACString nativeLeafName; */
NS_IMETHODIMP nsLocalFile::GetNativeLeafName(nsACString& aNativeLeafName)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  nsresult rv = NS_ERROR_FAILURE;
  CFStringRef leafStrRef = ::CFURLCopyLastPathComponent(mBaseRef);
  if (leafStrRef) {
    rv = CFStringReftoUTF8(leafStrRef, aNativeLeafName);
    ::CFRelease(leafStrRef);
  }
  return rv;                                  
}

NS_IMETHODIMP nsLocalFile::SetNativeLeafName(const nsACString& aNativeLeafName)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  nsresult rv = NS_ERROR_FAILURE;
  CFURLRef parentURLRef = ::CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, mBaseRef);
  if (parentURLRef) {
    CFStringRef nodeStrRef = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                    PromiseFlatCString(aNativeLeafName).get(),
                                    kCFStringEncodingUTF8);

    if (nodeStrRef) {
      CFURLRef newURLRef = ::CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
                                    parentURLRef, nodeStrRef, PR_FALSE);
      if (newURLRef) {
        SetBaseRef(newURLRef);
        ::CFRelease(newURLRef);
        rv = NS_OK;
      }
      ::CFRelease(nodeStrRef);
    }
    ::CFRelease(parentURLRef);
  }
  return rv;
}

/* void copyTo (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString& newName)
{
  return CopyInternal(newParentDir, newName, PR_FALSE);
}

/* [noscrpit] void CopyToNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString& newName)
{
  return CopyInternal(newParentDir, NS_ConvertUTF8toUTF16(newName), PR_FALSE);
}

/* void copyToFollowingLinks (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString& newName)
{
  return CopyInternal(newParentDir, newName, PR_TRUE);
}

/* [noscript] void copyToFollowingLinksNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString& newName)
{
  return CopyInternal(newParentDir, NS_ConvertUTF8toUTF16(newName), PR_TRUE);
}

/* void moveTo (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString& newName)
{
  return MoveToNative(newParentDir, NS_ConvertUTF16toUTF8(newName));
}

/* [noscript] void moveToNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString& newName)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  StFollowLinksState followLinks(*this, PR_FALSE);

  PRBool isDirectory;
  nsresult rv = IsDirectory(&isDirectory);
  if (NS_FAILED(rv))
    return rv;

  // Get the source path.
  nsCAutoString srcPath;
  rv = GetNativePath(srcPath);
  if (NS_FAILED(rv))
    return rv;

  // Build the destination path.
  nsCOMPtr<nsIFile> parentDir = newParentDir;
  if (!parentDir) {
    if (newName.IsEmpty())
      return NS_ERROR_INVALID_ARG;
    rv = GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv))
      return rv;   
  }
  else {
    PRBool exists;
    rv = parentDir->Exists(&exists);
    if (NS_FAILED(rv))
      return rv;
    if (!exists) {
      rv = parentDir->Create(nsIFile::DIRECTORY_TYPE, 0777);
      if (NS_FAILED(rv))
        return rv;
    }
  }

  nsCAutoString destPath;
  rv = parentDir->GetNativePath(destPath);
  if (NS_FAILED(rv))
    return rv;

  if (!newName.IsEmpty())
    destPath.Append(NS_LITERAL_CSTRING("/") + newName);
  else {
    nsCAutoString leafName;
    rv = GetNativeLeafName(leafName);
    if (NS_FAILED(rv))
      return rv;
    destPath.Append(NS_LITERAL_CSTRING("/") + leafName);
  }

  // Perform the move.
  if (rename(srcPath.get(), destPath.get()) != 0) {
    if (errno == EXDEV) {
      // Can't move across volume (device) boundaries.  Copy and remove.
      rv = CopyToNative(parentDir, newName);
      if (NS_SUCCEEDED(rv)) {
        // Permit removal failure.
        Remove(PR_TRUE);
      }
    }
    else
      rv = NSRESULT_FOR_ERRNO();

    if (NS_FAILED(rv))
      return rv;
  }

  // Update |this| to refer to the moved file.
  CFURLRef newBaseRef =
   ::CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*)destPath.get(),
                                             destPath.Length(), isDirectory);
  if (!newBaseRef)
    return NS_ERROR_FAILURE;
  SetBaseRef(newBaseRef);
  ::CFRelease(newBaseRef);

  return rv;
}

/* void remove (in boolean recursive); */
NS_IMETHODIMP nsLocalFile::Remove(PRBool recursive)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  // XXX If we're an alias, never remove target
  StFollowLinksState followLinks(*this, PR_FALSE);

  PRBool isDirectory;
  nsresult rv = IsDirectory(&isDirectory);
  if (NS_FAILED(rv))
    return rv;

  if (recursive && isDirectory) {
    FSRef fsRef;
    rv = GetFSRefInternal(fsRef);
    if (NS_FAILED(rv))
      return rv;

    // Call MoreFilesX to do a recursive removal.
    OSStatus err = ::FSDeleteContainer(&fsRef);
    rv = MacErrorMapper(err);
  }
  else {
    nsCAutoString path;
    rv = GetNativePath(path);
    if (NS_FAILED(rv))
      return rv;

    const char* pathPtr = path.get();
    int status;
    if (isDirectory)
      status = rmdir(pathPtr);
    else
      status = unlink(pathPtr);

    if (status != 0)
      rv = NSRESULT_FOR_ERRNO();
  }

  mCachedFSRefValid = PR_FALSE;
  return rv;
}

/* attribute unsigned long permissions; */
NS_IMETHODIMP nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
  NS_ENSURE_ARG_POINTER(aPermissions);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoPermissions, &catalogInfo,
                  nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  FSPermissionInfo *permPtr = (FSPermissionInfo*)catalogInfo.permissions;
  *aPermissions = permPtr->mode;
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
  
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoPermissions, &catalogInfo,
                  nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  FSPermissionInfo *permPtr = (FSPermissionInfo*)catalogInfo.permissions;
  permPtr->mode = (UInt16)aPermissions;
  err = ::FSSetCatalogInfo(&fsRef, kFSCatInfoPermissions, &catalogInfo);
  return MacErrorMapper(err);
}

/* attribute unsigned long permissionsOfLink; */
NS_IMETHODIMP nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissionsOfLink)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute PRInt64 lastModifiedTime; */
NS_IMETHODIMP nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModifiedTime)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(aLastModifiedTime);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoContentMod, &catalogInfo,
                                nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  *aLastModifiedTime = HFSPlustoNSPRTime(catalogInfo.contentModDate);  
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  OSErr err;
  nsresult rv;
  FSRef fsRef;
  FSCatalogInfo catalogInfo;

  rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;

  FSRef parentRef;
  PRBool notifyParent;

  /* Get the node flags, the content modification date and time, and the parent ref */
  err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags + kFSCatInfoContentMod,
                           &catalogInfo, NULL, NULL, &parentRef);
  if (err != noErr)
    return MacErrorMapper(err);
  
  /* Notify the parent if this is a file */
  notifyParent = (0 == (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask));

  NSPRtoHFSPlusTime(aLastModifiedTime, catalogInfo.contentModDate);
  err = ::FSSetCatalogInfo(&fsRef, kFSCatInfoContentMod, &catalogInfo);
  if (err != noErr)
    return MacErrorMapper(err);

  /* Send a notification for the parent of the file, or for the directory */
  err = FNNotify(notifyParent ? &parentRef : &fsRef, kFNDirectoryModifiedMessage, kNilOptions);
  if (err != noErr)
    return MacErrorMapper(err);

  return NS_OK;
}

/* attribute PRInt64 lastModifiedTimeOfLink; */
NS_IMETHODIMP nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModifiedTimeOfLink)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTimeOfLink)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute PRInt64 fileSize; */
NS_IMETHODIMP nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
  NS_ENSURE_ARG_POINTER(aFileSize);
  *aFileSize = 0;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
      
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags + kFSCatInfoDataSizes, &catalogInfo,
                                  nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  
  // FSGetCatalogInfo can return a bogus size for directories sometimes, so only
  // rely on the answer for files
  if ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == 0)
      *aFileSize = catalogInfo.dataLogicalSize;
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
  
  SInt16 refNum;    
  OSErr err = ::FSOpenFork(&fsRef, 0, nsnull, fsWrPerm, &refNum);
  if (err != noErr)
    return MacErrorMapper(err);
  err = ::FSSetForkSize(refNum, fsFromStart, aFileSize);
  ::FSCloseFork(refNum);  
  
  return MacErrorMapper(err);
}

/* readonly attribute PRInt64 fileSizeOfLink; */
NS_IMETHODIMP nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSizeOfLink)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(aFileSizeOfLink);
  
  StFollowLinksState followLinks(*this, PR_FALSE);
  return GetFileSize(aFileSizeOfLink);
}

/* readonly attribute AString target; */
NS_IMETHODIMP nsLocalFile::GetTarget(nsAString& aTarget)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] readonly attribute ACString nativeTarget; */
NS_IMETHODIMP nsLocalFile::GetNativeTarget(nsACString& aNativeTarget)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AString path; */
NS_IMETHODIMP nsLocalFile::GetPath(nsAString& aPath)
{
  nsCAutoString nativeString;
  nsresult rv = GetNativePath(nativeString);
  if (NS_FAILED(rv))
    return rv;
  CopyUTF8toUTF16NFC(nativeString, aPath);
  return NS_OK;
}

/* [noscript] readonly attribute ACString nativePath; */
NS_IMETHODIMP nsLocalFile::GetNativePath(nsACString& aNativePath)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  nsresult rv = NS_ERROR_FAILURE;
  CFStringRef pathStrRef = ::CFURLCopyFileSystemPath(mBaseRef, kCFURLPOSIXPathStyle);
  if (pathStrRef) {
    rv = CFStringReftoUTF8(pathStrRef, aNativePath);
    ::CFRelease(pathStrRef);
  }
  return rv;
}

/* boolean exists (); */
NS_IMETHODIMP nsLocalFile::Exists(PRBool *_retval)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  if (NS_SUCCEEDED(GetFSRefInternal(fsRef, PR_TRUE))) {
    *_retval = PR_TRUE;
  }
  
  return NS_OK;
}

/* boolean isWritable (); */
NS_IMETHODIMP nsLocalFile::IsWritable(PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mBaseRef();

    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_FALSE;
    
    FSRef fsRef;
    nsresult rv = GetFSRefInternal(fsRef);
    if (NS_FAILED(rv))
      return rv;
    if (::FSCheckLock(&fsRef) == noErr) {      
      PRUint32 permissions;
      rv = GetPermissions(&permissions);
      if (NS_FAILED(rv))
        return rv;
      *_retval = ((permissions & S_IWUSR) != 0);
    }
    return NS_OK;
}

/* boolean isReadable (); */
NS_IMETHODIMP nsLocalFile::IsReadable(PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mBaseRef();

    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = PR_FALSE;
    
    PRUint32 permissions;
    nsresult rv = GetPermissions(&permissions);
    if (NS_FAILED(rv))
      return rv;
    *_retval = ((permissions & S_IRUSR) != 0);
    return NS_OK;
}

/* boolean isExecutable (); */
NS_IMETHODIMP nsLocalFile::IsExecutable(PRBool *_retval)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  LSRequestedInfo theInfoRequest = kLSRequestAllInfo;
  LSItemInfoRecord theInfo;
  if (::LSCopyItemInfoForRef(&fsRef, theInfoRequest, &theInfo) == noErr) {
    if ((theInfo.flags & kLSItemInfoIsApplication) != 0)
    *_retval = PR_TRUE;
  }
  return NS_OK;
}

/* boolean isHidden (); */
NS_IMETHODIMP nsLocalFile::IsHidden(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
  
  FSCatalogInfo catalogInfo;
  HFSUniStr255 leafName;  
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoFinderInfo, &catalogInfo,
                                &leafName, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
      
  FileInfo *fInfoPtr = (FileInfo *)(catalogInfo.finderInfo); // Finder flags are in the same place whether we use FileInfo or FolderInfo
  if ((fInfoPtr->finderFlags & kIsInvisible) != 0) {
    *_retval = PR_TRUE;
  }
  else {
    // If the leaf name begins with a '.', consider it invisible
    if (leafName.length >= 1 && leafName.unicode[0] == UniChar('.'))
      *_retval = PR_TRUE;
  }
  return NS_OK;
}

/* boolean isDirectory (); */
NS_IMETHODIMP nsLocalFile::IsDirectory(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;
    
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags, &catalogInfo,
                                nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  *_retval = ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0);
  return NS_OK;
}

/* boolean isFile (); */
NS_IMETHODIMP nsLocalFile::IsFile(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;
    
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags, &catalogInfo,
                                nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  *_retval = ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == 0);
  return NS_OK;
}

/* boolean isSymlink (); */
NS_IMETHODIMP nsLocalFile::IsSymlink(PRBool *_retval)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG(_retval);
  *_retval = PR_FALSE;

  // Check we are correctly initialized.
  CHECK_mBaseRef();

  FSRef fsRef;
  if (::CFURLGetFSRef(mBaseRef, &fsRef)) {
    Boolean isAlias, isFolder;
    if (::FSIsAliasFile(&fsRef, &isAlias, &isFolder) == noErr)
        *_retval = isAlias;
  }
  return NS_OK;
}

/* boolean isSpecial (); */
NS_IMETHODIMP nsLocalFile::IsSpecial(PRBool *_retval)
{
    NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIFile clone (); */
NS_IMETHODIMP nsLocalFile::Clone(nsIFile **_retval)
{
    // Just copy-construct ourselves
    *_retval = new nsLocalFile(*this);
    if (!*_retval)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*_retval);
    
    return NS_OK;
}

/* boolean equals (in nsIFile inFile); */
NS_IMETHODIMP nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    return EqualsInternal(inFile, PR_TRUE, _retval);
}

nsresult
nsLocalFile::EqualsInternal(nsISupports* inFile, PRBool aUpdateCache,
                            PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  nsCOMPtr<nsILocalFileMac> inMacFile(do_QueryInterface(inFile));
  if (!inFile)
    return NS_OK;
    
  nsLocalFile* inLF =
      NS_STATIC_CAST(nsLocalFile*, (nsILocalFileMac*) inMacFile);

  // If both exist, compare FSRefs
  FSRef thisFSRef, inFSRef;
  nsresult rv1 = GetFSRefInternal(thisFSRef, aUpdateCache);
  nsresult rv2 = inLF->GetFSRefInternal(inFSRef, aUpdateCache);
  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
    *_retval = (thisFSRef == inFSRef);
    return NS_OK;
  }
  // If one exists and the other doesn't, not equal  
  if (rv1 != rv2)
    return NS_OK;
    
  // Arg, we have to get their paths and compare
  nsCAutoString thisPath, inPath;
  if (NS_FAILED(GetNativePath(thisPath)))
    return NS_ERROR_FAILURE;
  if (NS_FAILED(inMacFile->GetNativePath(inPath)))
    return NS_ERROR_FAILURE;
  *_retval = thisPath.Equals(inPath);
  
  return NS_OK;
}

/* boolean contains (in nsIFile inFile, in boolean recur); */
NS_IMETHODIMP nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  PRBool isDir;
  nsresult rv = IsDirectory(&isDir);
  if (NS_FAILED(rv))
    return rv;
  if (!isDir)
    return NS_OK;     // must be a dir to contain someone

  nsCAutoString thisPath, inPath;
  if (NS_FAILED(GetNativePath(thisPath)) || NS_FAILED(inFile->GetNativePath(inPath)))
    return NS_ERROR_FAILURE;
  size_t thisPathLen = thisPath.Length();
  if ((inPath.Length() > thisPathLen + 1) && (strncasecmp(thisPath.get(), inPath.get(), thisPathLen) == 0)) {
    // Now make sure that the |inFile|'s path has a separator at thisPathLen,
    // and there's at least one more character after that.
    if (inPath[thisPathLen] == kPathSepChar)
      *_retval = PR_TRUE;
  }  
  return NS_OK;
}

/* readonly attribute nsIFile parent; */
NS_IMETHODIMP nsLocalFile::GetParent(nsIFile * *aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nsnull;

  // Check we are correctly initialized.
  CHECK_mBaseRef();

  nsLocalFile *newFile = nsnull;

  // If it can be determined without error that a file does not
  // have a parent, return nsnull for the parent and NS_OK as the result.
  // See bug 133617.
  nsresult rv = NS_OK;
  CFURLRef parentURLRef = ::CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, mBaseRef);
  if (parentURLRef) {
    rv = NS_ERROR_FAILURE;
    newFile = new nsLocalFile;
    if (newFile) {
      rv = newFile->InitWithCFURL(parentURLRef);
      if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(*aParent = newFile);
        rv = NS_OK;
      }
    }
    ::CFRelease(parentURLRef);
  }  
  return rv;
}

/* readonly attribute nsISimpleEnumerator directoryEntries; */
NS_IMETHODIMP nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator **aDirectoryEntries)
{
  NS_ENSURE_ARG_POINTER(aDirectoryEntries);
  *aDirectoryEntries = nsnull;

  nsresult rv;
  PRBool isDir;
  rv = IsDirectory(&isDir);
  if (NS_FAILED(rv)) 
    return rv;
  if (!isDir)
    return NS_ERROR_FILE_NOT_DIRECTORY;

  nsDirEnumerator* dirEnum = new nsDirEnumerator;
  if (dirEnum == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(dirEnum);
  rv = dirEnum->Init(this);
  if (NS_FAILED(rv)) {
    NS_RELEASE(dirEnum);
    return rv;
  }
  *aDirectoryEntries = dirEnum;
  
  return NS_OK;
}


//*****************************************************************************
//  nsLocalFile::nsILocalFile
//*****************************************************************************
#pragma mark -
#pragma mark [nsILocalFile]

/* void initWithPath (in AString filePath); */
NS_IMETHODIMP nsLocalFile::InitWithPath(const nsAString& filePath)
{
  return InitWithNativePath(NS_ConvertUTF16toUTF8(filePath));
}

/* [noscript] void initWithNativePath (in ACString filePath); */
NS_IMETHODIMP nsLocalFile::InitWithNativePath(const nsACString& filePath)
{
  nsCAutoString fixedPath;
  if (Substring(filePath, 0, 2).EqualsLiteral("~/")) {
    nsCOMPtr<nsIFile> homeDir;
    nsCAutoString homePath;
    nsresult rv = NS_GetSpecialDirectory(NS_OS_HOME_DIR,
                                        getter_AddRefs(homeDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = homeDir->GetNativePath(homePath);
    NS_ENSURE_SUCCESS(rv, rv);
    
    fixedPath = homePath + Substring(filePath, 1, filePath.Length() - 1);
  }
  else if (filePath.IsEmpty() || filePath.First() != '/')
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  else
    fixedPath.Assign(filePath);

  // A path with consecutive '/'s which are not between
  // nodes crashes CFURLGetFSRef(). Consecutive '/'s which
  // are between actual nodes are OK. So, convert consecutive
  // '/'s to a single one.
  fixedPath.ReplaceSubstring("//", "/");

  // On 10.2, huge paths also crash CFURLGetFSRef()
  if (fixedPath.Length() > PATH_MAX)
    return NS_ERROR_FILE_NAME_TOO_LONG;

  CFStringRef pathAsCFString;
  CFURLRef pathAsCFURL;

  pathAsCFString = ::CFStringCreateWithCString(nsnull, fixedPath.get(), kCFStringEncodingUTF8);
  if (!pathAsCFString)
    return NS_ERROR_FAILURE;
  pathAsCFURL = ::CFURLCreateWithFileSystemPath(nsnull, pathAsCFString, kCFURLPOSIXPathStyle, PR_FALSE);
  if (!pathAsCFURL) {
    ::CFRelease(pathAsCFString);
    return NS_ERROR_FAILURE;
  }
  SetBaseRef(pathAsCFURL);
  ::CFRelease(pathAsCFURL);
  ::CFRelease(pathAsCFString);
  return NS_OK;
}

/* void initWithFile (in nsILocalFile aFile); */
NS_IMETHODIMP nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  
  nsCOMPtr<nsILocalFileMac> aFileMac(do_QueryInterface(aFile));
  if (!aFileMac)
    return NS_ERROR_UNEXPECTED;
  CFURLRef urlRef;
  nsresult rv = aFileMac->GetCFURL(&urlRef);
  if (NS_FAILED(rv))
    return rv;
  rv = InitWithCFURL(urlRef);
  ::CFRelease(urlRef);
  return rv;
}

/* attribute PRBool followLinks; */
NS_IMETHODIMP nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
  NS_ENSURE_ARG_POINTER(aFollowLinks);
  
  *aFollowLinks = mFollowLinks;
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
  if (aFollowLinks != mFollowLinks) {
    mFollowLinks = aFollowLinks;
    UpdateTargetRef();
  }
  return NS_OK;
}

/* [noscript] PRFileDescStar openNSPRFileDesc (in long flags, in long mode); */
NS_IMETHODIMP nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCAutoString path;
  nsresult rv = GetPathInternal(path);
  if (NS_FAILED(rv))
    return rv;
    
  *_retval = PR_Open(path.get(), flags, mode);
  if (! *_retval)
    return NS_ErrorAccordingToNSPR();

  return NS_OK;
}

/* [noscript] FILE openANSIFileDesc (in string mode); */
NS_IMETHODIMP nsLocalFile::OpenANSIFileDesc(const char *mode, FILE **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCAutoString path;
  nsresult rv = GetPathInternal(path);
  if (NS_FAILED(rv))
    return rv;
    
  *_retval = fopen(path.get(), mode);
  if (! *_retval)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/* [noscript] PRLibraryStar load (); */
NS_IMETHODIMP nsLocalFile::Load(PRLibrary **_retval)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(_retval);

  NS_TIMELINE_START_TIMER("PR_LoadLibrary");

  nsCAutoString path;
  nsresult rv = GetPathInternal(path);
  if (NS_FAILED(rv))
    return rv;

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcntImpl::SetActivityIsLegal(PR_FALSE);
#endif

  *_retval = PR_LoadLibrary(path.get());

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcntImpl::SetActivityIsLegal(PR_TRUE);
#endif

  NS_TIMELINE_STOP_TIMER("PR_LoadLibrary");
  NS_TIMELINE_MARK_TIMER1("PR_LoadLibrary", path.get());

  if (!*_retval)
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

/* readonly attribute PRInt64 diskSpaceAvailable; */
NS_IMETHODIMP nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  NS_ENSURE_ARG_POINTER(aDiskSpaceAvailable);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  OSErr err;
  FSCatalogInfo catalogInfo;
  err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoVolume, &catalogInfo,
                           nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  
  FSVolumeInfo volumeInfo;  
  err = ::FSGetVolumeInfo(catalogInfo.volume, 0, nsnull, kFSVolInfoSizes,
                          &volumeInfo, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
    
  *aDiskSpaceAvailable = volumeInfo.freeBytes;
  return NS_OK;
}

/* void appendRelativePath (in AString relativeFilePath); */
NS_IMETHODIMP nsLocalFile::AppendRelativePath(const nsAString& relativeFilePath)
{
  return AppendRelativeNativePath(NS_ConvertUTF16toUTF8(relativeFilePath));
}

/* [noscript] void appendRelativeNativePath (in ACString relativeFilePath); */
NS_IMETHODIMP nsLocalFile::AppendRelativeNativePath(const nsACString& relativeFilePath)
{  
  if (relativeFilePath.IsEmpty())
    return NS_OK;
  // No leading '/' 
  if (relativeFilePath.First() == '/')
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;

  // Parse the nodes and call Append() for each
  nsACString::const_iterator nodeBegin, pathEnd;
  relativeFilePath.BeginReading(nodeBegin);
  relativeFilePath.EndReading(pathEnd);
  nsACString::const_iterator nodeEnd(nodeBegin);
  
  while (nodeEnd != pathEnd) {
    FindCharInReadable(kPathSepChar, nodeEnd, pathEnd);
    nsresult rv = AppendNative(Substring(nodeBegin, nodeEnd));
    if (NS_FAILED(rv))
      return rv;
    if (nodeEnd != pathEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
      ++nodeEnd;
    nodeBegin = nodeEnd;
  }
  return NS_OK;
}

/* attribute ACString persistentDescriptor; */
NS_IMETHODIMP nsLocalFile::GetPersistentDescriptor(nsACString& aPersistentDescriptor)
{
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  AliasHandle aliasH;
  OSErr err = ::FSNewAlias(nsnull, &fsRef, &aliasH);
  if (err != noErr)
    return MacErrorMapper(err);
    
   PRUint32 bytes = ::GetHandleSize((Handle) aliasH);
   ::HLock((Handle) aliasH);
   // Passing nsnull for dest makes NULL-term string
   char* buf = PL_Base64Encode((const char*)*aliasH, bytes, nsnull);
   ::DisposeHandle((Handle) aliasH);
   NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);
   
   aPersistentDescriptor = buf;
   PR_Free(buf);

  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetPersistentDescriptor(const nsACString& aPersistentDescriptor)
{
  if (aPersistentDescriptor.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  // Support pathnames as user-supplied descriptors if they begin with '/'
  // or '~'.  These characters do not collide with the base64 set used for
  // encoding alias records.
  char first = aPersistentDescriptor.First();
  if (first == '/' || first == '~')
    return InitWithNativePath(aPersistentDescriptor);

  nsresult rv = NS_OK;
  
  PRUint32 dataSize = aPersistentDescriptor.Length();    
  char* decodedData = PL_Base64Decode(PromiseFlatCString(aPersistentDescriptor).get(), dataSize, nsnull);
  if (!decodedData) {
    NS_ERROR("SetPersistentDescriptor was given bad data");
    return NS_ERROR_FAILURE;
  }
  
  // Cast to an alias record and resolve.
  AliasRecord aliasHeader = *(AliasPtr)decodedData;
  PRInt32 aliasSize = GetAliasSizeFromRecord(aliasHeader);
  if (aliasSize > (dataSize * 3) / 4) { // be paranoid about having too few data
    PR_Free(decodedData);
    return NS_ERROR_FAILURE;
  }
  
  // Move the now-decoded data into the Handle.
  // The size of the decoded data is 3/4 the size of the encoded data. See plbase64.h
  Handle	newHandle = nsnull;
  if (::PtrToHand(decodedData, &newHandle, aliasSize) != noErr)
    rv = NS_ERROR_OUT_OF_MEMORY;
  PR_Free(decodedData);
  if (NS_FAILED(rv))
    return rv;

  Boolean changed;
  FSRef resolvedFSRef;
  OSErr err = ::FSResolveAlias(nsnull, (AliasHandle)newHandle, &resolvedFSRef, &changed);
    
  rv = MacErrorMapper(err);
  DisposeHandle(newHandle);
  if (NS_FAILED(rv))
    return rv;
 
  return InitWithFSRef(&resolvedFSRef);
}

/* void reveal (); */
NS_IMETHODIMP nsLocalFile::Reveal()
{
  FSRef             fsRefToReveal;
  AppleEvent        aeEvent = {0, nil};
  AppleEvent        aeReply = {0, nil};
  StAEDesc          aeDirDesc, listElem, myAddressDesc, fileList;
  OSErr             err;
  ProcessSerialNumber   process;
    
  nsresult rv = GetFSRefInternal(fsRefToReveal);
  if (NS_FAILED(rv))
    return rv;
  
  err = ::FindRunningAppBySignature ('MACS', process);
  if (err == noErr) { 
    err = ::AECreateDesc(typeProcessSerialNumber, (Ptr)&process, sizeof(process), &myAddressDesc);
    if (err == noErr) {
      // Create the FinderEvent
      err = ::AECreateAppleEvent(kAEMiscStandards, kAEMakeObjectsVisible, &myAddressDesc,
                        kAutoGenerateReturnID, kAnyTransactionID, &aeEvent);   
      if (err == noErr) {
        // Create the file list
        err = ::AECreateList(nil, 0, false, &fileList);
        if (err == noErr) {
          FSSpec fsSpecToReveal;
          err = ::FSRefMakeFSSpec(&fsRefToReveal, &fsSpecToReveal);
          if (err == noErr) {
            err = ::AEPutPtr(&fileList, 0, typeFSS, &fsSpecToReveal, sizeof(FSSpec));
            if (err == noErr) {
              err = ::AEPutParamDesc(&aeEvent, keyDirectObject, &fileList);
              if (err == noErr) {
                err = ::AESend(&aeEvent, &aeReply, kAENoReply, kAENormalPriority, kAEDefaultTimeout, nil, nil);
                if (err == noErr)
                  ::SetFrontProcess(&process);
              }
            }
          }
        }
      }
    }
  }
    
  return NS_OK;
}

/* void launch (); */
NS_IMETHODIMP nsLocalFile::Launch()
{
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;

  OSErr err = ::LSOpenFSRef(&fsRef, NULL);
  return MacErrorMapper(err);
}


//*****************************************************************************
//  nsLocalFile::nsILocalFileMac
//*****************************************************************************
#pragma mark -
#pragma mark [nsILocalFileMac]

/* void initWithCFURL (in CFURLRef aCFURL); */
NS_IMETHODIMP nsLocalFile::InitWithCFURL(CFURLRef aCFURL)
{
  NS_ENSURE_ARG(aCFURL);
  
  SetBaseRef(aCFURL);
  return NS_OK;
}

/* void initWithFSRef ([const] in FSRefPtr aFSRef); */
NS_IMETHODIMP nsLocalFile::InitWithFSRef(const FSRef *aFSRef)
{
  NS_ENSURE_ARG(aFSRef);
  nsresult rv = NS_ERROR_FAILURE;
  
  CFURLRef newURLRef = ::CFURLCreateFromFSRef(kCFAllocatorDefault, aFSRef);
  if (newURLRef) {
    SetBaseRef(newURLRef);
    ::CFRelease(newURLRef);
    rv = NS_OK;
  }
  return rv;
}

/* void initWithFSSpec ([const] in FSSpecPtr aFileSpec); */
NS_IMETHODIMP nsLocalFile::InitWithFSSpec(const FSSpec *aFileSpec)
{
  NS_ENSURE_ARG(aFileSpec);
  
  FSRef fsRef;
  OSErr err = ::FSpMakeFSRef(aFileSpec, &fsRef);
  if (err == noErr)
    return InitWithFSRef(&fsRef);
  else if (err == fnfErr) {
    CInfoPBRec  pBlock;
    FSSpec parentDirSpec;
    
    memset(&pBlock, 0, sizeof(CInfoPBRec));
    parentDirSpec.name[0] = 0;
    pBlock.dirInfo.ioVRefNum = aFileSpec->vRefNum;
    pBlock.dirInfo.ioDrDirID = aFileSpec->parID;
    pBlock.dirInfo.ioNamePtr = (StringPtr)parentDirSpec.name;
    pBlock.dirInfo.ioFDirIndex = -1;        //get info on parID
    err = ::PBGetCatInfoSync(&pBlock);
    if (err != noErr)
      return MacErrorMapper(err);
    
    parentDirSpec.vRefNum = aFileSpec->vRefNum;
    parentDirSpec.parID = pBlock.dirInfo.ioDrParID;
    err = ::FSpMakeFSRef(&parentDirSpec, &fsRef);
    if (err != noErr)
      return MacErrorMapper(err);
    HFSUniStr255 unicodeName;
    err = ::HFSNameGetUnicodeName(aFileSpec->name, kTextEncodingUnknown, &unicodeName);
    if (err != noErr)
      return MacErrorMapper(err);
    nsresult rv = InitWithFSRef(&fsRef);
    if (NS_FAILED(rv))
      return rv;
    return Append(nsDependentString(unicodeName.unicode, unicodeName.length));  
  }
  return MacErrorMapper(err);
}

/* void initToAppWithCreatorCode (in OSType aAppCreator); */
NS_IMETHODIMP nsLocalFile::InitToAppWithCreatorCode(OSType aAppCreator)
{
  FSRef fsRef;
  OSErr err = ::LSFindApplicationForInfo(aAppCreator, nsnull, nsnull, &fsRef, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  return InitWithFSRef(&fsRef);
}

/* CFURLRef getCFURL (); */
NS_IMETHODIMP nsLocalFile::GetCFURL(CFURLRef *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  CFURLRef whichURLRef = mFollowLinks ? mTargetRef : mBaseRef;
  if (whichURLRef)
    ::CFRetain(whichURLRef);
  *_retval = whichURLRef;
  return whichURLRef ? NS_OK : NS_ERROR_FAILURE;
}

/* FSRef getFSRef (); */
NS_IMETHODIMP nsLocalFile::GetFSRef(FSRef *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  return GetFSRefInternal(*_retval);
}

/* FSSpec getFSSpec (); */
NS_IMETHODIMP nsLocalFile::GetFSSpec(FSSpec *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  // Check we are correctly initialized.
  CHECK_mBaseRef();

  OSErr err;
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_SUCCEEDED(rv)) {
    // If the leaf node exists, things are simple.
    err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone,
              nsnull, nsnull, _retval, nsnull);
    return MacErrorMapper(err); 
  }
  else if (rv == NS_ERROR_FILE_NOT_FOUND) {
    // If the parent of the leaf exists, make an FSSpec from that.
    CFURLRef parentURLRef = ::CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, mBaseRef);
    if (!parentURLRef)
      return NS_ERROR_FAILURE;

    err = fnfErr;
    if (::CFURLGetFSRef(parentURLRef, &fsRef)) {
      FSCatalogInfo catalogInfo;
      if ((err = ::FSGetCatalogInfo(&fsRef,
                        kFSCatInfoVolume + kFSCatInfoNodeID + kFSCatInfoTextEncoding,
                        &catalogInfo, nsnull, nsnull, nsnull)) == noErr) {
        nsAutoString leafName;
        if (NS_SUCCEEDED(GetLeafName(leafName))) {
          Str31 hfsName;
          if ((err = ::UnicodeNameGetHFSName(leafName.Length(),
                          leafName.get(),
                          catalogInfo.textEncodingHint,
                          catalogInfo.nodeID == fsRtDirID,
                          hfsName)) == noErr)
            err = ::FSMakeFSSpec(catalogInfo.volume, catalogInfo.nodeID, hfsName, _retval);        
        }
      }
    }
    ::CFRelease(parentURLRef);
    rv = MacErrorMapper(err);
  }
  return rv;
}

/* readonly attribute PRInt64 fileSizeWithResFork; */
NS_IMETHODIMP nsLocalFile::GetFileSizeWithResFork(PRInt64 *aFileSizeWithResFork)
{
  NS_ENSURE_ARG_POINTER(aFileSizeWithResFork);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
      
  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoDataSizes + kFSCatInfoRsrcSizes,
                                 &catalogInfo, nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
    
  *aFileSizeWithResFork = catalogInfo.dataLogicalSize + catalogInfo.rsrcLogicalSize;
  return NS_OK;
}

/* attribute OSType fileType; */
NS_IMETHODIMP nsLocalFile::GetFileType(OSType *aFileType)
{
  NS_ENSURE_ARG_POINTER(aFileType);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
  
  FinderInfo fInfo;  
  OSErr err = ::FSGetFinderInfo(&fsRef, &fInfo, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  *aFileType = fInfo.file.fileType;
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileType(OSType aFileType)
{
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  OSErr err = ::FSChangeCreatorType(&fsRef, 0, aFileType);
  return MacErrorMapper(err);
}

/* attribute OSType fileCreator; */
NS_IMETHODIMP nsLocalFile::GetFileCreator(OSType *aFileCreator)
{
  NS_ENSURE_ARG_POINTER(aFileCreator);
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
  
  FinderInfo fInfo;  
  OSErr err = ::FSGetFinderInfo(&fsRef, &fInfo, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  *aFileCreator = fInfo.file.fileCreator;
  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileCreator(OSType aFileCreator)
{
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  OSErr err = ::FSChangeCreatorType(&fsRef, aFileCreator, 0);
  return MacErrorMapper(err);
}

/* void setFileTypeAndCreatorFromMIMEType (in string aMIMEType); */
NS_IMETHODIMP nsLocalFile::SetFileTypeAndCreatorFromMIMEType(const char *aMIMEType)
{
  // XXX - This should be cut from the API. Would create an evil dependency.
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setFileTypeAndCreatorFromExtension (in string aExtension); */
NS_IMETHODIMP nsLocalFile::SetFileTypeAndCreatorFromExtension(const char *aExtension)
{
  // XXX - This should be cut from the API. Would create an evil dependency.
  NS_ERROR("NS_ERROR_NOT_IMPLEMENTED");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void launchWithDoc (in nsILocalFile aDocToLoad, in boolean aLaunchInBackground); */
NS_IMETHODIMP nsLocalFile::LaunchWithDoc(nsILocalFile *aDocToLoad, PRBool aLaunchInBackground)
{
  PRBool isExecutable;
  nsresult rv = IsExecutable(&isExecutable);
  if (NS_FAILED(rv))
    return rv;
  if (!isExecutable)
    return NS_ERROR_FILE_EXECUTION_FAILED;

  FSRef appFSRef, docFSRef;
  rv = GetFSRefInternal(appFSRef);
  if (NS_FAILED(rv))
    return rv;

  if (aDocToLoad) {
    nsCOMPtr<nsILocalFileMac> macDoc = do_QueryInterface(aDocToLoad);
    rv = macDoc->GetFSRef(&docFSRef);
    if (NS_FAILED(rv))
      return rv;
  }
  
  LSLaunchFlags       theLaunchFlags = kLSLaunchDefaults;
  LSLaunchFSRefSpec   thelaunchSpec;

  if (aLaunchInBackground)
    theLaunchFlags |= kLSLaunchDontSwitch;
  memset(&thelaunchSpec, 0, sizeof(LSLaunchFSRefSpec));

  thelaunchSpec.appRef = &appFSRef;
  if (aDocToLoad) {
    thelaunchSpec.numDocs = 1;
    thelaunchSpec.itemRefs = &docFSRef;
  }
  thelaunchSpec.launchFlags = theLaunchFlags;

  OSErr err = ::LSOpenFromRefSpec(&thelaunchSpec, NULL);
  if (err != noErr)
    return MacErrorMapper(err);

  return NS_OK;
}

/* void openDocWithApp (in nsILocalFile aAppToOpenWith, in boolean aLaunchInBackground); */
NS_IMETHODIMP nsLocalFile::OpenDocWithApp(nsILocalFile *aAppToOpenWith, PRBool aLaunchInBackground)
{
  nsresult rv;
  OSErr err;

  FSRef docFSRef, appFSRef;
  rv = GetFSRefInternal(docFSRef);
  if (NS_FAILED(rv))
    return rv;

  if (aAppToOpenWith) {
    nsCOMPtr<nsILocalFileMac> appFileMac = do_QueryInterface(aAppToOpenWith, &rv);
    if (!appFileMac)
      return rv;

    PRBool isExecutable;
    rv = appFileMac->IsExecutable(&isExecutable);
    if (NS_FAILED(rv))
      return rv;
    if (!isExecutable)
      return NS_ERROR_FILE_EXECUTION_FAILED;
    
    rv = appFileMac->GetFSRef(&appFSRef);
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    OSType  fileCreator;
    rv = GetFileCreator(&fileCreator);
    if (NS_FAILED(rv))
      return rv;

    err = ::LSFindApplicationForInfo(fileCreator, nsnull, nsnull, &appFSRef, nsnull);
    if (err != noErr)
      return MacErrorMapper(err);
  }
  
  LSLaunchFlags       theLaunchFlags = kLSLaunchDefaults;
  LSLaunchFSRefSpec   thelaunchSpec;

  if (aLaunchInBackground)
  theLaunchFlags |= kLSLaunchDontSwitch;
  memset(&thelaunchSpec, 0, sizeof(LSLaunchFSRefSpec));

  thelaunchSpec.appRef = &appFSRef;
  thelaunchSpec.numDocs = 1;
  thelaunchSpec.itemRefs = &docFSRef;
  thelaunchSpec.launchFlags = theLaunchFlags;

  err = ::LSOpenFromRefSpec(&thelaunchSpec, NULL);
  if (err != noErr)
    return MacErrorMapper(err);

  return NS_OK;
}

/* boolean isPackage (); */
NS_IMETHODIMP nsLocalFile::IsPackage(PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;

  FSCatalogInfo catalogInfo;
  OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags + kFSCatInfoFinderInfo,
                                 &catalogInfo, nsnull, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  if ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0) {
    FileInfo *fInfoPtr = (FileInfo *)(catalogInfo.finderInfo);
    if ((fInfoPtr->finderFlags & kHasBundle) != 0) {
      *_retval = PR_TRUE;
    }
    else {
     // Folders ending with ".app" are also considered to
     // be packages, even if the top-level folder doesn't have bundle set
      nsCAutoString name;
      if (NS_SUCCEEDED(rv = GetNativeLeafName(name))) {
        const char *extPtr = strrchr(name.get(), '.');
        if (extPtr) {
          if ((nsCRT::strcasecmp(extPtr, ".app") == 0))
            *_retval = PR_TRUE;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetBundleDisplayName(nsAString& outBundleName)
{
  PRBool isPackage = PR_FALSE;
  nsresult rv = IsPackage(&isPackage);
  if (NS_FAILED(rv) || !isPackage)
    return NS_ERROR_FAILURE;
  
  nsAutoString name;
  rv = GetLeafName(name);
  if (NS_FAILED(rv))
    return rv;
  
  PRInt32 length = name.Length();
  if (Substring(name, length - 4, length).EqualsLiteral(".app")) {
    // 4 characters in ".app"
    outBundleName = Substring(name, 0, length - 4);
  }
  else
    outBundleName = name;
  
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetBundleIdentifier(nsACString& outBundleIdentifier)
{
  nsresult rv = NS_ERROR_FAILURE;

  CFURLRef urlRef;
  if (NS_SUCCEEDED(GetCFURL(&urlRef))) {
    CFBundleRef bundle = ::CFBundleCreate(NULL, urlRef);
    if (bundle) {
      CFStringRef bundleIdentifier = ::CFBundleGetIdentifier(bundle);
      if (bundleIdentifier)
        rv = CFStringReftoUTF8(bundleIdentifier, outBundleIdentifier);

      ::CFRelease(bundle);
    }
    ::CFRelease(urlRef);
  }

  return rv;
}


//*****************************************************************************
//  nsLocalFile Methods
//*****************************************************************************
#pragma mark -
#pragma mark [Protected Methods]

nsresult nsLocalFile::SetBaseRef(CFURLRef aCFURLRef)
{
  NS_ENSURE_ARG(aCFURLRef);
  
  ::CFRetain(aCFURLRef);
  if (mBaseRef)
    ::CFRelease(mBaseRef);
  mBaseRef = aCFURLRef;

  mFollowLinksDirty = PR_TRUE;  
  UpdateTargetRef();
  mCachedFSRefValid = PR_FALSE;
  return NS_OK;
}

nsresult nsLocalFile::UpdateTargetRef()
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();
  
  if (mFollowLinksDirty) {
    if (mTargetRef) {
      ::CFRelease(mTargetRef);
      mTargetRef = nsnull;
    }
    if (mFollowLinks) {
      mTargetRef = mBaseRef;
      ::CFRetain(mTargetRef);

      FSRef fsRef;
      if (::CFURLGetFSRef(mBaseRef, &fsRef)) {
        Boolean targetIsFolder, wasAliased;
        if (FSResolveAliasFile(&fsRef, true /*resolveAliasChains*/, 
            &targetIsFolder, &wasAliased) == noErr && wasAliased) {
          ::CFRelease(mTargetRef);
          mTargetRef = CFURLCreateFromFSRef(NULL, &fsRef);
          if (!mTargetRef)
            return NS_ERROR_FAILURE;
        }
      }
      mFollowLinksDirty = PR_FALSE;
    }
  }
  return NS_OK;
}

nsresult nsLocalFile::GetFSRefInternal(FSRef& aFSRef, PRBool bForceUpdateCache)
{
  if (bForceUpdateCache || !mCachedFSRefValid) {
    mCachedFSRefValid = PR_FALSE;
    CFURLRef whichURLRef = mFollowLinks ? mTargetRef : mBaseRef;
    NS_ENSURE_TRUE(whichURLRef, NS_ERROR_NULL_POINTER);
    if (::CFURLGetFSRef(whichURLRef, &mCachedFSRef))
      mCachedFSRefValid = PR_TRUE;
  }
  if (mCachedFSRefValid) {
    aFSRef = mCachedFSRef;
    return NS_OK;
  }
  // CFURLGetFSRef only returns a Boolean for success,
  // so we have to assume what the error was. This is
  // the only probable cause.
  return NS_ERROR_FILE_NOT_FOUND;
}

nsresult nsLocalFile::GetPathInternal(nsACString& path)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  CFURLRef whichURLRef = mFollowLinks ? mTargetRef : mBaseRef;
  NS_ENSURE_TRUE(whichURLRef, NS_ERROR_NULL_POINTER);
   
  CFStringRef pathStrRef = ::CFURLCopyFileSystemPath(whichURLRef, kCFURLPOSIXPathStyle);
  if (pathStrRef) {
    rv = CFStringReftoUTF8(pathStrRef, path);
    ::CFRelease(pathStrRef);
  }
  return rv;
}

nsresult nsLocalFile::CopyInternal(nsIFile* aParentDir,
                                   const nsAString& newName,
                                   PRBool followLinks)
{
  // Check we are correctly initialized.
  CHECK_mBaseRef();

  StFollowLinksState srcFollowState(*this, followLinks);

  nsresult rv;
  OSErr err;
  FSRef srcFSRef, newFSRef;

  rv = GetFSRefInternal(srcFSRef);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> newParentDir = aParentDir;

  if (!newParentDir) {
    if (newName.IsEmpty())
      return NS_ERROR_INVALID_ARG;
    rv = GetParent(getter_AddRefs(newParentDir));
    if (NS_FAILED(rv))
      return rv;    
  }

  // If newParentDir does not exist, create it
  PRBool exists;
  rv = newParentDir->Exists(&exists);
  if (NS_FAILED(rv))
    return rv;
  if (!exists) {
    rv = newParentDir->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_FAILED(rv))
      return rv;
  }

  FSRef destFSRef;
  nsCOMPtr<nsILocalFileMac> newParentDirMac(do_QueryInterface(newParentDir));
  if (!newParentDirMac)
    return NS_ERROR_NO_INTERFACE;
  rv = newParentDirMac->GetFSRef(&destFSRef);
  if (NS_FAILED(rv))
    return rv;

  err =
   ::FSCopyObject(&srcFSRef, &destFSRef, newName.Length(),
                  newName.Length() ? PromiseFlatString(newName).get() : NULL,
                  0, kFSCatInfoNone, false, false, NULL, NULL, &newFSRef);

  return MacErrorMapper(err);
}

const PRInt64 kMillisecsPerSec = 1000LL;
const PRInt64 kUTCDateTimeFractionDivisor = 65535LL;

PRInt64 nsLocalFile::HFSPlustoNSPRTime(const UTCDateTime& utcTime)
{
  // Start with seconds since Jan. 1, 1904 GMT
  PRInt64 result = ((PRInt64)utcTime.highSeconds << 32) + (PRInt64)utcTime.lowSeconds; 
  // Subtract to convert to NSPR epoch of 1970
  result -= kJanuaryFirst1970Seconds;
  // Convert to millisecs
  result *= kMillisecsPerSec;
  // Convert the fraction to millisecs and add it
  result += ((PRInt64)utcTime.fraction * kMillisecsPerSec) / kUTCDateTimeFractionDivisor;

  return result;
}

void nsLocalFile::NSPRtoHFSPlusTime(PRInt64 nsprTime, UTCDateTime& utcTime)
{
  PRInt64 fraction = nsprTime % kMillisecsPerSec;
  PRInt64 seconds = (nsprTime / kMillisecsPerSec) + kJanuaryFirst1970Seconds;
  utcTime.highSeconds = (UInt16)((PRUint64)seconds >> 32);
  utcTime.lowSeconds = (UInt32)seconds;
  utcTime.fraction = (UInt16)((fraction * kUTCDateTimeFractionDivisor) / kMillisecsPerSec);
}

nsresult nsLocalFile::CFStringReftoUTF8(CFStringRef aInStrRef, nsACString& aOutStr)
{
  nsresult rv = NS_ERROR_FAILURE;
  CFIndex usedBufLen, inStrLen = ::CFStringGetLength(aInStrRef);
  CFIndex charsConverted = ::CFStringGetBytes(aInStrRef, CFRangeMake(0, inStrLen),
                              kCFStringEncodingUTF8, 0, PR_FALSE, nsnull, 0, &usedBufLen);
  if (charsConverted == inStrLen) {
    aOutStr.SetLength(usedBufLen);
    if (aOutStr.Length() != usedBufLen)
      return NS_ERROR_OUT_OF_MEMORY;
    UInt8 *buffer = (UInt8*) aOutStr.BeginWriting();

    ::CFStringGetBytes(aInStrRef, CFRangeMake(0, inStrLen),
                       kCFStringEncodingUTF8, 0, false, buffer, usedBufLen, &usedBufLen);
    rv = NS_OK;
  }
  return rv;
}

// nsIHashable

NS_IMETHODIMP
nsLocalFile::Equals(nsIHashable* aOther, PRBool *aResult)
{
    return EqualsInternal(aOther, PR_FALSE, aResult);
}

NS_IMETHODIMP
nsLocalFile::GetHashCode(PRUint32 *aResult)
{
    CFStringRef pathStrRef = ::CFURLCopyFileSystemPath(mBaseRef, kCFURLPOSIXPathStyle);
    nsCAutoString path;
    CFStringReftoUTF8(pathStrRef, path);
    *aResult = HashString(path);
    return NS_OK;
}

//*****************************************************************************
//  Global Functions
//*****************************************************************************
#pragma mark -
#pragma mark [Global Functions]

void nsLocalFile::GlobalInit()
{
}

void nsLocalFile::GlobalShutdown()
{
}

nsresult NS_NewLocalFile(const nsAString& path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile;
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    if (!path.IsEmpty()) {
        nsresult rv = file->InitWithPath(path);
        if (NS_FAILED(rv)) {
            NS_RELEASE(file);
            return rv;
        }
    }
    *result = file;
    return NS_OK;
}

nsresult NS_NewNativeLocalFile(const nsACString& path, PRBool followLinks, nsILocalFile **result)
{
    return NS_NewLocalFile(NS_ConvertUTF8toUTF16(path), followLinks, result);
}

nsresult NS_NewLocalFileWithFSSpec(const FSSpec* inSpec, PRBool followLinks, nsILocalFileMac **result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    nsresult rv = file->InitWithFSSpec(inSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(file);
        return rv;
    }
    *result = file;
    return NS_OK;
}

nsresult NS_NewLocalFileWithFSRef(const FSRef* aFSRef, PRBool aFollowLinks, nsILocalFileMac** result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(aFollowLinks);

    nsresult rv = file->InitWithFSRef(aFSRef);
    if (NS_FAILED(rv)) {
        NS_RELEASE(file);
        return rv;
    }
    *result = file;
    return NS_OK;
}

//*****************************************************************************
//  Static Functions
//*****************************************************************************

static nsresult MacErrorMapper(OSErr inErr)
{
    nsresult outErr;
    
    switch (inErr)
    {
        case noErr:
            outErr = NS_OK;
            break;

        case fnfErr:
            outErr = NS_ERROR_FILE_NOT_FOUND;
            break;

        case dupFNErr:
            outErr = NS_ERROR_FILE_ALREADY_EXISTS;
            break;
        
        case dskFulErr:
            outErr = NS_ERROR_FILE_DISK_FULL;
            break;
        
        case fLckdErr:
            outErr = NS_ERROR_FILE_IS_LOCKED;
            break;
        
        // Can't find good map for some
        case bdNamErr:
            outErr = NS_ERROR_FAILURE;
            break;

        default:    
            outErr = NS_ERROR_FAILURE;
            break;
    }
    return outErr;
}

static OSErr FindRunningAppBySignature(OSType aAppSig, ProcessSerialNumber& outPsn)
{
  ProcessInfoRec info;
  OSErr err = noErr;
  
  outPsn.highLongOfPSN = 0;
  outPsn.lowLongOfPSN  = kNoProcess;
  
  while (PR_TRUE)
  {
    err = ::GetNextProcess(&outPsn);
    if (err == procNotFound)
      break;
    if (err != noErr)
      return err;
    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = nil;
    info.processAppSpec = nil;
    err = ::GetProcessInformation(&outPsn, &info);
    if (err != noErr)
      return err;
    
    if (info.processSignature == aAppSig)
      return noErr;
  }
  return procNotFound;
}

// Convert a UTF-8 string to a UTF-16 string while normalizing to
// Normalization Form C (composed Unicode). We need this because
// Mac OS X file system uses NFD (Normalization Form D : decomposed Unicode)
// while most other OS', server-side programs usually expect NFC.

typedef void (*UnicodeNormalizer) (CFMutableStringRef, CFStringNormalizationForm);
static void CopyUTF8toUTF16NFC(const nsACString& aSrc, nsAString& aResult)
{
    static PRBool sChecked = PR_FALSE;
    static UnicodeNormalizer sUnicodeNormalizer = NULL;

    // CFStringNormalize was not introduced until Mac OS 10.2
    if (!sChecked) {
        CFBundleRef carbonBundle =
            CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Carbon"));
        if (carbonBundle)
            sUnicodeNormalizer = (UnicodeNormalizer)
                ::CFBundleGetFunctionPointerForName(carbonBundle,
                                                    CFSTR("CFStringNormalize"));
        sChecked = PR_TRUE;
    }

    if (!sUnicodeNormalizer) {  // OS X 10.2 or earlier
        CopyUTF8toUTF16(aSrc, aResult);
        return;  
    }

    const nsAFlatCString &inFlatSrc = PromiseFlatCString(aSrc);

    // The number of 16bit code units in a UTF-16 string will never be
    // larger than the number of bytes in the corresponding UTF-8 string.
    CFMutableStringRef inStr =
        ::CFStringCreateMutable(NULL, inFlatSrc.Length());

    if (!inStr) {
        CopyUTF8toUTF16(aSrc, aResult);
        return;  
    }
     
    ::CFStringAppendCString(inStr, inFlatSrc.get(), kCFStringEncodingUTF8); 

    sUnicodeNormalizer(inStr, kCFStringNormalizationFormC);

    CFIndex length = CFStringGetLength(inStr);
    const UniChar* chars = CFStringGetCharactersPtr(inStr);

    if (chars) 
        aResult.Assign(chars, length);
    else {
        nsAutoBuffer<UniChar, FILENAME_BUFFER_SIZE> buffer;
        if (!buffer.EnsureElemCapacity(length))
            CopyUTF8toUTF16(aSrc, aResult);
        else {
            CFStringGetCharacters(inStr, CFRangeMake(0, length), buffer.get());
            aResult.Assign(buffer.get(), length);
        }
    }
    CFRelease(inStr);
}
