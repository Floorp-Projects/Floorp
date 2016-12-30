/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Native implementation of Watcher operations.
 */
#include "NativeFileWatcherWin.h"

#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "mozilla/Logging.h"

namespace mozilla {

// Enclose everything which is not exported in an anonymous namespace.
namespace {

/**
 * An event used to notify the main thread when an error happens.
 */
class WatchedErrorEvent final : public Runnable
{
public:
  /**
   * @param aOnError The passed error callback.
   * @param aError The |nsresult| error value.
   * @param osError The error returned by GetLastError().
   */
  WatchedErrorEvent(const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError,
                    const nsresult& anError, const DWORD& osError)
    : mOnError(aOnError)
    , mError(anError)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Make sure we wrap a valid callback since it's not mandatory to provide
    // one when watching a resource.
    if (mOnError) {
      (void)mOnError->Complete(mError, mOsError);
    }

    return NS_OK;
  }

 private:
  nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback> mOnError;
  nsresult mError;
  DWORD mOsError;
};

/**
 * An event used to notify the main thread when an operation is successful.
 */
class WatchedSuccessEvent final : public Runnable
{
public:
  /**
   * @param aOnSuccess The passed success callback.
   * @param aResourcePath
   *        The path of the resource for which this event was generated.
   */
  WatchedSuccessEvent(const nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback>& aOnSuccess,
                      const nsAString& aResourcePath)
    : mOnSuccess(aOnSuccess)
    , mResourcePath(aResourcePath)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Make sure we wrap a valid callback since it's not mandatory to provide
    // one when watching a resource.
    if (mOnSuccess) {
      (void)mOnSuccess->Complete(mResourcePath);
    }

    return NS_OK;
  }

 private:
  nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback> mOnSuccess;
  nsString mResourcePath;
};

/**
 * An event used to notify the main thread of a change in a watched
 * resource.
 */
class WatchedChangeEvent final : public Runnable
{
public:
  /**
   * @param aOnChange The passed change callback.
   * @param aChangedResource The name of the changed resource.
   */
  WatchedChangeEvent(const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
                     const nsAString& aChangedResource)
    : mOnChange(aOnChange)
    , mChangedResource(aChangedResource)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // The second parameter is reserved for future uses: we use 0 as a placeholder.
    (void)mOnChange->Changed(mChangedResource, 0);
    return NS_OK;
  }

private:
  nsMainThreadPtrHandle<nsINativeFileWatcherCallback> mOnChange;
  nsString mChangedResource;
};

static mozilla::LazyLogModule gNativeWatcherPRLog("NativeFileWatcherService");
#define FILEWATCHERLOG(...) MOZ_LOG(gNativeWatcherPRLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

// The number of notifications to store within WatchedResourceDescriptor:mNotificationBuffer.
// If the buffer overflows, its contents are discarded and a change callback is dispatched
// with "*" as changed path.
const unsigned int WATCHED_RES_MAXIMUM_NOTIFICATIONS = 100;

// The size, in bytes, of the notification buffer used to store the changes notifications
// for each watched resource.
const size_t NOTIFICATION_BUFFER_SIZE =
  WATCHED_RES_MAXIMUM_NOTIFICATIONS * sizeof(FILE_NOTIFY_INFORMATION);

/**
 * AutoCloseHandle is a RAII wrapper for Windows |HANDLE|s
 */
struct AutoCloseHandleTraits
{
  typedef HANDLE type;
  static type empty() { return INVALID_HANDLE_VALUE; }
  static void release(type anHandle)
  {
    if (anHandle != INVALID_HANDLE_VALUE) {
      // If CancelIo is called on an |HANDLE| not yet associated to a Completion I/O
      // it simply does nothing.
      (void)CancelIo(anHandle);
      (void)CloseHandle(anHandle);
    }
  }
};
typedef Scoped<AutoCloseHandleTraits> AutoCloseHandle;

// Define these callback array types to make the code easier to read.
typedef nsTArray<nsMainThreadPtrHandle<nsINativeFileWatcherCallback>> ChangeCallbackArray;
typedef nsTArray<nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>> ErrorCallbackArray;

/**
 * A structure to keep track of the information related to a
 * watched resource.
 */
struct WatchedResourceDescriptor {
  // The path on the file system of the watched resource.
  nsString mPath;

  // A buffer containing the latest notifications received for the resource.
  // UniquePtr<FILE_NOTIFY_INFORMATION> cannot be used as the structure
  // contains a variable length field (FileName).
  UniquePtr<unsigned char> mNotificationBuffer;

  // Used to hold information for the asynchronous ReadDirectoryChangesW call
  // (does not need to be closed as it is not an |HANDLE|).
  OVERLAPPED mOverlappedInfo;

  // The OS handle to the watched resource.
  AutoCloseHandle mResourceHandle;

  WatchedResourceDescriptor(const nsAString& aPath, const HANDLE anHandle)
    : mPath(aPath)
    , mResourceHandle(anHandle)
  {
    memset(&mOverlappedInfo,  0, sizeof(OVERLAPPED));
    mNotificationBuffer.reset(new unsigned char[NOTIFICATION_BUFFER_SIZE]);
  }
};

/**
 * A structure used to pass the callbacks to the AddPathRunnableMethod() and
 * RemovePathRunnableMethod().
 */
struct PathRunnablesParametersWrapper {
  nsString mPath;
  nsMainThreadPtrHandle<nsINativeFileWatcherCallback> mChangeCallbackHandle;
  nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback> mErrorCallbackHandle;
  nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback> mSuccessCallbackHandle;

  PathRunnablesParametersWrapper(
    const nsAString& aPath,
    const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
    const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError,
    const nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback>& aOnSuccess)
    : mPath(aPath)
    , mChangeCallbackHandle(aOnChange)
    , mErrorCallbackHandle(aOnError)
    , mSuccessCallbackHandle(aOnSuccess)
  {
  }
};

/**
 * This runnable is dispatched to the main thread in order to safely
 * shutdown the worker thread.
 */
class NativeWatcherIOShutdownTask : public Runnable
{
public:
  NativeWatcherIOShutdownTask()
    : mWorkerThread(do_GetCurrentThread())
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mWorkerThread->Shutdown();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mWorkerThread;
};

