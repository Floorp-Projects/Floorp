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

#include "nsLocalFileOSX.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsITimelineService.h"

#include "plbase64.h"
#include "prmem.h"
#include "nsCRT.h"

#include "MoreFilesX.h"
#include "FSCopyObject.h"

// Mac Includes
#include <Aliases.h>
#include <Gestalt.h>
#include <AppleEvents.h>
#include <AEDataModel.h>
#include <Processes.h>
#include <Carbon/Carbon.h>

// Unix Includes
#include <sys/stat.h>

//*****************************************************************************
//  Static Function Prototypes
//*****************************************************************************

static nsresult MacErrorMapper(OSErr inErr);
static OSErr FindRunningAppBySignature(OSType aAppSig, ProcessSerialNumber& outPsn);

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

class nsDirEnumerator : public nsISimpleEnumerator
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
              nsLocalFile *newFile = new nsLocalFile(mFSRefsArray[mArrayIndex], nsString());
              if (!newFile)
                return NS_ERROR_OUT_OF_MEMORY;
              mArrayIndex++;
              mNext = newFile;
            } 
          }
          *result = mNext != nsnull;
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

        virtual ~nsDirEnumerator() 
        {
          if (mIterator)
            ::FSCloseIterator(mIterator);
          if (mFSRefsArray)
            nsMemory::Free(mFSRefsArray);
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

NS_IMPL_ISUPPORTS1(nsDirEnumerator, nsISimpleEnumerator)

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

#pragma mark -
#pragma mark [StBuffer]

template <class T>
class StBuffer
{
public:
  
  StBuffer() :
    mBufferPtr(mBuffer),
    mCurElemCapacity(kStackBufferNumElems)
  {
  }

  ~StBuffer()
  {
    DeleteBuffer();
  }

  PRBool EnsureElemCapacity(PRInt32 inElemCapacity)
  {
    if (inElemCapacity <= mCurElemCapacity)
      return PR_TRUE;
    
    if (inElemCapacity > kStackBufferNumElems)
    {
      DeleteBuffer();
      mBufferPtr = (T*)malloc(inElemCapacity * sizeof(T));
      mCurElemCapacity = inElemCapacity;
      return (mBufferPtr != NULL);
    }
    
    mCurElemCapacity = kStackBufferNumElems;
    return PR_TRUE;
  }
                
  T*          get()     { return mBufferPtr;    }
  
  PRInt32     GetElemCapacity()   { return mCurElemCapacity;  }

protected:

  void DeleteBuffer()
  {
    if (mBufferPtr != mBuffer)
    {
      free(mBufferPtr);
      mBufferPtr = mBuffer;
    }                
  }
  
protected:
  enum { kStackBufferNumElems		= 512 };

  T             *mBufferPtr;
  T             mBuffer[kStackBufferNumElems];
  PRInt32       mCurElemCapacity;
};


//*****************************************************************************
//  nsLocalFile
//*****************************************************************************

// The HFS+ epoch is Jan. 1, 1904 GMT - differs from HFS in which times were local
// The NSPR epoch is Jan. 1, 1970 GMT
// 2082844800 is the difference in seconds between those dates
PRInt64   nsLocalFile::kJanuaryFirst1970Seconds = 2082844800LL;
PRUnichar nsLocalFile::kPathSepUnichar;
char      nsLocalFile::kPathSepChar;
FSRef     nsLocalFile::kInvalidFSRef; // an FSRef filled with zeros is invalid
FSRef     nsLocalFile::kRootFSRef;

#pragma mark -
#pragma mark [CTORs/DTOR]

nsLocalFile::nsLocalFile() :
  mFollowLinks(PR_TRUE),
  mIdentityDirty(PR_TRUE),
  mFollowLinksDirty(PR_TRUE)
{
    mFSRef = kInvalidFSRef;
    mTargetFSRef = kInvalidFSRef;
}

nsLocalFile::nsLocalFile(const FSRef& aFSRef, const nsAString& aRelativePath) :
  mFSRef(aFSRef),
  mFollowLinks(PR_TRUE),
  mIdentityDirty(PR_TRUE),
  mFollowLinksDirty(PR_TRUE)
{
    mTargetFSRef = kInvalidFSRef;

    if (aRelativePath.Length()) {
      nsAString::const_iterator nodeBegin, pathEnd;
      aRelativePath.BeginReading(nodeBegin);
      aRelativePath.EndReading(pathEnd);
      nsAString::const_iterator nodeEnd(nodeBegin);
   
      while (nodeEnd != pathEnd) {
        FindCharInReadable(kPathSepUnichar, nodeEnd, pathEnd);
        mNonExtantNodes.push_back(nsString(Substring(nodeBegin, nodeEnd)));
        if (nodeEnd != pathEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
          ++nodeEnd;
        nodeBegin = nodeEnd;
      }
    }
}

nsLocalFile::nsLocalFile(const nsLocalFile& src) :
  mFSRef(src.mFSRef), mNonExtantNodes(src.mNonExtantNodes),
  mTargetFSRef(src.mTargetFSRef),
  mFollowLinks(src.mFollowLinks),
  mIdentityDirty(src.mIdentityDirty),
  mFollowLinksDirty(src.mFollowLinksDirty)
{
}

nsLocalFile::~nsLocalFile()
{
}


//*****************************************************************************
//  nsLocalFile::nsISupports
//*****************************************************************************
#pragma mark -
#pragma mark [nsISupports]

NS_IMPL_THREADSAFE_ISUPPORTS3(nsLocalFile,
                              nsILocalFileMac,
                              nsILocalFile,
                              nsIFile)
                              
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
    nsAString::const_iterator start, end;
    aNode.BeginReading(start);
    aNode.EndReading(end);
    if (FindCharInReadable(kPathSepChar, start, end))
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    
    mIdentityDirty = mFollowLinksDirty = PR_TRUE;
    
    if (!mNonExtantNodes.size()) {
        // Make sure the leaf is resolved before attempting to append to it.
        Boolean isFolder, isAlias;
        OSErr err = ::FSResolveAliasFile(&mFSRef, PR_TRUE, &isFolder, &isAlias);
        if (err == noErr) {
            if (!isFolder)
                return NS_ERROR_FILE_NOT_DIRECTORY;
            FSRef newLeafRef;
            err = ::FSMakeFSRefUnicode(&mFSRef, aNode.Length(),
                                        (UniChar *)PromiseFlatString(aNode).get(),
                                        kTextEncodingUnknown, &newLeafRef);
            if (err == noErr) {
                mFSRef = newLeafRef;
                mIdentityDirty = PR_FALSE;
            }
        }
        if (err == fnfErr) {
            mNonExtantNodes.push_back(nsString(aNode));
            err = noErr;
        }
        return MacErrorMapper(err);
    }
    else
        mNonExtantNodes.push_back(nsString(aNode));
    
    return NS_OK;
}

/* [noscript] void appendNative (in ACString node); */
NS_IMETHODIMP nsLocalFile::AppendNative(const nsACString& aNode)
{
    return Append(NS_ConvertUTF8toUCS2(aNode));
}

/* void normalize (); */
NS_IMETHODIMP nsLocalFile::Normalize()
{
    return NS_OK;
}

/* void create (in unsigned long type, in unsigned long permissions); */
NS_IMETHODIMP nsLocalFile::Create(PRUint32 type, PRUint32 permissions)
{
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = ResolveNonExtantNodes(PR_TRUE);
    if (NS_FAILED(rv))
        return rv;
    if (mNonExtantNodes.size() == 0)
        return NS_ERROR_FILE_ALREADY_EXISTS;
        
    OSErr err;
    FSRef newRef;    
    const nsString& leafName = mNonExtantNodes.back();
        
    switch (type)
    {
      case NORMAL_FILE_TYPE:
        //SetOSTypeAndCreatorFromExtension();
        err = ::FSCreateFileUnicode(&mFSRef,
                                    leafName.Length(),
                                    (const UniChar *)leafName.get(),
                                    kFSCatInfoNone,
                                    nsnull, &newRef, nsnull);
        break;

      case DIRECTORY_TYPE:
        err = ::FSCreateDirectoryUnicode(&mFSRef,
                                         leafName.Length(),
                                         (const UniChar *)leafName.get(),
                                         kFSCatInfoNone,
                                         nsnull, &newRef, nsnull, nsnull);
        break;
      
      default:
        return NS_ERROR_FILE_UNKNOWN_TYPE;
        break;
    }
    
    if (err == noErr) {
        mFSRef = newRef;
        mNonExtantNodes.clear();
        return NS_OK;
    }
        
    return MacErrorMapper(err);
}

/* attribute AString leafName; */
NS_IMETHODIMP nsLocalFile::GetLeafName(nsAString& aLeafName)
{
  if (mNonExtantNodes.size()) {
    const nsString& leafNode = mNonExtantNodes.back();
    aLeafName.Assign(leafNode);
    return NS_OK;
  }
  HFSUniStr255 leafName;
  OSErr err = ::FSGetCatalogInfo(&mFSRef,
                                 kFSCatInfoNone,
                                 nsnull,
                                 &leafName,
                                 nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
  aLeafName.Assign((PRUnichar *)leafName.unicode, leafName.length);
  return NS_OK;                                  
}

NS_IMETHODIMP nsLocalFile::SetLeafName(const nsAString& aLeafName)
{
  if (aLeafName.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  if (mNonExtantNodes.size()) {
    nsString& leafNode = mNonExtantNodes.back();
    leafNode.Assign(aLeafName);
    return NS_OK;
  }
  else {
    // Just get the parent ref and assign that to mFSRef
    // Stick the new leaf name in mAppendedNodes.
    FSRef parentRef;
    OSErr err = ::FSGetParentRef(&mFSRef, &parentRef);
    if (err != noErr)
      return MacErrorMapper(err);
    mFSRef = parentRef;
    mNonExtantNodes.push_back(nsString(aLeafName));
    mIdentityDirty = PR_TRUE;
  }
  return NS_OK;
}

/* [noscript] attribute ACString nativeLeafName; */
NS_IMETHODIMP nsLocalFile::GetNativeLeafName(nsACString& aNativeLeafName)
{
    nsAutoString temp;
    nsresult rv = GetLeafName(temp);
    if (NS_FAILED(rv))
        return rv;
    aNativeLeafName.Assign(NS_ConvertUCS2toUTF8(temp));    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetNativeLeafName(const nsACString& aNativeLeafName)
{
    return SetLeafName(NS_ConvertUTF8toUCS2(aNativeLeafName));
}

/* void copyTo (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString& newName)
{
    return MoveCopy(newParentDir, newName, PR_TRUE, PR_FALSE);
}

/* [noscrpit] void CopyToNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString& newName)
{
    return MoveCopy(newParentDir, NS_ConvertUTF8toUCS2(newName), PR_TRUE, PR_FALSE);
}

/* void copyToFollowingLinks (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString& newName)
{
    return MoveCopy(newParentDir, newName, PR_TRUE, PR_TRUE);
}

/* [noscript] void copyToFollowingLinksNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString& newName)
{
    return MoveCopy(newParentDir, NS_ConvertUTF8toUCS2(newName), PR_TRUE, PR_TRUE);
}

/* void moveTo (in nsIFile newParentDir, in AString newName); */
NS_IMETHODIMP nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString& newName)
{
    return MoveCopy(newParentDir, newName, FALSE, FALSE);
}

/* [noscript] void moveToNative (in nsIFile newParentDir, in ACString newName); */
NS_IMETHODIMP nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString& newName)
{
    return MoveCopy(newParentDir, NS_ConvertUTF8toUCS2(newName), FALSE, FALSE);
}

/* void remove (in boolean recursive); */
NS_IMETHODIMP nsLocalFile::Remove(PRBool recursive)
{
  StFollowLinksState followLinks(*this, PR_FALSE); // XXX If we're an alias, never remove target
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;
    
  PRBool isDir;
  rv = IsDirectory(&isDir);
  if (NS_FAILED(rv))
    return rv;
    
  // Since FSRefs can only refer to extant file objects and we're about to become non-extant,
  // re-specify ourselves as our parent FSRef + current leaf name

  OSErr err;
  FSCatalogInfo catalogInfo;
  HFSUniStr255 leafName;
  FSRef parentRef;
  
  err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone, &catalogInfo, &leafName, nsnull, &parentRef);
  if (err != noErr)
    return MacErrorMapper(err);
  
  mFSRef = parentRef;
  mNonExtantNodes.push_back(nsString(Substring(leafName.unicode, leafName.unicode + leafName.length)));
  mIdentityDirty = PR_TRUE;
  
  if (recursive && isDir)
    err = ::FSDeleteContainer(&fsRef);
  else
    err = ::FSDeleteObject(&fsRef);

  return MacErrorMapper(err);
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
  OSErr err;
  nsresult rv;
  FSRef fsRef;
  FSCatalogInfo catalogInfo;

  rv = GetFSRefInternal(fsRef);
  if (NS_FAILED(rv))
    return rv;

#if XP_MACOSX
  FSRef parentRef;
  PRBool notifyParent;

  /* Get the node flags, the content modification date and time, and the parent ref */
  err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNodeFlags + kFSCatInfoContentMod,
                           &catalogInfo, NULL, NULL, &parentRef);
  if (err != noErr)
    return MacErrorMapper(err);
  
  /* Notify the parent if this is a file */
  notifyParent = (0 == (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask));
#endif

  NSPRtoHFSPlusTime(aLastModifiedTime, catalogInfo.contentModDate);
  err = ::FSSetCatalogInfo(&fsRef, kFSCatInfoContentMod, &catalogInfo);
  if (err != noErr)
    return MacErrorMapper(err);

#if XP_MACOSX
  /* Send a notification for the parent of the file, or for the directory */
  err = FNNotify(notifyParent ? &parentRef : &fsRef, kFNDirectoryModifiedMessage, kNilOptions);
  if (err != noErr)
    return MacErrorMapper(err);
#endif

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
  UInt8 pathBuf[PATH_MAX + 1];
  OSErr err = ::FSRefMakePath(&mFSRef, pathBuf, sizeof(pathBuf));
  if (err != noErr)
    return MacErrorMapper(err);
  aPath.Assign(NS_ConvertUTF8toUCS2((char *)pathBuf));
  
  deque<nsString>::iterator iter(mNonExtantNodes.begin());
  deque<nsString>::iterator end(mNonExtantNodes.end());
  while (iter != end) {
    aPath.Append(kPathSepUnichar);
    aPath.Append(*iter);
    ++iter;
  }

  return NS_OK;
}

