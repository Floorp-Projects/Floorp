/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import org.mozilla.gecko.annotation.WrapForJNI;

// Bug 1618560: Currently we only profile the Java Main Thread. Ideally we should
// be able to profile multiple threads.
public class GeckoJavaSampler {
    private static final String LOGTAG = "JavaSampler";
    private static Thread sSamplingThread;
    private static SamplingThread sSamplingRunnable;
    private static Thread sMainThread;

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

    private static class SamplingThread implements Runnable {
        private final int mInterval;
        private final int mSampleCount;

        private boolean mPauseSampler;
        private boolean mStopSampler;

        private Sample[] mSamples;
        private int mSamplePos;

        public SamplingThread(final int aInterval, final int aSampleCount) {
            // If we sample faster then 10ms we get to many missed samples
            mInterval = Math.max(10, aInterval);
            mSampleCount = aSampleCount;
        }

        @Override
        public void run() {
            synchronized (GeckoJavaSampler.class) {
                mSamples = new Sample[mSampleCount];
                mSamplePos = 0;

                // Find the main thread
                sMainThread = Looper.getMainLooper().getThread();
                if (sMainThread == null) {
                    Log.e(LOGTAG, "Main thread not found");
                    return;
                }
            }

            while (true) {
                try {
                    Thread.sleep(mInterval);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                synchronized (GeckoJavaSampler.class) {
                    if (!mPauseSampler) {
                        StackTraceElement[] bt = sMainThread.getStackTrace();
                        mSamples[mSamplePos] = new Sample(bt);
                        mSamplePos = (mSamplePos + 1) % mSamples.length;
                    }
                    if (mStopSampler) {
                        break;
                    }
                }
            }
        }

        private Sample getSample(final int aSampleId) {
            if (aSampleId < mSamples.length && mSamples[aSampleId] != null) {
                int startPos = 0;
                if (mSamples[mSamplePos] != null) {
                    startPos = mSamplePos;
                }
                int readPos = (startPos + aSampleId) % mSamples.length;
                return mSamples[readPos];
            }
            return null;
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
            sSamplingRunnable = new SamplingThread(aInterval, aSamples);
            sSamplingThread = new Thread(sSamplingRunnable, "Java Sampler");
            sSamplingThread.start();
        }
    }

    @WrapForJNI
    public static void pause() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingRunnable.mPauseSampler = true;
        }
    }

    @WrapForJNI
    public static void unpause() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingRunnable.mPauseSampler = false;
        }
    }

    @WrapForJNI
    public static void stop() {
        Thread samplingThread;

        synchronized (GeckoJavaSampler.class) {
            if (sSamplingThread == null) {
                return;
            }

            sSamplingRunnable.mStopSampler = true;
            samplingThread = sSamplingThread;
            sSamplingThread = null;
            sSamplingRunnable = null;
        }

        boolean retry = true;
        while (retry) {
            try {
                samplingThread.join();
                retry = false;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
