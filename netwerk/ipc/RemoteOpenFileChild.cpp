/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/RemoteOpenFileChild.h"
#include "nsIRemoteOpenFileListener.h"
#include "mozilla/ipc/URIUtils.h"

// needed to alloc/free NSPR file descriptors
#include "private/pprio.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS2(RemoteOpenFileChild,
                              nsIFile,
                              nsIHashable)


RemoteOpenFileChild::RemoteOpenFileChild(const RemoteOpenFileChild& other)
  : mNSPRFileDesc(other.mNSPRFileDesc)
  , mAsyncOpenCalled(other.mAsyncOpenCalled)
  , mNSPROpenCalled(other.mNSPROpenCalled)
{
  // Note: don't clone mListener or we'll have a refcount leak.
  other.mURI->Clone(getter_AddRefs(mURI));
  other.mFile->Clone(getter_AddRefs(mFile));
}

RemoteOpenFileChild::~RemoteOpenFileChild()
{
  if (mNSPRFileDesc) {
    // If we handed out fd we shouldn't have pointer to it any more.
    MOZ_ASSERT(!mNSPROpenCalled);
    // PR_Close both closes the file and deallocates the PRFileDesc
    PR_Close(mNSPRFileDesc);
  }
}

nsresult
RemoteOpenFileChild::Init(nsIURI* aRemoteOpenUri)
{
  if (!aRemoteOpenUri) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString scheme;
  nsresult rv = aRemoteOpenUri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!scheme.EqualsLiteral("remoteopenfile")) {
    return NS_ERROR_INVALID_ARG;
  }

  // scheme of URI is not file:// so this is not a nsIFileURL.  Convert to one.
  nsCOMPtr<nsIURI> clonedURI;
  rv = aRemoteOpenUri->Clone(getter_AddRefs(clonedURI));
  NS_ENSURE_SUCCESS(rv, rv);

  clonedURI->SetScheme(NS_LITERAL_CSTRING("file"));
  nsAutoCString spec;
  clonedURI->GetSpec(spec);

  rv = NS_NewURI(getter_AddRefs(mURI), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get nsIFile
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mURI);
  if (!fileURL) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = fileURL->GetFile(getter_AddRefs(mFile));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
RemoteOpenFileChild::AsyncRemoteFileOpen(int32_t aFlags,
                                         nsIRemoteOpenFileListener* aListener,
                                         nsITabChild* aTabChild)
{
  if (!mFile) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!aListener) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mAsyncOpenCalled) {
    return NS_ERROR_ALREADY_OPENED;
  }

  if (aFlags != PR_RDONLY) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mozilla::dom::TabChild* tabChild = nullptr;
  if (aTabChild) {
    tabChild = static_cast<mozilla::dom::TabChild*>(aTabChild);
  }

#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
  // we do nothing on these platforms: we'll just open file locally when asked
  // for NSPR handle
  mListener->OnRemoteFileOpenComplete(NS_OK);
  mListener = nullptr;
  mAsyncOpenCalled = true;
  return NS_OK;

#else
  URIParams uri;
  SerializeURI(mURI, uri);

  gNeckoChild->SendPRemoteOpenFileConstructor(this, uri, tabChild);

  // Can't seem to reply from within IPDL Parent constructor, so send open as
  // separate message
  SendAsyncOpenFile();

  // The chrome process now has a logical ref to us until we call Send__delete
  AddIPDLReference();

  mListener = aListener;
  mAsyncOpenCalled = true;
  return NS_OK;
#endif
}


//-----------------------------------------------------------------------------
// RemoteOpenFileChild::PRemoteOpenFileChild
//-----------------------------------------------------------------------------

bool
RemoteOpenFileChild::RecvFileOpened(const FileDescriptor& aFD,
                                    const nsresult& aRV)
{
#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
  NS_NOTREACHED("osX and Windows shouldn't be doing IPDL here");
#else
  if (NS_SUCCEEDED(aRV)) {
    mNSPRFileDesc = PR_AllocFileDesc(aFD.PlatformHandle(), PR_GetFileMethods());
  }

  MOZ_ASSERT(mListener);

  mListener->OnRemoteFileOpenComplete(aRV);

  mListener = nullptr;     // release ref to listener

  // This calls NeckoChild::DeallocPRemoteOpenFile(), which deletes |this| if
  // IPDL holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);

#endif

  return true;
}

void
RemoteOpenFileChild::AddIPDLReference()
{
  AddRef();
}

void
RemoteOpenFileChild::ReleaseIPDLReference()
{
  // if we haven't gotten fd from parent yet, we're not going to.
  if (mListener) {
    mListener->OnRemoteFileOpenComplete(NS_ERROR_UNEXPECTED);
    mListener = nullptr;
  }

  Release();
}

//-----------------------------------------------------------------------------
// RemoteOpenFileChild::nsIFile functions that we override logic for
//-----------------------------------------------------------------------------

NS_IMETHODIMP
RemoteOpenFileChild::Clone(nsIFile **file)
{
  *file = new RemoteOpenFileChild(*this);
  NS_ADDREF(*file);

  // if we transferred ownership of file to clone, forget our pointer.
  if (mNSPRFileDesc) {
    mNSPRFileDesc = nullptr;
  }

  return NS_OK;
}

/* The main event: get file descriptor from parent process
 */
