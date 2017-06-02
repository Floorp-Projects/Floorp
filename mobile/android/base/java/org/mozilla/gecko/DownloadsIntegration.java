/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.io.File;
import java.lang.IllegalArgumentException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

public class DownloadsIntegration implements BundleEventListener
{
    private static final String LOGTAG = "GeckoDownloadsIntegration";

    private static final List<String> UNKNOWN_MIME_TYPES;
    static {
        final ArrayList<String> tempTypes = new ArrayList<>(3);
        tempTypes.add("unknown/unknown"); // This will be used as a default mime type for unknown files
        tempTypes.add("application/unknown");
        tempTypes.add("application/octet-stream"); // Github uses this for APK files
        UNKNOWN_MIME_TYPES = Collections.unmodifiableList(tempTypes);
    }

    private static final String DOWNLOAD_REMOVE = "Download:Remove";

    private DownloadsIntegration() {
        EventDispatcher.getInstance().registerBackgroundThreadListener(this, DOWNLOAD_REMOVE);
    }

    private static DownloadsIntegration sInstance;

    private static class Download {
        final File file;
        final long id;

        final private static int UNKNOWN_ID = -1;

        public Download(final String path) {
            this(path, UNKNOWN_ID);
        }

        public Download(final String path, final long id) {
            file = new File(path);
            this.id = id;
        }

        public static Download fromBundle(final GeckoBundle obj) {
            final String path = obj.getString("path");
            return new Download(path);
        }

        public static Download fromCursor(final Cursor c) {
            final String path = c.getString(c.getColumnIndexOrThrow(DownloadManager.COLUMN_LOCAL_FILENAME));
            final long id = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_ID));
            return new Download(path, id);
        }

        public boolean equals(final Download download) {
            return file.equals(download.file);
        }
    }

    public static void init() {
        if (sInstance == null) {
            sInstance = new DownloadsIntegration();
        }
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (DOWNLOAD_REMOVE.equals(event)) {
            final Download d = Download.fromBundle(message);
            removeDownload(d);
        }
    }

    private static boolean useSystemDownloadManager() {
        if (!AppConstants.ANDROID_DOWNLOADS_INTEGRATION) {
            return false;
        }

        int state = PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
        try {
            final PackageManager pm = GeckoAppShell.getApplicationContext()
                                                   .getPackageManager();
            state = pm.getApplicationEnabledSetting("com.android.providers.downloads");
        } catch (IllegalArgumentException e) {
            // Download Manager package does not exist
            return false;
        }

        return (PackageManager.COMPONENT_ENABLED_STATE_ENABLED == state ||
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT == state);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static String getTemporaryDownloadDirectory() {
        Context context = GeckoAppShell.getApplicationContext();

        if (Permissions.has(context, android.Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
            // We do have the STORAGE permission, so we can save the file directly to the public
            // downloads directory.
            return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                    .getAbsolutePath();
        } else {
            // Without the permission we are going to start to download the file to the cache
            // directory. Later in the process we will ask for the permission and the download
            // process will move the file to the actual downloads directory. If we do not get the
            // permission then the download will be cancelled.
            return context.getCacheDir().getAbsolutePath();
        }
    }


    @WrapForJNI(calledFrom = "gecko")
    public static void scanMedia(final String aFile, String aMimeType) {
        String mimeType = aMimeType;
        if (UNKNOWN_MIME_TYPES.contains(mimeType)) {
            // If this is a generic undefined mimetype, erase it so that we can try to determine
            // one from the file extension below.
            mimeType = "";
        }

        // If the platform didn't give us a mimetype, try to guess one from the filename
        if (TextUtils.isEmpty(mimeType)) {
            final int extPosition = aFile.lastIndexOf(".");
            if (extPosition > 0 && extPosition < aFile.length() - 1) {
                mimeType = GeckoAppShell.getMimeTypeFromExtension(aFile.substring(extPosition + 1));
            }
        }

        // addCompletedDownload will throw if it received any null parameters. Use aMimeType or a default
        // if we still don't have one.
        if (TextUtils.isEmpty(mimeType)) {
            if (TextUtils.isEmpty(aMimeType)) {
                mimeType = UNKNOWN_MIME_TYPES.get(0);
            } else {
                mimeType = aMimeType;
            }
        }

        if (useSystemDownloadManager()) {
            final File f = new File(aFile);
            final DownloadManager dm = (DownloadManager)
                    GeckoAppShell.getApplicationContext()
                                 .getSystemService(Context.DOWNLOAD_SERVICE);
            dm.addCompletedDownload(f.getName(),
                    f.getName(),
                    true, // Media scanner should scan this
                    mimeType,
                    f.getAbsolutePath(),
                    Math.max(1, f.length()), // Some versions of Android require downloads to be at least length 1
                    false); // Don't show a notification.
        } else {
            final Activity currentActivity =
                    GeckoActivityMonitor.getInstance().getCurrentActivity();
            if (currentActivity == null) {
                return;
            }
            final GeckoMediaScannerClient client =
                    new GeckoMediaScannerClient(currentActivity, aFile, mimeType);
            client.connect();
        }
    }

    public static void removeDownload(final Download download) {
        if (!useSystemDownloadManager()) {
            return;
        }

        final DownloadManager dm = (DownloadManager)
                GeckoAppShell.getApplicationContext()
                             .getSystemService(Context.DOWNLOAD_SERVICE);

        Cursor c = null;
        try {
            c = dm.query((new DownloadManager.Query()).setFilterByStatus(DownloadManager.STATUS_SUCCESSFUL));
            if (c == null || !c.moveToFirst()) {
                return;
            }

            do {
                final Download d = Download.fromCursor(c);
                // Try hard as we can to verify this download is the one we think it is
                if (download.equals(d)) {
                    dm.remove(d.id);
                }
            } while (c.moveToNext());
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    private static final class GeckoMediaScannerClient implements MediaScannerConnectionClient {
        private final String mFile;
        private final String mMimeType;
        private MediaScannerConnection mScanner;

        public GeckoMediaScannerClient(Context context, String file, String mimeType) {
            mFile = file;
            mMimeType = mimeType;
            mScanner = new MediaScannerConnection(context, this);
        }

        public void connect() {
            mScanner.connect();
        }

        @Override
        public void onMediaScannerConnected() {
            mScanner.scanFile(mFile, mMimeType);
        }

        @Override
        public void onScanCompleted(String path, Uri uri) {
            if (path.equals(mFile)) {
                mScanner.disconnect();
                mScanner = null;
            }
        }
    }
}