/**
 * This runnable is dispatched from the main thread to get the notifications of the
 * changes in the watched resources by continuously calling the blocking function
 * GetQueuedCompletionStatus. This function queries the status of the Completion I/O
 * port initialized in the main thread. The watched resources are registered to the
 * completion I/O port when calling |addPath|.
 *
 * Instead of using a loop within the Run() method, the Runnable reschedules itself
 * by issuing a NS_DispatchToCurrentThread(this) before exiting. This is done to allow
 * the execution of other runnables enqueued within the thread task queue.
 */
class NativeFileWatcherIOTask : public Runnable
{
public:
  explicit NativeFileWatcherIOTask(HANDLE aIOCompletionPort)
    : mIOCompletionPort(aIOCompletionPort)
    , mShuttingDown(false)
  {
  }

  NS_IMETHOD Run();
  nsresult AddPathRunnableMethod(PathRunnablesParametersWrapper* aWrappedParameters);
  nsresult RemovePathRunnableMethod(PathRunnablesParametersWrapper* aWrappedParameters);
  nsresult DeactivateRunnableMethod();

private:
  // Maintain 2 indexes - one by resource path, one by resource |HANDLE|.
  // Since |HANDLE| is basically a typedef to void*, we use nsVoidPtrHashKey to
  // compute the hashing key. We need 2 indexes in order to quickly look up the
  // changed resource in the Worker Thread.
  // The objects are not ref counted and get destroyed by mWatchedResourcesByPath
  // on NativeFileWatcherService::Destroy or in NativeFileWatcherService::RemovePath.
  nsClassHashtable<nsStringHashKey, WatchedResourceDescriptor> mWatchedResourcesByPath;
  nsDataHashtable<nsVoidPtrHashKey, WatchedResourceDescriptor*> mWatchedResourcesByHandle;

  // The same callback can be associated to multiple watches so we need to keep
  // them alive as long as there is a watch using them. We create two hashtables
  // to map directory names to lists of nsMainThreadPtr<callbacks>.
  nsClassHashtable<nsStringHashKey, ChangeCallbackArray> mChangeCallbacksTable;
  nsClassHashtable<nsStringHashKey, ErrorCallbackArray> mErrorCallbacksTable;

  // We hold a copy of the completion port |HANDLE|, which is owned by the main thread.
  HANDLE mIOCompletionPort;

  // Other methods need to know that a shutdown is in progress.
  bool mShuttingDown;

  nsresult RunInternal();

  nsresult DispatchChangeCallbacks(WatchedResourceDescriptor* aResourceDescriptor,
                                   const nsAString& aChangedResource);

  nsresult ReportChange(
    const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
    const nsAString& aChangedResource);

  nsresult DispatchErrorCallbacks(WatchedResourceDescriptor* aResourceDescriptor,
                                  nsresult anError, DWORD anOSError);

  nsresult ReportError(
    const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError,
    nsresult anError, DWORD anOSError);

  nsresult ReportSuccess(
    const nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback>& aOnSuccess,
    const nsAString& aResourcePath);

  nsresult AddDirectoryToWatchList(WatchedResourceDescriptor* aDirectoryDescriptor);

  void AppendCallbacksToHashtables(
    const nsAString& aPath,
    const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
    const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError);

  void RemoveCallbacksFromHashtables(
    const nsAString& aPath,
    const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
    const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError);

  nsresult MakeResourcePath(
    WatchedResourceDescriptor* changedDescriptor,
    const nsAString& resourceName,
    nsAString& nativeResourcePath);
};

/**
 * The watching thread logic.
 *
 * @return NS_OK if the watcher loop must be rescheduled, a failure code
 *         if it must not.
 */
