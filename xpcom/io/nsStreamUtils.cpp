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

#include "nsStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsIPipe.h"
#include "nsIEventTarget.h"
#include "nsAutoLock.h"

//-----------------------------------------------------------------------------

class nsInputStreamReadyEvent : public PLEvent
                              , public nsIInputStreamCallback
{
public:
    NS_DECL_ISUPPORTS

    nsInputStreamReadyEvent(nsIInputStreamCallback *callback,
                            nsIEventTarget *target)
        : mCallback(callback)
        , mTarget(target)
    {
    }

private:
    ~nsInputStreamReadyEvent()
    {
        if (mCallback) {
            nsresult rv;
            //
            // whoa!!  looks like we never posted this event.  take care to
            // release mCallback on the correct thread.  if mTarget lives on the
            // calling thread, then we are ok.  otherwise, we have to try to 
            // proxy the Release over the right thread.  if that thread is dead,
            // then there's nothing we can do... better to leak than crash.
            //
            PRBool val;
            rv = mTarget->IsOnCurrentThread(&val);
            if (NS_FAILED(rv) || !val) {
                nsCOMPtr<nsIInputStreamCallback> event;
                NS_NewInputStreamReadyEvent(getter_AddRefs(event), mCallback, mTarget);
                mCallback = 0;
                if (event) {
                    rv = event->OnInputStreamReady(nsnull);
                    if (NS_FAILED(rv)) {
                        NS_NOTREACHED("leaking stream event");
                        nsISupports *sup = event;
                        NS_ADDREF(sup);
                    }
                }
            }
        }
    }

public:
    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *stream)
    {
        mStream = stream;

        // will be released when event is handled
        NS_ADDREF_THIS();

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);

        if (NS_FAILED(mTarget->PostEvent(this))) {
            NS_WARNING("PostEvent failed");
            NS_RELEASE_THIS();
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>    mStream;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget>         mTarget;

    PR_STATIC_CALLBACK(void *) EventHandler(PLEvent *plevent)
    {
        nsInputStreamReadyEvent *ev = (nsInputStreamReadyEvent *) plevent;
        // bypass event delivery if this is a cleanup event...
        if (ev->mCallback)
            ev->mCallback->OnInputStreamReady(ev->mStream);
        ev->mCallback = 0;
        return NULL;
    }

    PR_STATIC_CALLBACK(void) EventCleanup(PLEvent *plevent)
    {
        nsInputStreamReadyEvent *ev = (nsInputStreamReadyEvent *) plevent;
        NS_RELEASE(ev);
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsInputStreamReadyEvent,
                              nsIInputStreamCallback)

//-----------------------------------------------------------------------------

class nsOutputStreamReadyEvent : public PLEvent
                               , public nsIOutputStreamCallback
{
public:
    NS_DECL_ISUPPORTS

    nsOutputStreamReadyEvent(nsIOutputStreamCallback *callback,
                             nsIEventTarget *target)
        : mCallback(callback)
        , mTarget(target)
    {
    }

private:
    ~nsOutputStreamReadyEvent()
    {
        if (mCallback) {
            nsresult rv;
            //
            // whoa!!  looks like we never posted this event.  take care to
            // release mCallback on the correct thread.  if mTarget lives on the
            // calling thread, then we are ok.  otherwise, we have to try to 
            // proxy the Release over the right thread.  if that thread is dead,
            // then there's nothing we can do... better to leak than crash.
            //
            PRBool val;
            rv = mTarget->IsOnCurrentThread(&val);
            if (NS_FAILED(rv) || !val) {
                nsCOMPtr<nsIOutputStreamCallback> event;
                NS_NewOutputStreamReadyEvent(getter_AddRefs(event), mCallback, mTarget);
                mCallback = 0;
                if (event) {
                    rv = event->OnOutputStreamReady(nsnull);
                    if (NS_FAILED(rv)) {
                        NS_NOTREACHED("leaking stream event");
                        nsISupports *sup = event;
                        NS_ADDREF(sup);
                    }
                }
            }
        }
    }

public:
    void Init(nsIOutputStreamCallback *callback, nsIEventTarget *target)
    {
        mCallback = callback;
        mTarget = target;

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);
    }

    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *stream)
    {
        mStream = stream;

        // this will be released when the event is handled
        NS_ADDREF_THIS();

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);

        if (NS_FAILED(mTarget->PostEvent(this))) {
            NS_WARNING("PostEvent failed");
            NS_RELEASE_THIS();
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncOutputStream>    mStream;
    nsCOMPtr<nsIOutputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget>          mTarget;

    PR_STATIC_CALLBACK(void *) EventHandler(PLEvent *plevent)
    {
        nsOutputStreamReadyEvent *ev = (nsOutputStreamReadyEvent *) plevent;
        if (ev->mCallback)
            ev->mCallback->OnOutputStreamReady(ev->mStream);
        ev->mCallback = 0;
        return NULL;
    }

    PR_STATIC_CALLBACK(void) EventCleanup(PLEvent *ev)
    {
        nsOutputStreamReadyEvent *event = (nsOutputStreamReadyEvent *) ev;
        NS_RELEASE(event);
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOutputStreamReadyEvent,
                              nsIOutputStreamCallback)

//-----------------------------------------------------------------------------

NS_COM nsresult
NS_NewInputStreamReadyEvent(nsIInputStreamCallback **event,
                            nsIInputStreamCallback *callback,
                            nsIEventTarget *target)
{
    nsInputStreamReadyEvent *ev = new nsInputStreamReadyEvent(callback, target);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*event = ev);
    return NS_OK;
}

