/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamUtils_h__
#define nsStreamUtils_h__

#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "nsIInputStream.h"
#include "nsTArray.h"
#include "nsIRunnable.h"

class nsIAsyncInputStream;
class nsIOutputStream;
class nsIInputStreamCallback;
class nsIOutputStreamCallback;
class nsIEventTarget;

/**
 * A "one-shot" proxy of the OnInputStreamReady callback.  The resulting
 * proxy object's OnInputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread corresponding to the given event target regardless of what thread
 * the proxy object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aTarget parameter is non-null.
 *
 * The optional aPriority parameter allows the input stream runnable events
 * to be dispatched with a different priority than normal.
 */
extern already_AddRefed<nsIInputStreamCallback> NS_NewInputStreamReadyEvent(
    const char* aName, nsIInputStreamCallback* aNotify, nsIEventTarget* aTarget,
    uint32_t aPriority = nsIRunnablePriority::PRIORITY_NORMAL);

/**
 * A "one-shot" proxy of the OnOutputStreamReady callback.  The resulting
 * proxy object's OnOutputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread corresponding to the given event target regardless of what thread
 * the proxy object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aTarget parameter is non-null.
 */
extern already_AddRefed<nsIOutputStreamCallback> NS_NewOutputStreamReadyEvent(
    nsIOutputStreamCallback* aNotify, nsIEventTarget* aTarget);

/* ------------------------------------------------------------------------- */

enum nsAsyncCopyMode {
  NS_ASYNCCOPY_VIA_READSEGMENTS,
  NS_ASYNCCOPY_VIA_WRITESEGMENTS
};

/**
 * This function is called when a new chunk of data has been copied.  The
 * reported count is the size of the current chunk.
 */
typedef void (*nsAsyncCopyProgressFun)(void* closure, uint32_t count);

/**
 * This function is called when the async copy process completes.  The reported
 * status is NS_OK on success and some error code on failure.
 */
typedef void (*nsAsyncCopyCallbackFun)(void* closure, nsresult status);

/**
 * This function asynchronously copies data from the source to the sink. All
 * data transfer occurs on the thread corresponding to the given event target.
 * A null event target is not permitted.
 *
 * The copier handles blocking or non-blocking streams transparently.  If a
 * stream operation returns NS_BASE_STREAM_WOULD_BLOCK, then the stream will
 * be QI'd to nsIAsync{In,Out}putStream and its AsyncWait method will be used
 * to determine when to resume copying.
 *
 * Source and sink are closed by default when copying finishes or when error
 * occurs. Caller can prevent closing source or sink by setting aCloseSource
 * or aCloseSink to false.
 *
 * Caller can obtain aCopierCtx to be able to cancel copying.
 */
extern nsresult NS_AsyncCopy(
    nsIInputStream* aSource, nsIOutputStream* aSink, nsIEventTarget* aTarget,
    nsAsyncCopyMode aMode = NS_ASYNCCOPY_VIA_READSEGMENTS,
    uint32_t aChunkSize = 4096, nsAsyncCopyCallbackFun aCallbackFun = nullptr,
    void* aCallbackClosure = nullptr, bool aCloseSource = true,
    bool aCloseSink = true, nsISupports** aCopierCtx = nullptr,
    nsAsyncCopyProgressFun aProgressCallbackFun = nullptr);

/**
 * This function cancels copying started by function NS_AsyncCopy.
 *
 * @param aCopierCtx
 *        Copier context returned by NS_AsyncCopy.
 * @param aReason
 *        A failure code indicating why the operation is being canceled.
 *        It is an error to pass a success code.
 */
extern nsresult NS_CancelAsyncCopy(nsISupports* aCopierCtx, nsresult aReason);

/**
 * This function copies all of the available data from the stream (up to at
 * most aMaxCount bytes) into the given buffer.  The buffer is truncated at
 * the start of the function.
 *
 * If an error occurs while reading from the stream or while attempting to
 * resize the buffer, then the corresponding error code is returned from this
 * function, and any data that has already been read will be returned in the
 * output buffer.  This allows one to use this function with a non-blocking
 * input stream that may return NS_BASE_STREAM_WOULD_BLOCK if it only has
 * partial data available.
 *
 * @param aSource
 *        The input stream to read.
 * @param aMaxCount
 *        The maximum number of bytes to consume from the stream.  Pass the
 *        value UINT32_MAX to consume the entire stream.  The number of
 *        bytes actually read is given by the length of aBuffer upon return.
 * @param aBuffer
 *        The string object that will contain the stream data upon return.
 *        Note: The data copied to the string may contain null bytes and may
 *        contain non-ASCII values.
 */
