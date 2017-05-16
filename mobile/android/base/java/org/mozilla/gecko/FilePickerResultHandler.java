/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Process;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.CursorLoader;
import android.support.v4.content.Loader;
import android.text.TextUtils;
import android.text.format.Time;
import android.util.Log;

class FilePickerResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoFilePickerResultHandler";
    private static final String UPLOADS_DIR = "uploads";

    private final FilePicker.ResultHandler handler;
    private final int tabId;
    private final File cacheDir;

    // this code is really hacky and doesn't belong anywhere so I'm putting it here for now
    // until I can come up with a better solution.
    private String mImageName = "";

    /* Use this constructor to asynchronously listen for results */
    public FilePickerResultHandler(final FilePicker.ResultHandler handler, final Context context, final int tabId) {
        this.tabId = tabId;
        this.cacheDir = new File(context.getCacheDir(), UPLOADS_DIR);
        this.handler = handler;
    }

    void sendResult(String res) {
        if (handler != null) {
            handler.gotFile(res);
        }
    }

    private <T> void initLoader(final LoaderCallbacks<T> callbacks) {
        final Loader<T> loader = callbacks.onCreateLoader(/* id */ 0, /* args */ null);
        loader.registerListener(/* id */ 0, new Loader.OnLoadCompleteListener<T>() {
            @Override
            public void onLoadComplete(final Loader<T> loader, final T data) {
                callbacks.onLoadFinished(loader, data);
                loader.unregisterListener(this);
            }
        });
        loader.startLoading();
    }

    @Override
    public void onActivityResult(int resultCode, Intent intent) {
        if (resultCode != Activity.RESULT_OK) {
            sendResult("");
            return;
        }

        // Camera results won't return an Intent. Use the file name we passed to the original intent.
        // In Android M, camera results return an empty Intent rather than null.
        if (intent == null || (intent.getAction() == null && intent.getData() == null)) {
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

        final Context context = GeckoAppShell.getApplicationContext();

        // Finally, Video pickers and some file pickers may return a content provider.
        final ContentResolver cr = context.getContentResolver();
        final Cursor cursor = cr.query(uri, new String[] { MediaStore.Video.Media.DATA }, null, null, null);
        if (cursor != null) {
            try {
                // Try a query to make sure the expected columns exist
                int index = cursor.getColumnIndex(MediaStore.Video.Media.DATA);
                if (index >= 0) {
                    initLoader(new VideoLoaderCallbacks(uri));
                    return;
                }
            } catch (Exception ex) {
                // We'll try a different loader below
            } finally {
                cursor.close();
            }
        }

        initLoader(new FileLoaderCallbacks(uri, cacheDir, tabId));
    }

    public String generateImageName() {
        Time now = new Time();
        now.setToNow();
        mImageName = now.format("%Y-%m-%d %H.%M.%S") + ".jpg";
        return mImageName;
    }

    private class VideoLoaderCallbacks implements LoaderCallbacks<Cursor> {
        final private Uri uri;
        public VideoLoaderCallbacks(Uri uri) {
            this.uri = uri;
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final Context context = GeckoAppShell.getApplicationContext();
            return new CursorLoader(context,
                                    uri,
                                    new String[] { MediaStore.Video.Media.DATA },
                                    null,  // selection
                                    null,  // selectionArgs
                                    null); // sortOrder
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            if (cursor.moveToFirst()) {
                String res = cursor.getString(cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA));

                // Some pickers (the KitKat Documents one for instance) won't return a temporary file here.
                // Fall back to the normal FileLoader if we didn't find anything.
                if (TextUtils.isEmpty(res)) {
                    tryFileLoaderCallback();
                    return;
                }

                sendResult(res);
            } else {
                tryFileLoaderCallback();
            }
        }

        private void tryFileLoaderCallback() {
            initLoader(new FileLoaderCallbacks(uri, cacheDir, tabId));
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) { }
    }

    /**
     * This class's only dependency on FilePickerResultHandler is sendResult.
     */
    private class FileLoaderCallbacks implements LoaderCallbacks<Cursor>,
                                                 Tabs.OnTabsChangedListener {
        private final Uri uri;
        private final File cacheDir;
        private final int tabId;
        private File tempDir;

        public FileLoaderCallbacks(Uri uri, File cacheDir, int tabId) {
            this.uri = uri;
            this.cacheDir = cacheDir;
            this.tabId = tabId;
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final Context context = GeckoAppShell.getApplicationContext();
            return new CursorLoader(context,
                                    uri,
                                    new String[] { OpenableColumns.DISPLAY_NAME },
                                    null,  // selection
                                    null,  // selectionArgs
                                    null); // sortOrder
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            if (cursor.moveToFirst()) {
                String fileName = cursor.getString(0);

                final Context context = GeckoAppShell.getApplicationContext();
                final ContentResolver cr = context.getContentResolver();

                // Generate an extension if we don't already have one
                if (fileName == null || fileName.lastIndexOf('.') == -1) {
                    String mimeType = cr.getType(uri);
                    String fileExt = "." + GeckoAppShell.getExtensionFromMimeType(mimeType);
                    if (fileName == null) {
                        // tmp filenames must be at least 3 characters long. Add a prefix to make sure that happens
                        fileName = "tmp_" + Process.myPid() + fileExt;
                    } else {
                        fileName += fileExt;
                    }
                }

                // Now write the data to the temp file
                FileOutputStream fos = null;
                try {
                    tempDir = FileUtils.createTempDir(cacheDir, "tmp_");

                    File file = new File(tempDir, fileName);
                    fos = new FileOutputStream(file);
                    InputStream is = cr.openInputStream(uri);
                    byte[] buf = new byte[4096];
                    int len = is.read(buf);
                    while (len != -1) {
                        fos.write(buf, 0, len);
                        len = is.read(buf);
                    }
                    fos.close();
                    is.close();
                    String tempFile = file.getAbsolutePath();
                    sendResult((tempFile == null) ? "" : tempFile);

                    if (tabId > -1 && tempDir != null) {
                        Tabs.registerOnTabsChangedListener(this);
                    }
                } catch (IOException ex) {
                    Log.i(LOGTAG, "Error writing file", ex);
                } finally {
                    if (fos != null) {
                        try {
                            fos.close();
                        } catch (IOException e) { /* not much to do here */ }
                    }
                }
            } else {
                sendResult("");
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) { }

        /*Tabs.OnTabsChangedListener*/
        // This cleans up our temp folder. If it doesn't run, we just hope that Android
        // will eventually does the cleanup for us.
        @Override
        public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
            if ((tab == null) || (tab.getId() != tabId)) {
                return;
            }

            if (msg == Tabs.TabEvents.LOCATION_CHANGE ||
                msg == Tabs.TabEvents.CLOSED) {
                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        FileUtils.delete(tempDir, true);
                    }
                });

                // Tabs' listener array is safe to modify during use: its
                // iteration pattern is based on snapshots.
                Tabs.unregisterOnTabsChangedListener(this);
            }
        }
    }

}

