/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;

/**
 * Study: Scan the catalog for "new" content available for download.
 */
public class StudyAction extends BaseAction {
    private static final String LOGTAG = "DLCStudyAction";

    public void perform(Context context, DownloadContentCatalog catalog) {
        Log.d(LOGTAG, "Studying catalog..");

        for (DownloadContent content : catalog.getContentWithoutState()) {
            if (content.isAssetArchive() && content.isFont()) {
                catalog.scheduleDownload(content);

                Log.d(LOGTAG, "Scheduled download: " + content);
            }
        }

        if (catalog.hasScheduledDownloads()) {
            startDownloads(context);
        }

        Log.v(LOGTAG, "Done");
    }

    protected void startDownloads(Context context) {
        DownloadContentService.startDownloads(context);
    }
}