extern nsresult NS_ConsumeStream(nsIInputStream* aSource, uint32_t aMaxCount,
                                 nsACString& aBuffer);

/**
 * Just like the above, but consumes into an nsTArray<uint8_t>.
 */
extern nsresult NS_ConsumeStream(nsIInputStream* aSource, uint32_t aMaxCount,
                                 nsTArray<uint8_t>& aBuffer);

/**
 * This function tests whether or not the input stream is buffered. A buffered
 * input stream is one that implements readSegments.  The test for this is to
 * 1/ check whether the input stream implements nsIBufferedInputStream;
 * 2/ if not, call readSegments, without actually consuming any data from the
 * stream, to verify that it functions.
 *
 * NOTE: If the stream is non-blocking and has no data available yet, then this
 * test will fail.  In that case, we return false even though the test is not
 * really conclusive.
 *
 * PERFORMANCE NOTE: If the stream does not implement nsIBufferedInputStream,
 * calling readSegments may cause I/O. Therefore, you should avoid calling
 * this function from the main thread.
 *
 * @param aInputStream
 *        The input stream to test.
 */
extern bool NS_InputStreamIsBuffered(nsIInputStream* aInputStream);

/**
 * This function tests whether or not the output stream is buffered.  A
 * buffered output stream is one that implements writeSegments.  The test for
 * this is to:
 * 1/ check whether the output stream implements nsIBufferedOutputStream;
 * 2/ if not, call writeSegments, without actually writing any data into
 * the stream, to verify that it functions.
 *
 * NOTE: If the stream is non-blocking and has no available space yet, then
 * this test will fail.  In that case, we return false even though the test is
 * not really conclusive.
 *
 * PERFORMANCE NOTE: If the stream does not implement nsIBufferedOutputStream,
 * calling writeSegments may cause I/O. Therefore, you should avoid calling
 * this function from the main thread.
 *
 * @param aOutputStream
 *        The output stream to test.
 */