/* [noscript] readonly attribute ACString nativePath; */
NS_IMETHODIMP nsLocalFile::GetNativePath(nsACString& aNativePath)
{
    UInt8 pathBuf[PATH_MAX + 1];
    OSErr err = ::FSRefMakePath(&mFSRef, pathBuf, sizeof(pathBuf));
    if (err != noErr)
        return MacErrorMapper(err);
    aNativePath.Assign((char *)pathBuf);
    
    deque<nsString>::iterator iter(mNonExtantNodes.begin());
    deque<nsString>::iterator end(mNonExtantNodes.end());
    while (iter != end) {
        aNativePath.Append(kPathSepChar);
        aNativePath.Append((NS_ConvertUCS2toUTF8(*iter)));
        ++iter;
    }

    return NS_OK;
}

/* boolean exists (); */
NS_IMETHODIMP nsLocalFile::Exists(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  FSRef fsRef;
  if (NS_SUCCEEDED(GetFSRefInternal(fsRef))) {
    *_retval = (::FSGetCatalogInfo(&mFSRef, kFSCatInfoNone,
                    nsnull, nsnull, nsnull, nsnull) == noErr);
  }
  return NS_OK;
}

/* boolean isWritable (); */
NS_IMETHODIMP nsLocalFile::IsWritable(PRBool *_retval)
{
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
  OSErr err = FSGetCatalogInfo(&fsRef, kFSCatInfoFinderInfo, &catalogInfo,
                               &leafName, nsnull, nsnull);
  if (err != noErr)
    return MacErrorMapper(err);
    
  FileInfo *fInfoPtr = (FileInfo *)(catalogInfo.finderInfo); // Finder flags are in the same place whether we use FileInfo or FolderInfo
  if ((fInfoPtr->finderFlags & kIsInvisible) != 0) {
    *_retval = PR_TRUE;
  }
  else {
    // If the leaf name begins with a '.', consider it invisible
    nsDependentString leaf(leafName.unicode, leafName.length);
    if (leaf.First() == PRUnichar('.'))
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
  nsresult rv = GetFSRefInternal(fsRef);
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
  nsresult rv = GetFSRefInternal(fsRef);
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
  NS_ENSURE_ARG(_retval);
  *_retval = PR_FALSE;
  
  nsresult rv = Resolve();
  if (NS_FAILED(rv))
    return rv;
  
  Boolean isAlias, isFolder;
  if (::FSIsAliasFile(&mFSRef, &isAlias, &isFolder) == noErr)
      *_retval = isAlias;
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
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    
    nsLocalFile *newFile = new nsLocalFile(*this);
    if (!newFile)
        return NS_ERROR_OUT_OF_MEMORY;
    *_retval = newFile;
    NS_ADDREF(*_retval);
    
    return NS_OK;
}

/* boolean equals (in nsIFile inFile); */
NS_IMETHODIMP nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  nsCOMPtr<nsILocalFileMac> inMacFile(do_QueryInterface(inFile));
  if (!inFile)
    return NS_OK;
    
  // If both exist, compare FSRefs
  FSRef thisFSRef, inFSRef;
  nsresult rv1 = GetFSRef(&thisFSRef);
  nsresult rv2 = inMacFile->GetFSRef(&inFSRef);
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
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  PRBool isDir;
  nsresult rv = IsDirectory(&isDir);
  if (NS_FAILED(rv))
    return rv;
  if (!isDir)
    return NS_OK;     // must be a dir to contain someone

  nsCOMPtr<nsILocalFileMac> inMacFile(do_QueryInterface(inFile));
  if (!inMacFile)
    return NS_OK;
    
  // if we can get FSRefs from both, use them
  FSRef thisFSRef, inFSRef;
  if (NS_SUCCEEDED(GetFSRef(&thisFSRef)) && NS_SUCCEEDED(inMacFile->GetFSRef(&inFSRef))) {
    FSRef parentFSRef;
    while ((::FSGetParentRef(&inFSRef, &parentFSRef) == noErr && ::FSRefValid(&parentFSRef))) {
      if (::FSCompareFSRefs(&parentFSRef, &thisFSRef) == noErr) {
        *_retval = PR_TRUE;
        return NS_OK;
      }
      if (!recur)
        return NS_OK;
      inFSRef = parentFSRef;
    }
    return NS_OK;
  }

  // If we fall thru to here, we couldn't get the FSRefs so compare paths
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
  
  nsresult rv;
  FSRef fsRef;
  nsLocalFile *newFile = nsnull;

  // If it can be determined without error that a file does not
  // have a parent, return nsnull for the parent and NS_OK as the result.
  // See bug 133617.
    
  rv = GetFSRefInternal(fsRef);
  if (NS_SUCCEEDED(rv)) {
    FSRef parentRef;
    // If an FSRef has no parent, FSGetParentRef returns noErr and
    // an FSRef filled with zeros (kInvalidFSRef).
    OSErr err = ::FSGetParentRef(&fsRef, &parentRef);
    if (err != noErr)
      return MacErrorMapper(err);
    if (::FSRefValid(&parentRef) == PR_FALSE)
      return NS_OK;
    newFile = new nsLocalFile(parentRef, nsString());
  }
  else {
    nsAutoString appendedNodes;
    if (mNonExtantNodes.size() > 1) {
      deque<nsString>::iterator iter(mNonExtantNodes.begin());
      deque<nsString>::iterator end(mNonExtantNodes.end() - 1);
      while (iter != end) {
        const nsString& currentNode = *iter;
        ++iter;
        appendedNodes.Append(currentNode);
        if (iter < end)
          appendedNodes.Append(kPathSepUnichar);
      }
      return NS_ERROR_FAILURE;
    }
    newFile = new nsLocalFile(mFSRef, appendedNodes);
  }
  if (!newFile)
    return NS_ERROR_FAILURE;
  *aParent = newFile;
  NS_ADDREF(*aParent);
  
  return NS_OK;
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
  return InitWithNativePath(NS_ConvertUCS2toUTF8(filePath));
}

