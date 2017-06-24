/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorFile.h"

#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "private/pprio.h"
#include "SerializedLoadContext.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(FileDescriptorFile, nsIFile)

LazyLogModule gFDFileLog("FDFile");
#undef DBG
#define DBG(...) MOZ_LOG(gFDFileLog, LogLevel::Debug, (__VA_ARGS__))

FileDescriptorFile::FileDescriptorFile(const FileDescriptor& aFD,
                                       nsIFile* aFile)
{
  MOZ_ASSERT(aFD.IsValid());
  auto platformHandle = aFD.ClonePlatformHandle();
  mFD = FileDescriptor(platformHandle.get());
  mFile = aFile;
}

FileDescriptorFile::FileDescriptorFile(const FileDescriptorFile& aOther)
{
  auto platformHandle = aOther.mFD.ClonePlatformHandle();
  mFD = FileDescriptor(platformHandle.get());
  aOther.mFile->Clone(getter_AddRefs(mFile));
}

//-----------------------------------------------------------------------------
// FileDescriptorFile::nsIFile functions that we override logic for
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FileDescriptorFile::Clone(nsIFile **aFileOut)
{
  RefPtr<FileDescriptorFile> fdFile = new FileDescriptorFile(*this);
  fdFile.forget(aFileOut);
  return NS_OK;
}

NS_IMETHODIMP
FileDescriptorFile::OpenNSPRFileDesc(int32_t aFlags, int32_t aMode,
                                     PRFileDesc **aRetval)
{
  // Remove optional OS_READAHEAD flag so we test against PR_RDONLY
  aFlags &= ~nsIFile::OS_READAHEAD;

  // Remove optional/deprecated DELETE_ON_CLOSE flag
  aFlags &= ~nsIFile::DELETE_ON_CLOSE;

  // All other flags require write access to the file and
  // this implementation only provides read access.
  if (aFlags != PR_RDONLY) {
    DBG("OpenNSPRFileDesc flags error (%" PRIu32 ")\n", aFlags);
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mFD.IsValid()) {
    DBG("OpenNSPRFileDesc error: no file descriptor\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto platformHandle = mFD.ClonePlatformHandle();
  *aRetval = PR_ImportFile(PROsfd(platformHandle.release()));

  if (!*aRetval) {
    DBG("OpenNSPRFileDesc Clone failure\n");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FileDescriptorFile::nsIFile functions that we delegate to underlying nsIFile
//-----------------------------------------------------------------------------

nsresult
FileDescriptorFile::GetLeafName(nsAString &aLeafName)
{
  return mFile->GetLeafName(aLeafName);
}

NS_IMETHODIMP
FileDescriptorFile::GetNativeLeafName(nsACString &aLeafName)
{
  return mFile->GetNativeLeafName(aLeafName);
}

nsresult
FileDescriptorFile::GetTarget(nsAString &_retval)
{
  return mFile->GetTarget(_retval);
}

NS_IMETHODIMP
FileDescriptorFile::GetNativeTarget(nsACString &_retval)
{
  return mFile->GetNativeTarget(_retval);
}

nsresult
FileDescriptorFile::GetPath(nsAString &_retval)
{
  return mFile->GetPath(_retval);
}

NS_IMETHODIMP
FileDescriptorFile::GetNativePath(nsACString &_retval)
{
  return mFile->GetNativePath(_retval);
}

NS_IMETHODIMP
FileDescriptorFile::Equals(nsIFile *inFile, bool *_retval)
{
  return mFile->Equals(inFile, _retval);
}

NS_IMETHODIMP
FileDescriptorFile::Contains(nsIFile *inFile, bool *_retval)
{
  return mFile->Contains(inFile, _retval);
}

NS_IMETHODIMP
FileDescriptorFile::GetParent(nsIFile **aParent)
{
  return mFile->GetParent(aParent);
}

NS_IMETHODIMP
FileDescriptorFile::GetFollowLinks(bool *aFollowLinks)
{
  return mFile->GetFollowLinks(aFollowLinks);
}

//-----------------------------------------------------------------------------
// FileDescriptorFile::nsIFile functions that are not currently supported
//-----------------------------------------------------------------------------

nsresult
FileDescriptorFile::Append(const nsAString &node)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::AppendNative(const nsACString &fragment)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Create(uint32_t type, uint32_t permissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::SetLeafName(const nsAString &aLeafName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetNativeLeafName(const nsACString &aLeafName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::InitWithPath(const nsAString &filePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::InitWithNativePath(const nsACString &filePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::InitWithFile(nsIFile *aFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetFollowLinks(bool aFollowLinks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::AppendRelativePath(const nsAString &node)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::AppendRelativeNativePath(const nsACString &fragment)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetRelativeDescriptor(nsIFile *fromFile, nsACString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetRelativeDescriptor(nsIFile *fromFile,
                                   const nsACString& relativeDesc)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetRelativePath(nsIFile *fromFile, nsACString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetRelativePath(nsIFile *fromFile,
                                     const nsACString& relativePath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::CopyToNative(nsIFile *newParent, const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::CopyToFollowingLinks(nsIFile *newParentDir,
                                  const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::CopyToFollowingLinksNative(nsIFile *newParent,
                                        const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
FileDescriptorFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::MoveToNative(nsIFile *newParent, const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::RenameTo(nsIFile *newParentDir, const nsAString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::RenameToNative(nsIFile *newParentDir, const nsACString &newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Remove(bool recursive)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetPermissions(uint32_t *aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetPermissions(uint32_t aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetPermissionsOfLink(uint32_t *aPermissionsOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetPermissionsOfLink(uint32_t aPermissions)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetLastModifiedTime(PRTime *aLastModTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetLastModifiedTime(PRTime aLastModTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetLastModifiedTimeOfLink(PRTime *aLastModTimeOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetLastModifiedTimeOfLink(PRTime aLastModTimeOfLink)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetFileSize(int64_t *aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::SetFileSize(int64_t aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetFileSizeOfLink(int64_t *aFileSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Exists(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsWritable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsReadable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsExecutable(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsHidden(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsDirectory(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsFile(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsSymlink(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::IsSpecial(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::CreateUnique(uint32_t type, uint32_t attributes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetDirectoryEntries(nsISimpleEnumerator **entries)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::OpenANSIFileDesc(const char *mode, FILE **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Load(PRLibrary **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::GetDiskSpaceAvailable(int64_t *aDiskSpaceAvailable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Reveal()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorFile::Launch()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace net
} // namespace mozilla