extern bool NS_OutputStreamIsBuffered(nsIOutputStream* aOutputStream);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * copy data from the nsIInputStream into a nsIOutputStream passed as the
 * aClosure parameter to the ReadSegments function.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern nsresult NS_CopySegmentToStream(nsIInputStream* aInputStream,
                                       void* aClosure, const char* aFromSegment,
                                       uint32_t aToOffset, uint32_t aCount,
                                       uint32_t* aWriteCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * copy data from the nsIInputStream into a character buffer passed as the
 * aClosure parameter to the ReadSegments function.  The character buffer
 * must be at least as large as the aCount parameter passed to ReadSegments.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern nsresult NS_CopySegmentToBuffer(nsIInputStream* aInputStream,
                                       void* aClosure, const char* aFromSegment,
                                       uint32_t aToOffset, uint32_t aCount,
                                       uint32_t* aWriteCount);

/**
 * This function is intended to be passed to nsIOutputStream::WriteSegments to
 * copy data into the nsIOutputStream from a character buffer passed as the
 * aClosure parameter to the WriteSegments function.
 *
 * @see nsIOutputStream.idl for a description of this function's parameters.
 */
extern nsresult NS_CopySegmentToBuffer(nsIOutputStream* aOutputStream,
                                       void* aClosure, char* aToSegment,
                                       uint32_t aFromOffset, uint32_t aCount,
                                       uint32_t* aReadCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * discard data from the nsIInputStream.  This can be used to efficiently read
 * data from the stream without actually copying any bytes.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern nsresult NS_DiscardSegment(nsIInputStream* aInputStream, void* aClosure,
                                  const char* aFromSegment, uint32_t aToOffset,
                                  uint32_t aCount, uint32_t* aWriteCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * adjust the aInputStream parameter passed to a consumer's WriteSegmentFun.
 * The aClosure parameter must be a pointer to a nsWriteSegmentThunk object.
 * The mStream and mClosure members of that object will be passed to the mFun
 * function, with the remainder of the parameters being what are passed to
 * NS_WriteSegmentThunk.
 *
 * This function comes in handy when implementing ReadSegments in terms of an
 * inner stream's ReadSegments.
 */
extern nsresult NS_WriteSegmentThunk(nsIInputStream* aInputStream,
                                     void* aClosure, const char* aFromSegment,
                                     uint32_t aToOffset, uint32_t aCount,
                                     uint32_t* aWriteCount);

struct MOZ_STACK_CLASS nsWriteSegmentThunk {
  nsCOMPtr<nsIInputStream> mStream;
  nsWriteSegmentFun mFun;
  void* mClosure;
};

/**
 * Read data from aInput and store in aDest.  A non-zero aKeep will keep that
 * many bytes from aDest (from the end).  New data is appended after the kept
 * bytes (if any).  aDest's new length on returning from this function is
 * aKeep + aNewBytes and is guaranteed to be less than or equal to aDest's
 * current capacity.
 * @param aDest the array to fill
 * @param aInput the stream to read from
 * @param aKeep number of bytes to keep (0 <= aKeep <= aDest.Length())
 * @param aNewBytes (out) number of bytes read from aInput or zero if Read()
 *        failed
 * @return the result from aInput->Read(...)
 */
extern nsresult NS_FillArray(FallibleTArray<char>& aDest,
                             nsIInputStream* aInput, uint32_t aKeep,
                             uint32_t* aNewBytes);

/**
 * Return true if the given stream can be directly cloned.
 */
extern bool NS_InputStreamIsCloneable(nsIInputStream* aSource);

/**
 * Clone the provided source stream in the most efficient way possible.  This
 * first attempts to QI to nsICloneableInputStream to use Clone().  If that is
 * not supported or its cloneable attribute is false, then a fallback clone is
 * provided by copying the source to a pipe.  In this case the caller must
 * replace the source stream with the resulting replacement stream.  The clone
 * and the replacement stream are then cloneable using nsICloneableInputStream
 * without duplicating memory.  This fallback clone using the pipe is only
 * performed if a replacement stream parameter is also passed in.
 * @param aSource         The input stream to clone.
 * @param aCloneOut       Required out parameter to hold resulting clone.
 * @param aReplacementOut Optional out parameter to hold stream to replace
 *                        original source stream after clone.  If not
 *                        provided then the fallback clone process is not
 *                        supported and a non-cloneable source will result
 *                        in failure.  Replacement streams are non-blocking.
 * @return NS_OK on successful clone.  Error otherwise.
 */
extern nsresult NS_CloneInputStream(nsIInputStream* aSource,
                                    nsIInputStream** aCloneOut,
                                    nsIInputStream** aReplacementOut = nullptr);

/*
 * This function returns a non-blocking nsIAsyncInputStream. Internally,
 * different approaches are used based on what |aSource| is and what it
 * implements.
 *
 * Note that this component takes the owninship of aSource.
 *
 * If the |aSource| is already a non-blocking and async stream,
 * |aAsyncInputStream| will be equal to |aSource|.
 *
 * Otherwise, if |aSource| is just non-blocking, NonBlockingAsyncInputStream
 * class is used in order to make it async.
 *
 * The last step is to use nsIStreamTransportService and create a pipe in order
 * to expose a non-blocking async inputStream and read |aSource| data from
 * a separate thread.
 *
 * In case we need to create a pipe, |aCloseWhenDone| will be used to create the
 * inputTransport, |aFlags|, |aSegmentSize|, |asegmentCount| will be used to
 * open the inputStream. If true, the input stream will be closed after it has
 * been read. Read more in nsITransport.idl.
 */
extern nsresult NS_MakeAsyncNonBlockingInputStream(
    already_AddRefed<nsIInputStream> aSource,
    nsIAsyncInputStream** aAsyncInputStream, bool aCloseWhenDone = true,
    uint32_t aFlags = 0, uint32_t aSegmentSize = 0, uint32_t aSegmentCount = 0);

#endif  // !nsStreamUtils_h__
