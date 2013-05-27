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

import java.util.Queue;

class CameraVideoResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoCameraVideoResultHandler";

    private final Queue<String> mFilePickerResult;

    CameraVideoResultHandler(Queue<String> resultQueue) {
        mFilePickerResult = resultQueue;
    }

    @Override
    public void onActivityResult(int resultCode, Intent data) {
        if (data == null || resultCode != Activity.RESULT_OK) {
            mFilePickerResult.offer("");
            return;
        }

        Cursor cursor = GeckoAppShell.getGeckoInterface().getActivity().managedQuery(data.getData(),
                new String[] { MediaStore.Video.Media.DATA },
                null,
                null,
                null);
        cursor.moveToFirst();
        mFilePickerResult.offer(cursor.getString(
            cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA)));
    }
}
