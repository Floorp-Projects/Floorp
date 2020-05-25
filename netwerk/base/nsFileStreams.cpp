/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/IPCMessageUtils.h"

#if defined(XP_UNIX) || defined(XP_BEOS)
#  include <unistd.h>
#elif defined(XP_WIN)
#  include <windows.h>
#  include "nsILocalFileWin.h"
#else
// XXX add necessary include file for ftruncate (or equivalent)
#endif

#include "private/pprio.h"
#include "prerror.h"

#include "IOActivityMonitor.h"
#include "nsFileStreams.h"
#include "nsIFile.h"
#include "nsReadLine.h"
#include "nsIClassInfoImpl.h"
#include "nsLiteralString.h"
#include "nsSocketTransport2.h"  // for ErrorAccordingToNSPR()
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/FileUtils.h"
#include "nsNetCID.h"
#include "nsXULAppAPI.h"

typedef mozilla::ipc::FileDescriptor::PlatformHandleType FileHandleType;

using namespace mozilla::ipc;
using namespace mozilla::net;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

////////////////////////////////////////////////////////////////////////////////
// nsFileStreamBase

nsFileStreamBase::nsFileStreamBase()
    : mFD(nullptr),
      mBehaviorFlags(0),
      mState(eUnitialized),
      mErrorValue(NS_ERROR_FAILURE) {}

nsFileStreamBase::~nsFileStreamBase() {
  // We don't want to try to rewrind the stream when shutting down.
  mBehaviorFlags &= ~nsIFileInputStream::REOPEN_ON_REWIND;

  Close();
}

NS_IMPL_ISUPPORTS(nsFileStreamBase, nsISeekableStream, nsITellableStream,
                  nsIFileMetadata)

NS_IMETHODIMP
nsFileStreamBase::Seek(int32_t whence, int64_t offset) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t cnt = PR_Seek64(mFD, offset, (PRSeekWhence)whence);
  if (cnt == int64_t(-1)) {
    return NS_ErrorAccordingToNSPR();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFileStreamBase::Tell(int64_t* result) {
  if (mState == eDeferredOpen && !(mOpenParams.ioFlags & PR_APPEND)) {
    *result = 0;
    return NS_OK;
  }

  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t cnt = PR_Seek64(mFD, 0, PR_SEEK_CUR);
  if (cnt == int64_t(-1)) {
    return NS_ErrorAccordingToNSPR();
  }
  *result = cnt;
  return NS_OK;
}

NS_IMETHODIMP
nsFileStreamBase::SetEOF() {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_UNIX) || defined(XP_BEOS)
  // Some system calls require an EOF offset.
  int64_t offset;
  rv = Tell(&offset);
  if (NS_FAILED(rv)) return rv;
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
  if (ftruncate(PR_FileDesc2NativeHandle(mFD), offset) != 0) {
    NS_ERROR("ftruncate failed");
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_WIN)
  if (!SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(mFD))) {
    NS_ERROR("SetEndOfFile failed");
    return NS_ERROR_FAILURE;
  }
#else
  // XXX not implemented
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsFileStreamBase::GetSize(int64_t* _retval) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  PRFileInfo64 info;
  if (PR_GetOpenFileInfo64(mFD, &info) == PR_FAILURE) {
    return NS_BASE_STREAM_OSERROR;
  }

  *_retval = int64_t(info.size);

  return NS_OK;
}

NS_IMETHODIMP
nsFileStreamBase::GetLastModified(int64_t* _retval) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  PRFileInfo64 info;
  if (PR_GetOpenFileInfo64(mFD, &info) == PR_FAILURE) {
    return NS_BASE_STREAM_OSERROR;
  }

  int64_t modTime = int64_t(info.modifyTime);
  if (modTime == 0) {
    *_retval = 0;
  } else {
    *_retval = modTime / int64_t(PR_USEC_PER_MSEC);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileStreamBase::GetFileDescriptor(PRFileDesc** _retval) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = mFD;
  return NS_OK;
}

nsresult nsFileStreamBase::Close() {
  CleanUpOpen();

  nsresult rv = NS_OK;
  if (mFD) {
    if (PR_Close(mFD) == PR_FAILURE) rv = NS_BASE_STREAM_OSERROR;
    mFD = nullptr;
    mState = eClosed;
  }
  return rv;
}

