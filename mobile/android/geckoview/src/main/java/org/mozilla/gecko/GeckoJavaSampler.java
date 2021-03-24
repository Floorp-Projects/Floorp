/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Queue;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.LinkedBlockingQueue;

import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.annotation.WrapForJNI;

// Bug 1618560: Currently we only profile the Android UI thread. Ideally we should
// be able to profile multiple threads.
public class GeckoJavaSampler {
    private static final String LOGTAG = "GeckoJavaSampler";
    private static SamplingRunnable sSamplingRunnable;
    private static ScheduledExecutorService sSamplingScheduler;
    private static ScheduledFuture<?> sSamplingFuture;
    private static final MarkerStorage sMarkerStorage = new MarkerStorage();

    /**
     * Returns true if profiler is running and unpaused at the moment which means
     * it's allowed to add a marker.
     */
    public static boolean isProfilerActive() {
        // sSamplingRunnable is present if profiler is running and sSamplingFuture
        // present if profiler is not paused.
        return sSamplingRunnable != null && sSamplingFuture != null;
    }

    // Use the same timer primitive as the profiler
    // to get a perfect sample syncing.
    @WrapForJNI
    private static native double getProfilerTime();

    /**
     * Try to get the profiler time. Returns null if profiler is not running.
     */
    public static @Nullable Double tryToGetProfilerTime() {
        if (!isProfilerActive()) {
            // Android profiler hasn't started yet.
            return null;
        }
        if (!GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
            // getProfilerTime is not available yet; either libs are not loaded,
            // or profiling hasn't started on the Gecko side yet
            return null;
        }

        return getProfilerTime();
    }

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
                mFrames[aStack.length - 1 - i].methodName = aStack[i].getMethodName();
                mFrames[aStack.length - 1 - i].className = aStack[i].getClassName();
            }
        }
    }

    private static class Frame {
        public String methodName;
        public String className;
    }

    private static class Marker extends JNIObject {
        /**
         * Name of the marker
         */
        private String mMarkerName;
        /**
         * Either start time for the duration markers or time for a point-in-time markers.
         */
        private double mTime;
        /**
         * A fallback field of {@link #mTime} but it only exists when {@link #getProfilerTime()} is failed.
         * It is non-zero if Android time is used.
         */
        private long mJavaTime;
        /**
         * End time for the duration markers.
         * It's zero for point-in-time markers.
         */
        private double mEndTime;
        /**
         * A fallback field of {@link #mEndTime} but it only exists when {@link #getProfilerTime()} is failed.
         * It is non-zero if Android time is used.
         */
        private long mEndJavaTime;
        /**
         * A nullable additional information field for the marker.
         */
        private @Nullable String mText;

        /**
         * Constructor for the Marker class. It initializes different kinds of markers depending on
         * the parameters.
         * Here are some combinations to create different kinds of markers:
         *
         * If you want to create a marker that points a single point in time:
         * <code>new Marker("name", null, null, null)</code> to implicitly get the time when this
         * marker is added, or <code>new Marker("name", null, endTime, null)</code> to use an explicit
         * time as an end time retrieved from {@link #tryToGetProfilerTime()}.
         *
         * If you want to create a marker that has a start and end time:
         * <code>new Marker("name", startTime, null, null)</code> to implicitly get the end time
         * when this marker is added, or <code>new Marker("name", startTime, endTime, null)</code>
         * to explicitly give the marker start and end time retrieved from {@link #tryToGetProfilerTime()}.
         *
         * Last parameter is optional and can be given with any combination. This gives users the
         * ability to add more context into a marker.
         *
         * @param aMarkerName Identifier of the marker as a string.
         * @param aStartTime Start time as Double. It can be null if you want to mark a point of time.
         * @param aEndTime End time as Double. If it's null, this function implicitly gets the end time.
         * @param aText An optional string field for more information about the marker.
         */
        public Marker(@NonNull final String aMarkerName,
                      @Nullable final Double aStartTime,
                      @Nullable final Double aEndTime,
                      @Nullable final String aText) {
            mMarkerName = aMarkerName;
            mText = aText;
            if (aStartTime != null) {
                // Start time is provided. This is an interval marker.
                mTime = aStartTime;
                if (aEndTime != null) {
                    // End time is also provided.
                    mEndTime = aEndTime;
                } else {
                    // End time is not provided. Get the profiler time now and use it.
                    if (GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
                        mEndTime = getProfilerTime();
                    }
                    if (mEndTime == 0.0d) {
                        // getProfilerTime is not available yet; either libs are not loaded,
                        // or profiling hasn't started on the Gecko side yet
                        mEndJavaTime = SystemClock.elapsedRealtime();
                    }
                }
            } else {
                // Start time is not provided. This is point-in-time marker.
                if (aEndTime != null) {
                    // End time is also provided. Use that to point the time.
                    mTime = aEndTime;
                } else {
                    if (GeckoThread.isStateAtLeast(GeckoThread.State.JNI_READY)) {
                        mTime = getProfilerTime();
                    }
                    if (mTime == 0.0d) {
                        // getProfilerTime is not available yet; either libs are not loaded,
                        // or profiling hasn't started on the Gecko side yet
                        mJavaTime = SystemClock.elapsedRealtime();
                    }
                }
            }
        }

        @WrapForJNI @Override // JNIObject
        protected native void disposeNative();

        @WrapForJNI
        public double getStartTime() {
            if (mJavaTime != 0) {
                return (mJavaTime -
                    SystemClock.elapsedRealtime()) + getProfilerTime();
            }
            return mTime;
        }

        @WrapForJNI
        public double getEndTime() {
            if (mEndJavaTime != 0) {
                return (mEndJavaTime -
                    SystemClock.elapsedRealtime()) + getProfilerTime();
            }
            return mEndTime;
        }

        @WrapForJNI
        public @NonNull String getMarkerName() {
            return mMarkerName;
        }

        @WrapForJNI
        public @Nullable String getMarkerText() {
            return mText;
        }
    }

    /**
     * Public method to add a new marker to Gecko profiler.
     * This can be used to add a marker *inside* the geckoview code, but ideally
     * ProfilerController methods should be used instead.
     * @see Marker#Marker(String, Double, Double, String) for information about the parameter options.
     */
    public static void addMarker(@NonNull final String aMarkerName,
                                 @Nullable final Double aStartTime,
                                 @Nullable final Double aEndTime,
                                 @Nullable final String aText) {
        sMarkerStorage.addMarker(aMarkerName, aStartTime, aEndTime, aText);
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
            mSampleCount = aSampleCount;
            mSamples = new Sample[mSampleCount];
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
    public static Marker pollNextMarker() {
        return sMarkerStorage.pollNextMarker();
    }

    @WrapForJNI
    public synchronized static double getSampleTime(final int aSampleId) {
        final Sample sample = getSample(aSampleId);
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
        final Sample sample = getSample(aSampleId);
        if (sample != null && aFrameId < sample.mFrames.length) {
            final Frame frame = sample.mFrames[aFrameId];
            if (frame == null) {
                return null;
            }
            return frame.className + "." + frame.methodName + "()";
        }
        return null;
    }


    private static class MarkerStorage {
        private volatile Queue<Marker> mMarkers;

        MarkerStorage() {}

        public synchronized void start(final int aMarkerCount) {
            if (this.mMarkers != null) {
                return;
            }
            this.mMarkers = new LinkedBlockingQueue<>(aMarkerCount);
        }

        public synchronized void stop() {
            if (this.mMarkers == null) {
                return;
            }
            this.mMarkers = null;
        }

        private void addMarker(@NonNull final String aMarkerName,
                               @Nullable final Double aStartTime,
                               @Nullable final Double aEndTime,
                               @Nullable final String aText) {
            final Queue<Marker> markersQueue = this.mMarkers;
            if (markersQueue == null) {
                // Profiler is not active.
                return;
            }

            // It would be good to use `Looper.getMainLooper().isCurrentThread()`
            // instead but it requires API level 23 and current min is 16.
            if (Looper.myLooper() != Looper.getMainLooper()) {
                // Bug 1618560: Currently only main thread is being profiled and
                // this marker doesn't belong to the main thread.
                throw new AssertionError("Currently only main thread is supported for markers.");
            }

            final Marker newMarker = new Marker(aMarkerName, aStartTime, aEndTime, aText);

            boolean successful = markersQueue.offer(newMarker);
            while (!successful) {
                // Marker storage is full, remove the head and add again.
                markersQueue.poll();
                successful = markersQueue.offer(newMarker);
            }
        }

        private Marker pollNextMarker() {
            final Queue<Marker> markersQueue = this.mMarkers;
            if (markersQueue == null) {
                // Profiler is not active.
                return null;
            }
            // Retrieve and return the head of this queue.
            // Returns null if the queue is empty.
            return markersQueue.poll();
        }
    }


    @WrapForJNI
    public static void start(final int aInterval, final int aEntryCount) {
        synchronized (GeckoJavaSampler.class) {
            if (sSamplingRunnable != null) {
                return;
            }

            if (sSamplingFuture != null && !sSamplingFuture.isDone()) {
                return;
            }

            // Setting a limit of 120000 (2 mins with 1ms interval) for samples and markers for now
            // to make sure we are not allocating too much.
            final int limitedEntryCount = Math.min(aEntryCount, 120000);
            sSamplingRunnable = new SamplingRunnable(aInterval, limitedEntryCount);
            sMarkerStorage.start(limitedEntryCount);
            sSamplingScheduler = Executors.newSingleThreadScheduledExecutor();
            sSamplingFuture = sSamplingScheduler.scheduleAtFixedRate(sSamplingRunnable, 0, sSamplingRunnable.mInterval, TimeUnit.MILLISECONDS);
        }
    }

    @WrapForJNI
    public static void pauseSampling() {
        synchronized (GeckoJavaSampler.class) {
            sSamplingFuture.cancel(false /* mayInterruptIfRunning */ );
            sSamplingFuture = null;
        }
    }

    @WrapForJNI
    public static void unpauseSampling() {
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
            } catch (final InterruptedException e) {
                Log.e(LOGTAG, "Sampling scheduler isn't terminated. Last sampling data might be broken.");
                sSamplingScheduler.shutdownNow();
            }
            sSamplingScheduler = null;
            sSamplingRunnable = null;
            sSamplingFuture = null;
            sMarkerStorage.stop();
        }
    }

    /**
     * Returns the device brand and model as a string.
     */
    @WrapForJNI
    public static String getDeviceInformation() {
        final StringBuilder sb = new StringBuilder(Build.BRAND);
        sb.append(" ");
        sb.append(Build.MODEL);
        return sb.toString();
    }
}
