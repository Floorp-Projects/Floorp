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
#include "nsIEventQueue.h"

//-----------------------------------------------------------------------------

class nsInputStreamReadyEvent : public PLEvent
                              , public nsIInputStreamNotify
{
public:
    NS_DECL_ISUPPORTS

    nsInputStreamReadyEvent(nsIInputStreamNotify *notify,
                            nsIEventQueue *eventQ)
        : mNotify(notify)
        , mEventQ(eventQ)
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsInputStreamReadyEvent()
    {
        if (mNotify) {
            //
            // whoa!! looks like we never posted this event. take care not to
            // delete mNotify on the calling thread (which might be the wrong
            // thread).
            //
            nsCOMPtr<nsIInputStreamNotify> event;
            NS_NewInputStreamReadyEvent(getter_AddRefs(event), mNotify, mEventQ);
            mNotify = 0;
            if (event) {
                nsresult rv = event->OnInputStreamReady(nsnull);
                if (NS_FAILED(rv)) {
                    // PostEvent failed, we must be shutting down.  better to
                    // leak than crash!
                    NS_NOTREACHED("leaking stream event");
                    nsISupports *sup = event;
                    NS_ADDREF(sup);
                }
            }
        }
    }

    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *stream)
    {
        mStream = stream;

        // will be released when event is handled
        NS_ADDREF_THIS();

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);

        if (mEventQ->PostEvent(this) == PR_FAILURE) {
            NS_WARNING("PostEvent failed");
            NS_RELEASE_THIS();
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>  mStream;
    nsCOMPtr<nsIInputStreamNotify> mNotify;
    nsCOMPtr<nsIEventQueue>        mEventQ;

    static void *PR_CALLBACK EventHandler(PLEvent *plevent)
    {
        nsInputStreamReadyEvent *ev = (nsInputStreamReadyEvent *) plevent;
        // bypass event delivery if this is a cleanup event...
        if (ev->mStream)
            ev->mNotify->OnInputStreamReady(ev->mStream);
        ev->mNotify = 0;
        return NULL;
    }

    static void PR_CALLBACK EventCleanup(PLEvent *plevent)
    {
        nsInputStreamReadyEvent *ev = (nsInputStreamReadyEvent *) plevent;
        NS_RELEASE(ev);
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsInputStreamReadyEvent,
                              nsIInputStreamNotify)

//-----------------------------------------------------------------------------

class nsOutputStreamReadyEvent : public PLEvent
                               , public nsIOutputStreamNotify
{
public:
    NS_DECL_ISUPPORTS

    nsOutputStreamReadyEvent(nsIOutputStreamNotify *notify,
                             nsIEventQueue *eventQ)
        : mNotify(notify)
        , mEventQ(eventQ)
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsOutputStreamReadyEvent()
    {
        if (mNotify) {
            //
            // whoa!! looks like we never posted this event. take care not to
            // delete mNotify on the calling thread (which might be the wrong
            // thread).
            //
            nsCOMPtr<nsIOutputStreamNotify> event;
            NS_NewOutputStreamReadyEvent(getter_AddRefs(event), mNotify, mEventQ);
            mNotify = 0;
            if (event) {
                nsresult rv = event->OnOutputStreamReady(nsnull);
                if (NS_FAILED(rv)) {
                    // PostEvent failed, we must be shutting down.  better to
                    // leak than crash!
                    NS_NOTREACHED("leaking stream event");
                    nsISupports *sup = event;
                    NS_ADDREF(sup);
                }
            }
        }
    }

    void Init(nsIOutputStreamNotify *notify, nsIEventQueue *eventQ)
    {
        mNotify = notify;
        mEventQ = eventQ;

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);
    }

    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *stream)
    {
        mStream = stream;

        // this will be released when the event is handled
        NS_ADDREF_THIS();

        PL_InitEvent(this, nsnull, EventHandler, EventCleanup);

        if (mEventQ->PostEvent(this) == PR_FAILURE) {
            NS_WARNING("PostEvent failed");
            NS_RELEASE_THIS();
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncOutputStream>  mStream;
    nsCOMPtr<nsIOutputStreamNotify> mNotify;
    nsCOMPtr<nsIEventQueue>         mEventQ;

    static void *PR_CALLBACK EventHandler(PLEvent *plevent)
    {
        nsOutputStreamReadyEvent *ev = (nsOutputStreamReadyEvent *) plevent;
        if (ev->mNotify)
            ev->mNotify->OnOutputStreamReady(ev->mStream);
        ev->mNotify = 0;
        return NULL;
    }

    static void PR_CALLBACK EventCleanup(PLEvent *ev)
    {
        nsOutputStreamReadyEvent *event = (nsOutputStreamReadyEvent *) ev;
        NS_RELEASE(event);
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOutputStreamReadyEvent,
                              nsIOutputStreamNotify)

//-----------------------------------------------------------------------------

NS_COM nsresult
NS_NewInputStreamReadyEvent(nsIInputStreamNotify **event,
                            nsIInputStreamNotify *notify,
                            nsIEventQueue *eventQ)
{
    nsInputStreamReadyEvent *ev = new nsInputStreamReadyEvent(notify, eventQ);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*event = ev);
    return NS_OK;
}

NS_COM nsresult
NS_NewOutputStreamReadyEvent(nsIOutputStreamNotify **event,
                             nsIOutputStreamNotify *notify,
                             nsIEventQueue *eventQ)
{
    nsOutputStreamReadyEvent *ev = new nsOutputStreamReadyEvent(notify, eventQ);
    if (!ev)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*event = ev);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// NS_AsyncCopy implementation

// this stream copier assumes the input stream is buffered (ReadSegments OK)
class nsStreamCopierIB : public nsIInputStreamNotify
                       , public nsIOutputStreamNotify
{
public:
    NS_DECL_ISUPPORTS

    nsStreamCopierIB(nsIAsyncInputStream *in,
                     nsIAsyncOutputStream *out,
                     PRUint32 chunksize)
        : mSource(in)
        , mSink(out)
        , mChunkSize(chunksize)
        { NS_INIT_ISUPPORTS(); }
    virtual ~nsStreamCopierIB() {}

    static NS_METHOD ConsumeInputBuffer(nsIInputStream *inStr,
                                        void *closure,
                                        const char *buffer,
                                        PRUint32 offset,
                                        PRUint32 count,
                                        PRUint32 *countWritten)
    {
        nsStreamCopierIB *self = (nsStreamCopierIB *) closure;

        nsresult rv = self->mSink->Write(buffer, count, countWritten);
        if (NS_FAILED(rv))
            self->mSinkCondition = rv;
        else if (*countWritten == 0)
            self->mSinkCondition = NS_BASE_STREAM_CLOSED;

        return self->mSinkCondition;
    }

    // called on some random thread
    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *in)
    {
        NS_ASSERTION(in == mSource, "unexpected stream");
        // Do all of our work from OnOutputStreamReady.  This
        // way we're more likely to always be working on the
        // same thread.
        return mSink->AsyncWait(this, 0, nsnull);
    }

    // called on some random thread
    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *out)
    {
        NS_ASSERTION(out == mSink, "unexpected stream");
        for (;;) {
            mSinkCondition = NS_OK; // reset
            PRUint32 n;
            nsresult rv = mSource->ReadSegments(ConsumeInputBuffer, this, mChunkSize, &n);
            if (NS_FAILED(rv) || (n == 0)) {
                if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                    mSource->AsyncWait(this, 0, nsnull);
                else if (mSinkCondition == NS_BASE_STREAM_WOULD_BLOCK)
                    mSink->AsyncWait(this, 0, nsnull);
                else {
                    mSink = 0;
                    mSource->CloseEx(mSinkCondition);
                    mSource = 0;
                }
                break;
            }
        }
        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>  mSource;
    nsCOMPtr<nsIAsyncOutputStream> mSink;
    PRUint32                       mChunkSize;
    nsresult                       mSinkCondition;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStreamCopierIB,
                              nsIInputStreamNotify,
                              nsIOutputStreamNotify)

