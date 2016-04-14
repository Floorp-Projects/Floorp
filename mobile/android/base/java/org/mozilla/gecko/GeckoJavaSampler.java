/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;

import org.mozilla.gecko.annotation.WrapForJNI;

import java.lang.Thread;
import java.util.Set;

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
        public Sample(StackTraceElement[] aStack) {
            mFrames = new Frame[aStack.length];
            if (GeckoThread.isStateAtLeast(GeckoThread.State.LIBS_READY)) {
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

        private final SparseArray<Sample[]> mSamples = new SparseArray<Sample[]>();
        private int mSamplePos;

        public SamplingThread(final int aInterval, final int aSampleCount) {
            // If we sample faster then 10ms we get to many missed samples
            mInterval = Math.max(10, aInterval);
            mSampleCount = aSampleCount;
        }

        @Override
        public void run() {
            synchronized (GeckoJavaSampler.class) {
                mSamples.put(0, new Sample[mSampleCount]);
                mSamplePos = 0;

                // Find the main thread
                Set<Thread> threadSet = Thread.getAllStackTraces().keySet();
                for (Thread t : threadSet) {
                    if (t.getName().compareToIgnoreCase("main") == 0) {
                        sMainThread = t;
                        break;
                    }
                }

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
                        mSamples.get(0)[mSamplePos] = new Sample(bt);
                        mSamplePos = (mSamplePos + 1) % mSamples.get(0).length;
                    }
                    if (mStopSampler) {
                        break;
                    }
                }
            }
        }

        private Sample getSample(int aThreadId, int aSampleId) {
            if (aThreadId < mSamples.size() && aSampleId < mSamples.get(aThreadId).length &&
                mSamples.get(aThreadId)[aSampleId] != null) {
                int startPos = 0;
                if (mSamples.get(aThreadId)[mSamplePos] != null) {
                    startPos = mSamplePos;
                }
                int readPos = (startPos + aSampleId) % mSamples.get(aThreadId).length;
                return mSamples.get(aThreadId)[readPos];
            }
            return null;
        }
    }


    @WrapForJNI(allowMultithread = true, stubName = "GetThreadNameJavaProfilingWrapper")
    public synchronized static String getThreadName(int aThreadId) {
        if (aThreadId == 0 && sMainThread != null) {
            return sMainThread.getName();
        }
        return null;
    }

    private synchronized static Sample getSample(int aThreadId, int aSampleId) {
        return sSamplingRunnable.getSample(aThreadId, aSampleId);
    }

    @WrapForJNI(allowMultithread = true, stubName = "GetSampleTimeJavaProfiling")
    public synchronized static double getSampleTime(int aThreadId, int aSampleId) {
        Sample sample = getSample(aThreadId, aSampleId);
        if (sample != null) {
            if (sample.mJavaTime != 0) {
                return (sample.mJavaTime -
                    SystemClock.elapsedRealtime()) + getProfilerTime();
            }
            System.out.println("Sample: " + sample.mTime);
            return sample.mTime;
        }
        return 0;
    }

    @WrapForJNI(allowMultithread = true, stubName = "GetFrameNameJavaProfilingWrapper")
    public synchronized static String getFrameName(int aThreadId, int aSampleId, int aFrameId) {
        Sample sample = getSample(aThreadId, aSampleId);
        if (sample != null && aFrameId < sample.mFrames.length) {
            Frame frame = sample.mFrames[aFrameId];
            if (frame == null) {
                return null;
            }
            return frame.className + "." + frame.methodName + "()";
        }
        return null;
    }

    @WrapForJNI(allowMultithread = true, stubName = "StartJavaProfiling")
    public static void start(int aInterval, int aSamples) {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingRunnable != null) {
                return;
            }
            sSamplingRunnable = new SamplingThread(aInterval, aSamples);
            sSamplingThread = new Thread(sSamplingRunnable, "Java Sampler");
            sSamplingThread.start();
        }
    }

    @WrapForJNI(allowMultithread = true, stubName = "PauseJavaProfiling")
    public static void pause() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingRunnable.mPauseSampler = true;
        }
    }

    @WrapForJNI(allowMultithread = true, stubName = "UnpauseJavaProfiling")
    public static void unpause() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingRunnable.mPauseSampler = false;
        }
    }

    @WrapForJNI(allowMultithread = true, stubName = "StopJavaProfiling")
    public static void stop() {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingThread == null) {
                return;
            }

            sSamplingRunnable.mStopSampler = true;
            try {
                sSamplingThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            sSamplingThread = null;
            sSamplingRunnable = null;
        }
    }
}
