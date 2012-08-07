/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;

import android.app.Activity;
import android.content.Intent;
import android.os.Environment;
import android.text.format.Time;
import android.util.Log;

import java.io.File;
import java.util.concurrent.SynchronousQueue;

class CameraImageResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoCameraImageResultHandler";

    private final SynchronousQueue<String> mFilePickerResult;

    CameraImageResultHandler(SynchronousQueue<String> resultQueue) {
        mFilePickerResult = resultQueue;
    }

    public void onActivityResult(int resultCode, Intent data) {
        try {
            if (resultCode != Activity.RESULT_OK) {
                mFilePickerResult.put("");
                return;
            }

            File file = new File(Environment.getExternalStorageDirectory(), sImageName);
            sImageName = "";
            mFilePickerResult.put(file.getAbsolutePath());
        } catch (InterruptedException e) {
            Log.i(LOGTAG, "error returning file picker result", e);
        }
    }

    // this code is really hacky and doesn't belong anywhere so I'm putting it here for now
    // until I can come up with a better solution.

    private static String sImageName = "";

    static String generateImageName() {
        Time now = new Time();
        now.setToNow();
        sImageName = now.format("%Y-%m-%d %H.%M.%S") + ".jpg";
        return sImageName;
    }
}
