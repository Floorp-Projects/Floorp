/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
 *   Chris Lord <chrislord.net@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import java.util.ArrayList;
import java.util.List;
import android.os.SystemClock;
import android.util.Log;

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

    public static void startFrameTimeRecording() {
        if (mRecordingFrames) {
            Log.e(LOGTAG, "Error: startFrameTimeRecording() called while already recording!");
            return;
        }
        mRecordingFrames = true;
        if (mFrameTimes == null) {
            mFrameTimes = new ArrayList<Long>(EXPECTED_FRAME_COUNT);
        } else {
            mFrameTimes.clear();
        }
        mFrameStartTime = SystemClock.uptimeMillis();
    }

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

    public static void startCheckerboardRecording() {
        if (mRecordingCheckerboard) {
            Log.e(LOGTAG, "Error: startCheckerboardRecording() called while already recording!");
            return;
        }
        mRecordingCheckerboard = true;
        if (mCheckerboardAmounts == null) {
            mCheckerboardAmounts = new ArrayList<Float>(EXPECTED_FRAME_COUNT);
        } else {
            mCheckerboardAmounts.clear();
        }
        mCheckerboardStartTime = SystemClock.uptimeMillis();
    }

    public static List<Float> stopCheckerboardRecording() {
        if (!mRecordingCheckerboard) {
            Log.e(LOGTAG, "Error: stopCheckerboardRecording() called when not recording!");
            return null;
        }
        mRecordingCheckerboard = false;
        return mCheckerboardAmounts;
    }

    public static void recordCheckerboard(float amount) {
        // this will be called often, so try to make it as quick as possible
        if (mRecordingCheckerboard) {
            mCheckerboardAmounts.add(amount);
        }
    }
}
