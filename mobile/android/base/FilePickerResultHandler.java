/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.ActivityResultHandler;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.CursorLoader;
import android.support.v4.content.Loader;
import android.text.format.Time;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.util.Queue;

class FilePickerResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoFilePickerResultHandler";

    protected final Queue<String> mFilePickerResult;
    protected final FilePicker.ResultHandler mHandler;

    // this code is really hacky and doesn't belong anywhere so I'm putting it here for now
    // until I can come up with a better solution.
    private String mImageName = "";

    public FilePickerResultHandler(Queue<String> resultQueue) {
        mFilePickerResult = resultQueue;
        mHandler = null;
    }

    /* Use this constructor to asynchronously listen for results */
    public FilePickerResultHandler(FilePicker.ResultHandler handler) {
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
    public void onActivityResult(int resultCode, Intent intent) {
        if (resultCode != Activity.RESULT_OK) {
            sendResult("");
            return;
        }

        // Camera results won't return an Intent. Use the file name we passed to the original intent.
        if (intent == null) {
            if (mImageName != null) {
                File file = new File(Environment.getExternalStorageDirectory(), mImageName);
                sendResult(file.getAbsolutePath());
            } else {
                sendResult("");
            }
            return;
        }

        Uri uri = intent.getData();
        if (uri == null) {
            sendResult("");
            return;
        }

        // Some file pickers may return a file uri
        if ("file".equals(uri.getScheme())) {
            String path = uri.getPath();
            sendResult(path == null ? "" : path);
            return;
        }

        final FragmentActivity fa = (FragmentActivity) GeckoAppShell.getGeckoInterface().getActivity();
        final LoaderManager lm = fa.getSupportLoaderManager();
        // Finally, Video pickers and some file pickers may return a content provider.
        Cursor cursor = null;
        try {
            // Try a query to make sure the expected columns exist
            final ContentResolver cr = fa.getContentResolver();
            cursor = cr.query(uri, new String[] { MediaStore.Video.Media.DATA }, null, null, null);

            int index = cursor.getColumnIndex(MediaStore.Video.Media.DATA);
            if (index >= 0) {
                lm.initLoader(intent.hashCode(), null, new VideoLoaderCallbacks(uri));
                return;
            }
        } catch(Exception ex) {
            // We'll try a different loader below
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        lm.initLoader(uri.hashCode(), null, new FileLoaderCallbacks(uri));
        return;
    }

    public String generateImageName() {
        Time now = new Time();
        now.setToNow();
        mImageName = now.format("%Y-%m-%d %H.%M.%S") + ".jpg";
        return mImageName;
    }

    private class VideoLoaderCallbacks implements LoaderCallbacks<Cursor> {
        final private Uri mUri;
        public VideoLoaderCallbacks(Uri uri) {
            mUri = uri;
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final FragmentActivity fa = (FragmentActivity) GeckoAppShell.getGeckoInterface().getActivity();
            return new CursorLoader(fa,
                                    mUri,
                                    new String[] { MediaStore.Video.Media.DATA },
                                    null,  // selection
                                    null,  // selectionArgs
                                    null); // sortOrder
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            if (cursor.moveToFirst()) {
                String res = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA));
                sendResult(res);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) { }
    }

    private class FileLoaderCallbacks implements LoaderCallbacks<Cursor> {
        final private Uri mUri;
        public FileLoaderCallbacks(Uri uri) {
            mUri = uri;
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final FragmentActivity fa = (FragmentActivity) GeckoAppShell.getGeckoInterface().getActivity();
            return new CursorLoader(fa,
                                    mUri,
                                    new String[] { OpenableColumns.DISPLAY_NAME },
                                    null,  // selection
                                    null,  // selectionArgs
                                    null); // sortOrder
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            if (cursor.moveToFirst()) {
                String name = cursor.getString(0);
                // tmp filenames must be at least 3 characters long. Add a prefix to make sure that happens
                String fileName = "tmp_";
                String fileExt = null;
                int period;

                final FragmentActivity fa = (FragmentActivity) GeckoAppShell.getGeckoInterface().getActivity();
                final ContentResolver cr = fa.getContentResolver();

                // Generate an extension if we don't already have one
                if (name == null || (period = name.lastIndexOf('.')) == -1) {
                    String mimeType = cr.getType(mUri);
                    fileExt = "." + GeckoAppShell.getExtensionFromMimeType(mimeType);
                } else {
                    fileExt = name.substring(period);
                    fileName += name.substring(0, period);
                }

                // Now write the data to the temp file
                try {
                    File file = File.createTempFile(fileName, fileExt, GeckoLoader.getGREDir(GeckoAppShell.getContext()));
                    FileOutputStream fos = new FileOutputStream(file);
                    InputStream is = cr.openInputStream(mUri);
                    byte[] buf = new byte[4096];
                    int len = is.read(buf);
                    while (len != -1) {
                        fos.write(buf, 0, len);
                        len = is.read(buf);
                    }
                    fos.close();

                    String path = file.getAbsolutePath();
                    sendResult((path == null) ? "" : path);
                } catch(IOException ex) {
                    Log.i(LOGTAG, "Error writing file", ex);
                }
            } else {
                sendResult("");
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) { }
    }

}