nsresult
NativeFileWatcherIOTask::RunInternal()
{
  // Contains the address of the |OVERLAPPED| structure passed
  // to ReadDirectoryChangesW (used to check for |HANDLE| closing).
  OVERLAPPED* overlappedStructure;

  // The number of bytes transferred by GetQueuedCompletionStatus
  // (used to check for |HANDLE| closing).
  DWORD transferredBytes = 0;

  // Will hold the |HANDLE| to the watched resource returned by GetQueuedCompletionStatus
  // which generated the change events.
  ULONG_PTR changedResourceHandle = 0;

  // Check for changes in the resource status by querying the |mIOCompletionPort|
  // (blocking). GetQueuedCompletionStatus is always called before the first call
  // to ReadDirectoryChangesW. This isn't a problem, since mIOCompletionPort is
  // already a valid |HANDLE| even though it doesn't have any associated notification
  // handles (through ReadDirectoryChangesW).
  if (!GetQueuedCompletionStatus(mIOCompletionPort, &transferredBytes,
                                 &changedResourceHandle, &overlappedStructure,
                                 INFINITE)) {
    // Ok, there was some error.
    DWORD errCode = GetLastError();
    switch (errCode) {
      case ERROR_NOTIFY_ENUM_DIR: {
        // There were too many changes and the notification buffer has overflowed.
        // We dispatch the special value "*" and reschedule.
        FILEWATCHERLOG("NativeFileWatcherIOTask::Run - Notification buffer has overflowed");

        WatchedResourceDescriptor* changedRes =
          mWatchedResourcesByHandle.Get((HANDLE)changedResourceHandle);

        nsresult rv = DispatchChangeCallbacks(changedRes, NS_LITERAL_STRING("*"));
        if (NS_FAILED(rv)) {
          // We failed to dispatch the error callbacks. Something very
          // bad happened to the main thread, so we bail out from the watcher thread.
          FILEWATCHERLOG(
            "NativeFileWatcherIOTask::Run - Failed to dispatch change callbacks (%x).",
            rv);
          return rv;
        }

        return NS_OK;
      }
      case ERROR_ABANDONED_WAIT_0:
      case ERROR_INVALID_HANDLE: {
        // If we reach this point, mIOCompletionPort was probably closed
        // and we need to close this thread. This condition is identified
        // by catching the  ERROR_INVALID_HANDLE error.
        FILEWATCHERLOG(
          "NativeFileWatcherIOTask::Run - The completion port was closed (%x).",
          errCode);
        return NS_ERROR_ABORT;
      }
      case ERROR_OPERATION_ABORTED: {
        // Some path was unwatched! That's not really an error, now it is safe
        // to free the memory for the resource and call GetQueuedCompletionStatus
        // again.
        FILEWATCHERLOG("NativeFileWatcherIOTask::Run - Path unwatched (%x).", errCode);

        WatchedResourceDescriptor* toFree =
          mWatchedResourcesByHandle.Get((HANDLE)changedResourceHandle);

        if (toFree) {
          // Take care of removing the resource and freeing the memory

          mWatchedResourcesByHandle.Remove((HANDLE)changedResourceHandle);

          // This last call eventually frees the memory
          mWatchedResourcesByPath.Remove(toFree->mPath);
        }

        return NS_OK;
      }
      default: {
        // It should probably never get here, but it's better to be safe.
        FILEWATCHERLOG("NativeFileWatcherIOTask::Run - Unknown error (%x).", errCode);

        return NS_ERROR_FAILURE;
      }
    }
  }

  // When an |HANDLE| associated to the completion I/O port is gracefully
  // closed, GetQueuedCompletionStatus still may return a status update. Moreover,
  // this can also be triggered when watching files on a network folder and losing
  // the connection.
  // That's an edge case we need to take care of for consistency by checking
  // for (!transferredBytes && overlappedStructure). See http://xania.org/200807/iocp
  if (!transferredBytes &&
      (overlappedStructure ||
      (!overlappedStructure && !changedResourceHandle))) {
    // Note: if changedResourceHandle is nullptr as well, the wait on the Completion
    // I/O was interrupted by a call to PostQueuedCompletionStatus with 0 transferred
    // bytes and nullptr as |OVERLAPPED| and |HANDLE|. This is done to allow addPath
    // and removePath to work on this thread.
    return NS_OK;
  }

  // Check to see which resource is changedResourceHandle.
  WatchedResourceDescriptor* changedRes =
    mWatchedResourcesByHandle.Get((HANDLE)changedResourceHandle);
  MOZ_ASSERT(changedRes, "Could not find the changed resource in the list of watched ones.");

  // Parse the changes and notify the main thread.
  const unsigned char* rawNotificationBuffer = changedRes->mNotificationBuffer.get();

  while (true) {
    FILE_NOTIFY_INFORMATION* notificationInfo =
      (FILE_NOTIFY_INFORMATION*)rawNotificationBuffer;

    // FileNameLength is in bytes, but we need FileName length
    // in characters, so divide it by sizeof(WCHAR).
    nsAutoString resourceName(notificationInfo->FileName,
                              notificationInfo->FileNameLength / sizeof(WCHAR));

    // Handle path normalisation using nsILocalFile.
    nsString resourcePath;
    nsresult rv = MakeResourcePath(changedRes, resourceName, resourcePath);
    if (NS_SUCCEEDED(rv)) {
      rv = DispatchChangeCallbacks(changedRes, resourcePath);
      if (NS_FAILED(rv)) {
        // Log that we failed to dispatch the change callbacks.
        FILEWATCHERLOG(
          "NativeFileWatcherIOTask::Run - Failed to dispatch change callbacks (%x).",
          rv);
        return rv;
      }
    }

    if (!notificationInfo->NextEntryOffset) {
      break;
    }

    rawNotificationBuffer += notificationInfo->NextEntryOffset;
  }

  // We need to keep watching for further changes.
  nsresult rv = AddDirectoryToWatchList(changedRes);
  if (NS_FAILED(rv)) {
    // We failed to watch the folder.
    if (rv == NS_ERROR_ABORT) {
      // Log that we also failed to dispatch the error callbacks.
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::Run - Failed to watch %s and"
        " to dispatch the related error callbacks", changedRes->mPath.get());
      return rv;
    }
  }

  return NS_OK;
}

/**
 * Wraps the watcher logic and takes care of rescheduling
 * the watcher loop based on the return code of |RunInternal|
 * in order to help with code readability.
 *
 * @return NS_OK or a failure error code from |NS_DispatchToCurrentThread|.
 */
NS_IMETHODIMP
NativeFileWatcherIOTask::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // We return immediately if |mShuttingDown| is true (see below for
  // details about the shutdown protocol being followed).
  if (mShuttingDown) {
    return NS_OK;
  }

  nsresult rv = RunInternal();
  if (NS_FAILED(rv)) {
    // A critical error occurred in the watcher loop, don't reschedule.
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::Run - Stopping the watcher loop (error %S)", rv);

    // We log the error but return NS_OK instead: we don't want to
    // propagate an exception through XPCOM.
    return NS_OK;
  }

  // No error occurred, reschedule.
  return NS_DispatchToCurrentThread(this);
}

/**
 * Adds the resource to the watched list. This function is enqueued on the worker
 * thread by NativeFileWatcherService::AddPath. All the errors are reported to the main
 * thread using the error callback function mErrorCallback.
 *
 * @param pathToWatch
 *        The path of the resource to watch for changes.
 *
 * @return NS_ERROR_FILE_NOT_FOUND if the path is invalid or does not exist.
 *         Returns NS_ERROR_UNEXPECTED if OS |HANDLE|s are unexpectedly closed.
 *         If the ReadDirectoryChangesW call fails, returns NS_ERROR_FAILURE,
 *         otherwise NS_OK.
 */
