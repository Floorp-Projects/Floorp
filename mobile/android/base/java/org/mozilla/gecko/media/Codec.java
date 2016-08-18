/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.TransactionTooLargeException;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.Queue;

/* package */ final class Codec extends ICodec.Stub implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteCodec";
    private static final boolean DEBUG = false;

    public enum Error {
        DECODE, FATAL
    };

    private final class Callbacks implements AsyncCodec.Callbacks {
        private ICodecCallbacks mRemote;

        public Callbacks(ICodecCallbacks remote) {
            mRemote = remote;
        }

        @Override
        public void onInputBufferAvailable(AsyncCodec codec, int index) {
            if (mFlushing) {
                // Flush invalidates all buffers.
                return;
            }
            if (!mInputProcessor.onBuffer(index)) {
                reportError(Error.FATAL, new Exception("FAIL: input buffer queue is full"));
            }
        }

        @Override
        public void onOutputBufferAvailable(AsyncCodec codec, int index, MediaCodec.BufferInfo info) {
            if (mFlushing) {
                // Flush invalidates all buffers.
                return;
            }
            ByteBuffer buffer = codec.getOutputBuffer(index);
            try {
                mRemote.onOutput(new Sample(buffer, info));
            } catch (TransactionTooLargeException ttle) {
                Log.e(LOGTAG, "Output is too large:" + ttle.getMessage());
                outputDummy(info);
            } catch (RemoteException e) {
                // Dead recipient.
                e.printStackTrace();
            }
            mCodec.releaseOutputBuffer(index, true);
            boolean eos = (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            if (DEBUG && eos) {
                Log.d(LOGTAG, "output EOS");
            }
        }

        private void outputDummy(MediaCodec.BufferInfo info) {
            try {
                if (DEBUG) Log.d(LOGTAG, "return dummy sample");
                mRemote.onOutput(Sample.createDummyWithInfo(info));
            } catch (RemoteException e) {
                // Dead recipient.
                e.printStackTrace();
            }
        }

        @Override
        public void onError(AsyncCodec codec, int error) {
            reportError(Error.FATAL, new Exception("codec error:" + error));
        }

        @Override
        public void onOutputFormatChanged(AsyncCodec codec, MediaFormat format) {
            try {
                mRemote.onOutputFormatChanged(new FormatParam(format));
            } catch (RemoteException re) {
                // Dead recipient.
                re.printStackTrace();
            }
        }
    }

    private final class InputProcessor {
        private Queue<Sample> mInputSamples = new LinkedList<Sample>();
        private Queue<Integer> mAvailableInputBuffers = new LinkedList<Integer>();

        private synchronized boolean onSample(Sample sample) {
            if (!mInputSamples.offer(sample)) {
                return false;
            }
            feedSampleToBuffer();
            return true;
        }

        private synchronized boolean onBuffer(int index) {
            if (!mAvailableInputBuffers.offer(index)) {
                return false;
            }
            feedSampleToBuffer();
            return true;
        }

        private void feedSampleToBuffer() {
            while (!mAvailableInputBuffers.isEmpty() && !mInputSamples.isEmpty()) {
                int index = mAvailableInputBuffers.poll();
                Sample sample = mInputSamples.poll();
                int len = 0;
                if (!sample.isEOS() && sample.bytes != null) {
                    len = sample.info.size;
                    ByteBuffer buf = mCodec.getInputBuffer(index);
                    buf.put(sample.bytes);
                    try {
                        mCallbacks.onInputExhausted();
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                }
                mCodec.queueInputBuffer(index, 0, len, sample.info.presentationTimeUs, sample.info.flags);
            }
        }

        private synchronized void reset() {
            mInputSamples.clear();
            mAvailableInputBuffers.clear();
        }
   }
    private volatile ICodecCallbacks mCallbacks;
    private AsyncCodec mCodec;
    private InputProcessor mInputProcessor;
    private volatile boolean mFlushing = false;

    public synchronized void setCallbacks(ICodecCallbacks callbacks) throws RemoteException {
        mCallbacks = callbacks;
        callbacks.asBinder().linkToDeath(this, 0);
    }

    // IBinder.DeathRecipient
    @Override
    public synchronized void binderDied() {
        Log.e(LOGTAG, "Callbacks is dead");
        try {
            release();
        } catch (RemoteException e) {
            // Nowhere to report the error.
        }
    }

    @Override
    public synchronized boolean configure(FormatParam format, Surface surface, int flags) throws RemoteException {
        if (mCallbacks == null) {
            Log.e(LOGTAG, "FAIL: callbacks must be set before calling configure()");
            return false;
        }

        if (mCodec != null) {
            if (DEBUG) Log.d(LOGTAG, "release existing codec: " + mCodec);
            releaseCodec();
        }

        if (DEBUG) Log.d(LOGTAG, "configure " + this);

        MediaFormat fmt = format.asFormat();
        String codecName = getDecoderForFormat(fmt);
        if (codecName == null) {
            Log.e(LOGTAG, "FAIL: cannot find codec");
            return false;
        }

        try {
            AsyncCodec codec = AsyncCodecFactory.create(codecName);
            codec.setCallbacks(new Callbacks(mCallbacks), null);
            codec.configure(fmt, surface, flags);
            mCodec = codec;
            mInputProcessor = new InputProcessor();
            if (DEBUG) Log.d(LOGTAG, codec.toString() + " created");
            return true;
        } catch (Exception e) {
            if (DEBUG) Log.d(LOGTAG, "FAIL: cannot create codec -- " + codecName);
            e.printStackTrace();
            return false;
        }
    }

    private void releaseCodec() {
        mInputProcessor.reset();
        try {
            mCodec.release();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
        mCodec = null;
    }

    private String getDecoderForFormat(MediaFormat format) {
        String mime = format.getString(MediaFormat.KEY_MIME);
        if (mime == null) {
            return null;
        }
        int numCodecs = MediaCodecList.getCodecCount();
        for (int i = 0; i < numCodecs; i++) {
            MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            if (info.isEncoder()) {
                continue;
            }
            String[] types = info.getSupportedTypes();
            for (String t : types) {
                if (t.equalsIgnoreCase(mime)) {
                    return info.getName();
                }
            }
        }
        return null;
        // TODO: API 21+ is simpler.
        //static MediaCodecList sCodecList = new MediaCodecList(MediaCodecList.ALL_CODECS);
        //return sCodecList.findDecoderForFormat(format);
    }

    @Override
    public synchronized void start() throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "start " + this);
        mFlushing = false;
        try {
            mCodec.start();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    private void reportError(Error error, Exception e) {
        if (e != null) {
            e.printStackTrace();
        }
        try {
            mCallbacks.onError(error == Error.FATAL);
        } catch (RemoteException re) {
            re.printStackTrace();
        }
    }

    @Override
    public synchronized void stop() throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "stop " + this);
        try {
            mCodec.stop();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized void flush() throws RemoteException {
        mFlushing = true;
        if (DEBUG) Log.d(LOGTAG, "flush " + this);
        mInputProcessor.reset();
        try {
            mCodec.flush();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }

        mFlushing = false;
        if (DEBUG) Log.d(LOGTAG, "flushed " + this);
    }

    @Override
    public synchronized void queueInput(Sample sample) throws RemoteException {
        if (!mInputProcessor.onSample(sample)) {
            reportError(Error.FATAL, new Exception("FAIL: input sample queue is full"));
        }
    }

    @Override
    public synchronized void release() throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "release " + this);
        releaseCodec();
        mCallbacks.asBinder().unlinkToDeath(this, 0);
        mCallbacks = null;
    }
}