NS_COM nsresult
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback **event,
                             nsIOutputStreamCallback *callback,
                             nsIEventTarget *target)
{
    nsOutputStreamReadyEvent *ev = new nsOutputStreamReadyEvent(callback, target);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*event = ev);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// NS_AsyncCopy implementation

// abstract stream copier...
class nsAStreamCopier : public nsIInputStreamCallback
                      , public nsIOutputStreamCallback
{
public:
    NS_DECL_ISUPPORTS

    nsAStreamCopier()
        : mLock(nsnull)
        , mCallback(nsnull)
        , mClosure(nsnull)
        , mChunkSize(0)
        , mEventInProcess(PR_FALSE)
        , mEventIsPending(PR_FALSE)
    {
    }

    // virtual since subclasses call superclass Release()
    virtual ~nsAStreamCopier()
    {
        if (mLock)
            PR_DestroyLock(mLock);
    }

    // kick off the async copy...
    nsresult Start(nsIInputStream *source,
                   nsIOutputStream *sink,
                   nsIEventTarget *target,
                   nsAsyncCopyCallbackFun callback,
                   void *closure,
                   PRUint32 chunksize)
    {
        mSource = source;
        mSink = sink;
        mTarget = target;
        mCallback = callback;
        mClosure = closure;
        mChunkSize = chunksize;

        mLock = PR_NewLock();
        if (!mLock)
            return NS_ERROR_OUT_OF_MEMORY;

        mAsyncSource = do_QueryInterface(mSource);
        mAsyncSink = do_QueryInterface(mSink);

        return PostContinuationEvent();
    }

    // implemented by subclasses, returns number of bytes copied and
    // sets source and sink condition before returning.
    virtual PRUint32 DoCopy(nsresult *sourceCondition, nsresult *sinkCondition) = 0;

    void Process()
    {
        if (!mSource || !mSink)
            return;

        nsresult sourceCondition, sinkCondition;

        // ok, copy data from source to sink.
        for (;;) {
            PRUint32 n = DoCopy(&sourceCondition, &sinkCondition);
            if (NS_FAILED(sourceCondition) || NS_FAILED(sinkCondition) || n == 0) {
                if (sourceCondition == NS_BASE_STREAM_WOULD_BLOCK && mAsyncSource) {
                    // need to wait for more data from source.  while waiting for
                    // more source data, be sure to observe failures on output end.
                    mAsyncSource->AsyncWait(this, 0, 0, nsnull);

                    if (mAsyncSink)
                        mAsyncSink->AsyncWait(this,
                                              nsIAsyncOutputStream::WAIT_CLOSURE_ONLY,
                                              0, nsnull);
                }
                else if (sinkCondition == NS_BASE_STREAM_WOULD_BLOCK && mAsyncSink) {
                    // need to wait for more room in the sink.  while waiting for
                    // more room in the sink, be sure to observer failures on the
                    // input end.
                    mAsyncSink->AsyncWait(this, 0, 0, nsnull);

                    if (mAsyncSource)
                        mAsyncSource->AsyncWait(this,
                                                nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                                0, nsnull);
                }
                else {
                    // close source
                    if (mAsyncSource)
                        mAsyncSource->CloseWithStatus(sinkCondition);
                    else
                        mSource->Close();
                    mAsyncSource = nsnull;
                    mSource = nsnull;

                    // close sink
                    if (mAsyncSink)
                        mAsyncSink->CloseWithStatus(sourceCondition);
                    else
                        mSink->Close();
                    mAsyncSink = nsnull;
                    mSink = nsnull;

                    // notify state complete...
                    if (mCallback) {
                        nsresult status = sourceCondition;
                        if (NS_SUCCEEDED(status))
                            status = sinkCondition;
                        if (status == NS_BASE_STREAM_CLOSED)
                            status = NS_OK;
                        mCallback(mClosure, status);
                    }
                }
                break;
            }
        }
    }

    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *source)
    {
        PostContinuationEvent();
        return NS_OK;
    }

    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *sink)
    {
        PostContinuationEvent();
        return NS_OK;
    }

    PR_STATIC_CALLBACK(void*) HandleContinuationEvent(PLEvent *event)
    {
        nsAStreamCopier *self = (nsAStreamCopier *) event->owner;
        self->Process();

        // clear "in process" flag and post any pending continuation event
        nsAutoLock lock(self->mLock);
        self->mEventInProcess = PR_FALSE;
        if (self->mEventIsPending) {
            self->mEventIsPending = PR_FALSE;
            self->PostContinuationEvent_Locked();
        }
        return nsnull;
    }

    PR_STATIC_CALLBACK(void) DestroyContinuationEvent(PLEvent *event)
    {
        nsAStreamCopier *self = (nsAStreamCopier *) event->owner;
        NS_RELEASE(self);
        delete event;
    }

    nsresult PostContinuationEvent()
    {
        // we cannot post a continuation event if there is currently
        // an event in process.  doing so could result in Process being
        // run simultaneously on multiple threads, so we mark the event
        // as pending, and if an event is already in process then we 
        // just let that existing event take care of posting the real
        // continuation event.

        nsAutoLock lock(mLock);
        return PostContinuationEvent_Locked();
    }

    nsresult PostContinuationEvent_Locked()
    {
        nsresult rv = NS_OK;
        if (mEventInProcess)
            mEventIsPending = PR_TRUE;
        else {
            PLEvent *event = new PLEvent;
            if (!event)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else {
                NS_ADDREF_THIS();
                PL_InitEvent(event, this,
                             HandleContinuationEvent,
                             DestroyContinuationEvent);

                rv = mTarget->PostEvent(event);
                if (NS_SUCCEEDED(rv))
                    mEventInProcess = PR_TRUE;
                else {
                    NS_ERROR("unable to post continuation event");
                    PL_DestroyEvent(event);
                }
            }
        }
        return rv;
    }

