/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;

import java.nio.ByteBuffer;
import java.util.LinkedList;

import org.mozilla.gecko.mozglue.WebRTCJNITarget;

class CodecState {
    private static final String TAG = "CodecState";

    private ViEMediaCodecDecoder mView;
    private MediaFormat mFormat;
    private boolean mSawInputEOS, mSawOutputEOS;

    private MediaCodec mCodec;
    private MediaFormat mOutputFormat;
    private ByteBuffer[] mCodecInputBuffers;
    private ByteBuffer[] mCodecOutputBuffers;

    private LinkedList<Integer> mAvailableInputBufferIndices;
    private LinkedList<Integer> mAvailableOutputBufferIndices;
    private LinkedList<MediaCodec.BufferInfo> mAvailableOutputBufferInfos;

    private long mLastMediaTimeUs;

    @WebRTCJNITarget
    public CodecState(
            ViEMediaCodecDecoder view,
            MediaFormat format,
            MediaCodec codec) {
        mView = view;
        mFormat = format;
        mSawInputEOS = mSawOutputEOS = false;

        mCodec = codec;

        mCodec.start();
        mCodecInputBuffers = mCodec.getInputBuffers();
        mCodecOutputBuffers = mCodec.getOutputBuffers();

        mAvailableInputBufferIndices = new LinkedList<Integer>();
        mAvailableOutputBufferIndices = new LinkedList<Integer>();
        mAvailableOutputBufferInfos = new LinkedList<MediaCodec.BufferInfo>();

        mLastMediaTimeUs = 0;
    }

    public void release() {
        mCodec.stop();
        mCodecInputBuffers = null;
        mCodecOutputBuffers = null;
        mOutputFormat = null;

        mAvailableOutputBufferInfos = null;
        mAvailableOutputBufferIndices = null;
        mAvailableInputBufferIndices = null;

        mCodec.release();
        mCodec = null;
    }

    public void start() {
    }

    public void pause() {
    }

    public long getCurrentPositionUs() {
        return mLastMediaTimeUs;
    }

    public void flush() {
        mAvailableInputBufferIndices.clear();
        mAvailableOutputBufferIndices.clear();
        mAvailableOutputBufferInfos.clear();

        mSawInputEOS = false;
        mSawOutputEOS = false;

        mCodec.flush();
    }

    public void doSomeWork() {
        int index = mCodec.dequeueInputBuffer(0 /* timeoutUs */);

        if (index != MediaCodec.INFO_TRY_AGAIN_LATER) {
            mAvailableInputBufferIndices.add(new Integer(index));
        }

        while (feedInputBuffer()) {}

        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        index = mCodec.dequeueOutputBuffer(info, 0 /* timeoutUs */);

        if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
            mOutputFormat = mCodec.getOutputFormat();
        } else if (index == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
            mCodecOutputBuffers = mCodec.getOutputBuffers();
        } else if (index != MediaCodec.INFO_TRY_AGAIN_LATER) {
            mAvailableOutputBufferIndices.add(new Integer(index));
            mAvailableOutputBufferInfos.add(info);
        }

        while (drainOutputBuffer()) {}
    }

    /** returns true if more input data could be fed */
    private boolean feedInputBuffer() {
        if (mSawInputEOS || mAvailableInputBufferIndices.isEmpty()) {
            return false;
        }

        int index = mAvailableInputBufferIndices.peekFirst().intValue();

        ByteBuffer codecData = mCodecInputBuffers[index];

        if (mView.hasFrame()) {
            Frame frame = mView.dequeueFrame();
            ByteBuffer buffer = frame.mBuffer;
            if (buffer == null) {
                return false;
            }
            if (codecData.capacity() < buffer.capacity()) {
                Log.e(TAG, "Buffer is too small to copy a frame.");
                // TODO(dwkang): split the frame into the multiple buffer.
            }
            buffer.rewind();
            codecData.rewind();
            codecData.put(buffer);
            codecData.rewind();

            try {
                mCodec.queueInputBuffer(
                        index, 0 /* offset */, buffer.capacity(), frame.mTimeStampUs,
                        0 /* flags */);

                mAvailableInputBufferIndices.removeFirst();
            } catch (MediaCodec.CryptoException e) {
                Log.d(TAG, "CryptoException w/ errorCode "
                        + e.getErrorCode() + ", '" + e.getMessage() + "'");
            }

            return true;
        }
        return false;
    }


    /** returns true if more output data could be drained */
    private boolean drainOutputBuffer() {
        if (mSawOutputEOS || mAvailableOutputBufferIndices.isEmpty()) {
            return false;
        }

        int index = mAvailableOutputBufferIndices.peekFirst().intValue();
        MediaCodec.BufferInfo info = mAvailableOutputBufferInfos.peekFirst();

        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
            Log.d(TAG, "saw output EOS.");

            mSawOutputEOS = true;
            return false;
        }

        long realTimeUs =
            mView.getRealTimeUsForMediaTime(info.presentationTimeUs);
        long nowUs = System.currentTimeMillis() * 1000;
        long lateUs = nowUs - realTimeUs;

        // video
        boolean render;

        // TODO(dwkang): For some extreme cases, just not doing rendering is not enough.
        //               Need to seek to the next key frame.
        if (lateUs < -10000) {
            // too early;
            return false;
        } else if (lateUs > 30000) {
            Log.d(TAG, "video late by " + lateUs + " us. Skipping...");
            render = false;
        } else {
            render = true;
            mLastMediaTimeUs = info.presentationTimeUs;
        }

        MediaFormat format= mCodec.getOutputFormat();
        Log.d(TAG, "Video output format :" + format.getInteger(MediaFormat.KEY_COLOR_FORMAT));
        mCodec.releaseOutputBuffer(index, render);

        mAvailableOutputBufferIndices.removeFirst();
        mAvailableOutputBufferInfos.removeFirst();
        return true;
    }
}