// this stream copier assumes the output stream is buffered (WriteSegments OK)
class nsStreamCopierOB : public nsIInputStreamNotify
                       , public nsIOutputStreamNotify
{
public:
    NS_DECL_ISUPPORTS

    nsStreamCopierOB(nsIAsyncInputStream *in,
                     nsIAsyncOutputStream *out,
                     PRUint32 chunksize)
        : mSource(in)
        , mSink(out)
        , mChunkSize(chunksize)
        { NS_INIT_ISUPPORTS(); }
    virtual ~nsStreamCopierOB() {}

    static NS_METHOD FillOutputBuffer(nsIOutputStream *outStr,
                                      void *closure,
                                      char *buffer,
                                      PRUint32 offset,
                                      PRUint32 count,
                                      PRUint32 *countRead)
    {
        nsStreamCopierOB *self = (nsStreamCopierOB *) closure;

        nsresult rv = self->mSource->Read(buffer, count, countRead);
        if (NS_FAILED(rv))
            self->mSourceCondition = rv;
        else if (*countRead == 0)
            self->mSourceCondition = NS_BASE_STREAM_CLOSED;

        return self->mSourceCondition;
    }

    // called on some random thread
    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *in)
    {
        NS_ASSERTION(in == mSource, "unexpected stream");
        for (;;) {
            mSourceCondition = NS_OK; // reset
            PRUint32 n;
            nsresult rv = mSink->WriteSegments(FillOutputBuffer, this, mChunkSize, &n);
            if (NS_FAILED(rv) || (n == 0)) {
                if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                    mSink->AsyncWait(this, 0, nsnull);
                else if (mSourceCondition == NS_BASE_STREAM_WOULD_BLOCK)
                    mSource->AsyncWait(this, 0, nsnull);
                else {
                    mSource = 0;
                    mSink->CloseEx(mSourceCondition);
                    mSink = 0;
                }
                break;
            }
        }
        return NS_OK;
    }

    // called on some random thread
    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *out)
    {
        NS_ASSERTION(out == mSink, "unexpected stream");
        // Do all of our work from OnInputStreamReady.  This
        // way we're more likely to always be working on the
        // same thread.
        return mSource->AsyncWait(this, 0, nsnull);
    }

