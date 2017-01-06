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
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

/* package */ final class Codec extends ICodec.Stub implements IBinder.DeathRecipient {
    private static final String LOGTAG = "GeckoRemoteCodec";
    private static final boolean DEBUG = false;

    public enum Error {
        DECODE, FATAL
    }

    private final class Callbacks implements AsyncCodec.Callbacks {
        @Override
        public void onInputBufferAvailable(AsyncCodec codec, int index) {
            mInputProcessor.onBuffer(index);
        }

        @Override
        public void onOutputBufferAvailable(AsyncCodec codec, int index, MediaCodec.BufferInfo info) {
            mOutputProcessor.onBuffer(index, info);
        }

        @Override
        public void onError(AsyncCodec codec, int error) {
            reportError(Error.FATAL, new Exception("codec error:" + error));
        }

        @Override
        public void onOutputFormatChanged(AsyncCodec codec, MediaFormat format) {
            mOutputProcessor.onFormatChanged(format);
        }
    }

    private final class InputProcessor {
        private boolean mHasInputCapacitySet;
        private Queue<Integer> mAvailableInputBuffers = new LinkedList<>();
        private Queue<Sample> mDequeuedSamples = new LinkedList<>();
        private Queue<Sample> mInputSamples = new LinkedList<>();
        private boolean mStopped;

        private synchronized Sample onAllocate(int size) {
            Sample sample = mSamplePool.obtainInput(size);
            mDequeuedSamples.add(sample);
            return sample;
        }

        private synchronized void onSample(Sample sample) {
            if (sample == null) {
                Log.w(LOGTAG, "WARN: null input sample");
                return;
            }

            if (!sample.isEOS()) {
                Sample temp = sample;
                sample = mDequeuedSamples.remove();
                sample.info = temp.info;
                sample.cryptoInfo = temp.cryptoInfo;
                temp.dispose();
            }

            if (mInputSamples.offer(sample)) {
                feedSampleToBuffer();
            } else {
                reportError(Error.FATAL, new Exception("FAIL: input sample queue is full"));
            }
        }

        private synchronized void onBuffer(int index) {
            if (mStopped) {
                return;
            }

            if (!mHasInputCapacitySet) {
                int capacity = mCodec.getInputBuffer(index).capacity();
                if (capacity > 0) {
                    mSamplePool.setInputBufferSize(capacity);
                    mHasInputCapacitySet = true;
                }
            }

            if (mAvailableInputBuffers.offer(index)) {
                feedSampleToBuffer();
            } else {
                reportError(Error.FATAL, new Exception("FAIL: input buffer queue is full"));
            }

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
            for (Sample s : mInputSamples) {
                if (!s.isEOS()) {
                    mSamplePool.recycleInput(s);
                }
            }
            mInputSamples.clear();

            for (Sample s : mDequeuedSamples) {
                mSamplePool.recycleInput(s);
            }
            mDequeuedSamples.clear();

            mAvailableInputBuffers.clear();
        }

        private synchronized void start() {
            if (!mStopped) {
                return;
            }
            mStopped = false;
        }

        private synchronized void stop() {
            if (mStopped) {
                return;
            }
            mStopped = true;
            reset();
        }
    }

    private class OutputProcessor {
        private boolean mHasOutputCapacitySet;
        private Queue<Integer> mSentIndices = new LinkedList<>();
        private Queue<Sample> mSentOutputs = new LinkedList<>();
        private boolean mStopped;

        private synchronized void onBuffer(int index, MediaCodec.BufferInfo info) {
            if (mStopped) {
                return;
            }

            ByteBuffer output = mCodec.getOutputBuffer(index);
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
                mSentIndices.add(index);
                mSentOutputs.add(copy);
                mCallbacks.onOutput(copy);
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

            boolean eos = (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            if (DEBUG && eos) {
                Log.d(LOGTAG, "output EOS");
            }
       }

        private void outputDummy(MediaCodec.BufferInfo info) {
            try {
                if (DEBUG) { Log.d(LOGTAG, "return dummy sample"); }
                mCallbacks.onOutput(Sample.create(null, info, null));
            } catch (RemoteException e) {
                // Dead recipient.
                e.printStackTrace();
            }
        }

        private synchronized void onRelease(Sample sample, boolean render) {
            Integer i = mSentIndices.poll();
            Sample output = mSentOutputs.poll();
            if (i == null || output == null) {
                Log.d(LOGTAG, "output buffer#" + i + "(" + output + ")" + ": " + sample + " already released");
                return;
            }
            mCodec.releaseOutputBuffer(i, render);
            mSamplePool.recycleOutput(output);

            sample.dispose();
        }

        private void onFormatChanged(MediaFormat format) {
            try {
                mCallbacks.onOutputFormatChanged(new FormatParam(format));
            } catch (RemoteException re) {
                // Dead recipient.
                re.printStackTrace();
            }
        }

        private synchronized void reset() {
            for (int i : mSentIndices) {
                mCodec.releaseOutputBuffer(i, false);
            }
            mSentIndices.clear();
            for (Sample s : mSentOutputs) {
                mSamplePool.recycleOutput(s);
            }
            mSentOutputs.clear();
        }

        private synchronized void start() {
            if (!mStopped) {
                return;
            }
            mStopped = false;
        }

        private synchronized void stop() {
            if (mStopped) {
                return;
            }
            mStopped = true;
            reset();
        }
    }

    private volatile ICodecCallbacks mCallbacks;
    private AsyncCodec mCodec;
    private InputProcessor mInputProcessor;
    private OutputProcessor mOutputProcessor;
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
            if (DEBUG) { Log.d(LOGTAG, "release existing codec: " + mCodec); }
            releaseCodec();
        }

        if (DEBUG) { Log.d(LOGTAG, "configure " + this); }

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

            codec.setCallbacks(new Callbacks(), null);

            // Video decoder should config with adaptive playback capability.
            if (surface != null) {
                mIsAdaptivePlaybackSupported = codec.isAdaptivePlaybackSupported(
                                                   fmt.getString(MediaFormat.KEY_MIME));
                if (mIsAdaptivePlaybackSupported) {
                    if (DEBUG) { Log.d(LOGTAG, "codec supports adaptive playback  = " + mIsAdaptivePlaybackSupported); }
                    // TODO: may need to find a way to not use hard code to decide the max w/h.
                    fmt.setInteger(MediaFormat.KEY_MAX_WIDTH, 1920);
                    fmt.setInteger(MediaFormat.KEY_MAX_HEIGHT, 1080);
                }
            }

            codec.configure(fmt, surface, crypto, flags);
            mCodec = codec;
            mInputProcessor = new InputProcessor();
            mOutputProcessor = new OutputProcessor();
            mSamplePool = new SamplePool(codecName);
            if (DEBUG) { Log.d(LOGTAG, codec.toString() + " created"); }
            return true;
        } catch (Exception e) {
            if (DEBUG) { Log.d(LOGTAG, "FAIL: cannot create codec -- " + codecName); }
            e.printStackTrace();
            return false;
        }
    }

    @Override
    public synchronized boolean isAdaptivePlaybackSupported() {
        return mIsAdaptivePlaybackSupported;
    }

    private void releaseCodec() {
        // In case Codec.stop() is not called yet.
        mInputProcessor.stop();
        mOutputProcessor.stop();
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
        if (DEBUG) { Log.d(LOGTAG, "start " + this); }
        mInputProcessor.start();
        mOutputProcessor.start();
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
        if (DEBUG) { Log.d(LOGTAG, "stop " + this); }
        mInputProcessor.stop();
        mOutputProcessor.stop();
        try {
            mCodec.stop();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized void flush() throws RemoteException {
        if (DEBUG) { Log.d(LOGTAG, "flush " + this); }
        mInputProcessor.stop();
        mOutputProcessor.stop();
        try {
            mCodec.flush();
            if (DEBUG) { Log.d(LOGTAG, "flushed " + this); }
            mInputProcessor.start();
            mOutputProcessor.start();
            mCodec.resumeReceivingInputs();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized Sample dequeueInput(int size) {
        return mInputProcessor.onAllocate(size);
    }

    @Override
    public synchronized void queueInput(Sample sample) throws RemoteException {
        mInputProcessor.onSample(sample);
    }

    @Override
    public synchronized void releaseOutput(Sample sample, boolean render) {
        mOutputProcessor.onRelease(sample, render);
    }

    @Override
    public synchronized void release() throws RemoteException {
        if (DEBUG) { Log.d(LOGTAG, "release " + this); }
        releaseCodec();
        mSamplePool.reset();
        mSamplePool = null;
        mCallbacks.asBinder().unlinkToDeath(this, 0);
        mCallbacks = null;
    }
}
