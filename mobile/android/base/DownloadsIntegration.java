/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

public class DownloadsIntegration
{
    private static final String LOGTAG = "GeckoDownloadsIntegration";

    @SuppressWarnings("serial")
    private static final List<String> UNKNOWN_MIME_TYPES = new ArrayList<String>(3) {{
        add("unknown/unknown"); // This will be used as a default mime type for unknown files
        add("application/unknown");
        add("application/octet-stream"); // Github uses this for APK files
    }};

    @WrapElementForJNI
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
                mimeType = GeckoAppShell.getMimeTypeFromExtension(aFile.substring(extPosition+1));
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

        if (AppConstants.ANDROID_DOWNLOADS_INTEGRATION) {
            final File f = new File(aFile);
            final DownloadManager dm = (DownloadManager) GeckoAppShell.getContext().getSystemService(Context.DOWNLOAD_SERVICE);
            dm.addCompletedDownload(f.getName(),
                    f.getName(),
                    true, // Media scanner should scan this
                    mimeType,
                    f.getAbsolutePath(),
                    Math.max(1, f.length()), // Some versions of Android require downloads to be at least length 1
                    false); // Don't show a notification.
        } else {
            final Context context = GeckoAppShell.getContext();
            final GeckoMediaScannerClient client = new GeckoMediaScannerClient(context, aFile, mimeType);
            client.connect();
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
            if(path.equals(mFile)) {
                mScanner.disconnect();
                mScanner = null;
            }
        }
    }
}