nsresult nsFileStreamBase::Available(uint64_t* aResult) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  // PR_Available with files over 4GB returns an error, so we have to
  // use the 64-bit version of PR_Available.
  int64_t avail = PR_Available64(mFD);
  if (avail == -1) {
    return NS_ErrorAccordingToNSPR();
  }

  // If available is greater than 4GB, return 4GB
  *aResult = (uint64_t)avail;
  return NS_OK;
}

nsresult nsFileStreamBase::Read(char* aBuf, uint32_t aCount,
                                uint32_t* aResult) {
  nsresult rv = DoPendingOpen();
  if (rv == NS_BASE_STREAM_CLOSED) {
    *aResult = 0;
    return NS_OK;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t bytesRead = PR_Read(mFD, aBuf, aCount);
  if (bytesRead == -1) {
    return NS_ErrorAccordingToNSPR();
  }

  *aResult = bytesRead;
  return NS_OK;
}

nsresult nsFileStreamBase::ReadSegments(nsWriteSegmentFun aWriter,
                                        void* aClosure, uint32_t aCount,
                                        uint32_t* aResult) {
  // ReadSegments is not implemented because it would be inefficient when
  // the writer does not consume all data.  If you want to call ReadSegments,
  // wrap a BufferedInputStream around the file stream.  That will call
  // Read().

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsFileStreamBase::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = false;
  return NS_OK;
}

nsresult nsFileStreamBase::Flush(void) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t cnt = PR_Sync(mFD);
  if (cnt == -1) {
    return NS_ErrorAccordingToNSPR();
  }
  return NS_OK;
}

nsresult nsFileStreamBase::Write(const char* buf, uint32_t count,
                                 uint32_t* result) {
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t cnt = PR_Write(mFD, buf, count);
  if (cnt == -1) {
    return NS_ErrorAccordingToNSPR();
  }
  *result = cnt;
  return NS_OK;
}

nsresult nsFileStreamBase::WriteFrom(nsIInputStream* inStr, uint32_t count,
                                     uint32_t* _retval) {
  MOZ_ASSERT_UNREACHABLE("WriteFrom (see source comment)");
  return NS_ERROR_NOT_IMPLEMENTED;
  // File streams intentionally do not support this method.
  // If you need something like this, then you should wrap
  // the file stream using nsIBufferedOutputStream
}

nsresult nsFileStreamBase::WriteSegments(nsReadSegmentFun reader, void* closure,
                                         uint32_t count, uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
  // File streams intentionally do not support this method.
  // If you need something like this, then you should wrap
  // the file stream using nsIBufferedOutputStream
}

nsresult nsFileStreamBase::MaybeOpen(nsIFile* aFile, int32_t aIoFlags,
                                     int32_t aPerm, bool aDeferred) {
  NS_ENSURE_STATE(aFile);

  mOpenParams.ioFlags = aIoFlags;
  mOpenParams.perm = aPerm;

  if (aDeferred) {
    // Clone the file, as it may change between now and the deferred open
    nsCOMPtr<nsIFile> file;
    nsresult rv = aFile->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    mOpenParams.localFile = std::move(file);
    NS_ENSURE_TRUE(mOpenParams.localFile, NS_ERROR_UNEXPECTED);

    mState = eDeferredOpen;
    return NS_OK;
  }

  mOpenParams.localFile = aFile;

  // Following call open() at main thread.
  // Main thread might be blocked, while open a remote file.
  return DoOpen();
}

void nsFileStreamBase::CleanUpOpen() { mOpenParams.localFile = nullptr; }

