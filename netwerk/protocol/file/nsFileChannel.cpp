/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOService.h"
#include "nsFileChannel.h"
#include "nsBaseContentStream.h"
#include "nsDirectoryIndexStream.h"
#include "nsThreadUtils.h"
#include "nsTransportUtils.h"
#include "nsStreamUtils.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIOutputStream.h"
#include "nsIFileStreams.h"
#include "nsFileProtocolHandler.h"
#include "nsProxyRelease.h"
#include "nsIContentPolicy.h"
#include "nsContentUtils.h"

#include "nsIFileURL.h"
#include "nsIURIMutator.h"
#include "nsIFile.h"
#include "nsIMIMEService.h"
#include "prio.h"
#include <algorithm>

#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::net;

//-----------------------------------------------------------------------------

class nsFileCopyEvent : public Runnable {
 public:
  nsFileCopyEvent(nsIOutputStream* dest, nsIInputStream* source, int64_t len)
      : mozilla::Runnable("nsFileCopyEvent"),
        mDest(dest),
        mSource(source),
        mLen(len),
        mStatus(NS_OK),
        mInterruptStatus(NS_OK) {}

  // Read the current status of the file copy operation.
  nsresult Status() { return mStatus; }

  // Call this method to perform the file copy synchronously.
  void DoCopy();

  // Call this method to perform the file copy on a background thread.  The
  // callback is dispatched when the file copy completes.
  nsresult Dispatch(nsIRunnable* callback, nsITransportEventSink* sink,
                    nsIEventTarget* target);

  // Call this method to interrupt a file copy operation that is occuring on
  // a background thread.  The status parameter passed to this function must
  // be a failure code and is set as the status of this file copy operation.
  void Interrupt(nsresult status) {
    NS_ASSERTION(NS_FAILED(status), "must be a failure code");
    mInterruptStatus = status;
  }

