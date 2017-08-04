/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.util.HardwareUtils;

import android.app.IntentService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Service to handle downloadable content that did not ship with the APK.
 */
public class DownloadContentService extends IntentService {
    private static final String LOGTAG = "GeckoDLCService";

    /**
     * Study: Scan the catalog for "new" content available for download.
     */
    private static final String ACTION_STUDY_CATALOG = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.STUDY";

    /**
     * Verify: Validate downloaded content. Does it still exist and does it have the correct checksum?
     */
    private static final String ACTION_VERIFY_CONTENT = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.VERIFY";

    /**
     * Download content that has been scheduled during "study" or "verify".
     */
    private static final String ACTION_DOWNLOAD_CONTENT = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.DOWNLOAD";

    /**
     * Sync: Synchronize catalog from a Kinto instance.
     */
    private static final String ACTION_SYNCHRONIZE_CATALOG = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.SYNC";

    /**
     * CleanupAction: Remove content that is no longer needed (e.g. Removed from the catalog after a sync).
     */
    private static final String ACTION_CLEANUP_FILES = AppConstants.ANDROID_PACKAGE_NAME + ".DLC.CLEANUP";

    public static void startStudy(Context context) {
        Intent intent = new Intent(ACTION_STUDY_CATALOG);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startVerification(Context context) {
        Intent intent = new Intent(ACTION_VERIFY_CONTENT);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startDownloads(Context context) {
        Intent intent = new Intent(ACTION_DOWNLOAD_CONTENT);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startSync(Context context) {
        Intent intent = new Intent(ACTION_SYNCHRONIZE_CATALOG);
        intent.setComponent(new ComponentName(context, DownloadContentService.class));
        context.startService(intent);
    }

    public static void startCleanup(Context context) {
        Intent intent = new Intent(ACTION_CLEANUP_FILES);
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

        if (!HardwareUtils.isSupportedSystem()) {
            // This service is running very early before checks in BrowserApp can prevent us from running.
            Log.w(LOGTAG, "System is not supported. Stop.");
            return;
        }

        if (intent == null) {
            return;
        }

        final BaseAction action;

        switch (intent.getAction()) {
            case ACTION_STUDY_CATALOG:
                action = new StudyAction();
                break;

            case ACTION_DOWNLOAD_CONTENT:
                action = new DownloadAction(new DownloadAction.Callback() {
                    @Override
                    public void onContentDownloaded(DownloadContent content) {
                        if (content.isFont()) {
                            EventDispatcher.getInstance().dispatch("Fonts:Reload", null);
                        }
                    }
                });
                break;

            case ACTION_VERIFY_CONTENT:
                action = new VerifyAction();
                break;

            case ACTION_SYNCHRONIZE_CATALOG:
                action = new SyncAction();
                break;

            default:
                Log.e(LOGTAG, "Unknown action: " + intent.getAction());
                return;
        }

        action.perform(this, catalog);
        catalog.persistChanges();
    }
}