/* [noscript] void initWithNativePath (in ACString filePath); */
NS_IMETHODIMP nsLocalFile::InitWithNativePath(const nsACString& filePath)
{
  if (filePath.First() != '/' || filePath.Length() == 0)
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  // On 10.2, huge paths crash CFURLGetFSRef()
  if (filePath.Length() > PATH_MAX)
    return NS_ERROR_FILE_NAME_TOO_LONG;
  // And, a path with consecutive '/'s which are not between
  // nodes also crashes CFURLGetFSRef(). Consecutive '/'s which
  // are between actual nodes are OK. So, convert consecutive
  // '/'s to a single one.
  nsCAutoString fixedPath;
  fixedPath.Assign(filePath);
  fixedPath.ReplaceSubstring("//", "/");

  mNonExtantNodes.clear();
  mIdentityDirty = PR_TRUE;

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

  CFStringRef leaf = nsnull;
  StBuffer<UniChar> buffer;
  PRBool success;
  while ((success = ::CFURLGetFSRef(pathAsCFURL, &mFSRef)) == PR_FALSE) {
    // Extract the last component and push onto the front of mNonExtantNodes
    leaf = ::CFURLCopyLastPathComponent(pathAsCFURL);
    if (!leaf)
      break;
    CFIndex leafLen = ::CFStringGetLength(leaf);
    if (!buffer.EnsureElemCapacity(leafLen + 1))
      break;
    ::CFStringGetCharacters(leaf, CFRangeMake(0, leafLen), buffer.get());
    buffer.get()[leafLen] = '\0';
    mNonExtantNodes.push_front(nsString(nsDependentString(buffer.get())));
    ::CFRelease(leaf);
    leaf = nsnull;
    
    // Get the parent of the leaf for the next go round
    CFURLRef parent = ::CFURLCreateCopyDeletingLastPathComponent(NULL, pathAsCFURL);
    if (!parent)
      break;
    ::CFRelease(pathAsCFURL);
    pathAsCFURL = parent;
  }
  ::CFRelease(pathAsCFString);
  ::CFRelease(pathAsCFURL);
  if (leaf)
    ::CFRelease(leaf);

  return success ? NS_OK : NS_ERROR_FAILURE;
}

