/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.broadcastreceiver;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.FileProvider;
import android.view.View;
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.mozilla.focus.utils.IntentUtils;

import java.io.File;
import java.util.HashSet;

/**
 * BroadcastReceiver for finished downloads
 */
public class DownloadBroadcastReceiver extends BroadcastReceiver {
    private static final String FILE_SCHEME = "file://";
    private static final String FILE_PROVIDER_EXTENSION = ".fileprovider";

    private final HashSet<Long> queuedDownloadReferences = new HashSet<>();
    private final View browserContainer;
    private final DownloadManager downloadManager;

    public DownloadBroadcastReceiver(View view, DownloadManager downloadManager) {
        this.browserContainer = view;
        this.downloadManager = downloadManager;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final long downloadReference = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);
        displaySnackbar(context, downloadReference, downloadManager);
    }

    private void displaySnackbar(final Context context, long completedDownloadReference, DownloadManager downloadManager) {
        if (!isFocusDownload(completedDownloadReference)) {
            return;
        }

        final DownloadManager.Query query = new DownloadManager.Query();
        query.setFilterById(completedDownloadReference);
        try (Cursor cursor = downloadManager.query(query)) {
            if (cursor.moveToFirst()) {
                int statusColumnIndex = cursor.getColumnIndex(DownloadManager.COLUMN_STATUS);
                if (DownloadManager.STATUS_SUCCESSFUL == cursor.getInt(statusColumnIndex)) {
                    String uriString = cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_LOCAL_URI));

                    final String localUri = uriString.startsWith(FILE_SCHEME) ? uriString.substring(FILE_SCHEME.length()) : uriString;
                    final String fileExtension = MimeTypeMap.getFileExtensionFromUrl(localUri);
                    final String mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(fileExtension);
                    final String fileName = URLUtil.guessFileName(Uri.decode(localUri), null, mimeType);

                    final File file = new File(Uri.decode(localUri));
                    final Uri uriForFile = FileProvider.getUriForFile(context, BuildConfig.APPLICATION_ID + FILE_PROVIDER_EXTENSION, file);
                    final Intent openFileIntent = IntentUtils.INSTANCE.createOpenFileIntent(uriForFile, mimeType);
                    showSnackbarForFilename(openFileIntent, context, fileName);
                }
            }
        }
        removeFromHashSet(completedDownloadReference);
    }

    private void showSnackbarForFilename(final Intent openFileIntent, final Context context, String fileName) {
        final Snackbar snackbar = Snackbar
                .make(browserContainer, String.format(context.getString(R.string.download_snackbar_finished), fileName), Snackbar.LENGTH_LONG);
        if (IntentUtils.INSTANCE.activitiesFoundForIntent(context, openFileIntent)) {
            snackbar.setAction(context.getString(R.string.download_snackbar_open), new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    context.startActivity(openFileIntent);
                }
            });
            snackbar.setActionTextColor(ContextCompat.getColor(context, R.color.snackbarActionText));
        }
        snackbar.show();
    }

    private boolean isFocusDownload(long completedDownloadReference) {
        return (queuedDownloadReferences.contains(completedDownloadReference));
    }

    private void removeFromHashSet(long completedDownloadReference) {
        queuedDownloadReferences.remove(completedDownloadReference);
    }

    public void addQueuedDownload(long referenceId) {
        queuedDownloadReferences.add(referenceId);
    }
}