private:
    nsCOMPtr<nsIAsyncInputStream>  mSource;
    nsCOMPtr<nsIAsyncOutputStream> mSink;
    PRUint32                       mChunkSize;
    nsresult                       mSourceCondition;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStreamCopierOB,
                              nsIInputStreamNotify,
                              nsIOutputStreamNotify)

//-----------------------------------------------------------------------------

NS_COM nsresult
NS_AsyncCopy(nsIAsyncInputStream *source,
             nsIAsyncOutputStream *sink,
             PRBool bufferedSource,
             PRBool bufferedSink,
             PRUint32 segmentSize,
             PRUint32 segmentCount,
             nsIMemory *segmentAlloc)
{
    nsresult rv;

    // we need to insert a pipe if both the source and sink are not buffered.
    if (!bufferedSource && !bufferedSink) {
        nsCOMPtr<nsIAsyncInputStream> pipeIn;
        nsCOMPtr<nsIAsyncOutputStream> pipeOut;

        rv = NS_NewPipe2(getter_AddRefs(pipeIn),
                         getter_AddRefs(pipeOut),
                         PR_TRUE, PR_TRUE,
                         segmentSize, segmentCount, segmentAlloc);
        if (NS_FAILED(rv)) return rv;

        //
        // fire off two async copies :-)
        //
        rv = NS_AsyncCopy(source, pipeOut, PR_FALSE, PR_TRUE, segmentSize, 1, segmentAlloc);
        if (NS_FAILED(rv)) return rv;
 
        rv = NS_AsyncCopy(pipeIn, sink, PR_TRUE, PR_FALSE, segmentSize, 1, segmentAlloc);

        // maybe calling NS_AsyncCopy twice is a bad idea!
        NS_ASSERTION(NS_SUCCEEDED(rv), "uh-oh");

        return rv;
    }

    if (bufferedSource) {
        // copy assuming ReadSegments OK
        nsStreamCopierIB *copier = new nsStreamCopierIB(source, sink, segmentSize);
        if (!copier)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(copier);
        // wait on the sink
        rv = sink->AsyncWait(copier, 0, nsnull);
        NS_RELEASE(copier);
    }
    else {
        // copy assuming WriteSegments OK
        nsStreamCopierOB *copier = new nsStreamCopierOB(source, sink, segmentSize);
        if (!copier)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(copier);
        // wait on the source since the sink is buffered and should therefore
        // already have room.
        rv = source->AsyncWait(copier, 0, nsnull);
        NS_RELEASE(copier);
    }

    return rv;
}
