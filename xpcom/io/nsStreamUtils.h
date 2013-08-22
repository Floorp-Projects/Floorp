/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamUtils_h__
#define nsStreamUtils_h__

#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "nsIInputStream.h"
#include "nsTArray.h"

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
 */
extern already_AddRefed<nsIInputStreamCallback>
NS_NewInputStreamReadyEvent(nsIInputStreamCallback  *aNotify,
                            nsIEventTarget          *aTarget);

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
extern already_AddRefed<nsIOutputStreamCallback>
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback  *aNotify,
                             nsIEventTarget           *aTarget);

/* ------------------------------------------------------------------------- */

enum nsAsyncCopyMode {
    NS_ASYNCCOPY_VIA_READSEGMENTS,
    NS_ASYNCCOPY_VIA_WRITESEGMENTS
};

/**
 * This function is called when a new chunk of data has been copied.  The
 * reported count is the size of the current chunk.
 */
typedef void (* nsAsyncCopyProgressFun)(void *closure, uint32_t count);

/**
 * This function is called when the async copy process completes.  The reported
 * status is NS_OK on success and some error code on failure.
 */
typedef void (* nsAsyncCopyCallbackFun)(void *closure, nsresult status);

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
extern nsresult
NS_AsyncCopy(nsIInputStream         *aSource,
             nsIOutputStream        *aSink,
             nsIEventTarget         *aTarget,
             nsAsyncCopyMode         aMode = NS_ASYNCCOPY_VIA_READSEGMENTS,
             uint32_t                aChunkSize = 4096,
             nsAsyncCopyCallbackFun  aCallbackFun = nullptr,
             void                   *aCallbackClosure = nullptr,
             bool                    aCloseSource = true,
             bool                    aCloseSink = true,
             nsISupports           **aCopierCtx = nullptr,
             nsAsyncCopyProgressFun  aProgressCallbackFun = nullptr);

/**
 * This function cancels copying started by function NS_AsyncCopy.
 *
 * @param aCopierCtx
 *        Copier context returned by NS_AsyncCopy.
 * @param aReason
 *        A failure code indicating why the operation is being canceled.
 *        It is an error to pass a success code.
 */
extern nsresult
NS_CancelAsyncCopy(nsISupports *aCopierCtx, nsresult aReason);

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
extern nsresult
NS_ConsumeStream(nsIInputStream *aSource, uint32_t aMaxCount,
                 nsACString &aBuffer);

/**
 * This function tests whether or not the input stream is buffered.  A buffered
 * input stream is one that implements readSegments.  The test for this is to
 * simply call readSegments, without actually consuming any data from the
 * stream, to verify that it functions.
 *
 * NOTE: If the stream is non-blocking and has no data available yet, then this
 * test will fail.  In that case, we return false even though the test is not 
 * really conclusive.
 *
 * @param aInputStream
 *        The input stream to test.
 */
extern bool
NS_InputStreamIsBuffered(nsIInputStream *aInputStream);

/**
 * This function tests whether or not the output stream is buffered.  A
 * buffered output stream is one that implements writeSegments.  The test for
 * this is to simply call writeSegments, without actually writing any data into
 * the stream, to verify that it functions.
 *
 * NOTE: If the stream is non-blocking and has no available space yet, then
 * this test will fail.  In that case, we return false even though the test is
 * not really conclusive.
 *
 * @param aOutputStream
 *        The output stream to test.
 */
extern bool
NS_OutputStreamIsBuffered(nsIOutputStream *aOutputStream);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * copy data from the nsIInputStream into a nsIOutputStream passed as the
 * aClosure parameter to the ReadSegments function.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern NS_METHOD
NS_CopySegmentToStream(nsIInputStream *aInputStream, void *aClosure,
                       const char *aFromSegment, uint32_t aToOffset,
                       uint32_t aCount, uint32_t *aWriteCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * copy data from the nsIInputStream into a character buffer passed as the
 * aClosure parameter to the ReadSegments function.  The character buffer
 * must be at least as large as the aCount parameter passed to ReadSegments.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern NS_METHOD
NS_CopySegmentToBuffer(nsIInputStream *aInputStream, void *aClosure,
                       const char *aFromSegment, uint32_t aToOffset,
                       uint32_t aCount, uint32_t *aWriteCount);

/**
 * This function is intended to be passed to nsIOutputStream::WriteSegments to
 * copy data into the nsIOutputStream from a character buffer passed as the
 * aClosure parameter to the WriteSegments function.
 *
 * @see nsIOutputStream.idl for a description of this function's parameters.
 */
extern NS_METHOD
NS_CopySegmentToBuffer(nsIOutputStream *aOutputStream, void *aClosure,
                       char *aToSegment, uint32_t aFromOffset,
                       uint32_t aCount, uint32_t *aReadCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * discard data from the nsIInputStream.  This can be used to efficiently read
 * data from the stream without actually copying any bytes.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern NS_METHOD
NS_DiscardSegment(nsIInputStream *aInputStream, void *aClosure,
                  const char *aFromSegment, uint32_t aToOffset,
                  uint32_t aCount, uint32_t *aWriteCount);

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
extern NS_METHOD
NS_WriteSegmentThunk(nsIInputStream *aInputStream, void *aClosure,
                     const char *aFromSegment, uint32_t aToOffset,
                     uint32_t aCount, uint32_t *aWriteCount);

struct nsWriteSegmentThunk {
  nsIInputStream    *mStream;
  nsWriteSegmentFun  mFun;
  void              *mClosure;
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
extern NS_METHOD
NS_FillArray(FallibleTArray<char> &aDest, nsIInputStream *aInput,
             uint32_t aKeep, uint32_t *aNewBytes);

#endif // !nsStreamUtils_h__