nsresult
NativeFileWatcherIOTask::AddPathRunnableMethod(
  PathRunnablesParametersWrapper* aWrappedParameters)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoPtr<PathRunnablesParametersWrapper> wrappedParameters(aWrappedParameters);

  // We return immediately if |mShuttingDown| is true (see below for
  // details about the shutdown protocol being followed).
  if (mShuttingDown) {
    return NS_OK;
  }

  if (!wrappedParameters ||
      !wrappedParameters->mChangeCallbackHandle) {
    FILEWATCHERLOG("NativeFileWatcherIOTask::AddPathRunnableMethod - Invalid arguments.");
    return NS_ERROR_NULL_POINTER;
  }

  // Is aPathToWatch already being watched?
  WatchedResourceDescriptor* watchedResource =
    mWatchedResourcesByPath.Get(wrappedParameters->mPath);
  if (watchedResource) {
    // Append it to the hash tables.
    AppendCallbacksToHashtables(
      watchedResource->mPath,
      wrappedParameters->mChangeCallbackHandle,
      wrappedParameters->mErrorCallbackHandle);

    return NS_OK;
  }

  // Retrieve a file handle to associate with the completion port. Makes
  // sure to request the appropriate rights (i.e. read files and list
  // files contained in a folder). Note: the nullptr security flag prevents
  // the |HANDLE| to be passed to child processes.
  HANDLE resHandle = CreateFileW(wrappedParameters->mPath.get(),
                                 FILE_LIST_DIRECTORY, // Access rights
                                 FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, // Share
                                 nullptr, // Security flags
                                 OPEN_EXISTING, // Returns an handle only if the resource exists
                                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                 nullptr); // Template file (only used when creating files)
  if (resHandle == INVALID_HANDLE_VALUE) {
    DWORD dwError = GetLastError();
    nsresult rv;
    if (dwError == ERROR_FILE_NOT_FOUND) {
      rv = NS_ERROR_FILE_NOT_FOUND;
    } else if (dwError == ERROR_ACCESS_DENIED) {
      rv = NS_ERROR_FILE_ACCESS_DENIED;
    } else {
      rv = NS_ERROR_FAILURE;
    }

    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::AddPathRunnableMethod - CreateFileW failed (error %x) for %S.",
      dwError, wrappedParameters->mPath.get());

    rv = ReportError(wrappedParameters->mErrorCallbackHandle, rv, dwError);
    if (NS_FAILED(rv)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::AddPathRunnableMethod - "
        "Failed to dispatch the error callback (%x).",
        rv);
      return rv;
    }

    // Error has already been reported through mErrorCallback.
    return NS_OK;
  }

  // Initialise the resource descriptor.
  UniquePtr<WatchedResourceDescriptor> resourceDesc(
    new WatchedResourceDescriptor(wrappedParameters->mPath, resHandle));

  // Associate the file with the previously opened completion port.
  if (!CreateIoCompletionPort(resourceDesc->mResourceHandle, mIOCompletionPort,
                              (ULONG_PTR)resourceDesc->mResourceHandle.get(), 0)) {
    DWORD dwError = GetLastError();

    FILEWATCHERLOG("NativeFileWatcherIOTask::AddPathRunnableMethod"
                   " - CreateIoCompletionPort failed (error %x) for %S.",
                   dwError, wrappedParameters->mPath.get());

    // This could fail because passed parameters could be invalid |HANDLE|s
    // i.e. mIOCompletionPort was unexpectedly closed or failed.
    nsresult rv =
      ReportError(wrappedParameters->mErrorCallbackHandle, NS_ERROR_UNEXPECTED, dwError);
    if (NS_FAILED(rv)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::AddPathRunnableMethod - "
        "Failed to dispatch the error callback (%x).",
        rv);
      return rv;
    }

    // Error has already been reported through mErrorCallback.
    return NS_OK;
  }

  // Append the callbacks to the hash tables. We do this now since
  // AddDirectoryToWatchList could use the error callback, but we
  // need to make sure to remove them if AddDirectoryToWatchList fails.
  AppendCallbacksToHashtables(
    wrappedParameters->mPath,
    wrappedParameters->mChangeCallbackHandle,
    wrappedParameters->mErrorCallbackHandle);

  // We finally watch the resource for changes.
  nsresult rv = AddDirectoryToWatchList(resourceDesc.get());
  if (NS_SUCCEEDED(rv)) {
    // Add the resource pointer to both indexes.
    WatchedResourceDescriptor* resource = resourceDesc.release();
    mWatchedResourcesByPath.Put(wrappedParameters->mPath, resource);
    mWatchedResourcesByHandle.Put(resHandle, resource);

    // Dispatch the success callback.
    nsresult rv =
      ReportSuccess(wrappedParameters->mSuccessCallbackHandle, wrappedParameters->mPath);
    if (NS_FAILED(rv)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::AddPathRunnableMethod - "
        "Failed to dispatch the success callback (%x).",
        rv);
      return rv;
    }

    return NS_OK;
  }

  // We failed to watch the folder. Remove the callbacks
  // from the hash tables.
  RemoveCallbacksFromHashtables(
    wrappedParameters->mPath,
    wrappedParameters->mChangeCallbackHandle,
    wrappedParameters->mErrorCallbackHandle);

  if (rv != NS_ERROR_ABORT) {
    // Just don't add the descriptor to the watch list.
    return NS_OK;
  }

  // We failed to dispatch the error callbacks as well.
  FILEWATCHERLOG(
    "NativeFileWatcherIOTask::AddPathRunnableMethod - Failed to watch %s and"
    " to dispatch the related error callbacks", resourceDesc->mPath.get());

  return rv;
}

/**
 * Removes the path from the list of watched resources. Silently ignores the request
 * if the path was not being watched.
 *
 * Remove Protocol:
 *
 * 1. Find the resource to unwatch through the provided path.
 * 2. Remove the error and change callbacks from the list of callbacks
 *    associated with the resource.
 * 3. Remove the error and change callbacks from the callback hash maps.
 * 4. If there are no more change callbacks for the resource, close
 *    its file |HANDLE|. We do not free the buffer memory just yet, it's
 *    still needed for the next call to GetQueuedCompletionStatus. That
 *    memory will be freed in NativeFileWatcherIOTask::Run.
 *
 * @param aWrappedParameters
 *        The structure containing the resource path, the error and change callback
 *        handles.
 */
