/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.annotation.WrapForJNI;

// Bug 1618560: Currently we only profile the Java Main Thread. Ideally we should
// be able to profile multiple threads.
public class GeckoJavaSampler {
    private static final String LOGTAG = "GeckoJavaSampler";
    private static SamplingRunnable sSamplingRunnable;
    private static ScheduledExecutorService sSamplingScheduler;
    private static ScheduledFuture<?> sSamplingFuture;

    // Use the same timer primitive as the profiler
    // to get a perfect sample syncing.
    @WrapForJNI
    private static native double getProfilerTime();

    private static class Sample {
        public Frame[] mFrames;
        public double mTime;
        public long mJavaTime; // non-zero if Android system time is used
        public Sample(final StackTraceElement[] aStack) {
            mFrames = new Frame[aStack.length];
            if (GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
                mTime = getProfilerTime();
            }
            if (mTime == 0.0d) {
                // getProfilerTime is not available yet; either libs are not loaded,
                // or profiling hasn't started on the Gecko side yet
                mJavaTime = SystemClock.elapsedRealtime();
            }
            for (int i = 0; i < aStack.length; i++) {
                mFrames[aStack.length - 1 - i] = new Frame();
                mFrames[aStack.length - 1 - i].fileName = aStack[i].getFileName();
                mFrames[aStack.length - 1 - i].lineNo = aStack[i].getLineNumber();
                mFrames[aStack.length - 1 - i].methodName = aStack[i].getMethodName();
                mFrames[aStack.length - 1 - i].className = aStack[i].getClassName();
            }
        }
    }

    private static class Frame {
        public String fileName;
        public int lineNo;
        public String methodName;
        public String className;
    }

    private static class SamplingRunnable implements Runnable {
        // Sampling interval that is used by start and unpause
        public final int mInterval;
        private final int mSampleCount;

        private boolean mBufferOverflowed = false;

        private Thread mMainThread;
        private Sample[] mSamples;
        private int mSamplePos;

        public SamplingRunnable(final int aInterval, final int aSampleCount) {
            // Sanity check of sampling interval.
            mInterval = Math.max(1, aInterval);
            // Setting a limit of 100000 for now to make sure we are not
            // allocating too much.
            mSampleCount = Math.min(aSampleCount, 100000);
            mSamples = new Sample[aSampleCount];
            mSamplePos = 0;

            // Find the main thread
            mMainThread = Looper.getMainLooper().getThread();
            if (mMainThread == null) {
                Log.e(LOGTAG, "Main thread not found");
            }
        }

        @Override
        public void run() {
            synchronized (GeckoJavaSampler.class) {
                if (mMainThread == null) {
                    return;
                }
                final StackTraceElement[] bt = mMainThread.getStackTrace();
                mSamples[mSamplePos] = new Sample(bt);
                mSamplePos += 1;
                if (mSamplePos == mSampleCount) {
                    // Sample array is full now, go back to start of
                    // the array and override old samples
                    mSamplePos = 0;
                    mBufferOverflowed = true;
                }
            }
        }

        private Sample getSample(final int aSampleId) {
            if (aSampleId >= mSampleCount) {
                // Return early because there is no more sample left.
                return null;
            }

            int samplePos = aSampleId;
            if (mBufferOverflowed) {
                // This is a circular buffer and the buffer is overflowed. Start
                // of the buffer is mSamplePos now. Calculate the real index.
                samplePos = (samplePos + mSamplePos) % mSampleCount;
            }

            // Since the array elements are initialized to null, it will return
            // null whenever we access to an element that's not been written yet.
            // We want it to return null in that case, so it's okay.
            return mSamples[samplePos];
        }
    }

    private synchronized static Sample getSample(final int aSampleId) {
        return sSamplingRunnable.getSample(aSampleId);
    }

    @WrapForJNI
    public synchronized static double getSampleTime(final int aSampleId) {
        Sample sample = getSample(aSampleId);
        if (sample != null) {
            if (sample.mJavaTime != 0) {
                return (sample.mJavaTime -
                    SystemClock.elapsedRealtime()) + getProfilerTime();
            }
            return sample.mTime;
        }
        return 0;
    }

    @WrapForJNI
    public synchronized static String getFrameName(final int aSampleId, final int aFrameId) {
        Sample sample = getSample(aSampleId);
        if (sample != null && aFrameId < sample.mFrames.length) {
            Frame frame = sample.mFrames[aFrameId];
            if (frame == null) {
                return null;
            }
            return frame.className + "." + frame.methodName + "()";
        }
        return null;
    }

    @WrapForJNI
    public static void start(final int aInterval, final int aSamples) {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingRunnable != null) {
                return;
            }

            if (sSamplingFuture != null && !sSamplingFuture.isDone()) {
                return;
            }

            sSamplingRunnable = new SamplingRunnable(aInterval, aSamples);
            sSamplingScheduler = Executors.newSingleThreadScheduledExecutor();
            sSamplingFuture = sSamplingScheduler.scheduleAtFixedRate(sSamplingRunnable, 0, sSamplingRunnable.mInterval, TimeUnit.MILLISECONDS);
        }
    }

    @WrapForJNI
    public static void pause() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingFuture.cancel(false /* mayInterruptIfRunning */ );
            sSamplingFuture = null;
        }
    }

    @WrapForJNI
    public static void unpause() {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingFuture != null) {
                return;
            }
            sSamplingFuture = sSamplingScheduler.scheduleAtFixedRate(sSamplingRunnable, 0, sSamplingRunnable.mInterval, TimeUnit.MILLISECONDS);
        }
    }

    @WrapForJNI
    public static void stop() {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingRunnable == null) {
                return;
            }

            try {
                sSamplingScheduler.shutdown();
                // 1s is enough to wait shutdown.
                sSamplingScheduler.awaitTermination(1000, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Log.e(LOGTAG, "Sampling scheduler isn't terminated. Last sampling data might be broken.");
                sSamplingScheduler.shutdownNow();
            }
            sSamplingScheduler = null;
            sSamplingRunnable = null;
            sSamplingFuture = null;
        }
    }
}
