/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.provider.MediaStore;
import android.util.Log;

import java.util.concurrent.SynchronousQueue;

class CameraVideoResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoCameraVideoResultHandler";

    private final SynchronousQueue<String> mFilePickerResult;

    CameraVideoResultHandler(SynchronousQueue<String> resultQueue) {
        mFilePickerResult = resultQueue;
    }

    public void onActivityResult(int resultCode, Intent data) {
        try {
            if (data == null || resultCode != Activity.RESULT_OK) {
                mFilePickerResult.put("");
                return;
            }

            Cursor cursor = GeckoApp.mAppContext.managedQuery(data.getData(),
                    new String[] { MediaStore.Video.Media.DATA },
                    null,
                    null,
                    null);
            cursor.moveToFirst();
            mFilePickerResult.put(cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA)));
        } catch (InterruptedException e) {
            Log.i(LOGTAG, "error returning file picker result", e);
        }
    }
}
