/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsIOService.h"
#include "nsFileChannel.h"
#include "nsBaseContentStream.h"
#include "nsDirectoryIndexStream.h"
#include "nsThreadUtils.h"
#include "nsTransportUtils.h"
#include "nsStreamUtils.h"
#include "nsURLHelper.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsAutoPtr.h"
#include "nsStandardURL.h"

#include "nsIFileURL.h"
#include "nsIMIMEService.h"

//-----------------------------------------------------------------------------

class nsFileCopyEvent : public nsRunnable {
public:
  nsFileCopyEvent(nsIOutputStream *dest, nsIInputStream *source, PRInt64 len)
    : mDest(dest)
    , mSource(source)
    , mLen(len)
    , mStatus(NS_OK)
    , mInterruptStatus(NS_OK) {
  }

  // Read the current status of the file copy operation.
  nsresult Status() { return mStatus; }
  
  // Call this method to perform the file copy synchronously.
  void DoCopy();

  // Call this method to perform the file copy on a background thread.  The
  // callback is dispatched when the file copy completes.
  nsresult Dispatch(nsIRunnable *callback,
                    nsITransportEventSink *sink,
                    nsIEventTarget *target);

  // Call this method to interrupt a file copy operation that is occuring on
  // a background thread.  The status parameter passed to this function must
  // be a failure code and is set as the status of this file copy operation.
  void Interrupt(nsresult status) {
    NS_ASSERTION(NS_FAILED(status), "must be a failure code");
    mInterruptStatus = status;
  }

