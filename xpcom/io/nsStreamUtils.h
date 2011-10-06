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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsStreamUtils_h__
#define nsStreamUtils_h__

#include "nsStringFwd.h"
#include "nsIInputStream.h"

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
extern nsresult
NS_NewInputStreamReadyEvent(nsIInputStreamCallback **aEvent,
                            nsIInputStreamCallback  *aNotify,
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
extern nsresult
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback **aEvent,
                             nsIOutputStreamCallback  *aNotify,
                             nsIEventTarget           *aTarget);

/* ------------------------------------------------------------------------- */

enum nsAsyncCopyMode {
    NS_ASYNCCOPY_VIA_READSEGMENTS,
    NS_ASYNCCOPY_VIA_WRITESEGMENTS
};

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
 * or aCloseSink to PR_FALSE.
 *
 * Caller can obtain aCopierCtx to be able to cancel copying.
 */
extern nsresult
NS_AsyncCopy(nsIInputStream         *aSource,
             nsIOutputStream        *aSink,
             nsIEventTarget         *aTarget,
             nsAsyncCopyMode         aMode = NS_ASYNCCOPY_VIA_READSEGMENTS,
             PRUint32                aChunkSize = 4096,
             nsAsyncCopyCallbackFun  aCallbackFun = nsnull,
             void                   *aCallbackClosure = nsnull,
             PRBool                  aCloseSource = PR_TRUE,
             PRBool                  aCloseSink = PR_TRUE,
             nsISupports           **aCopierCtx = nsnull);

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
 *        value PR_UINT32_MAX to consume the entire stream.  The number of
 *        bytes actually read is given by the length of aBuffer upon return.
 * @param aBuffer
 *        The string object that will contain the stream data upon return.
 *        Note: The data copied to the string may contain null bytes and may
 *        contain non-ASCII values.
 */
extern nsresult
NS_ConsumeStream(nsIInputStream *aSource, PRUint32 aMaxCount,
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
extern PRBool
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
extern PRBool
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
                       const char *aFromSegment, PRUint32 aToOffset,
                       PRUint32 aCount, PRUint32 *aWriteCount);

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
                       const char *aFromSegment, PRUint32 aToOffset,
                       PRUint32 aCount, PRUint32 *aWriteCount);

/**
 * This function is intended to be passed to nsIInputStream::ReadSegments to
 * discard data from the nsIInputStream.  This can be used to efficiently read
 * data from the stream without actually copying any bytes.
 *
 * @see nsIInputStream.idl for a description of this function's parameters.
 */
extern NS_METHOD
NS_DiscardSegment(nsIInputStream *aInputStream, void *aClosure,
                  const char *aFromSegment, PRUint32 aToOffset,
                  PRUint32 aCount, PRUint32 *aWriteCount);

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
                     const char *aFromSegment, PRUint32 aToOffset,
                     PRUint32 aCount, PRUint32 *aWriteCount);

struct nsWriteSegmentThunk {
  nsIInputStream    *mStream;
  nsWriteSegmentFun  mFun;
  void              *mClosure;
};

#endif // !nsStreamUtils_h__
