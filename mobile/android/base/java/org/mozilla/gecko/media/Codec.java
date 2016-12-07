/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.TransactionTooLargeException;
import android.util.Log;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.NoSuchElementException;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

/* package */ final class Codec extends ICodec.Stub implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteCodec";
    private static final boolean DEBUG = false;

    public enum Error {
        DECODE, FATAL
    };

    private final class Callbacks implements AsyncCodec.Callbacks {
        private ICodecCallbacks mRemote;
        private boolean mHasInputCapacitySet;
        private boolean mHasOutputCapacitySet;

        public Callbacks(ICodecCallbacks remote) {
            mRemote = remote;
        }

        @Override
        public void onInputBufferAvailable(AsyncCodec codec, int index) {
            if (mFlushing) {
                // Flush invalidates all buffers.
                return;
            }
            if (!mHasInputCapacitySet) {
                int capacity = codec.getInputBuffer(index).capacity();
                if (capacity > 0) {
                    mSamplePool.setInputBufferSize(capacity);
                    mHasInputCapacitySet = true;
                }
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
            ByteBuffer output = codec.getOutputBuffer(index);
            if (!mHasOutputCapacitySet) {
                int capacity = output.capacity();
                if (capacity > 0) {
                    mSamplePool.setOutputBufferSize(capacity);
                    mHasOutputCapacitySet = true;
                }
            }
            Sample copy = mSamplePool.obtainOutput(info);
            try {
                if (info.size > 0) {
                    copy.buffer.readFromByteBuffer(output, info.offset, info.size);
                }
                mSentOutputs.add(copy);
                mRemote.onOutput(copy);
            } catch (IOException e) {
                Log.e(LOGTAG, "Fail to read output buffer:" + e.getMessage());
                outputDummy(info);
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
                mRemote.onOutput(Sample.create(null, info, null));
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
        private Queue<Sample> mInputSamples = new LinkedList<>();
        private Queue<Integer> mAvailableInputBuffers = new LinkedList<>();
        private Queue<Sample> mDequeuedSamples = new LinkedList<>();

        private synchronized Sample onAllocate(int size) {
            Sample sample = mSamplePool.obtainInput(size);
            mDequeuedSamples.add(sample);
            return sample;
        }

        private synchronized boolean onSample(Sample sample) {
            if (sample == null) {
                return false;
            }

            if (!sample.isEOS()) {
                Sample temp = sample;
                sample = mDequeuedSamples.remove();
                sample.info = temp.info;
                sample.cryptoInfo = temp.cryptoInfo;
                temp.dispose();
            }

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
                int len = 0;
                Sample sample = mInputSamples.poll();
                long pts = sample.info.presentationTimeUs;
                int flags = sample.info.flags;
                MediaCodec.CryptoInfo cryptoInfo = sample.cryptoInfo;
                if (!sample.isEOS() && sample.buffer != null) {
                    len = sample.info.size;
                    ByteBuffer buf = mCodec.getInputBuffer(index);
                    try {
                        sample.writeToByteBuffer(buf);
                        mCallbacks.onInputExhausted();
                    } catch (IOException e) {
                        e.printStackTrace();
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                    mSamplePool.recycleInput(sample);
                }

                if (cryptoInfo != null) {
                    mCodec.queueSecureInputBuffer(index, 0, cryptoInfo, pts, flags);
                } else {
                    mCodec.queueInputBuffer(index, 0, len, pts, flags);
                }
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
    private SamplePool mSamplePool;
    private Queue<Sample> mSentOutputs = new ConcurrentLinkedQueue<>();
    // Value will be updated after configure called.
    private volatile boolean mIsAdaptivePlaybackSupported = false;

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
    public synchronized boolean configure(FormatParam format,
                                          Surface surface,
                                          int flags,
                                          String drmStubId) throws RemoteException {
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

            MediaCrypto crypto = RemoteMediaDrmBridgeStub.getMediaCrypto(drmStubId);
            if (DEBUG) {
                boolean hasCrypto = crypto != null;
                Log.d(LOGTAG, "configure mediacodec with crypto(" + hasCrypto + ") / Id :" + drmStubId);
            }

            codec.setCallbacks(new Callbacks(mCallbacks), null);

            // Video decoder should config with adaptive playback capability.
            if (surface != null) {
                mIsAdaptivePlaybackSupported = codec.isAdaptivePlaybackSupported(
                                                   fmt.getString(MediaFormat.KEY_MIME));
                if (mIsAdaptivePlaybackSupported) {
                    if (DEBUG) Log.d(LOGTAG, "codec supports adaptive playback  = " + mIsAdaptivePlaybackSupported);
                    // TODO: may need to find a way to not use hard code to decide the max w/h.
                    fmt.setInteger(MediaFormat.KEY_MAX_WIDTH, 1920);
                    fmt.setInteger(MediaFormat.KEY_MAX_HEIGHT, 1080);
                }
            }

            codec.configure(fmt, surface, crypto, flags);
            mCodec = codec;
            mInputProcessor = new InputProcessor();
            mSamplePool = new SamplePool(codecName);
            if (DEBUG) Log.d(LOGTAG, codec.toString() + " created");
            return true;
        } catch (Exception e) {
            if (DEBUG) Log.d(LOGTAG, "FAIL: cannot create codec -- " + codecName);
            e.printStackTrace();
            return false;
        }
    }

    @Override
    public synchronized boolean isAdaptivePlaybackSupported() {
        return mIsAdaptivePlaybackSupported;
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
    public synchronized Sample dequeueInput(int size) {
        return mInputProcessor.onAllocate(size);
    }

    @Override
    public synchronized void queueInput(Sample sample) throws RemoteException {
        if (!mInputProcessor.onSample(sample)) {
            reportError(Error.FATAL, new Exception("FAIL: input sample queue is full"));
        }
    }

    @Override
    public synchronized void releaseOutput(Sample sample) {
        try {
            mSamplePool.recycleOutput(mSentOutputs.remove());
        } catch (Exception e) {
            Log.e(LOGTAG, "failed to release output:" + sample);
            e.printStackTrace();
        }
        sample.dispose();
    }

    @Override
    public synchronized void release() throws RemoteException {
        if (DEBUG) Log.d(LOGTAG, "release " + this);
        releaseCodec();
        mSamplePool.reset();
        mSamplePool = null;
        mCallbacks.asBinder().unlinkToDeath(this, 0);
        mCallbacks = null;
    }
}