nsresult
NativeFileWatcherIOTask::RemovePathRunnableMethod(
  PathRunnablesParametersWrapper* aWrappedParameters)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoPtr<PathRunnablesParametersWrapper> wrappedParameters(aWrappedParameters);

  // We return immediately if |mShuttingDown| is true (see below for
  // details about the shutdown protocol being followed).
  if (mShuttingDown) {
    return NS_OK;
  }

  if (!wrappedParameters ||
      !wrappedParameters->mChangeCallbackHandle) {
    return NS_ERROR_NULL_POINTER;
  }

  WatchedResourceDescriptor* toRemove =
    mWatchedResourcesByPath.Get(wrappedParameters->mPath);
  if (!toRemove) {
    // We are trying to remove a path which wasn't being watched. Silently ignore
    // and dispatch the success callback.
    nsresult rv =
      ReportSuccess(wrappedParameters->mSuccessCallbackHandle, wrappedParameters->mPath);
    if (NS_FAILED(rv)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::RemovePathRunnableMethod - "
        "Failed to dispatch the success callback (%x).",
        rv);
      return rv;
    }
    return NS_OK;
  }

  ChangeCallbackArray* changeCallbackArray =
    mChangeCallbacksTable.Get(toRemove->mPath);

  // This should always be valid.
  MOZ_ASSERT(changeCallbackArray);

  bool removed =
    changeCallbackArray->RemoveElement(wrappedParameters->mChangeCallbackHandle);
  if (!removed) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::RemovePathRunnableMethod - Unable to remove the change "
      "callback from the change callback hash map for %S.",
      wrappedParameters->mPath.get());
    MOZ_CRASH();
  }

  ErrorCallbackArray* errorCallbackArray =
    mErrorCallbacksTable.Get(toRemove->mPath);

  MOZ_ASSERT(errorCallbackArray);

  removed =
    errorCallbackArray->RemoveElement(wrappedParameters->mErrorCallbackHandle);
  if (!removed) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::RemovePathRunnableMethod - Unable to remove the error "
      "callback from the error callback hash map for %S.",
      wrappedParameters->mPath.get());
    MOZ_CRASH();
  }

  // If there are still callbacks left, keep the descriptor.
  // We don't check for error callbacks since there's no point in keeping
  // the descriptor if there are no change callbacks but some error callbacks.
  if (changeCallbackArray->Length()) {
    // Dispatch the success callback.
    nsresult rv =
      ReportSuccess(wrappedParameters->mSuccessCallbackHandle, wrappedParameters->mPath);
    if (NS_FAILED(rv)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::RemovePathRunnableMethod - "
        "Failed to dispatch the success callback (%x).",
        rv);
      return rv;
    }
    return NS_OK;
  }

  // In this runnable, we just cancel callbacks (see above) and I/O (see below).
  // Resources are freed by the worker thread when GetQueuedCompletionStatus
  // detects that a resource was removed from the watch list.
  // Since when closing |HANDLE|s relative to watched resources
  // GetQueuedCompletionStatus is notified one last time, it would end
  // up referring to deallocated memory if we were to free the memory here.
  // This happens because the worker IO is scheduled to watch the resources
  // again once we complete executing this function.

  // Enforce CloseHandle/CancelIO by disposing the AutoCloseHandle. We don't
  // remove the entry from mWatchedResourceBy* since the completion port might
  // still be using the notification buffer. Entry remove is performed when
  // handling ERROR_OPERATION_ABORTED in NativeFileWatcherIOTask::Run.
  toRemove->mResourceHandle.dispose();

  // Dispatch the success callback.
  nsresult rv =
    ReportSuccess(wrappedParameters->mSuccessCallbackHandle, wrappedParameters->mPath);
  if (NS_FAILED(rv)) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::RemovePathRunnableMethod - "
      "Failed to dispatch the success callback (%x).",
      rv);
    return rv;
  }

  return NS_OK;
}

/**
 * Removes all the watched resources from the watch list and stops the
 * watcher thread. Frees all the used resources.
 */
nsresult
NativeFileWatcherIOTask::DeactivateRunnableMethod()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Remind users to manually remove the watches before quitting.
  MOZ_ASSERT(!mWatchedResourcesByHandle.Count(),
             "Clients of the nsINativeFileWatcher must remove "
             "watches manually before quitting.");

  // Log any pending watch.
  for (auto it = mWatchedResourcesByHandle.Iter(); !it.Done(); it.Next()) {
    FILEWATCHERLOG("NativeFileWatcherIOTask::DeactivateRunnableMethod - "
                   "%S is still being watched.", it.UserData()->mPath.get());

  }

  // We return immediately if |mShuttingDown| is true (see below for
  // details about the shutdown protocol being followed).
  if (mShuttingDown) {
    // If this happens, we are in a strange situation.
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::DeactivateRunnableMethod - We are already shutting down.");
    MOZ_CRASH();
    return NS_OK;
  }

  // Deactivate all the non-shutdown methods of this object.
  mShuttingDown = true;

  // Remove all the elements from the index. Memory will be freed by
  // calling Clear() on mWatchedResourcesByPath.
  mWatchedResourcesByHandle.Clear();

  // Clear frees the memory associated with each element and clears the table.
  // Since we are using Scoped |HANDLE|s, they get automatically closed as well.
  mWatchedResourcesByPath.Clear();

  // Now that all the descriptors are closed, release the callback hahstables.
  mChangeCallbacksTable.Clear();
  mErrorCallbacksTable.Clear();

  // Close the IO completion port, eventually making
  // the watcher thread exit from the watching loop.
  if (mIOCompletionPort) {
    if (!CloseHandle(mIOCompletionPort)) {
      FILEWATCHERLOG(
        "NativeFileWatcherIOTask::DeactivateRunnableMethod - "
        "Failed to close the IO completion port HANDLE.");
    }
  }

  // Now we just need to reschedule a final call to Shutdown() back to the main thread.
  RefPtr<NativeWatcherIOShutdownTask> shutdownRunnable =
    new NativeWatcherIOShutdownTask();

  return NS_DispatchToMainThread(shutdownRunnable);
}

/**
 * Helper function to dispatch a change notification to all the registered callbacks.
 * @param aResourceDescriptor
 *        The resource descriptor.
 * @param aChangedResource
 *        The path of the changed resource.
 * @return NS_OK if all the callbacks are dispatched correctly, a |nsresult| error code
 *         otherwise.
 */