protected:
    nsCOMPtr<nsIInputStream>       mSource;
    nsCOMPtr<nsIOutputStream>      mSink;
    nsCOMPtr<nsIAsyncInputStream>  mAsyncSource;
    nsCOMPtr<nsIAsyncOutputStream> mAsyncSink;
    nsCOMPtr<nsIEventTarget>       mTarget;
    PRLock                        *mLock;
    nsAsyncCopyCallbackFun         mCallback;
    void                          *mClosure;
    PRUint32                       mChunkSize;
    PRPackedBool                   mEventInProcess;
    PRPackedBool                   mEventIsPending;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAStreamCopier,
                              nsIInputStreamCallback,
                              nsIOutputStreamCallback)

class nsStreamCopierIB : public nsAStreamCopier
{
public:
    nsStreamCopierIB() : nsAStreamCopier() {}
    virtual ~nsStreamCopierIB() {}

    struct ReadSegmentsState {
        nsIOutputStream *mSink;
        nsresult         mSinkCondition;
    };

    static NS_METHOD ConsumeInputBuffer(nsIInputStream *inStr,
                                        void *closure,
                                        const char *buffer,
                                        PRUint32 offset,
                                        PRUint32 count,
                                        PRUint32 *countWritten)
    {
        ReadSegmentsState *state = (ReadSegmentsState *) closure;

        nsresult rv = state->mSink->Write(buffer, count, countWritten);
        if (NS_FAILED(rv))
            state->mSinkCondition = rv;
        else if (*countWritten == 0)
            state->mSinkCondition = NS_BASE_STREAM_CLOSED;

        return state->mSinkCondition;
    }

