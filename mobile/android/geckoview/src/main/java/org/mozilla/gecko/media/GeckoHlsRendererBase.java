/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.util.Log;

import org.mozilla.gecko.AppConstants;

import com.google.android.exoplayer2.BaseRenderer;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.FormatHolder;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.Iterator;

public abstract class GeckoHlsRendererBase extends BaseRenderer {
    protected static final int QUEUED_INPUT_SAMPLE_DURATION_THRESHOLD = 1000000; //1sec
    protected final FormatHolder mFormatHolder = new FormatHolder();
    /*
     *  DEBUG/LOGTAG will be set in the 2 subclass GeckoHlsAudioRenderer and
     *  GeckoHlsVideoRenderer, and we still wants to log message in the base class
     *  GeckoHlsRendererBase, so neither 'static' nor 'final' are applied to them.
     */
    protected boolean DEBUG;
    protected String LOGTAG;
    // Notify GeckoHlsPlayer about renderer's status, i.e. data has arrived.
    protected GeckoHlsPlayer.ComponentEventDispatcher mPlayerEventDispatcher;

    protected ConcurrentLinkedQueue<GeckoHLSSample> mDemuxedInputSamples = new ConcurrentLinkedQueue<>();

    protected ByteBuffer mInputBuffer = null;
    protected ArrayList<Format> mFormats = new ArrayList<Format>();
    protected boolean mInitialized = false;
    protected boolean mWaitingForData = true;
    protected boolean mInputStreamEnded = false;
    protected long mFirstSampleStartTime = Long.MIN_VALUE;

    protected abstract void createInputBuffer();
    protected abstract void handleReconfiguration(DecoderInputBuffer bufferForRead);
    protected abstract void handleFormatRead(DecoderInputBuffer bufferForRead);
    protected abstract void handleEndOfStream(DecoderInputBuffer bufferForRead);
    protected abstract void handleSamplePreparation(DecoderInputBuffer bufferForRead);
    protected abstract void resetRenderer();
    protected abstract boolean clearInputSamplesQueue();
    protected abstract void notifyPlayerInputFormatChanged(Format newFormat);

    private DecoderInputBuffer mBufferForRead =
        new DecoderInputBuffer(DecoderInputBuffer.BUFFER_REPLACEMENT_MODE_NORMAL);
    private final DecoderInputBuffer mflagsOnlyBuffer = DecoderInputBuffer.newFlagsOnlyInstance();

    protected void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    public GeckoHlsRendererBase(int trackType, GeckoHlsPlayer.ComponentEventDispatcher eventDispatcher) {
        super(trackType);
        mPlayerEventDispatcher = eventDispatcher;
    }

    private boolean isQueuedEnoughData() {
        if (mDemuxedInputSamples.isEmpty()) {
            return false;
        }

        Iterator<GeckoHLSSample> iter = mDemuxedInputSamples.iterator();
        long firstPTS = 0;
        if (iter.hasNext()) {
            GeckoHLSSample sample = iter.next();
            firstPTS = sample.info.presentationTimeUs;
        }
        long lastPTS = firstPTS;
        while (iter.hasNext()) {
            GeckoHLSSample sample = iter.next();
            lastPTS = sample.info.presentationTimeUs;
        }
        return Math.abs(lastPTS - firstPTS) > QUEUED_INPUT_SAMPLE_DURATION_THRESHOLD;
    }

    public Format getFormat(int index) {
        assertTrue(index >= 0);
        Format fmt = index < mFormats.size() ? mFormats.get(index) : null;
        if (DEBUG) { Log.d(LOGTAG, "getFormat : index = " + index + ", format : " + fmt); }
        return fmt;
    }

    public synchronized long getFirstSamplePTS() { return mFirstSampleStartTime; }

    public synchronized ConcurrentLinkedQueue<GeckoHLSSample> getQueuedSamples(int number) {
        ConcurrentLinkedQueue<GeckoHLSSample> samples =
            new ConcurrentLinkedQueue<GeckoHLSSample>();

        GeckoHLSSample sample = null;
        int queuedSize = mDemuxedInputSamples.size();
        for (int i = 0; i < queuedSize; i++) {
            if (i >= number) {
                break;
            }
            sample = mDemuxedInputSamples.poll();
            samples.offer(sample);
        }

        sample = samples.isEmpty() ? null : samples.peek();
        if (sample == null) {
            if (DEBUG) { Log.d(LOGTAG, "getQueuedSamples isEmpty, mWaitingForData = true !"); }
            mWaitingForData = true;
        } else if (mFirstSampleStartTime == Long.MIN_VALUE) {
            mFirstSampleStartTime = sample.info.presentationTimeUs;
            if (DEBUG) { Log.d(LOGTAG, "mFirstSampleStartTime = " + mFirstSampleStartTime); }
        }
        return samples;
    }

    protected void handleDrmInitChanged(Format oldFormat, Format newFormat) {
        Object oldDrmInit = oldFormat == null ? null : oldFormat.drmInitData;
        Object newDrnInit = newFormat.drmInitData;

        // TODO: Notify MFR if the content is encrypted or not.
        if (newDrnInit != oldDrmInit) {
            if (newDrnInit != null) {
            } else {
            }
        }
    }

