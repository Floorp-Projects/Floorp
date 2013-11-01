/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;

import android.app.Activity;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.CursorLoader;
import android.support.v4.content.Loader;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;

import java.util.Queue;

class CameraVideoResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoCameraVideoResultHandler";

    private final Queue<String> mFilePickerResult;
    private final ActivityHandlerHelper.FileResultHandler mHandler;

    CameraVideoResultHandler(Queue<String> resultQueue) {
        mFilePickerResult = resultQueue;
        mHandler = null;
    }

    /* Use this constructor to asynchronously listen for results */
    public CameraVideoResultHandler(ActivityHandlerHelper.FileResultHandler handler) {
        mFilePickerResult = null;
        mHandler = handler;
    }

    private void sendResult(String res) {
        if (mFilePickerResult != null)
            mFilePickerResult.offer(res);

        if (mHandler != null)
            mHandler.gotFile(res);
    }

    @Override
    public void onActivityResult(int resultCode, final Intent data) {
        // Intent.getData() can return null. Avoid a crash. See bug 904551.
        if (data == null || data.getData() == null || resultCode != Activity.RESULT_OK) {
            sendResult("");
            return;
        }

        final FragmentActivity fa = (FragmentActivity) GeckoAppShell.getGeckoInterface().getActivity();
        final LoaderManager lm = fa.getSupportLoaderManager();
        lm.initLoader(data.hashCode(), null, new LoaderCallbacks<Cursor>() {
            @Override
            public Loader<Cursor> onCreateLoader(int id, Bundle args) {
                return new CursorLoader(fa,
                                        data.getData(),
                                        new String[] { MediaStore.Video.Media.DATA },
                                        null,  // selection
                                        null,  // selectionArgs
                                        null); // sortOrder
            }

            @Override
            public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
                if (cursor.moveToFirst()) {
                    sendResult(cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA)));
                } else {
                    sendResult("");
                }
            }

            @Override
            public void onLoaderReset(Loader<Cursor> loader) { }
        });
    }
}
