/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.dlc.DownloadContentHelper;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;

import android.app.IntentService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import ch.boye.httpclientandroidlib.client.HttpClient;

import java.io.File;

/**
 * Service to handle downloadable content that did not ship with the APK.
 */
public class DownloadContentService extends IntentService {
    private static final String LOGTAG = "GeckoDLCService";

    public static void startStudy(Context context) {
        Intent intent = new Intent(DownloadContentHelper.ACTION_STUDY_CATALOG);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startVerification(Context context) {
        Intent intent = new Intent(DownloadContentHelper.ACTION_VERIFY_CONTENT);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startDownloads(Context context) {
        Intent intent = new Intent(DownloadContentHelper.ACTION_DOWNLOAD_CONTENT);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    private DownloadContentCatalog catalog;

    public DownloadContentService() {
        super(LOGTAG);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        catalog = new DownloadContentCatalog(this);
    }

    protected void onHandleIntent(Intent intent) {
        if (!AppConstants.MOZ_ANDROID_DOWNLOAD_CONTENT_SERVICE) {
            Log.w(LOGTAG, "Download content is not enabled. Stop.");
            return;
        }

        if (intent == null) {
            return;
        }

        switch (intent.getAction()) {
            case DownloadContentHelper.ACTION_STUDY_CATALOG:
                studyCatalog();
                break;

            case DownloadContentHelper.ACTION_DOWNLOAD_CONTENT:
                downloadContent();
                break;

            case DownloadContentHelper.ACTION_VERIFY_CONTENT:
                verifyCatalog();
                break;

            default:
                Log.e(LOGTAG, "Unknown action: " + intent.getAction());
        }

        catalog.persistChanges();
    }

    /**
     * Study: Scan the catalog for "new" content available for download.
     */
    private void studyCatalog() {
        Log.d(LOGTAG, "Studying catalog..");

        for (DownloadContent content : catalog.getContentWithoutState()) {
            if (content.isAssetArchive() && content.isFont()) {
                catalog.scheduleDownload(content);

                Log.d(LOGTAG, "Scheduled download: " + content);
            }
        }

        if (catalog.hasScheduledDownloads()) {
            startDownloads(this);
        }

        Log.v(LOGTAG, "Done");
    }

    /**
     * Verify: Validate downloaded content. Does it still exist and does it have the correct checksum?
     */
    private void verifyCatalog() {
        Log.d(LOGTAG, "Verifying catalog..");

        for (DownloadContent content : catalog.getDownloadedContent()) {
            try {
                File destinationFile = DownloadContentHelper.getDestinationFile(this, content);

                if (!destinationFile.exists()) {
                    Log.d(LOGTAG, "Downloaded content does not exist anymore: " + content);

                    // This file does not exist even though it is marked as downloaded in the catalog. Scheduling a
                    // download to fetch it again.
                    catalog.scheduleDownload(content);
                }

                if (!DownloadContentHelper.verify(destinationFile, content.getChecksum())) {
                    catalog.scheduleDownload(content);
                    Log.d(LOGTAG, "Wrong checksum. Scheduling download: " + content);
                    continue;
                }

                Log.v(LOGTAG, "Content okay: " + content);
            } catch (DownloadContentHelper.UnrecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Unrecoverable exception while verifying downloaded file", e);
            } catch (DownloadContentHelper.RecoverableDownloadContentException e) {
                // That's okay, we are just verifying already existing content. No log.
            }
        }

        if (catalog.hasScheduledDownloads()) {
            startDownloads(this);
        }

        Log.v(LOGTAG, "Done");
    }

    /**
     * Download content that has been scheduled during "study" or "verify".
     */
    private void downloadContent() {
        Log.d(LOGTAG, "Downloading content..");

        if (DownloadContentHelper.isActiveNetworkMetered(this)) {
            Log.d(LOGTAG, "Network is metered. Postponing download.");
            // TODO: Reschedule download (bug 1209498)
            return;
        }

        HttpClient client = DownloadContentHelper.buildHttpClient();

        for (DownloadContent content : catalog.getScheduledDownloads()) {
            Log.d(LOGTAG, "Downloading: " + content);

            File temporaryFile = null;

            try {
                File destinationFile = DownloadContentHelper.getDestinationFile(this, content);
                if (destinationFile.exists() && DownloadContentHelper.verify(destinationFile, content.getChecksum())) {
                    Log.d(LOGTAG, "Content already exists and is up-to-date.");
                    continue;
                }

                temporaryFile = DownloadContentHelper.createTemporaryFile(this, content);

                // TODO: Check space on disk before downloading content (bug 1220145)
                final String url = DownloadContentHelper.createDownloadURL(content);
                DownloadContentHelper.download(client, url, temporaryFile);

                if (!DownloadContentHelper.verify(temporaryFile, content.getDownloadChecksum())) {
                    Log.w(LOGTAG, "Wrong checksum after download, content=" + content.getId());
                    temporaryFile.delete();
                    continue;
                }

                if (!content.isAssetArchive()) {
                    Log.e(LOGTAG, "Downloaded content is not of type 'asset-archive': " + content.getType());
                    continue;
                }

                DownloadContentHelper.extract(temporaryFile, destinationFile, content.getChecksum());

                catalog.markAsDownloaded(content);

                Log.d(LOGTAG, "Successfully downloaded: " + content);

                onContentDownloaded(content);
            } catch (DownloadContentHelper.RecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Downloading content failed (Recoverable): " + content, e);
                // TODO: Reschedule download (bug 1209498)
            } catch (DownloadContentHelper.UnrecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Downloading content failed (Unrecoverable): " + content, e);

                catalog.markAsPermanentlyFailed(content);
            } finally {
                if (temporaryFile != null && temporaryFile.exists()) {
                    temporaryFile.delete();
                }
            }
        }

        Log.v(LOGTAG, "Done");
    }

    private void onContentDownloaded(DownloadContent content) {
        if (content.isFont()) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Fonts:Reload", ""));
        }
    }
}