/* void initWithFile (in nsILocalFile aFile); */
NS_IMETHODIMP nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  
  // Argh - since no rtti, we have to use a static_cast.
  // At least QI to an nsILocalFileMac, making the static_cast less of a leap.
  nsCOMPtr<nsILocalFileMac> asLocalFileMac(do_QueryInterface(aFile));
  if (!asLocalFileMac)
    return NS_ERROR_UNEXPECTED;
    
  nsLocalFile *asLocalFile = NS_STATIC_CAST(nsLocalFile*, aFile);
  mFSRef = asLocalFile->mFSRef;
  
  // XXX - deque::operator= generates a signed/unsigned comparison warning which,
  // because of templates, is long and ugly. Do it by hand here because, either way,
  // the expense here is in copy constructing nsStrings.
  mNonExtantNodes.clear();
  deque<nsString>::iterator iter(asLocalFile->mNonExtantNodes.begin());
  while (iter != asLocalFile->mNonExtantNodes.end()) {
    mNonExtantNodes.push_back(*iter);
    ++iter;
  }
  mTargetFSRef = asLocalFile->mTargetFSRef;
  mIdentityDirty = PR_TRUE;
  return NS_OK;
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
  mFollowLinks = aFollowLinks;
  mFollowLinksDirty = PR_TRUE;
  return NS_OK;
}