  NS_IMETHOD Run() override {
    DoCopy();
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  nsCOMPtr<nsIRunnable> mCallback;
  nsCOMPtr<nsITransportEventSink> mSink;
  nsCOMPtr<nsIOutputStream> mDest;
  nsCOMPtr<nsIInputStream> mSource;
  int64_t mLen;
  nsresult mStatus;           // modified on i/o thread only
  nsresult mInterruptStatus;  // modified on main thread only
};

void nsFileCopyEvent::DoCopy() {
  // We'll copy in chunks this large by default.  This size affects how
  // frequently we'll check for interrupts.
  const int32_t chunk =
      nsIOService::gDefaultSegmentSize * nsIOService::gDefaultSegmentCount;

  nsresult rv = NS_OK;

  int64_t len = mLen, progress = 0;
  while (len) {
    // If we've been interrupted, then stop copying.
    rv = mInterruptStatus;
    if (NS_FAILED(rv)) break;

    int32_t num = std::min((int32_t)len, chunk);

    uint32_t result;
    rv = mSource->ReadSegments(NS_CopySegmentToStream, mDest, num, &result);
    if (NS_FAILED(rv)) break;
    if (result != (uint32_t)num) {
      // stopped prematurely (out of disk space)
      rv = NS_ERROR_FILE_NO_DEVICE_SPACE;
      break;
    }

    // Dispatch progress notification
    if (mSink) {
      progress += num;
      mSink->OnTransportStatus(nullptr, NS_NET_STATUS_WRITING, progress, mLen);
    }

    len -= num;
  }

  if (NS_FAILED(rv)) mStatus = rv;

  // Close the output stream before notifying our callback so that others may
  // freely "play" with the file.
  mDest->Close();

  // Notify completion
  if (mCallback) {
    mCallbackTarget->Dispatch(mCallback, NS_DISPATCH_NORMAL);

    // Release the callback on the target thread to avoid destroying stuff on
    // the wrong thread.
    NS_ProxyRelease("nsFileCopyEvent::mCallback", mCallbackTarget,
                    mCallback.forget());
  }
}

nsresult nsFileCopyEvent::Dispatch(nsIRunnable* callback,
                                   nsITransportEventSink* sink,
                                   nsIEventTarget* target) {
  // Use the supplied event target for all asynchronous operations.

  mCallback = callback;
  mCallbackTarget = target;

  // Build a coalescing proxy for progress events
  nsresult rv =
      net_NewTransportEventSinkProxy(getter_AddRefs(mSink), sink, target);

  if (NS_FAILED(rv)) return rv;

  // Dispatch ourselves to I/O thread pool...
  nsCOMPtr<nsIEventTarget> pool =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  return pool->Dispatch(this, NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------

// This is a dummy input stream that when read, performs the file copy.  The
// copy happens on a background thread via mCopyEvent.

class nsFileUploadContentStream : public nsBaseContentStream {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsFileUploadContentStream,
                                       nsBaseContentStream)

  nsFileUploadContentStream(bool nonBlocking, nsIOutputStream* dest,
                            nsIInputStream* source, int64_t len,
                            nsITransportEventSink* sink)
      : nsBaseContentStream(nonBlocking),
        mCopyEvent(new nsFileCopyEvent(dest, source, len)),
        mSink(sink) {}

  bool IsInitialized() { return mCopyEvent != nullptr; }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun fun, void* closure, uint32_t count,
                          uint32_t* result) override;
  NS_IMETHOD AsyncWait(nsIInputStreamCallback* callback, uint32_t flags,
                       uint32_t count, nsIEventTarget* target) override;

 private:
  virtual ~nsFileUploadContentStream() = default;

  void OnCopyComplete();

  RefPtr<nsFileCopyEvent> mCopyEvent;
  nsCOMPtr<nsITransportEventSink> mSink;
};

NS_IMETHODIMP
nsFileUploadContentStream::ReadSegments(nsWriteSegmentFun fun, void* closure,
                                        uint32_t count, uint32_t* result) {
  *result = 0;  // nothing is ever actually read from this stream

  if (IsClosed()) return NS_OK;

  if (IsNonBlocking()) {
    // Inform the caller that they will have to wait for the copy operation to
    // complete asynchronously.  We'll kick of the copy operation once they
    // call AsyncWait.
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  // Perform copy synchronously, and then close out the stream.
  mCopyEvent->DoCopy();
  nsresult status = mCopyEvent->Status();
  CloseWithStatus(NS_FAILED(status) ? status : NS_BASE_STREAM_CLOSED);
  return status;
}

NS_IMETHODIMP
nsFileUploadContentStream::AsyncWait(nsIInputStreamCallback* callback,
                                     uint32_t flags, uint32_t count,
                                     nsIEventTarget* target) {
  nsresult rv = nsBaseContentStream::AsyncWait(callback, flags, count, target);
  if (NS_FAILED(rv) || IsClosed()) return rv;

  if (IsNonBlocking()) {
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("nsFileUploadContentStream::OnCopyComplete", this,
                          &nsFileUploadContentStream::OnCopyComplete);
    mCopyEvent->Dispatch(callback, mSink, target);
  }

  return NS_OK;
}

void nsFileUploadContentStream::OnCopyComplete() {
  // This method is being called to indicate that we are done copying.
  nsresult status = mCopyEvent->Status();

  CloseWithStatus(NS_FAILED(status) ? status : NS_BASE_STREAM_CLOSED);
}

//-----------------------------------------------------------------------------

nsFileChannel::nsFileChannel(nsIURI* uri) : mUploadLength(0), mFileURI(uri) {}

nsresult nsFileChannel::Init() {
  NS_ENSURE_STATE(mLoadInfo);

  // If we have a link file, we should resolve its target right away.
  // This is to protect against a same origin attack where the same link file
  // can point to different resources right after the first resource is loaded.
  nsCOMPtr<nsIFile> file;
  nsCOMPtr<nsIURI> targetURI;
#ifdef XP_WIN
  nsAutoString fileTarget;
#else
  nsAutoCString fileTarget;
#endif
  nsCOMPtr<nsIFile> resolvedFile;
  bool symLink;
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mFileURI);
  if (fileURL && NS_SUCCEEDED(fileURL->GetFile(getter_AddRefs(file))) &&
      NS_SUCCEEDED(file->IsSymlink(&symLink)) && symLink &&
#ifdef XP_WIN
      NS_SUCCEEDED(file->GetTarget(fileTarget)) &&
      NS_SUCCEEDED(
          NS_NewLocalFile(fileTarget, true, getter_AddRefs(resolvedFile))) &&
#else
      NS_SUCCEEDED(file->GetNativeTarget(fileTarget)) &&
      NS_SUCCEEDED(NS_NewNativeLocalFile(fileTarget, true,
                                         getter_AddRefs(resolvedFile))) &&
#endif
      NS_SUCCEEDED(
          NS_NewFileURI(getter_AddRefs(targetURI), resolvedFile, nullptr))) {
    // Make an effort to match up the query strings.
    nsCOMPtr<nsIURL> origURL = do_QueryInterface(mFileURI);
    nsCOMPtr<nsIURL> targetURL = do_QueryInterface(targetURI);
    nsAutoCString queryString;
    if (origURL && targetURL && NS_SUCCEEDED(origURL->GetQuery(queryString))) {
      Unused
          << NS_MutateURI(targetURI).SetQuery(queryString).Finalize(targetURI);
    }

    SetURI(targetURI);
    SetOriginalURI(mFileURI);
    mLoadInfo->SetResultPrincipalURI(targetURI);
  } else {
    SetURI(mFileURI);
  }

  return NS_OK;
}

nsresult nsFileChannel::MakeFileInputStream(nsIFile* file,
                                            nsCOMPtr<nsIInputStream>& stream,
                                            nsCString& contentType,
                                            bool async) {
  // we accept that this might result in a disk hit to stat the file
  bool isDir;
  nsresult rv = file->IsDirectory(&isDir);
  if (NS_FAILED(rv)) {
    // canonicalize error message
    if (rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) rv = NS_ERROR_FILE_NOT_FOUND;

    if (rv == NS_ERROR_FILE_NOT_FOUND) {
      CheckForBrokenChromeURL(mLoadInfo, OriginalURI());
    }

    if (async && (NS_ERROR_FILE_NOT_FOUND == rv)) {
      // We don't return "Not Found" errors here. Since we could not find
      // the file, it's not a directory anyway.
      isDir = false;
    } else {
      return rv;
    }
  }

  if (isDir) {
    rv = nsDirectoryIndexStream::Create(file, getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv) && !HasContentTypeHint()) {
      contentType.AssignLiteral(APPLICATION_HTTP_INDEX_FORMAT);
    }
  } else {
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file, -1, -1,
                                    async ? nsIFileInputStream::DEFER_OPEN : 0);
    if (NS_SUCCEEDED(rv) && !HasContentTypeHint()) {
      // Use file extension to infer content type
      nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        mime->GetTypeFromFile(file, contentType);
      }
    }
  }
  return rv;
}