NS_IMETHODIMP
RemoteOpenFileChild::OpenNSPRFileDesc(int32_t aFlags, int32_t aMode,
                                      PRFileDesc **aRetval)
{
#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
  // Windows and OSX builds: just open nsIFile locally.
  return mFile->OpenNSPRFileDesc(aFlags, aMode, aRetval);

#else
  if (aFlags != PR_RDONLY) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Unlike regular nsIFile we can't (easily) support multiple open()s.
  if (mNSPROpenCalled) {
    return NS_ERROR_ALREADY_OPENED;
  }

  if (!mNSPRFileDesc) {
    // client skipped AsyncRemoteFileOpen() or didn't wait for result, or this
    // object has been cloned
    return NS_ERROR_NOT_AVAILABLE;
  }

  // hand off ownership (i.e responsibility to PR_Close() file handle) to caller
  *aRetval = mNSPRFileDesc;
  mNSPRFileDesc = nullptr;
  mNSPROpenCalled = true;

  return NS_OK;
#endif
}


//-----------------------------------------------------------------------------
// RemoteOpenFileChild::nsIFile functions that we delegate to underlying nsIFile
//-----------------------------------------------------------------------------

nsresult
RemoteOpenFileChild::GetLeafName(nsAString &aLeafName)
{
  return mFile->GetLeafName(aLeafName);
}

NS_IMETHODIMP
RemoteOpenFileChild::GetNativeLeafName(nsACString &aLeafName)
{
  return mFile->GetNativeLeafName(aLeafName);
}

nsresult
RemoteOpenFileChild::GetTarget(nsAString &_retval)
{
  return mFile->GetTarget(_retval);
}

NS_IMETHODIMP
RemoteOpenFileChild::GetNativeTarget(nsACString &_retval)
{
  return mFile->GetNativeTarget(_retval);
}

nsresult
RemoteOpenFileChild::GetPath(nsAString &_retval)
{
  return mFile->GetPath(_retval);
}

NS_IMETHODIMP
RemoteOpenFileChild::GetNativePath(nsACString &_retval)
{
  return mFile->GetNativePath(_retval);
}

NS_IMETHODIMP
RemoteOpenFileChild::Equals(nsIFile *inFile, bool *_retval)
{
  return mFile->Equals(inFile, _retval);
}

NS_IMETHODIMP
RemoteOpenFileChild::Contains(nsIFile *inFile, bool recur, bool *_retval)
{
  return mFile->Contains(inFile, recur, _retval);
}

NS_IMETHODIMP
RemoteOpenFileChild::GetParent(nsIFile **aParent)
{
  return mFile->GetParent(aParent);
}

NS_IMETHODIMP
RemoteOpenFileChild::GetFollowLinks(bool *aFollowLinks)
{
  return mFile->GetFollowLinks(aFollowLinks);
}

//-----------------------------------------------------------------------------
// RemoteOpenFileChild::nsIFile functions that are not currently supported
//-----------------------------------------------------------------------------

nsresult
RemoteOpenFileChild::Append(const nsAString &node)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::AppendNative(const nsACString &fragment)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Create(uint32_t type, uint32_t permissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RemoteOpenFileChild::SetLeafName(const nsAString &aLeafName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetNativeLeafName(const nsACString &aLeafName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RemoteOpenFileChild::InitWithPath(const nsAString &filePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::InitWithNativePath(const nsACString &filePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::InitWithFile(nsIFile *aFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetFollowLinks(bool aFollowLinks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult  
RemoteOpenFileChild::AppendRelativePath(const nsAString &node)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::AppendRelativeNativePath(const nsACString &fragment)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetRelativeDescriptor(nsIFile *fromFile, nsACString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetRelativeDescriptor(nsIFile *fromFile,
                                   const nsACString& relativeDesc)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RemoteOpenFileChild::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::CopyToNative(nsIFile *newParent, const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RemoteOpenFileChild::CopyToFollowingLinks(nsIFile *newParentDir,
                                  const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::CopyToFollowingLinksNative(nsIFile *newParent,
                                        const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RemoteOpenFileChild::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::MoveToNative(nsIFile *newParent, const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Remove(bool recursive)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetPermissions(uint32_t *aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetPermissions(uint32_t aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetPermissionsOfLink(uint32_t *aPermissionsOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetPermissionsOfLink(uint32_t aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetLastModifiedTime(PRTime *aLastModTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetLastModifiedTime(PRTime aLastModTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetLastModifiedTimeOfLink(PRTime *aLastModTimeOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetLastModifiedTimeOfLink(PRTime aLastModTimeOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetFileSize(int64_t *aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::SetFileSize(int64_t aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetFileSizeOfLink(int64_t *aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Exists(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsWritable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsReadable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsExecutable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsHidden(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsDirectory(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsFile(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsSymlink(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::IsSpecial(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::CreateUnique(uint32_t type, uint32_t attributes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetDirectoryEntries(nsISimpleEnumerator **entries)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::OpenANSIFileDesc(const char *mode, FILE **_retval)
{
  // TODO: can implement using fdopen()?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Load(PRLibrary **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetDiskSpaceAvailable(int64_t *aDiskSpaceAvailable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Reveal()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::Launch()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// RemoteOpenFileChild::nsIHashable functions that we delegate to underlying nsIFile
//-----------------------------------------------------------------------------

NS_IMETHODIMP
RemoteOpenFileChild::Equals(nsIHashable* aOther, bool *aResult)
{
  nsCOMPtr<nsIHashable> hashable = do_QueryInterface(mFile);

  MOZ_ASSERT(hashable);

  if (hashable) {
    return hashable->Equals(aOther, aResult);
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
RemoteOpenFileChild::GetHashCode(uint32_t *aResult)
{
  nsCOMPtr<nsIHashable> hashable = do_QueryInterface(mFile);

  MOZ_ASSERT(hashable);

  if (hashable) {
    return hashable->GetHashCode(aResult);
  }
  return NS_ERROR_UNEXPECTED;
}

} // namespace net
} // namespace mozilla