nsresult nsFileStreamBase::DoOpen() {
  MOZ_ASSERT(mState == eDeferredOpen || mState == eUnitialized ||
             mState == eClosed);
  NS_ASSERTION(!mFD, "Already have a file descriptor!");
  NS_ASSERTION(mOpenParams.localFile, "Must have a file to open");

  PRFileDesc* fd;
  nsresult rv;

  if (mOpenParams.ioFlags & PR_CREATE_FILE) {
    nsCOMPtr<nsIFile> parent;
    mOpenParams.localFile->GetParent(getter_AddRefs(parent));

    // Result doesn't need to be checked. If the file's parent path does not
    // exist, make it. If it does exist, do nothing.
    if (parent) {
      Unused << parent->Create(nsIFile::DIRECTORY_TYPE, 0755);
    }
  }

#ifdef XP_WIN
  if (mBehaviorFlags & nsIFileInputStream::SHARE_DELETE) {
    nsCOMPtr<nsILocalFileWin> file = do_QueryInterface(mOpenParams.localFile);
    MOZ_ASSERT(file);

    rv = file->OpenNSPRFileDescShareDelete(mOpenParams.ioFlags,
                                           mOpenParams.perm, &fd);
  } else
#endif  // XP_WIN
  {
    rv = mOpenParams.localFile->OpenNSPRFileDesc(mOpenParams.ioFlags,
                                                 mOpenParams.perm, &fd);
  }

  if (rv == NS_OK && IOActivityMonitor::IsActive()) {
    auto nativePath = mOpenParams.localFile->NativePath();
    if (!nativePath.IsEmpty()) {
// registering the file to the activity monitor
#ifdef XP_WIN
      // 16 bits unicode
      IOActivityMonitor::MonitorFile(
          fd, NS_ConvertUTF16toUTF8(nativePath.get()).get());
#else
      // 8 bit unicode
      IOActivityMonitor::MonitorFile(fd, nativePath.get());
#endif
    }
  }

  CleanUpOpen();

  if (NS_FAILED(rv)) {
    mState = eError;
    mErrorValue = rv;
    return rv;
  }

  mFD = fd;
  mState = eOpened;

  return NS_OK;
}