nsresult
NativeFileWatcherIOTask::DispatchChangeCallbacks(
  WatchedResourceDescriptor* aResourceDescriptor,
  const nsAString& aChangedResource)
{
  MOZ_ASSERT(aResourceDescriptor);

  // Retrieve the change callbacks array.
  ChangeCallbackArray* changeCallbackArray =
    mChangeCallbacksTable.Get(aResourceDescriptor->mPath);

  // This should always be valid.
  MOZ_ASSERT(changeCallbackArray);

  for (size_t i = 0; i < changeCallbackArray->Length(); i++) {
    nsresult rv =
      ReportChange((*changeCallbackArray)[i], aChangedResource);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

/**
 * Helper function to post a change runnable to the main thread.
 *
 * @param aOnChange
 *        The change callback handle.
 * @param aChangedResource
 *        The resource name to dispatch thorough the change callback.
 *
 * @return NS_OK if the callback is dispatched correctly.
 */
nsresult
NativeFileWatcherIOTask::ReportChange(
  const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChange,
  const nsAString& aChangedResource)
{
  RefPtr<WatchedChangeEvent> changeRunnable =
    new WatchedChangeEvent(aOnChange, aChangedResource);
  return NS_DispatchToMainThread(changeRunnable);
}

/**
 * Helper function to dispatch a error notification to all the registered callbacks.
 * @param aResourceDescriptor
 *        The resource descriptor.
 * @param anError
 *        The error to dispatch thorough the error callback.
 * @param anOSError
 *        An OS specific error code to send with the callback.
 * @return NS_OK if all the callbacks are dispatched correctly, a |nsresult| error code
 *         otherwise.
 */
nsresult
NativeFileWatcherIOTask::DispatchErrorCallbacks(
  WatchedResourceDescriptor* aResourceDescriptor,
  nsresult anError, DWORD anOSError)
{
  MOZ_ASSERT(aResourceDescriptor);

  // Retrieve the error callbacks array.
  ErrorCallbackArray* errorCallbackArray =
    mErrorCallbacksTable.Get(aResourceDescriptor->mPath);

  // This must be valid.
  MOZ_ASSERT(errorCallbackArray);

  for (size_t i = 0; i < errorCallbackArray->Length(); i++) {
    nsresult rv =
      ReportError((*errorCallbackArray)[i], anError, anOSError);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

/**
 * Helper function to post an error runnable to the main thread.
 *
 * @param aOnError
 *        The error callback handle.
 * @param anError
 *        The error to dispatch thorough the error callback.
 * @param anOSError
 *        An OS specific error code to send with the callback.
 *
 * @return NS_OK if the callback is dispatched correctly.
 */
nsresult
NativeFileWatcherIOTask::ReportError(
  const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnError,
  nsresult anError, DWORD anOSError)
{
  RefPtr<WatchedErrorEvent> errorRunnable =
    new WatchedErrorEvent(aOnError, anError, anOSError);
  return NS_DispatchToMainThread(errorRunnable);
}

/**
 * Helper function to post a success runnable to the main thread.
 *
 * @param aOnSuccess
 *        The success callback handle.
 * @param aResource
 *        The resource name to dispatch thorough the success callback.
 *
 * @return NS_OK if the callback is dispatched correctly.
 */
nsresult
NativeFileWatcherIOTask::ReportSuccess(
  const nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback>& aOnSuccess,
  const nsAString& aResource)
{
  RefPtr<WatchedSuccessEvent> successRunnable =
    new WatchedSuccessEvent(aOnSuccess, aResource);
  return NS_DispatchToMainThread(successRunnable);
}


/**
 * Instructs the OS to report the changes concerning the directory of interest.
 *
 * @param aDirectoryDescriptor
 *        A |WatchedResourceDescriptor| instance describing the directory to watch.
 * @param aDispatchErrorCode
 *        If |ReadDirectoryChangesW| fails and dispatching an error callback to the
 *        main thread fails as well, the error code is stored here. If the OS API call
 *        does not fail, it gets set to NS_OK.
 * @return |true| if |ReadDirectoryChangesW| returned no error, |false| otherwise.
 */
nsresult
NativeFileWatcherIOTask::AddDirectoryToWatchList(
  WatchedResourceDescriptor* aDirectoryDescriptor)
{
  MOZ_ASSERT(!mShuttingDown);

  DWORD dwPlaceholder;
  // Tells the OS to watch out on mResourceHandle for the changes specified
  // with the FILE_NOTIFY_* flags. We monitor the creation, renaming and
  // deletion of a file (FILE_NOTIFY_CHANGE_FILE_NAME), changes to the last
  // modification time (FILE_NOTIFY_CHANGE_LAST_WRITE) and the creation and
  // deletion of a folder (FILE_NOTIFY_CHANGE_DIR_NAME). Moreover, when you
  // first call this function, the system allocates a buffer to store change
  // information for the watched directory.
  if (!ReadDirectoryChangesW(aDirectoryDescriptor->mResourceHandle,
                             aDirectoryDescriptor->mNotificationBuffer.get(),
                             NOTIFICATION_BUFFER_SIZE,
                             true, // watch subtree (recurse)
                             FILE_NOTIFY_CHANGE_LAST_WRITE
                             | FILE_NOTIFY_CHANGE_FILE_NAME
                             | FILE_NOTIFY_CHANGE_DIR_NAME,
                             &dwPlaceholder,
                             &aDirectoryDescriptor->mOverlappedInfo,
                             nullptr)) {
    // NOTE: GetLastError() could return ERROR_INVALID_PARAMETER if the buffer length
    // is greater than 64 KB and the application is monitoring a directory over the
    // network. The same error could be returned when trying to watch a file instead
    // of a directory.
    // It could return ERROR_NOACCESS if the buffer is not aligned on a DWORD boundary.
    DWORD dwError = GetLastError();

    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::AddDirectoryToWatchList "
      " - ReadDirectoryChangesW failed (error %x) for %S.",
      dwError, aDirectoryDescriptor->mPath.get());

    nsresult rv =
      DispatchErrorCallbacks(aDirectoryDescriptor, NS_ERROR_FAILURE, dwError);
    if (NS_FAILED(rv)) {
      // That's really bad. We failed to watch the directory and failed to
      // dispatch the error callbacks.
      return NS_ERROR_ABORT;
    }

    // We failed to watch the directory, but we correctly dispatched the error callbacks.
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/**
 * Appends the change and error callbacks to their respective hash tables.
 * It also checks if the callbacks are already attached to them.
 * @param aPath
 *        The watched directory path.
 * @param aOnChangeHandle
 *        The callback to invoke when a change is detected.
 * @param aOnErrorHandle
 *        The callback to invoke when an error is detected.
 */
void
NativeFileWatcherIOTask::AppendCallbacksToHashtables(
  const nsAString& aPath,
  const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChangeHandle,
  const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnErrorHandle)
{
  // First check to see if we've got an entry already.
  ChangeCallbackArray* callbacksArray = mChangeCallbacksTable.Get(aPath);
  if (!callbacksArray) {
    // We don't have an entry. Create an array and put it into the hash table.
    callbacksArray = new ChangeCallbackArray();
    mChangeCallbacksTable.Put(aPath, callbacksArray);
  }

  // We do have an entry for that path. Check to see if the callback is
  // already there.
  ChangeCallbackArray::index_type changeCallbackIndex =
    callbacksArray->IndexOf(aOnChangeHandle);

  // If the callback is not attached to the descriptor, append it.
  if (changeCallbackIndex == ChangeCallbackArray::NoIndex) {
    callbacksArray->AppendElement(aOnChangeHandle);
  }

  // Same thing for the error callback.
  ErrorCallbackArray* errorCallbacksArray = mErrorCallbacksTable.Get(aPath);
  if (!errorCallbacksArray) {
    // We don't have an entry. Create an array and put it into the hash table.
    errorCallbacksArray = new ErrorCallbackArray();
    mErrorCallbacksTable.Put(aPath, errorCallbacksArray);
  }

  ErrorCallbackArray::index_type errorCallbackIndex =
    errorCallbacksArray->IndexOf(aOnErrorHandle);

  if (errorCallbackIndex == ErrorCallbackArray::NoIndex) {
    errorCallbacksArray->AppendElement(aOnErrorHandle);
  }
}

/**
 * Removes the change and error callbacks from their respective hash tables.
 * @param aPath
 *        The watched directory path.
 * @param aOnChangeHandle
 *        The change callback to remove.
 * @param aOnErrorHandle
 *        The error callback to remove.
 */
void
NativeFileWatcherIOTask::RemoveCallbacksFromHashtables(
  const nsAString& aPath,
  const nsMainThreadPtrHandle<nsINativeFileWatcherCallback>& aOnChangeHandle,
  const nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback>& aOnErrorHandle)
{
  // Find the change callback array for |aPath|.
  ChangeCallbackArray* callbacksArray = mChangeCallbacksTable.Get(aPath);
  if (callbacksArray) {
    // Remove the change callback.
    callbacksArray->RemoveElement(aOnChangeHandle);
  }

  // Find the error callback array for |aPath|.
  ErrorCallbackArray* errorCallbacksArray = mErrorCallbacksTable.Get(aPath);
  if (errorCallbacksArray) {
    // Remove the error callback.
    errorCallbacksArray->RemoveElement(aOnErrorHandle);
  }
}

/**
 * Creates a string representing the native path for the changed resource.
 * It appends the resource name to the path of the changed descriptor by
 * using nsILocalFile.
 * @param changedDescriptor
 *        The descriptor of the watched resource.
 * @param resourceName
 *        The resource which triggered the change.
 * @param nativeResourcePath
 *        The full path to the changed resource.
 * @return NS_OK if nsILocalFile succeeded in building the path.
 */
nsresult
NativeFileWatcherIOTask::MakeResourcePath(
  WatchedResourceDescriptor* changedDescriptor,
  const nsAString& resourceName,
  nsAString& nativeResourcePath)
{
  nsCOMPtr<nsILocalFile>
    localPath(do_CreateInstance("@mozilla.org/file/local;1"));
  if (!localPath) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::MakeResourcePath - Failed to create a nsILocalFile instance.");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = localPath->InitWithPath(changedDescriptor->mPath);
  if (NS_FAILED(rv)) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::MakeResourcePath - Failed to init nsILocalFile with %S (%x).",
      changedDescriptor->mPath.get(), rv);
    return rv;
  }

  rv = localPath->AppendRelativePath(resourceName);
  if (NS_FAILED(rv)) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::MakeResourcePath - Failed to append to %S (%x).",
      changedDescriptor->mPath.get(), rv);
    return rv;
  }

  rv = localPath->GetPath(nativeResourcePath);
  if (NS_FAILED(rv)) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::MakeResourcePath - Failed to get native path from nsILocalFile (%x).",
      rv);
    return rv;
  }

  return NS_OK;
}

} // namespace