nsresult nsFileChannel::OpenContentStream(bool async, nsIInputStream** result,
                                          nsIChannel** channel) {
  // NOTE: the resulting file is a clone, so it is safe to pass it to the
  //       file input stream which will be read on a background thread.
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> newURI;
  if (NS_SUCCEEDED(fileHandler->ReadURLFile(file, getter_AddRefs(newURI))) ||
      NS_SUCCEEDED(fileHandler->ReadShellLink(file, getter_AddRefs(newURI)))) {
    nsCOMPtr<nsIChannel> newChannel;
    rv = NS_NewChannel(getter_AddRefs(newChannel), newURI,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER);

    if (NS_FAILED(rv)) return rv;

    *result = nullptr;
    newChannel.forget(channel);
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> stream;

  if (mUploadStream) {
    // Pass back a nsFileUploadContentStream instance that knows how to perform
    // the file copy when "read" (the resulting stream in this case does not
    // actually return any data).

    nsCOMPtr<nsIOutputStream> fileStream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileStream), file,
                                     PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                     PR_IRUSR | PR_IWUSR);
    if (NS_FAILED(rv)) return rv;

    RefPtr<nsFileUploadContentStream> uploadStream =
        new nsFileUploadContentStream(async, fileStream, mUploadStream,
                                      mUploadLength, this);
    if (!uploadStream || !uploadStream->IsInitialized()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    stream = std::move(uploadStream);

    mContentLength = 0;

    // Since there isn't any content to speak of we just set the content-type
    // to something other than "unknown" to avoid triggering the content-type
    // sniffer code in nsBaseChannel.
    // However, don't override explicitly set types.
    if (!HasContentTypeHint()) {
      SetContentType(nsLiteralCString(APPLICATION_OCTET_STREAM));
    }
  } else {
    nsAutoCString contentType;
    rv = MakeFileInputStream(file, stream, contentType, async);
    if (NS_FAILED(rv)) return rv;

    EnableSynthesizedProgressEvents(true);

    // fixup content length and type

    // when we are called from asyncOpen, the content length fixup will be
    // performed on a background thread and block the listener invocation via
    // ListenerBlockingPromise method
    if (!async && mContentLength < 0) {
      rv = FixupContentLength(false);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    if (!contentType.IsEmpty()) {
      SetContentType(contentType);
    }
  }

  *result = nullptr;
  stream.swap(*result);
  return NS_OK;
}

nsresult nsFileChannel::ListenerBlockingPromise(BlockingPromise** aPromise) {
  NS_ENSURE_ARG(aPromise);
  *aPromise = nullptr;

  if (mContentLength >= 0) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> sts(
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID));
  if (!sts) {
    return FixupContentLength(true);
  }

  RefPtr<TaskQueue> taskQueue = new TaskQueue(sts.forget(), "FileChannel");
  RefPtr<nsFileChannel> self = this;
  RefPtr<BlockingPromise> promise =
      mozilla::InvokeAsync(taskQueue, __func__, [self{std::move(self)}]() {
        nsresult rv = self->FixupContentLength(true);
        if (NS_FAILED(rv)) {
          return BlockingPromise::CreateAndReject(rv, __func__);
        }
        return BlockingPromise::CreateAndResolve(NS_OK, __func__);
      });

  promise.forget(aPromise);
  return NS_OK;
}