class Frame {
    public ByteBuffer mBuffer;
    public long mTimeStampUs;

    Frame(ByteBuffer buffer, long timeStampUs) {
        mBuffer = buffer;
        mTimeStampUs = timeStampUs;
    }
}

class ViEMediaCodecDecoder {
    private static final String TAG = "ViEMediaCodecDecoder";

    private MediaExtractor mExtractor;

    private CodecState mCodecState;

    private int mState;
    private static final int STATE_IDLE         = 1;
    private static final int STATE_PREPARING    = 2;
    private static final int STATE_PLAYING      = 3;
    private static final int STATE_PAUSED       = 4;

    private Handler mHandler;
    private static final int EVENT_PREPARE            = 1;
    private static final int EVENT_DO_SOME_WORK       = 2;

    private long mDeltaTimeUs;
    private long mDurationUs;

    private SurfaceView mSurfaceView;
    private LinkedList<Frame> mFrameQueue = new LinkedList<Frame>();

    private Thread mLooperThread;

    @WebRTCJNITarget
    public void configure(SurfaceView surfaceView, int width, int height) {
        mSurfaceView = surfaceView;
        Log.d(TAG, "configure " + "width" + width + "height" + height + mSurfaceView.toString());

        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, "video/x-vnd.on2.vp8");
        format.setInteger(MediaFormat.KEY_WIDTH, width);
        format.setInteger(MediaFormat.KEY_HEIGHT, height);
        MediaCodec codec = MediaCodec.createDecoderByType("video/x-vnd.on2.vp8");
        // SW VP8 decoder
        // MediaCodec codec = MediaCodec.createByCodecName("OMX.google.vpx.decoder");
        // Nexus10 HW VP8 decoder
        // MediaCodec codec = MediaCodec.createByCodecName("OMX.Exynos.VP8.Decoder");
        Surface surface = mSurfaceView.getHolder().getSurface();
        Log.d(TAG, "Surface " + surface.isValid());
        codec.configure(
                format, surface, null, 0);
        mCodecState = new CodecState(this, format, codec);

        initMediaCodecView();
    }

    @WebRTCJNITarget
    public void setEncodedImage(ByteBuffer buffer, long renderTimeMs) {
        // TODO(dwkang): figure out why exceptions just make this thread finish.
        try {
            final long renderTimeUs = renderTimeMs * 1000;
            ByteBuffer buf = ByteBuffer.allocate(buffer.capacity());
            buf.put(buffer);
            buf.rewind();
            synchronized(mFrameQueue) {
                mFrameQueue.add(new Frame(buf, renderTimeUs));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public boolean hasFrame() {
        synchronized(mFrameQueue) {
            return !mFrameQueue.isEmpty();
        }
    }

    public Frame dequeueFrame() {
        synchronized(mFrameQueue) {
            return mFrameQueue.removeFirst();
        }
    }

    private void initMediaCodecView() {
        Log.d(TAG, "initMediaCodecView");
        mState = STATE_IDLE;

        mLooperThread = new Thread()
        {
            @Override
            public void run() {
                Log.d(TAG, "Looper prepare");
                Looper.prepare();
                mHandler = new Handler() {
                    @Override
                    public void handleMessage(Message msg) {
                        // TODO(dwkang): figure out exceptions just make this thread finish.
                        try {
                            switch (msg.what) {
                                case EVENT_PREPARE:
                                {
                                    mState = STATE_PAUSED;
                                    ViEMediaCodecDecoder.this.start();
                                    break;
                                }

                                case EVENT_DO_SOME_WORK:
                                {
                                    ViEMediaCodecDecoder.this.doSomeWork();

                                    mHandler.sendMessageDelayed(
                                            mHandler.obtainMessage(EVENT_DO_SOME_WORK), 5);
                                    break;
                                }

                                default:
                                    break;
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                };
                Log.d(TAG, "Looper loop");
                synchronized(ViEMediaCodecDecoder.this) {
                    ViEMediaCodecDecoder.this.notify();
                }
                Looper.loop();
            }
        };
        mLooperThread.start();

        // Wait until handler is set up.
        synchronized(ViEMediaCodecDecoder.this) {
            try {
                ViEMediaCodecDecoder.this.wait(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        Log.d(TAG, "initMediaCodecView end");
    }

    @WebRTCJNITarget
    public void start() {
        Log.d(TAG, "start");

        if (mState == STATE_PLAYING || mState == STATE_PREPARING) {
            return;
        } else if (mState == STATE_IDLE) {
            mState = STATE_PREPARING;
            Log.d(TAG, "Sending EVENT_PREPARE");
            mHandler.sendMessage(mHandler.obtainMessage(EVENT_PREPARE));
            return;
        } else if (mState != STATE_PAUSED) {
            throw new IllegalStateException();
        }

        mCodecState.start();

        mHandler.sendMessage(mHandler.obtainMessage(EVENT_DO_SOME_WORK));

        mDeltaTimeUs = -1;
        mState = STATE_PLAYING;

        Log.d(TAG, "start end");
    }

    public void reset() {
        if (mState == STATE_PLAYING) {
            mCodecState.pause();
        }

        mCodecState.release();

        mDurationUs = -1;
        mState = STATE_IDLE;
    }

    private void doSomeWork() {
        mCodecState.doSomeWork();
    }

    public long getRealTimeUsForMediaTime(long mediaTimeUs) {
        if (mDeltaTimeUs == -1) {
            long nowUs = System.currentTimeMillis() * 1000;
            mDeltaTimeUs = nowUs - mediaTimeUs;
        }

        return mDeltaTimeUs + mediaTimeUs;
    }
}