// The NativeFileWatcherService component

NS_IMPL_ISUPPORTS(NativeFileWatcherService, nsINativeFileWatcherService, nsIObserver);

NativeFileWatcherService::NativeFileWatcherService()
{
}

NativeFileWatcherService::~NativeFileWatcherService()
{
}

/**
 * Sets the required resources and starts the watching IO thread.
 *
 * @return NS_OK if there was no error with thread creation and execution.
 */
nsresult
NativeFileWatcherService::Init()
{
  // Creates an IO completion port and allows at most 2 thread to access it concurrently.
  AutoCloseHandle completionPort(
    CreateIoCompletionPort(INVALID_HANDLE_VALUE, // FileHandle
                           nullptr, // ExistingCompletionPort
                           0, // CompletionKey
                           2)); // NumberOfConcurrentThreads
  if (!completionPort) {
    return NS_ERROR_FAILURE;
  }

  // Add an observer for the shutdown.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  observerService->AddObserver(this, "xpcom-shutdown-threads", false);

  // Start the IO worker thread.
  mWorkerIORunnable = new NativeFileWatcherIOTask(completionPort);
  nsresult rv = NS_NewThread(getter_AddRefs(mIOThread), mWorkerIORunnable);
  if (NS_FAILED(rv)) {
    FILEWATCHERLOG(
      "NativeFileWatcherIOTask::Init - Unable to create and dispatch the worker thread (%x).",
      rv);
    return rv;
  }

  // Set the name for the worker thread.
  NS_SetThreadName(mIOThread, "FileWatcher IO");

  mIOCompletionPort = completionPort.forget();

  return NS_OK;
}

/**
 * Watches a path for changes: monitors the creations, name changes and
 * content changes to the files contained in the watched path.
 *
 * @param aPathToWatch
 *        The path of the resource to watch for changes.
 * @param aOnChange
 *        The callback to invoke when a change is detected.
 * @param aOnError
 *        The optional callback to invoke when there's an error.
 * @param aOnSuccess
 *        The optional callback to invoke when the file watcher starts
 *        watching the resource for changes.
 *
 * @return NS_OK or NS_ERROR_NOT_INITIALIZED if the instance was not initialized.
 *         Other errors are reported by the error callback function.
 */
