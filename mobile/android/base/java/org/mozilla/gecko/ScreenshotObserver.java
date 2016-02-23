/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.util.ThreadUtils;

import android.Manifest;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;

public class ScreenshotObserver {
    private static final String LOGTAG = "GeckoScreenshotObserver";
    public Context context;

    /**
     * Listener for screenshot changes.
     */
    public interface OnScreenshotListener {
        /**
         * This callback is executed on the UI thread.
         */
        public void onScreenshotTaken(String data, String title);
    }

    private OnScreenshotListener listener;

    public ScreenshotObserver() {
    }

    public void setListener(Context context, OnScreenshotListener listener) {
        this.context = context;
        this.listener = listener;
    }

    private MediaObserver mediaObserver;
    private String[] mediaProjections = new String[] {
                    MediaStore.Images.ImageColumns.DATA,
                    MediaStore.Images.ImageColumns.DISPLAY_NAME,
                    MediaStore.Images.ImageColumns.BUCKET_DISPLAY_NAME,
                    MediaStore.Images.ImageColumns.DATE_TAKEN,
                    MediaStore.Images.ImageColumns.TITLE
    };

    /**
     * Start ScreenshotObserver if this device is supported and all required runtime permissions
     * have been granted by the user. Calling this method will not prompt for permissions.
     */
    public void start() {
        if (!Versions.feature14Plus) {
            return;
        }

        Permissions.from(context)
                   .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                   .doNotPrompt()
                   .run(startObserverRunnable());
    }

    private Runnable startObserverRunnable() {
        return new Runnable() {
            @Override
            public void run() {
                try {
                    if (mediaObserver == null) {
                        mediaObserver = new MediaObserver();
                        context.getContentResolver().registerContentObserver(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, false, mediaObserver);
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "Failure to start watching media: ", e);
                }
            }
        };
    }

    public void stop() {
        if (!Versions.feature14Plus) {
            return;
        }

        if (mediaObserver == null) {
            return;
        }

        try {
            context.getContentResolver().unregisterContentObserver(mediaObserver);
            mediaObserver = null;
        } catch (Exception e) {
            Log.e(LOGTAG, "Failure to stop watching media: ", e);
        }
    }

    public void onMediaChange(final Uri uri) {
        // Make sure we are on not on the main thread.
        final ContentResolver cr = context.getContentResolver();
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Find the most recent image added to the MediaStore and see if it's a screenshot.
                final Cursor cursor = cr.query(uri, mediaProjections, null, null, MediaStore.Images.ImageColumns.DATE_ADDED + " DESC LIMIT 1");
                try {
                    if (cursor == null) {
                        return;
                    }

                    while (cursor.moveToNext()) {
                        String data = cursor.getString(0);
                        Log.i(LOGTAG, "data: " + data);
                        String display = cursor.getString(1);
                        Log.i(LOGTAG, "display: " + display);
                        String album = cursor.getString(2);
                        Log.i(LOGTAG, "album: " + album);
                        long date = cursor.getLong(3);
                        String title = cursor.getString(4);
                        Log.i(LOGTAG, "title: " + title);
                        if (album != null && album.toLowerCase().contains("screenshot")) {
                            if (listener != null) {
                                listener.onScreenshotTaken(data, title);
                                break;
                            }
                        }
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "Failure to process media change: ", e);
                } finally {
                    if (cursor != null) {
                        cursor.close();
                    }
                }
            }
        });
    }

    private class MediaObserver extends ContentObserver {
        public MediaObserver() {
            super(null);
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);
            onMediaChange(MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        }
    }
}