    PRUint32 DoCopy(nsresult *sourceCondition, nsresult *sinkCondition)
    {
        ReadSegmentsState state;
        state.mSink = mSink;
        state.mSinkCondition = NS_OK;

        PRUint32 n;
        *sourceCondition =
            mSource->ReadSegments(ConsumeInputBuffer, &state, mChunkSize, &n);
        *sinkCondition = state.mSinkCondition;
        return n;
    }
};

class nsStreamCopierOB : public nsAStreamCopier
{
public:
    nsStreamCopierOB() : nsAStreamCopier() {}
    virtual ~nsStreamCopierOB() {}

    struct WriteSegmentsState {
        nsIInputStream *mSource;
        nsresult        mSourceCondition;
    };

    static NS_METHOD FillOutputBuffer(nsIOutputStream *outStr,
                                      void *closure,
                                      char *buffer,
                                      PRUint32 offset,
                                      PRUint32 count,
                                      PRUint32 *countRead)
    {
        WriteSegmentsState *state = (WriteSegmentsState *) closure;

        nsresult rv = state->mSource->Read(buffer, count, countRead);
        if (NS_FAILED(rv))
            state->mSourceCondition = rv;
        else if (*countRead == 0)
            state->mSourceCondition = NS_BASE_STREAM_CLOSED;

        return state->mSourceCondition;
    }

    PRUint32 DoCopy(nsresult *sourceCondition, nsresult *sinkCondition)
    {
        WriteSegmentsState state;
        state.mSource = mSource;
        state.mSourceCondition = NS_OK;

        PRUint32 n;
        *sinkCondition =
            mSink->WriteSegments(FillOutputBuffer, &state, mChunkSize, &n);
        *sourceCondition = state.mSourceCondition;
        return n;
    }
};

//-----------------------------------------------------------------------------

NS_COM nsresult
NS_AsyncCopy(nsIInputStream         *source,
             nsIOutputStream        *sink,
             nsIEventTarget         *target,
             nsAsyncCopyMode         mode,
             PRUint32                chunkSize,
             nsAsyncCopyCallbackFun  callback,
             void                   *closure)
{
    NS_ASSERTION(target, "non-null target required");

    nsresult rv;
    nsAStreamCopier *copier;

    if (mode == NS_ASYNCCOPY_VIA_READSEGMENTS)
        copier = new nsStreamCopierIB();
    else
        copier = new nsStreamCopierOB();

    if (!copier)
        return NS_ERROR_OUT_OF_MEMORY;

    // Start() takes an owning ref to the copier...
    NS_ADDREF(copier);
    rv = copier->Start(source, sink, target, callback, closure, chunkSize);
    NS_RELEASE(copier);

    return rv;
}