NS_IMETHODIMP
NativeFileWatcherService::AddPath(const nsAString& aPathToWatch,
                                  nsINativeFileWatcherCallback* aOnChange,
                                  nsINativeFileWatcherErrorCallback* aOnError,
                                  nsINativeFileWatcherSuccessCallback* aOnSuccess)
{
  // Make sure the instance was initialized.
  if (!mIOThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Be sure a valid change callback was passed.
  if (!aOnChange) {
    return NS_ERROR_NULL_POINTER;
  }

  nsMainThreadPtrHandle<nsINativeFileWatcherCallback> changeCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherCallback>(aOnChange));

  nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback> errorCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherErrorCallback>(aOnError));

  nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback> successCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherSuccessCallback>(aOnSuccess));

  // Wrap the path and the callbacks in order to pass them using NewRunnableMethod.
  UniquePtr<PathRunnablesParametersWrapper> wrappedCallbacks(
    new PathRunnablesParametersWrapper(
      aPathToWatch,
      changeCallbackHandle,
      errorCallbackHandle,
      successCallbackHandle));

  // Since this function does a bit of I/O stuff , run it in the IO thread.
  nsresult rv =
    mIOThread->Dispatch(
      NewRunnableMethod<PathRunnablesParametersWrapper*>(
        static_cast<NativeFileWatcherIOTask*>(mWorkerIORunnable.get()),
        &NativeFileWatcherIOTask::AddPathRunnableMethod,
        wrappedCallbacks.get()),
      nsIEventTarget::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Since the dispatch succeeded, we let the runnable own the pointer.
  wrappedCallbacks.release();

  WakeUpWorkerThread();

  return NS_OK;
}

/**
 * Removes the path from the list of watched resources. Silently ignores the request
 * if the path was not being watched or the callbacks were not registered.
 *
 * @param aPathToRemove
 *        The path of the resource to remove from the watch list.
 * @param aOnChange
 *        The callback to invoke when a change is detected.
 * @param aOnError
 *        The optionally registered callback invoked when there's an error.
 * @param aOnSuccess
 *        The optional callback to invoke when the file watcher stops
 *        watching the resource for changes.
 *
 * @return NS_OK or NS_ERROR_NOT_INITIALIZED if the instance was not initialized.
 *         Other errors are reported by the error callback function.
 */
NS_IMETHODIMP
NativeFileWatcherService::RemovePath(const nsAString& aPathToRemove,
                                     nsINativeFileWatcherCallback* aOnChange,
                                     nsINativeFileWatcherErrorCallback* aOnError,
                                     nsINativeFileWatcherSuccessCallback* aOnSuccess)
{
  // Make sure the instance was initialized.
  if (!mIOThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Be sure a valid change callback was passed.
  if (!aOnChange) {
    return NS_ERROR_NULL_POINTER;
  }

  nsMainThreadPtrHandle<nsINativeFileWatcherCallback> changeCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherCallback>(aOnChange));

  nsMainThreadPtrHandle<nsINativeFileWatcherErrorCallback> errorCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherErrorCallback>(aOnError));

  nsMainThreadPtrHandle<nsINativeFileWatcherSuccessCallback> successCallbackHandle(
    new nsMainThreadPtrHolder<nsINativeFileWatcherSuccessCallback>(aOnSuccess));

  // Wrap the path and the callbacks in order to pass them using NewRunnableMethod.
  UniquePtr<PathRunnablesParametersWrapper> wrappedCallbacks(
    new PathRunnablesParametersWrapper(
      aPathToRemove,
      changeCallbackHandle,
      errorCallbackHandle,
      successCallbackHandle));

  // Since this function does a bit of I/O stuff, run it in the IO thread.
  nsresult rv =
    mIOThread->Dispatch(
      NewRunnableMethod<PathRunnablesParametersWrapper*>(
        static_cast<NativeFileWatcherIOTask*>(mWorkerIORunnable.get()),
        &NativeFileWatcherIOTask::RemovePathRunnableMethod,
        wrappedCallbacks.get()),
      nsIEventTarget::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Since the dispatch succeeded, we let the runnable own the pointer.
  wrappedCallbacks.release();

  WakeUpWorkerThread();

  return NS_OK;
}

/**
 * Removes all the watched resources from the watch list and stops the
 * watcher thread. Frees all the used resources.
 *
 * To avoid race conditions, we need a Shutdown Protocol:
 *
 * 1. [MainThread]
 *    When the "xpcom-shutdown-threads" event is detected, Uninit() gets called.
 * 2. [MainThread]
 *    Uninit sends DeactivateRunnableMethod() to the WorkerThread.
 * 3. [WorkerThread]
 *    DeactivateRunnableMethod makes it clear to other methods that shutdown is
 *    in progress, stops the IO completion port wait and schedules the rest of the
 *    deactivation for after every currently pending method call is complete.
 */
nsresult
NativeFileWatcherService::Uninit()
{
  // Make sure the instance was initialized (and not de-initialized yet).
  if (!mIOThread) {
    return NS_OK;
  }

  // We need to be sure that there will be no calls to 'mIOThread' once we have entered
  // 'Uninit()', even if we exit due to an error.
  nsCOMPtr<nsIThread> ioThread;
  ioThread.swap(mIOThread);

  // Since this function does a bit of I/O stuff (close file handle), run it
  // in the IO thread.
  nsresult rv =
    ioThread->Dispatch(
      NewRunnableMethod(
        static_cast<NativeFileWatcherIOTask*>(mWorkerIORunnable.get()),
        &NativeFileWatcherIOTask::DeactivateRunnableMethod),
      nsIEventTarget::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  WakeUpWorkerThread();

  return NS_OK;
}

/**
 * Tells |NativeFileWatcherIOTask| to quit and to reschedule itself in order to
 * execute the other runnables enqueued in the worker tread.
 * This works by posting a bogus event to the blocking |GetQueuedCompletionStatus|
 * call in |NativeFileWatcherIOTask::Run()|.
 */
void
NativeFileWatcherService::WakeUpWorkerThread()
{
  // The last 3 parameters represent the number of transferred bytes, the changed
  // resource |HANDLE| and the address of the |OVERLAPPED| structure passed to
  // GetQueuedCompletionStatus: we set them to nullptr so that we can recognize
  // that we requested an interruption from the Worker thread.
  PostQueuedCompletionStatus(mIOCompletionPort, 0, 0, nullptr);
}

/**
 * This method is used to catch the "xpcom-shutdown-threads" event in order
 * to shutdown this service when closing the application.
 */
NS_IMETHODIMP
NativeFileWatcherService::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp("xpcom-shutdown-threads", aTopic)) {
    nsresult rv = Uninit();
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return NS_OK;
  }

  MOZ_ASSERT(false, "NativeFileWatcherService got an unexpected topic!");

  return NS_ERROR_UNEXPECTED;
}

} // namespace mozilla