/* [noscript] PRFileDescStar openNSPRFileDesc (in long flags, in long mode); */
NS_IMETHODIMP nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
  // XXX - needs change for CFM
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
  // XXX - needs change for CFM
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
  // XXX - needs change for CFM
  NS_ENSURE_ARG_POINTER(_retval);

  NS_TIMELINE_START_TIMER("PR_LoadLibrary");

  nsCAutoString path;
  nsresult rv = GetPathInternal(path);
  if (NS_FAILED(rv))
    return rv;

  *_retval = PR_LoadLibrary(path.get());

  NS_TIMELINE_STOP_TIMER("PR_LoadLibrary");
  NS_TIMELINE_MARK_TIMER1("PR_LoadLibrary", mPath.get());

  if (!*_retval)
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

/* readonly attribute PRInt64 diskSpaceAvailable; */
NS_IMETHODIMP nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
  NS_ENSURE_ARG_POINTER(aDiskSpaceAvailable);
  
  OSErr err;
  FSCatalogInfo catalogInfo;
  err = ::FSGetCatalogInfo(&mFSRef, kFSCatInfoVolume, &catalogInfo,
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
  if (relativeFilePath.IsEmpty())
    return NS_OK;
  // No leading '/' 
  if (relativeFilePath.First() == '/')
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  
  // Parse the nodes and call Append() for each
  nsAString::const_iterator nodeBegin, pathEnd;
  relativeFilePath.BeginReading(nodeBegin);
  relativeFilePath.EndReading(pathEnd);
  nsAString::const_iterator nodeEnd(nodeBegin);
  
  while (nodeEnd != pathEnd) {
    FindCharInReadable(kPathSepUnichar, nodeEnd, pathEnd);
    nsresult rv = Append(Substring(nodeBegin, nodeEnd));
    if (NS_FAILED(rv))
      return rv;
    if (nodeEnd != pathEnd) // If there's more left in the string, inc over the '/' nodeEnd is on.
      ++nodeEnd;
    nodeBegin = nodeEnd;
  }
  return NS_OK;
}