    protected boolean canReconfigure(Format oldFormat, Format newFormat) {
        // Referring to ExoPlayer's MediaCodecBaseRenderer, the default is set
        // to false. Only override it in video renderer subclass.
        return false;
    }

    protected void prepareReconfiguration() {
        // Referring to ExoPlayer's MediaCodec related renderers, only video
        // renderer handles this.
    }

    protected void updateCSDInfo(Format format) {
        // do nothing.
    }

    protected void onInputFormatChanged(Format newFormat) {
        Format oldFormat;
        try {
            oldFormat = mFormats.get(mFormats.size() - 1);
        } catch (IndexOutOfBoundsException e) {
            oldFormat = null;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "[onInputFormatChanged] old : " + oldFormat +
                  " => new : " + newFormat);
        }
        mFormats.add(newFormat);
        handleDrmInitChanged(oldFormat, newFormat);

        if (mInitialized && canReconfigure(oldFormat, newFormat)) {
            prepareReconfiguration();
        } else {
            resetRenderer();
            maybeInitRenderer();
        }

        updateCSDInfo(newFormat);
        notifyPlayerInputFormatChanged(newFormat);
    }

    protected void maybeInitRenderer() {
        if (mInitialized || mFormats.size() == 0) {
            return;
        }
        if (DEBUG) { Log.d(LOGTAG, "Initializing ... "); }
        createInputBuffer();
        mInitialized = true;
    }

    /*
     * The place we get demuxed data from HlsMediaSource(ExoPlayer).
     * The data will then be converted to GeckoHLSSample and deliver to
     * GeckoHlsDemuxerWrapper for further use.
     * If the return value is ture, that means a GeckoHLSSample is queued
     * successfully. We can try to feed more samples into queue.
     * If the return value is false, that means we might encounter following
     * situation 1) not initialized 2) input stream is ended 3) queue is full.
     * 4) format changed. 5) exception happened.
     */
    protected synchronized boolean feedInputBuffersQueue() {
        if (!mInitialized || mInputStreamEnded || isQueuedEnoughData()) {
            // Need to reinitialize the renderer or the input stream has ended
            // or we just reached the maximum queue size.
            return false;
        }

        mBufferForRead.data = mInputBuffer;
        if (mBufferForRead.data != null) {
            mBufferForRead.clear();
        }

        handleReconfiguration(mBufferForRead);

        // Read data from HlsMediaSource
        int result = C.RESULT_NOTHING_READ;
        try {
            result = readSource(mFormatHolder, mBufferForRead, false);
        } catch (Exception e) {
            Log.e(LOGTAG, "[feedInput] Exception when readSource :", e);
            return false;
        }

        if (result == C.RESULT_NOTHING_READ) {
            return false;
        }

        if (result == C.RESULT_FORMAT_READ) {
            handleFormatRead(mBufferForRead);
            return true;
        }

        // We've read a buffer.
        if (mBufferForRead.isEndOfStream()) {
            if (DEBUG) { Log.d(LOGTAG, "Now we're at the End Of Stream."); }
            handleEndOfStream(mBufferForRead);
            return false;
        }

        mBufferForRead.flip();

        handleSamplePreparation(mBufferForRead);

        maybeNotifyDataArrived();
        return true;
    }

    private void maybeNotifyDataArrived() {
        if (mWaitingForData && isQueuedEnoughData()) {
            if (DEBUG) { Log.d(LOGTAG, "onDataArrived"); }
            mPlayerEventDispatcher.onDataArrived(getTrackType());
            mWaitingForData = false;
        }
    }

    private void readFormat() {
        mflagsOnlyBuffer.clear();
        int result = readSource(mFormatHolder, mflagsOnlyBuffer, true);
        if (result == C.RESULT_FORMAT_READ) {
            onInputFormatChanged(mFormatHolder.format);
        }
    }

    @Override
    protected void onEnabled(boolean joining) {
        // Do nothing.
    }

    @Override
    protected void onDisabled() {
        mFormats.clear();
        resetRenderer();
    }

    @Override
    public boolean isReady() {
        return mFormats.size() != 0;
    }

    @Override
    public boolean isEnded() {
        return mInputStreamEnded;
    }

    @Override
    protected synchronized void onPositionReset(long positionUs, boolean joining) {
        if (DEBUG) { Log.d(LOGTAG, "onPositionReset : positionUs = " + positionUs); }
        mInputStreamEnded = false;
        if (mInitialized) {
            clearInputSamplesQueue();
        }
    }

    /*
     * This is called by ExoPlayerImplInternal.java.
     * ExoPlayer checks the status of renderer, i.e. isReady() / isEnded(), and
     * calls renderer.render by passing its wall clock time.
     */
    @Override
    public void render(long positionUs, long elapsedRealtimeUs) {
        if (AppConstants.DEBUG_BUILD) {
            Log.d(LOGTAG, "positionUs = " + positionUs +
                          ", mInputStreamEnded = " + mInputStreamEnded);
        }
        if (mInputStreamEnded) {
            return;
        }
        if (mFormats.size() == 0) {
            readFormat();
        }

        maybeInitRenderer();
        while (feedInputBuffersQueue()) {
            // Do nothing
        }
    }
}
