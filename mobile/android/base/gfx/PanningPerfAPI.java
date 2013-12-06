/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.RobocopTarget;

import android.os.SystemClock;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class PanningPerfAPI {
    private static final String LOGTAG = "GeckoPanningPerfAPI";

    // make this large enough to avoid having to resize the frame time
    // list, as that may be expensive and impact the thing we're trying
    // to measure.
    private static final int EXPECTED_FRAME_COUNT = 2048;

    private static boolean mRecordingFrames = false;
    private static List<Long> mFrameTimes;
    private static long mFrameStartTime;

    private static boolean mRecordingCheckerboard = false;
    private static List<Float> mCheckerboardAmounts;
    private static long mCheckerboardStartTime;

    private static void initialiseRecordingArrays() {
        if (mFrameTimes == null) {
            mFrameTimes = new ArrayList<Long>(EXPECTED_FRAME_COUNT);
        } else {
            mFrameTimes.clear();
        }
        if (mCheckerboardAmounts == null) {
            mCheckerboardAmounts = new ArrayList<Float>(EXPECTED_FRAME_COUNT);
        } else {
            mCheckerboardAmounts.clear();
        }
    }

    @RobocopTarget
    public static void startFrameTimeRecording() {
        if (mRecordingFrames || mRecordingCheckerboard) {
            Log.e(LOGTAG, "Error: startFrameTimeRecording() called while already recording!");
            return;
        }
        mRecordingFrames = true;
        initialiseRecordingArrays();
        mFrameStartTime = SystemClock.uptimeMillis();
    }

    @RobocopTarget
    public static List<Long> stopFrameTimeRecording() {
        if (!mRecordingFrames) {
            Log.e(LOGTAG, "Error: stopFrameTimeRecording() called when not recording!");
            return null;
        }
        mRecordingFrames = false;
        return mFrameTimes;
    }

    public static void recordFrameTime() {
        // this will be called often, so try to make it as quick as possible
        if (mRecordingFrames) {
            mFrameTimes.add(SystemClock.uptimeMillis() - mFrameStartTime);
        }
    }

    public static boolean isRecordingCheckerboard() {
        return mRecordingCheckerboard;
    }

    @RobocopTarget
    public static void startCheckerboardRecording() {
        if (mRecordingCheckerboard || mRecordingFrames) {
            Log.e(LOGTAG, "Error: startCheckerboardRecording() called while already recording!");
            return;
        }
        mRecordingCheckerboard = true;
        initialiseRecordingArrays();
        mCheckerboardStartTime = SystemClock.uptimeMillis();
    }

    @RobocopTarget
    public static List<Float> stopCheckerboardRecording() {
        if (!mRecordingCheckerboard) {
            Log.e(LOGTAG, "Error: stopCheckerboardRecording() called when not recording!");
            return null;
        }
        mRecordingCheckerboard = false;

        // We take the number of values in mCheckerboardAmounts here, as there's
        // the possibility that this function is called while recordCheckerboard
        // is still executing. As values are added to this list last, we use
        // this number as the canonical number of recordings.
        int values = mCheckerboardAmounts.size();

        // The score will be the sum of all the values in mCheckerboardAmounts,
        // so weight the checkerboard values by time so that frame-rate and
        // run-length don't affect score.
        long lastTime = 0;
        float totalTime = mFrameTimes.get(values - 1);
        for (int i = 0; i < values; i++) {
            long elapsedTime = mFrameTimes.get(i) - lastTime;
            mCheckerboardAmounts.set(i, mCheckerboardAmounts.get(i) * elapsedTime / totalTime);
            lastTime += elapsedTime;
        }

        return mCheckerboardAmounts;
    }

    public static void recordCheckerboard(float amount) {
        // this will be called often, so try to make it as quick as possible
        if (mRecordingCheckerboard) {
            mFrameTimes.add(SystemClock.uptimeMillis() - mCheckerboardStartTime);
            mCheckerboardAmounts.add(amount);
        }
    }
}
