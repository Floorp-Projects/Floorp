/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.util.ContextUtils;

/**
 * Study: Scan the catalog for "new" content available for download.
 */
public class StudyAction extends BaseAction {
    private static final String LOGTAG = "DLCStudyAction";

    public void perform(Context context, DownloadContentCatalog catalog) {
        Log.d(LOGTAG, "Studying catalog..");

        for (DownloadContent content : catalog.getContentToStudy()) {
            if (!isMatching(context, content)) {
                // This content is not for this particular version of the application or system
                continue;
            }

            if (content.isKnownContent()) {
                catalog.scheduleDownload(content);

                Log.d(LOGTAG, "Scheduled download: " + content);
            }
        }

        if (catalog.hasScheduledDownloads()) {
            startDownloads(context);
        }

        Log.v(LOGTAG, "Done");
    }

    protected boolean isMatching(Context context, DownloadContent content) {
        final String androidApiPattern = content.getAndroidApiPattern();
        if (!TextUtils.isEmpty(androidApiPattern)) {
            final String apiVersion = String.valueOf(Build.VERSION.SDK_INT);
            if (apiVersion.matches(androidApiPattern)) {
                Log.d(LOGTAG, String.format("Android API (%s) does not match pattern: %s", apiVersion, androidApiPattern));
                return false;
            }
        }

        final String appIdPattern = content.getAppIdPattern();
        if (!TextUtils.isEmpty(appIdPattern)) {
            final String appId = context.getPackageName();
            if (!appId.matches(appIdPattern)) {
                Log.d(LOGTAG, String.format("App ID (%s) does not match pattern: %s", appId, appIdPattern));
                return false;
            }
        }

        final String appVersionPattern = content.getAppVersionPattern();
        if (!TextUtils.isEmpty(appVersionPattern)) {
            final String appVersion = ContextUtils.getCurrentPackageInfo(context).versionName;
            if (!appVersion.matches(appVersionPattern)) {
                Log.d(LOGTAG, String.format("App version (%s) does not match pattern: %s", appVersion, appVersionPattern));
                return false;
            }
        }

        // There are no patterns or all patterns have matched.
        return true;
    }

    protected void startDownloads(Context context) {
        DlcDownloadService.enqueueServiceWork(context);
    }
}