/* [noscript] void appendRelativeNativePath (in ACString relativeFilePath); */
NS_IMETHODIMP nsLocalFile::AppendRelativeNativePath(const nsACString& relativeFilePath)
{
  return AppendRelativePath(NS_ConvertUTF8toUCS2(relativeFilePath));
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

  nsresult rv = NS_OK;
  
  PRUint32 dataSize = aPersistentDescriptor.Length();    
  char* decodedData = PL_Base64Decode(PromiseFlatCString(aPersistentDescriptor).get(), dataSize, nsnull);
  if (!decodedData) {
    NS_ERROR("SetPersistentDescriptor was given bad data");
    return NS_ERROR_FAILURE;
  }
  
  // Cast to an alias record and resolve.
  PRInt32		aliasSize = (dataSize * 3) / 4;
  AliasRecord	aliasHeader = *(AliasPtr)decodedData;
  if (aliasHeader.aliasSize > aliasSize) {		// be paranoid about having too few data
    PR_Free(decodedData);
    return NS_ERROR_FAILURE;
  }
  
  aliasSize = aliasHeader.aliasSize;
  
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
  // XXX - Because we define DARWIN, we can't use CFURLGetFSRef. This blows.
  nsresult rv = NS_ERROR_FAILURE;
  UInt8 buffer[512];
  if (::CFURLGetFileSystemRepresentation(aCFURL, TRUE, buffer, sizeof(buffer)))
    rv = InitWithNativePath(nsDependentCString((char *)buffer));
      
  return rv;
}

/* void initWithFSRef ([const] in FSRefPtr aFSRef); */
NS_IMETHODIMP nsLocalFile::InitWithFSRef(const FSRef *aFSRef)
{
  NS_ENSURE_ARG(aFSRef);
  
  mFSRef = *aFSRef;
  mNonExtantNodes.clear();
  mTargetFSRef = kInvalidFSRef;
  mIdentityDirty = PR_FALSE;
  mFollowLinksDirty = PR_TRUE;
  return NS_OK; 
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
      
    mFSRef = fsRef;
    mNonExtantNodes.clear();
    mNonExtantNodes.push_back(nsString(nsDependentString(unicodeName.unicode, unicodeName.length)));
    mTargetFSRef = kInvalidFSRef;
    mIdentityDirty = PR_FALSE;
    mFollowLinksDirty = PR_TRUE;
    
    return NS_OK;
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
  *_retval = nsnull;
  
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_SUCCEEDED(rv)) {
    *_retval = ::CFURLCreateFromFSRef(NULL, &fsRef);
  }
  else {
    nsCAutoString path;
    rv = GetPathInternal(path);
    if (NS_FAILED(rv))
      return rv;
    CFStringRef pathAsCFString;
    pathAsCFString = ::CFStringCreateWithCString(nsnull, path.get(), kCFStringEncodingUTF8);
    if (!pathAsCFString)
      return NS_ERROR_FAILURE;
    *_retval = ::CFURLCreateWithFileSystemPath(nsnull, pathAsCFString, kCFURLPOSIXPathStyle, PR_FALSE);
    ::CFRelease(pathAsCFString);
  }
  return *_retval ? NS_OK : NS_ERROR_FAILURE;
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
  
  OSErr err;
  FSRef fsRef;
  nsresult rv = GetFSRefInternal(fsRef);
  if (NS_SUCCEEDED(rv)) {
    // If the leaf node exists, things are simple.
    err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone,
              nsnull, nsnull, _retval, nsnull);
    return MacErrorMapper(err); 
  }
  else if (rv == NS_ERROR_FILE_NOT_FOUND && mNonExtantNodes.size() == 1) {
    // If the parent of the leaf exists, make an FSSpec from that.
    FSCatalogInfo catalogInfo;
    err = ::FSGetCatalogInfo(&mFSRef,
              kFSCatInfoVolume + kFSCatInfoNodeID + kFSCatInfoTextEncoding,
              &catalogInfo, nsnull, nsnull, nsnull);
    if (err != noErr)
      return MacErrorMapper(err);

    Str31 hfsName;    
    const nsString& leafName = mNonExtantNodes.back();
    err = ::UnicodeNameGetHFSName(leafName.Length(),
                                  PromiseFlatString(leafName).get(),
                                  catalogInfo.textEncodingHint,
                                  catalogInfo.nodeID == fsRtDirID,
                                  hfsName);
    err = ::FSMakeFSSpec(catalogInfo.volume, catalogInfo.nodeID, hfsName, _retval);
    return MacErrorMapper(err);
  }
  return NS_ERROR_FAILURE;
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