nsresult nsFileChannel::FixupContentLength(bool async) {
  MOZ_ASSERT(mContentLength < 0);

  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return rv;
  }

  int64_t size;
  rv = file->GetFileSize(&size);
  if (NS_FAILED(rv)) {
    if (async && (NS_ERROR_FILE_NOT_FOUND == rv ||
                  NS_ERROR_FILE_TARGET_DOES_NOT_EXIST == rv)) {
      size = 0;
    } else {
      return rv;
    }
  }
  mContentLength = size;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsFileChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED(nsFileChannel, nsBaseChannel, nsIUploadChannel,
                            nsIFileChannel)

//-----------------------------------------------------------------------------
// nsFileChannel::nsIFileChannel

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile** file) {
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(URI());
  NS_ENSURE_STATE(fileURL);

  // This returns a cloned nsIFile
  return fileURL->GetFile(file);
}

//-----------------------------------------------------------------------------
// nsFileChannel::nsIUploadChannel

NS_IMETHODIMP
nsFileChannel::SetUploadStream(nsIInputStream* stream,
                               const nsACString& contentType,
                               int64_t contentLength) {
  NS_ENSURE_TRUE(!Pending(), NS_ERROR_IN_PROGRESS);

  if ((mUploadStream = stream)) {
    mUploadLength = contentLength;
    if (mUploadLength < 0) {
      // Make sure we know how much data we are uploading.
      uint64_t avail;
      nsresult rv = mUploadStream->Available(&avail);
      if (NS_FAILED(rv)) return rv;
      // if this doesn't fit in the javascript MAX_SAFE_INTEGER
      // pretend we don't know the size
      mUploadLength = InScriptableRange(avail) ? avail : -1;
    }
  } else {
    mUploadLength = -1;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetUploadStream(nsIInputStream** result) {
  *result = do_AddRef(mUploadStream).take();
  return NS_OK;
}