  NS_IMETHOD Run() {
    DoCopy();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  nsCOMPtr<nsIRunnable> mCallback;
  nsCOMPtr<nsITransportEventSink> mSink;
  nsCOMPtr<nsIOutputStream> mDest;
  nsCOMPtr<nsIInputStream> mSource;
  PRInt64 mLen;
  nsresult mStatus;           // modified on i/o thread only
  nsresult mInterruptStatus;  // modified on main thread only
};

void
nsFileCopyEvent::DoCopy()
{
  // We'll copy in chunks this large by default.  This size affects how
  // frequently we'll check for interrupts.
  const PRInt32 chunk = nsIOService::gDefaultSegmentSize * nsIOService::gDefaultSegmentCount;

  nsresult rv = NS_OK;

  PRInt64 len = mLen, progress = 0;
  while (len) {
    // If we've been interrupted, then stop copying.
    rv = mInterruptStatus;
    if (NS_FAILED(rv))
      break;

    PRInt32 num = NS_MIN((PRInt32) len, chunk);

    PRUint32 result;
    rv = mSource->ReadSegments(NS_CopySegmentToStream, mDest, num, &result);
    if (NS_FAILED(rv))
      break;
    if (result != (PRUint32) num) {
      rv = NS_ERROR_FILE_DISK_FULL;  // stopped prematurely (out of disk space)
      break;
    }

    // Dispatch progress notification
    if (mSink) {
      progress += num;
      mSink->OnTransportStatus(nsnull, nsITransport::STATUS_WRITING, progress,
                               mLen);
    }
                               
    len -= num;
  }

  if (NS_FAILED(rv))
    mStatus = rv;

  // Close the output stream before notifying our callback so that others may
  // freely "play" with the file.
  mDest->Close();

  // Notify completion
  if (mCallback) {
    mCallbackTarget->Dispatch(mCallback, NS_DISPATCH_NORMAL);

    // Release the callback on the target thread to avoid destroying stuff on
    // the wrong thread.
    nsIRunnable *doomed = nsnull;
    mCallback.swap(doomed);
    NS_ProxyRelease(mCallbackTarget, doomed);
  }
}

nsresult
nsFileCopyEvent::Dispatch(nsIRunnable *callback,
                          nsITransportEventSink *sink,
                          nsIEventTarget *target)
{
  // Use the supplied event target for all asynchronous operations.

  mCallback = callback;
  mCallbackTarget = target;

  // Build a coalescing proxy for progress events
  nsresult rv = net_NewTransportEventSinkProxy(getter_AddRefs(mSink), sink,
                                               target, true);
  if (NS_FAILED(rv))
    return rv;

  // Dispatch ourselves to I/O thread pool...
  nsCOMPtr<nsIEventTarget> pool =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  return pool->Dispatch(this, NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------

// This is a dummy input stream that when read, performs the file copy.  The
// copy happens on a background thread via mCopyEvent.

class nsFileUploadContentStream : public nsBaseContentStream {
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsFileUploadContentStream(bool nonBlocking,
                            nsIOutputStream *dest,
                            nsIInputStream *source,
                            PRInt64 len,
                            nsITransportEventSink *sink)
    : nsBaseContentStream(nonBlocking)
    , mCopyEvent(new nsFileCopyEvent(dest, source, len))
    , mSink(sink) {
  }

  bool IsInitialized() {
    return mCopyEvent != nsnull;
  }

  NS_IMETHODIMP ReadSegments(nsWriteSegmentFun fun, void *closure,
                             PRUint32 count, PRUint32 *result);
  NS_IMETHODIMP AsyncWait(nsIInputStreamCallback *callback, PRUint32 flags,
                          PRUint32 count, nsIEventTarget *target);

private:
  void OnCopyComplete();

  nsRefPtr<nsFileCopyEvent> mCopyEvent;
  nsCOMPtr<nsITransportEventSink> mSink;
};

NS_IMPL_ISUPPORTS_INHERITED0(nsFileUploadContentStream,
                             nsBaseContentStream)

NS_IMETHODIMP
nsFileUploadContentStream::ReadSegments(nsWriteSegmentFun fun, void *closure,
                                        PRUint32 count, PRUint32 *result)
{
  *result = 0;  // nothing is ever actually read from this stream

  if (IsClosed())
    return NS_OK;

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
nsFileUploadContentStream::AsyncWait(nsIInputStreamCallback *callback,
                                     PRUint32 flags, PRUint32 count,
                                     nsIEventTarget *target)
{
  nsresult rv = nsBaseContentStream::AsyncWait(callback, flags, count, target);
  if (NS_FAILED(rv) || IsClosed())
    return rv;

  if (IsNonBlocking()) {
    nsCOMPtr<nsIRunnable> callback =
      NS_NewRunnableMethod(this, &nsFileUploadContentStream::OnCopyComplete);
    mCopyEvent->Dispatch(callback, mSink, target);
  }

  return NS_OK;
}

void
nsFileUploadContentStream::OnCopyComplete()
{
  // This method is being called to indicate that we are done copying.
  nsresult status = mCopyEvent->Status();

  CloseWithStatus(NS_FAILED(status) ? status : NS_BASE_STREAM_CLOSED);
}

//-----------------------------------------------------------------------------

nsFileChannel::nsFileChannel(nsIURI *uri) 
{
  // If we have a link file, we should resolve its target right away.
  // This is to protect against a same origin attack where the same link file
  // can point to different resources right after the first resource is loaded.
  nsCOMPtr<nsIFile> file;
  nsCOMPtr <nsIURI> targetURI;
  nsCAutoString fileTarget;
  nsCOMPtr<nsILocalFile> resolvedFile;
  bool symLink;
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri);
  if (fileURL && 
      NS_SUCCEEDED(fileURL->GetFile(getter_AddRefs(file))) &&
      NS_SUCCEEDED(file->IsSymlink(&symLink)) && 
      symLink &&
      NS_SUCCEEDED(file->GetNativeTarget(fileTarget)) &&
      NS_SUCCEEDED(NS_NewNativeLocalFile(fileTarget, PR_TRUE, 
                                         getter_AddRefs(resolvedFile))) &&
      NS_SUCCEEDED(NS_NewFileURI(getter_AddRefs(targetURI), 
                   resolvedFile, nsnull))) {
    SetURI(targetURI);
    SetOriginalURI(uri);
    nsLoadFlags loadFlags = 0;
    GetLoadFlags(&loadFlags);
    SetLoadFlags(loadFlags | nsIChannel::LOAD_REPLACE);
  } else {
    SetURI(uri);
  }
}

nsresult
nsFileChannel::MakeFileInputStream(nsIFile *file,
                                   nsCOMPtr<nsIInputStream> &stream,
                                   nsCString &contentType)
{
  // we accept that this might result in a disk hit to stat the file
  bool isDir;
  nsresult rv = file->IsDirectory(&isDir);
  if (NS_FAILED(rv)) {
    // canonicalize error message
    if (rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
      rv = NS_ERROR_FILE_NOT_FOUND;
    return rv;
  }

  if (isDir) {
    rv = nsDirectoryIndexStream::Create(file, getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv) && !HasContentTypeHint())
      contentType.AssignLiteral(APPLICATION_HTTP_INDEX_FORMAT);
  } else {
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
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

nsresult
nsFileChannel::OpenContentStream(bool async, nsIInputStream **result,
                                 nsIChannel** channel)
{
  // NOTE: the resulting file is a clone, so it is safe to pass it to the
  //       file input stream which will be read on a background thread.
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler));
  if (NS_FAILED(rv))
    return rv;
    
  nsCOMPtr<nsIURI> newURI;
  rv = fileHandler->ReadURLFile(file, getter_AddRefs(newURI));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIChannel> newChannel;
    rv = NS_NewChannel(getter_AddRefs(newChannel), newURI);
    if (NS_FAILED(rv))
      return rv;

    *result = nsnull;
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
    if (NS_FAILED(rv))
      return rv;

    nsFileUploadContentStream *uploadStream =
        new nsFileUploadContentStream(async, fileStream, mUploadStream,
                                      mUploadLength, this);
    if (!uploadStream || !uploadStream->IsInitialized()) {
      delete uploadStream;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    stream = uploadStream;

    SetContentLength64(0);

    // Since there isn't any content to speak of we just set the content-type
    // to something other than "unknown" to avoid triggering the content-type
    // sniffer code in nsBaseChannel.
    // However, don't override explicitly set types.
    if (!HasContentTypeHint())
      SetContentType(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM));
  } else {
    nsCAutoString contentType;
    rv = MakeFileInputStream(file, stream, contentType);
    if (NS_FAILED(rv))
      return rv;

    EnableSynthesizedProgressEvents(true);

    // fixup content length and type
    if (ContentLength64() < 0) {
      PRInt64 size;
      rv = file->GetFileSize(&size);
      if (NS_FAILED(rv))
        return rv;
      SetContentLength64(size);
    }
    if (!contentType.IsEmpty())
      SetContentType(contentType);
  }

  *result = nsnull;
  stream.swap(*result);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsFileChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED2(nsFileChannel,
                             nsBaseChannel,
                             nsIUploadChannel,
                             nsIFileChannel)

//-----------------------------------------------------------------------------
// nsFileChannel::nsIFileChannel

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile **file)
{
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(URI());
    NS_ENSURE_STATE(fileURL);

    // This returns a cloned nsIFile
    return fileURL->GetFile(file);
}

//-----------------------------------------------------------------------------
// nsFileChannel::nsIUploadChannel

NS_IMETHODIMP
nsFileChannel::SetUploadStream(nsIInputStream *stream,
                               const nsACString &contentType,
                               PRInt32 contentLength)
{
  NS_ENSURE_TRUE(!IsPending(), NS_ERROR_IN_PROGRESS);

  if ((mUploadStream = stream)) {
    mUploadLength = contentLength;
    if (mUploadLength < 0) {
      // Make sure we know how much data we are uploading.
      PRUint32 avail;
      nsresult rv = mUploadStream->Available(&avail);
      if (NS_FAILED(rv))
        return rv;
      mUploadLength = avail;
    }
  } else {
    mUploadLength = -1;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetUploadStream(nsIInputStream **result)
{
    NS_IF_ADDREF(*result = mUploadStream);
    return NS_OK;
}