nsresult nsFileStreamBase::DoPendingOpen() {
  switch (mState) {
    case eUnitialized:
      MOZ_CRASH("This should not happen.");
      return NS_ERROR_FAILURE;

    case eDeferredOpen:
      return DoOpen();

    case eOpened:
      MOZ_ASSERT(mFD);
      if (NS_WARN_IF(!mFD)) {
        return NS_ERROR_FAILURE;
      }
      return NS_OK;

    case eClosed:
      MOZ_ASSERT(!mFD);
      return NS_BASE_STREAM_CLOSED;

    case eError:
      return mErrorValue;
  }

  MOZ_CRASH("Invalid mState value.");
  return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// nsFileInputStream

NS_IMPL_ADDREF_INHERITED(nsFileInputStream, nsFileStreamBase)
NS_IMPL_RELEASE_INHERITED(nsFileInputStream, nsFileStreamBase)

NS_IMPL_CLASSINFO(nsFileInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_LOCALFILEINPUTSTREAM_CID)

NS_INTERFACE_MAP_BEGIN(nsFileInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIFileInputStream)
  NS_INTERFACE_MAP_ENTRY(nsILineInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIIPCSerializableInputStream)
  NS_IMPL_QUERY_CLASSINFO(nsFileInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream, IsCloneable())
NS_INTERFACE_MAP_END_INHERITING(nsFileStreamBase)

NS_IMPL_CI_INTERFACE_GETTER(nsFileInputStream, nsIInputStream,
                            nsIFileInputStream, nsISeekableStream,
                            nsITellableStream, nsILineInputStream)

nsresult nsFileInputStream::Create(nsISupports* aOuter, REFNSIID aIID,
                                   void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  RefPtr<nsFileInputStream> stream = new nsFileInputStream();
  return stream->QueryInterface(aIID, aResult);
}

nsresult nsFileInputStream::Open(nsIFile* aFile, int32_t aIOFlags,
                                 int32_t aPerm) {
  nsresult rv = NS_OK;

  // If the previous file is open, close it
  if (mFD) {
    rv = Close();
    if (NS_FAILED(rv)) return rv;
  }

  // Open the file
  if (aIOFlags == -1) aIOFlags = PR_RDONLY;
  if (aPerm == -1) aPerm = 0;

  return MaybeOpen(aFile, aIOFlags, aPerm,
                   mBehaviorFlags & nsIFileInputStream::DEFER_OPEN);
}

NS_IMETHODIMP
nsFileInputStream::Init(nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
                        int32_t aBehaviorFlags) {
  NS_ENSURE_TRUE(!mFD, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_TRUE(mState == eUnitialized || mState == eClosed,
                 NS_ERROR_ALREADY_INITIALIZED);

  mBehaviorFlags = aBehaviorFlags;
  mState = eUnitialized;

  mFile = aFile;
  mIOFlags = aIOFlags;
  mPerm = aPerm;

  return Open(aFile, aIOFlags, aPerm);
}

NS_IMETHODIMP
nsFileInputStream::Close() {
  // Get the cache position at the time the file was close. This allows
  // NS_SEEK_CUR on a closed file that has been opened with
  // REOPEN_ON_REWIND.
  if (mBehaviorFlags & REOPEN_ON_REWIND) {
    // Get actual position. Not one modified by subclasses
    nsFileStreamBase::Tell(&mCachedPosition);
  }

  // null out mLineBuffer in case Close() is called again after failing
  mLineBuffer = nullptr;
  return nsFileStreamBase::Close();
}

NS_IMETHODIMP
nsFileInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* _retval) {
  nsresult rv = nsFileStreamBase::Read(aBuf, aCount, _retval);
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    // Don't warn if this is a deffered file not found.
    return rv;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // Check if we're at the end of file and need to close
  if (mBehaviorFlags & CLOSE_ON_EOF && *_retval == 0) {
    Close();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::ReadLine(nsACString& aLine, bool* aResult) {
  if (!mLineBuffer) {
    mLineBuffer = MakeUnique<nsLineBuffer<char>>();
  }
  return NS_ReadLine(this, mLineBuffer.get(), aLine, aResult);
}

NS_IMETHODIMP
nsFileInputStream::Seek(int32_t aWhence, int64_t aOffset) {
  return SeekInternal(aWhence, aOffset);
}

nsresult nsFileInputStream::SeekInternal(int32_t aWhence, int64_t aOffset,
                                         bool aClearBuf) {
  nsresult rv = DoPendingOpen();
  if (rv != NS_OK && rv != NS_BASE_STREAM_CLOSED) {
    return rv;
  }

  if (aClearBuf) {
    mLineBuffer = nullptr;
  }

  if (rv == NS_BASE_STREAM_CLOSED) {
    if (mBehaviorFlags & REOPEN_ON_REWIND) {
      rv = Open(mFile, mIOFlags, mPerm);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the file was closed, and we do a relative seek, use the
      // position we cached when we closed the file to seek to the right
      // location.
      if (aWhence == NS_SEEK_CUR) {
        aWhence = NS_SEEK_SET;
        aOffset += mCachedPosition;
      }
    } else {
      return NS_BASE_STREAM_CLOSED;
    }
  }

  return nsFileStreamBase::Seek(aWhence, aOffset);
}

NS_IMETHODIMP
nsFileInputStream::Tell(int64_t* aResult) {
  return nsFileStreamBase::Tell(aResult);
}

NS_IMETHODIMP
nsFileInputStream::Available(uint64_t* aResult) {
  return nsFileStreamBase::Available(aResult);
}

void nsFileInputStream::Serialize(InputStreamParams& aParams,
                                  FileDescriptorArray& aFileDescriptors,
                                  bool aDelayedStart, uint32_t aMaxSize,
                                  uint32_t* aSizeUsed,
                                  ParentToChildStreamActorManager* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  SerializeInternal(aParams, aFileDescriptors);
}

void nsFileInputStream::Serialize(InputStreamParams& aParams,
                                  FileDescriptorArray& aFileDescriptors,
                                  bool aDelayedStart, uint32_t aMaxSize,
                                  uint32_t* aSizeUsed,
                                  ChildToParentStreamActorManager* aManager) {
  MOZ_ASSERT(aSizeUsed);
  *aSizeUsed = 0;

  SerializeInternal(aParams, aFileDescriptors);
}

void nsFileInputStream::SerializeInternal(
    InputStreamParams& aParams, FileDescriptorArray& aFileDescriptors) {
  FileInputStreamParams params;

  if (NS_SUCCEEDED(DoPendingOpen())) {
    MOZ_ASSERT(mFD);
    FileHandleType fd = FileHandleType(PR_FileDesc2NativeHandle(mFD));
    NS_ASSERTION(fd, "This should never be null!");

    DebugOnly dbgFD = aFileDescriptors.AppendElement(fd);
    NS_ASSERTION(dbgFD->IsValid(), "Sending an invalid file descriptor!");

    params.fileDescriptorIndex() = aFileDescriptors.Length() - 1;

    Close();
  } else {
    NS_WARNING(
        "This file has not been opened (or could not be opened). "
        "Sending an invalid file descriptor to the other process!");

    params.fileDescriptorIndex() = UINT32_MAX;
  }

  int32_t behaviorFlags = mBehaviorFlags;

  // The receiving process (or thread) is going to have an open file
  // descriptor automatically so transferring this flag is meaningless.
  behaviorFlags &= ~nsIFileInputStream::DEFER_OPEN;

  params.behaviorFlags() = behaviorFlags;
  params.ioFlags() = mIOFlags;

  aParams = params;
}

bool nsFileInputStream::Deserialize(
    const InputStreamParams& aParams,
    const FileDescriptorArray& aFileDescriptors) {
  NS_ASSERTION(!mFD, "Already have a file descriptor?!");
  NS_ASSERTION(mState == nsFileStreamBase::eUnitialized, "Deferring open?!");
  NS_ASSERTION(!mFile, "Should never have a file here!");
  NS_ASSERTION(!mPerm, "This should always be 0!");

  if (aParams.type() != InputStreamParams::TFileInputStreamParams) {
    NS_WARNING("Received unknown parameters from the other process!");
    return false;
  }

  const FileInputStreamParams& params = aParams.get_FileInputStreamParams();

  uint32_t fileDescriptorIndex = params.fileDescriptorIndex();

  FileDescriptor fd;
  if (fileDescriptorIndex < aFileDescriptors.Length()) {
    fd = aFileDescriptors[fileDescriptorIndex];
    NS_WARNING_ASSERTION(fd.IsValid(), "Received an invalid file descriptor!");
  } else {
    NS_WARNING("Received a bad file descriptor index!");
  }

  if (fd.IsValid()) {
    auto rawFD = fd.ClonePlatformHandle();
    PRFileDesc* fileDesc = PR_ImportFile(PROsfd(rawFD.release()));
    if (!fileDesc) {
      NS_WARNING("Failed to import file handle!");
      return false;
    }
    mFD = fileDesc;
    mState = eOpened;
  } else {
    mState = eError;
    mErrorValue = NS_ERROR_FILE_NOT_FOUND;
  }

  mBehaviorFlags = params.behaviorFlags();

  if (!XRE_IsParentProcess()) {
    // A child process shouldn't close when it reads the end because it will
    // not be able to reopen the file later.
    mBehaviorFlags &= ~nsIFileInputStream::CLOSE_ON_EOF;

    // A child process will not be able to reopen the file so this flag is
    // meaningless.
    mBehaviorFlags &= ~nsIFileInputStream::REOPEN_ON_REWIND;
  }

  mIOFlags = params.ioFlags();

  return true;
}

bool nsFileInputStream::IsCloneable() const {
  // This inputStream is cloneable only if has been created using Init() and
  // it owns a nsIFile. This is not true when it is deserialized from IPC.
  return XRE_IsParentProcess() && mFile;
}

NS_IMETHODIMP
nsFileInputStream::GetCloneable(bool* aCloneable) {
  *aCloneable = IsCloneable();
  return NS_OK;
}

NS_IMETHODIMP
nsFileInputStream::Clone(nsIInputStream** aResult) {
  MOZ_ASSERT(IsCloneable());
  return NS_NewLocalFileInputStream(aResult, mFile, mIOFlags, mPerm,
                                    mBehaviorFlags);
}

////////////////////////////////////////////////////////////////////////////////
// nsFileOutputStream

NS_IMPL_ISUPPORTS_INHERITED(nsFileOutputStream, nsFileStreamBase,
                            nsIOutputStream, nsIFileOutputStream)

nsresult nsFileOutputStream::Create(nsISupports* aOuter, REFNSIID aIID,
                                    void** aResult) {
  NS_ENSURE_NO_AGGREGATION(aOuter);

  RefPtr<nsFileOutputStream> stream = new nsFileOutputStream();
  return stream->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsFileOutputStream::Init(nsIFile* file, int32_t ioFlags, int32_t perm,
                         int32_t behaviorFlags) {
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_TRUE(mState == eUnitialized || mState == eClosed,
                 NS_ERROR_ALREADY_INITIALIZED);

  mBehaviorFlags = behaviorFlags;
  mState = eUnitialized;

  if (ioFlags == -1) ioFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
  if (perm <= 0) perm = 0664;

  return MaybeOpen(file, ioFlags, perm,
                   mBehaviorFlags & nsIFileOutputStream::DEFER_OPEN);
}

nsresult nsFileOutputStream::InitWithFileDescriptor(
    const mozilla::ipc::FileDescriptor& aFd) {
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_TRUE(mState == eUnitialized || mState == eClosed,
                 NS_ERROR_ALREADY_INITIALIZED);

  if (aFd.IsValid()) {
    auto rawFD = aFd.ClonePlatformHandle();
    PRFileDesc* fileDesc = PR_ImportFile(PROsfd(rawFD.release()));
    if (!fileDesc) {
      NS_WARNING("Failed to import file handle!");
      return NS_ERROR_FAILURE;
    }
    mFD = fileDesc;
    mState = eOpened;
  } else {
    mState = eError;
    mErrorValue = NS_ERROR_FILE_NOT_FOUND;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFileOutputStream::Preallocate(int64_t aLength) {
  if (!mFD) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mozilla::fallocate(mFD, aLength)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAtomicFileOutputStream

NS_IMPL_ISUPPORTS_INHERITED(nsAtomicFileOutputStream, nsFileOutputStream,
                            nsISafeOutputStream, nsIOutputStream,
                            nsIFileOutputStream)

NS_IMETHODIMP
nsAtomicFileOutputStream::Init(nsIFile* file, int32_t ioFlags, int32_t perm,
                               int32_t behaviorFlags) {
  // While `PR_APPEND` is not supported, `-1` is used as `ioFlags` parameter
  // in some places, and `PR_APPEND | PR_TRUNCATE` does not require appending
  // to existing file. So, throw an exception only if `PR_APPEND` is
  // explicitly specified without `PR_TRUNCATE`.
  if ((ioFlags & PR_APPEND) && !(ioFlags & PR_TRUNCATE)) {
    return NS_ERROR_INVALID_ARG;
  }
  return nsFileOutputStream::Init(file, ioFlags, perm, behaviorFlags);
}

nsresult nsAtomicFileOutputStream::DoOpen() {
  // Make sure mOpenParams.localFile will be empty if we bail somewhere in
  // this function
  nsCOMPtr<nsIFile> file;
  file.swap(mOpenParams.localFile);

  if (!file) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = file->Exists(&mTargetFileExists);
  if (NS_FAILED(rv)) {
    NS_ERROR("Can't tell if target file exists");
    mTargetFileExists =
        true;  // Safer to assume it exists - we just do more work.
  }

  // follow symlinks, for two reasons:
  // 1) if a user has deliberately set up a profile file as a symlink, we
  //    honor it
  // 2) to make the MoveToNative() in Finish() an atomic operation (which may
  //    not be the case if moving across directories on different
  //    filesystems).
  nsCOMPtr<nsIFile> tempResult;
  rv = file->Clone(getter_AddRefs(tempResult));
  if (NS_SUCCEEDED(rv)) {
    tempResult->SetFollowLinks(true);

    // XP_UNIX ignores SetFollowLinks(), so we have to normalize.
    if (mTargetFileExists) {
      tempResult->Normalize();
    }
  }

  if (NS_SUCCEEDED(rv) && mTargetFileExists) {
    // Abort if |file| is not writable; it won't work as an output stream.
    bool isWritable;
    if (NS_SUCCEEDED(file->IsWritable(&isWritable)) && !isWritable) {
      return NS_ERROR_FILE_ACCESS_DENIED;
    }

    uint32_t origPerm;
    if (NS_FAILED(file->GetPermissions(&origPerm))) {
      NS_ERROR("Can't get permissions of target file");
      origPerm = mOpenParams.perm;
    }

    // XXX What if |perm| is more restrictive then |origPerm|?
    // This leaves the user supplied permissions as they were.
    rv = tempResult->CreateUnique(nsIFile::NORMAL_FILE_TYPE, origPerm);
  }
  if (NS_SUCCEEDED(rv)) {
    // nsFileOutputStream::DoOpen will work on the temporary file, so we
    // prepare it and place it in mOpenParams.localFile.
    mOpenParams.localFile = tempResult;
    mTempFile = tempResult;
    mTargetFile = file;
    rv = nsFileOutputStream::DoOpen();
  }
  return rv;
}

NS_IMETHODIMP
nsAtomicFileOutputStream::Close() {
  nsresult rv = nsFileOutputStream::Close();

  // the consumer doesn't want the original file overwritten -
  // so clean up by removing the temp file.
  if (mTempFile) {
    mTempFile->Remove(false);
    mTempFile = nullptr;
  }

  return rv;
}

NS_IMETHODIMP
nsAtomicFileOutputStream::Finish() {
  nsresult rv = nsFileOutputStream::Close();

  // if there is no temp file, don't try to move it over the original target.
  // It would destroy the targetfile if close() is called twice.
  if (!mTempFile) return rv;

  // Only overwrite if everything was ok, and the temp file could be closed.
  if (NS_SUCCEEDED(mWriteResult) && NS_SUCCEEDED(rv)) {
    NS_ENSURE_STATE(mTargetFile);

    if (!mTargetFileExists) {
      // If the target file did not exist when we were initialized, then the
      // temp file we gave out was actually a reference to the target file.
      // since we succeeded in writing to the temp file (and hence succeeded
      // in writing to the target file), there is nothing more to do.
#ifdef DEBUG
      bool equal;
      if (NS_FAILED(mTargetFile->Equals(mTempFile, &equal)) || !equal)
        NS_WARNING("mTempFile not equal to mTargetFile");
#endif
    } else {
      nsAutoString targetFilename;
      rv = mTargetFile->GetLeafName(targetFilename);
      if (NS_SUCCEEDED(rv)) {
        // This will replace target.
        rv = mTempFile->MoveTo(nullptr, targetFilename);
        if (NS_FAILED(rv)) mTempFile->Remove(false);
      }
    }
  } else {
    mTempFile->Remove(false);

    // if writing failed, propagate the failure code to the caller.
    if (NS_FAILED(mWriteResult)) rv = mWriteResult;
  }
  mTempFile = nullptr;
  return rv;
}

NS_IMETHODIMP
nsAtomicFileOutputStream::Write(const char* buf, uint32_t count,
                                uint32_t* result) {
  nsresult rv = nsFileOutputStream::Write(buf, count, result);
  if (NS_SUCCEEDED(mWriteResult)) {
    if (NS_FAILED(rv))
      mWriteResult = rv;
    else if (count != *result)
      mWriteResult = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

    if (NS_FAILED(mWriteResult) && count > 0)
      NS_WARNING("writing to output stream failed! data may be lost");
  }
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsSafeFileOutputStream

NS_IMETHODIMP
nsSafeFileOutputStream::Finish() {
  (void)Flush();
  return nsAtomicFileOutputStream::Finish();
}

////////////////////////////////////////////////////////////////////////////////
// nsFileStream

NS_IMPL_ISUPPORTS_INHERITED(nsFileStream, nsFileStreamBase, nsIInputStream,
                            nsIOutputStream, nsIFileStream)

NS_IMETHODIMP
nsFileStream::Init(nsIFile* file, int32_t ioFlags, int32_t perm,
                   int32_t behaviorFlags) {
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_TRUE(mState == eUnitialized || mState == eClosed,
                 NS_ERROR_ALREADY_INITIALIZED);

  mBehaviorFlags = behaviorFlags;
  mState = eUnitialized;

  if (ioFlags == -1) ioFlags = PR_RDWR;
  if (perm <= 0) perm = 0;

  return MaybeOpen(file, ioFlags, perm,
                   mBehaviorFlags & nsIFileStream::DEFER_OPEN);
}

////////////////////////////////////////////////////////////////////////////////