//*****************************************************************************
//  nsLocalFile Methods
//*****************************************************************************
#pragma mark -
#pragma mark [Protected Methods]

nsresult nsLocalFile::Resolve()
{
  nsresult rv = NS_OK;
  
  if (mIdentityDirty) {
    rv = ResolveNonExtantNodes(PR_FALSE);
    if (NS_SUCCEEDED(rv))
      mIdentityDirty = PR_FALSE;
    mFollowLinksDirty = PR_TRUE;
  }
  if (mFollowLinksDirty && NS_SUCCEEDED(rv)) {
    if (mFollowLinks) {
      Boolean isFolder, isAlias;
      mTargetFSRef = mFSRef;
      OSErr err = ::FSResolveAliasFile(&mTargetFSRef, PR_TRUE, &isFolder, &isAlias);
      if (err == noErr)
        mFollowLinksDirty = PR_FALSE;
      else
        rv = MacErrorMapper(err);
    }
    else {
      mTargetFSRef = kInvalidFSRef;
      mFollowLinksDirty = PR_FALSE;
    }
  }
  return rv;
}

nsresult nsLocalFile::GetFSRefInternal(FSRef& aFSSpec)
{
  nsresult rv = Resolve();
  if (NS_FAILED(rv))
    return rv;
  aFSSpec = mFollowLinks ? mTargetFSRef : mFSRef;  
  return NS_OK;
}

nsresult nsLocalFile::GetPathInternal(nsACString& path)
{
  FSRef fsRef;
  UInt8 pathBuf[PATH_MAX + 1];
 
  if (NS_SUCCEEDED(Resolve()))
    fsRef = mFollowLinks ? mTargetFSRef : mFSRef;    
  else
    fsRef = mFSRef;
 
  OSErr err = ::FSRefMakePath(&fsRef, pathBuf, sizeof(pathBuf));
  if (err != noErr)
      return MacErrorMapper(err);
  path.Assign((char *)pathBuf);

  // If Resolve() succeeds, mNonExtantNodes is empty.
  deque<nsString>::iterator iter(mNonExtantNodes.begin());
  deque<nsString>::iterator end(mNonExtantNodes.end());
  while (iter != end) {
    path.Append(kPathSepChar);
    path.Append((NS_ConvertUCS2toUTF8(*iter)));
    ++iter;
  }

  return NS_OK;
}

nsresult nsLocalFile::ResolveNonExtantNodes(PRBool aCreateDirs)
{
    deque<nsString>::iterator iter(mNonExtantNodes.begin());
    while (iter != mNonExtantNodes.end()) {
        const nsString& currNode = *iter;
        ++iter;
        
        FSRef newRef;
        OSErr err = ::FSMakeFSRefUnicode(&mFSRef,
                                        currNode.Length(),
                                        (const UniChar *)currNode.get(),
                                        kTextEncodingUnknown,
                                        &newRef);
         
        if (err == fnfErr && aCreateDirs) {
          if (iter == mNonExtantNodes.end()) // Don't create the terminal node
            return NS_OK;
          err = ::FSCreateDirectoryUnicode(&mFSRef,
                                          currNode.Length(),
                                          (const UniChar *)currNode.get(),
                                          kFSCatInfoNone,
                                          nsnull, &newRef, nsnull, nsnull);
        }
                           
        // If it exists or we successfully created it,
        // pop the node from the front and continue.
        if (err == noErr) {
            mNonExtantNodes.pop_front();
            iter = mNonExtantNodes.begin();
            mFSRef = newRef;
        }
        else
            return NS_ERROR_FILE_NOT_FOUND;
    }
    return NS_OK;                
}

