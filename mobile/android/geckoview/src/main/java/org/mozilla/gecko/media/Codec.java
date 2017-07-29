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
import android.util.Log;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.gfx.GeckoSurface;

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

    private static final class Input {
        public final Sample sample;
        public boolean reported;

        public Input(final Sample sample) {
            this.sample = sample;
        }
    }

    private final class InputProcessor {
        private boolean mHasInputCapacitySet;
        private Queue<Integer> mAvailableInputBuffers = new LinkedList<>();
        private Queue<Sample> mDequeuedSamples = new LinkedList<>();
        private Queue<Input> mInputSamples = new LinkedList<>();
        private boolean mStopped;

        private synchronized Sample onAllocate(int size) {
            Sample sample = mSamplePool.obtainInput(size);
            mDequeuedSamples.add(sample);
            return sample;
        }

        private synchronized void onSample(Sample sample) {
            if (sample == null) {
                // Ignore empty input.
                mSamplePool.recycleInput(mDequeuedSamples.remove());
                Log.w(LOGTAG, "WARN: empty input sample");
                return;
            }

            if (sample.isEOS()) {
                queueSample(sample);
                return;
            }

            Sample dequeued = mDequeuedSamples.remove();
            dequeued.info = sample.info;
            dequeued.cryptoInfo = sample.cryptoInfo;
            queueSample(dequeued);

            sample.dispose();
        }

        private void queueSample(Sample sample) {
            if (!mInputSamples.offer(new Input(sample))) {
                reportError(Error.FATAL, new Exception("FAIL: input sample queue is full"));
                return;
            }

            try {
                feedSampleToBuffer();
            } catch (Exception e) {
                reportError(Error.FATAL, e);
            }
        }

        private synchronized void onBuffer(int index) {
            if (mStopped || !isValidBuffer(index)) {
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

        private boolean isValidBuffer(final int index) {
            try {
                return mCodec.getInputBuffer(index) != null;
            } catch (IllegalStateException e) {
                if (DEBUG) { Log.d(LOGTAG, "invalid input buffer#" + index, e); }
                return false;
            }
        }

        private void feedSampleToBuffer() {
            while (!mAvailableInputBuffers.isEmpty() && !mInputSamples.isEmpty()) {
                int index = mAvailableInputBuffers.poll();
                int len = 0;
                final Sample sample = mInputSamples.poll().sample;
                long pts = sample.info.presentationTimeUs;
                int flags = sample.info.flags;
                MediaCodec.CryptoInfo cryptoInfo = sample.cryptoInfo;
                if (!sample.isEOS() && sample.buffer != null) {
                    len = sample.info.size;
                    ByteBuffer buf = mCodec.getInputBuffer(index);
                    try {
                        sample.writeToByteBuffer(buf);
                    } catch (IOException e) {
                        e.printStackTrace();
                        len = 0;
                    }
                    mSamplePool.recycleInput(sample);
                }

                try {
                    if (cryptoInfo != null && len > 0) {
                        mCodec.queueSecureInputBuffer(index, 0, cryptoInfo, pts, flags);
                    } else {
                        mCodec.queueInputBuffer(index, 0, len, pts, flags);
                    }
                    mCallbacks.onInputQueued(pts);
                } catch (RemoteException e) {
                    e.printStackTrace();
                } catch (Exception e) {
                    reportError(Error.FATAL, e);
                    return;
                }
            }
            reportPendingInputs();
        }

        private void reportPendingInputs() {
            try {
                for (Input i : mInputSamples) {
                    if (!i.reported) {
                        i.reported = true;
                        mCallbacks.onInputPending(i.sample.info.presentationTimeUs);
                    }
                }
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }

        private synchronized void reset() {
            for (Input i : mInputSamples) {
                if (!i.sample.isEOS()) {
                    mSamplePool.recycleInput(i.sample);
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

    private static final class Output {
        public final Sample sample;
        public final int index;

        public Output(final Sample sample, int index) {
            this.sample = sample;
            this.index = index;
        }
    }

    private class OutputProcessor {
        private final boolean mRenderToSurface;
        private boolean mHasOutputCapacitySet;
        private Queue<Output> mSentOutputs = new LinkedList<>();
        private boolean mStopped;

        private OutputProcessor(boolean renderToSurface) {
            mRenderToSurface = renderToSurface;
        }

        private synchronized void onBuffer(int index, MediaCodec.BufferInfo info) {
            if (mStopped || !isValidBuffer(index)) {
                return;
            }

            try {
                Sample output = obtainOutputSample(index, info);
                mSentOutputs.add(new Output(output, index));
                mCallbacks.onOutput(output);
            } catch (Exception e) {
                e.printStackTrace();
                mCodec.releaseOutputBuffer(index, false);
            }

            boolean eos = (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            if (DEBUG && eos) {
                Log.d(LOGTAG, "output EOS");
            }
        }

        private boolean isValidBuffer(final int index) {
            try {
                return (mCodec.getOutputBuffer(index) != null) || mRenderToSurface;
            } catch (IllegalStateException e) {
                if (DEBUG) { Log.e(LOGTAG, "invalid buffer#" + index, e); }
                return false;
            }
        }

        private Sample obtainOutputSample(int index, MediaCodec.BufferInfo info) {
            Sample sample = mSamplePool.obtainOutput(info);

            if (mRenderToSurface) {
                return sample;
            }

            ByteBuffer output = mCodec.getOutputBuffer(index);
            if (!mHasOutputCapacitySet) {
                int capacity = output.capacity();
                if (capacity > 0) {
                    mSamplePool.setOutputBufferSize(capacity);
                    mHasOutputCapacitySet = true;
                }
            }

            if (info.size > 0) {
                try {
                    sample.buffer.readFromByteBuffer(output, info.offset, info.size);
                } catch (IOException e) {
                    Log.e(LOGTAG, "Fail to read output buffer:" + e.getMessage());
                }
            }

            return sample;
        }

        private synchronized void onRelease(Sample sample, boolean render) {
            final Output output = mSentOutputs.poll();
            if (output == null) {
                if (DEBUG) { Log.d(LOGTAG, sample + " already released"); }
                return;
            }
            mCodec.releaseOutputBuffer(output.index, render);
            mSamplePool.recycleOutput(output.sample);

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
            for (final Output o : mSentOutputs) {
                mCodec.releaseOutputBuffer(o.index, false);
                mSamplePool.recycleOutput(o.sample);
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
                                          GeckoSurface surface,
                                          int flags,
                                          String drmStubId) throws RemoteException {
        if (mCallbacks == null) {
            Log.e(LOGTAG, "FAIL: callbacks must be set before calling configure()");
            return false;
        }

        if (mCodec != null) {
            if (DEBUG) { Log.d(LOGTAG, "release existing codec: " + mCodec); }
            mCodec.release();
        }

        if (DEBUG) { Log.d(LOGTAG, "configure " + this); }

        final MediaFormat fmt = format.asFormat();
        final String mime = fmt.getString(MediaFormat.KEY_MIME);
        if (mime == null || mime.isEmpty()) {
            Log.e(LOGTAG, "invalid MIME type: " + mime);
            return false;
        }

        final List<String> found = findMatchingCodecNames(mime, flags == MediaCodec.CONFIGURE_FLAG_ENCODE);
        for (final String name : found) {
            final AsyncCodec codec = configureCodec(name, fmt, surface, flags, drmStubId);
            if (codec == null) {
                Log.w(LOGTAG, "unable to configure " + name + ". Try next.");
                continue;
            }
            mCodec = codec;
            mInputProcessor = new InputProcessor();
            final boolean renderToSurface = surface != null;
            mOutputProcessor = new OutputProcessor(renderToSurface);
            mSamplePool = new SamplePool(name, renderToSurface);
            if (DEBUG) { Log.d(LOGTAG, codec.toString() + " created. Render to surface?" + renderToSurface); }
            return true;
        }

        return false;
    }

    private List<String> findMatchingCodecNames(final String mimeType, final boolean isEncoder) {
        final int numCodecs = MediaCodecList.getCodecCount();
        final List<String> found = new ArrayList<>();
        for (int i = 0; i < numCodecs; i++) {
            final MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            if (info.isEncoder() == !isEncoder) {
                continue;
            }

            final String[] types = info.getSupportedTypes();
            for (final String t : types) {
                if (t.equalsIgnoreCase(mimeType)) {
                    found.add(info.getName());
                    if (DEBUG) {
                        Log.d(LOGTAG, "found " + (isEncoder ? "encoder:" : "decoder:") +
                                info.getName() + " for mime:" + mimeType);
                    }
                }
            }
        }
        return found;
    }

    private AsyncCodec configureCodec(final String name, final MediaFormat format,
            final Surface surface, final int flags, final String drmStubId) {
        try {
            final AsyncCodec codec = AsyncCodecFactory.create(name);
            codec.setCallbacks(new Callbacks(), null);

            final MediaCrypto crypto = RemoteMediaDrmBridgeStub.getMediaCrypto(drmStubId);
            if (DEBUG) {
                Log.d(LOGTAG, "configure mediacodec with crypto(" + (crypto != null) + ") / Id :" + drmStubId);
            }

            if (surface != null) {
                setupAdaptivePlayback(codec, format);
            }

            codec.configure(format, surface, crypto, flags);
            return codec;
        } catch (final Exception e) {
            Log.e(LOGTAG, "codec creation error", e);
            return null;
        }
    }

    private void setupAdaptivePlayback(final AsyncCodec codec, final MediaFormat format) {
        // Video decoder should config with adaptive playback capability.
        mIsAdaptivePlaybackSupported = codec.isAdaptivePlaybackSupported(
                format.getString(MediaFormat.KEY_MIME));
        if (mIsAdaptivePlaybackSupported) {
            if (DEBUG) { Log.d(LOGTAG, "codec supports adaptive playback  = " + mIsAdaptivePlaybackSupported); }
            // TODO: may need to find a way to not use hard code to decide the max w/h.
            format.setInteger(MediaFormat.KEY_MAX_WIDTH, 1920);
            format.setInteger(MediaFormat.KEY_MAX_HEIGHT, 1080);
        }
    }

    @Override
    public synchronized boolean isAdaptivePlaybackSupported() {
        return mIsAdaptivePlaybackSupported;
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
        } catch (NullPointerException ne) {
            // mCallbacks has been disposed by release().
        } catch (RemoteException re) {
            re.printStackTrace();
        }
    }

    @Override
    public synchronized void stop() throws RemoteException {
        if (DEBUG) { Log.d(LOGTAG, "stop " + this); }
        try {
            mInputProcessor.stop();
            mOutputProcessor.stop();

            mCodec.stop();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized void flush() throws RemoteException {
        if (DEBUG) { Log.d(LOGTAG, "flush " + this); }
        try {
            mInputProcessor.stop();
            mOutputProcessor.stop();

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
    public synchronized Sample dequeueInput(int size) throws RemoteException {
        try {
            return mInputProcessor.onAllocate(size);
        } catch (Exception e) {
            // Translate allocation error to remote exception.
            throw new RemoteException(e.getMessage());
        }
    }

    @Override
    public synchronized void queueInput(Sample sample) throws RemoteException {
        try {
            mInputProcessor.onSample(sample);
        } catch (Exception e) {
            throw new RemoteException(e.getMessage());
        }
    }

    @Override
    public synchronized void setRates(int newBitRate) {
        try {
            mCodec.setRates(newBitRate);
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized void releaseOutput(Sample sample, boolean render) {
        try {
            mOutputProcessor.onRelease(sample, render);
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
    }

    @Override
    public synchronized void release() throws RemoteException {
        if (DEBUG) { Log.d(LOGTAG, "release " + this); }
        try {
            // In case Codec.stop() is not called yet.
            mInputProcessor.stop();
            mOutputProcessor.stop();

            mCodec.release();
        } catch (Exception e) {
            reportError(Error.FATAL, e);
        }
        mCodec = null;
        mSamplePool.reset();
        mSamplePool = null;
        mCallbacks.asBinder().unlinkToDeath(this, 0);
        mCallbacks = null;
    }
}