nsresult nsLocalFile::MoveCopy(nsIFile* aParentDir, const nsAString& newName, PRBool isCopy, PRBool followLinks)
{
  StFollowLinksState srcFollowState(*this, followLinks);

  nsresult rv;
  OSErr err;
  FSRef srcRef, newRef;
  
  rv = GetFSRefInternal(srcRef);
  if (NS_FAILED(rv))
    return rv;
    
  nsCOMPtr<nsIFile> newParentDir = aParentDir;

  if (!newParentDir) {
    if (newName.IsEmpty())
      return NS_ERROR_INVALID_ARG;
    // A move with no new parent dir is a simple rename.    
    if (!isCopy) {
      err = ::FSRenameUnicode(&srcRef, newName.Length(), PromiseFlatString(newName).get(), kTextEncodingUnknown, &newRef);
      if (err != noErr)
        return MacErrorMapper(err);
      mFSRef = newRef;
      return NS_OK;
    }
    else {
      rv = GetParent(getter_AddRefs(newParentDir));
      if (NS_FAILED(rv))
        return rv;    
    }
  }
  
  // If newParentDir does not exist, create it
  PRBool exists;
  rv = newParentDir->Exists(&exists);
  if (NS_FAILED(rv))
    return rv;
  if (!exists) {
    rv = newParentDir->Create(nsIFile::DIRECTORY_TYPE, 0666);
    if (NS_FAILED(rv))
      return rv;
  }
    
  FSRef destRef;
  nsCOMPtr<nsILocalFileMac> newParentDirMac(do_QueryInterface(newParentDir));
  if (!newParentDirMac)
    return NS_ERROR_NO_INTERFACE;
  rv = newParentDirMac->GetFSRef(&destRef);
  if (NS_FAILED(rv))
    return rv;
    
  if (isCopy) {
    err = ::FSCopyObject(&srcRef, &destRef,
                         newName.Length(), newName.Length() ? PromiseFlatString(newName).get() : nsnull,
                         0, kFSCatInfoNone, false, false, nsnull, nsnull, &newRef);
    // don't update mFSRef on a copy
  }
  else {
    // First, try the quick way which works only within the same volume
    err = ::FSMoveRenameObjectUnicode(&srcRef, &destRef,
              newName.Length(), newName.Length() ? PromiseFlatString(newName).get() : nsnull,
              kTextEncodingUnknown, &newRef);
              
    if (err == diffVolErr) {
      // If on different volumes, resort to copy & delete
      err = ::FSCopyObject(&srcRef, &destRef,
                           newName.Length(), newName.Length() ? PromiseFlatString(newName).get() : nsnull,
                           0, kFSCatInfoNone, false, false, nsnull, nsnull, &newRef);

      if (err == noErr)
        mFSRef = newRef;
      err = ::FSDeleteObjects(&srcRef);
    }
  }
  return MacErrorMapper(err);
}

const PRInt64 kMilisecsPerSec = 1000LL;
const PRInt64 kUTCDateTimeFractionDivisor = 65535LL;

PRInt64 nsLocalFile::HFSPlustoNSPRTime(const UTCDateTime& utcTime)
{
  // Start with seconds since Jan. 1, 1904 GMT
  PRInt64 result = ((PRInt64)utcTime.highSeconds << 32) + (PRInt64)utcTime.lowSeconds; 
  // Subtract to convert to NSPR epoch of 1970
  result -= kJanuaryFirst1970Seconds;
  // Convert to milisecs
  result *= kMilisecsPerSec;
  // Convert the fraction to milisecs and add it
  result += ((PRInt64)utcTime.fraction * kMilisecsPerSec) / kUTCDateTimeFractionDivisor;

  return result;
}

void nsLocalFile::NSPRtoHFSPlusTime(PRInt64 nsprTime, UTCDateTime& utcTime)
{
  PRInt64 fraction = nsprTime % kMilisecsPerSec;
  PRInt64 seconds = (nsprTime / kMilisecsPerSec) + kJanuaryFirst1970Seconds;
  utcTime.highSeconds = (UInt16)((PRUint64)seconds >> 32);
  utcTime.lowSeconds = (UInt32)seconds;
  utcTime.fraction = (UInt16)((fraction * kUTCDateTimeFractionDivisor) / kMilisecsPerSec);
}

//*****************************************************************************
//  Global Functions
//*****************************************************************************
#pragma mark -
#pragma mark [Global Functions]

void nsLocalFile::GlobalInit()
{
  long response;
  OSErr err = ::Gestalt(gestaltFSAttr, &response);
  if (err == noErr && (response & (1 << gestaltFSUsesPOSIXPathsForConversion)))
    kPathSepChar = '/';
  else
    kPathSepChar = ':';
  kPathSepUnichar = PRUnichar(kPathSepChar);

#ifdef XP_MACOSX  
  // Get an FSRef of the root.
  err = FSPathMakeRef((UInt8 *)"/", &kRootFSRef, nsnull);
  NS_ASSERTION(err == noErr, "Could not make root FSRef");
#endif
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
    return NS_NewLocalFile(NS_ConvertUTF8toUCS2(path), followLinks, result);
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
