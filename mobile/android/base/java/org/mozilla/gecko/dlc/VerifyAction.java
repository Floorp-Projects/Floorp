/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;

import java.io.File;

/**
 * Verify: Validate downloaded content. Does it still exist and does it have the correct checksum?
 */
public class VerifyAction extends BaseAction {
    private static final String LOGTAG = "DLCVerifyAction";

    @Override
    public void perform(Context context, DownloadContentCatalog catalog) {
        Log.d(LOGTAG, "Verifying catalog..");

        for (DownloadContent content : catalog.getDownloadedContent()) {
            try {
                File destinationFile = getDestinationFile(context, content);

                if (!destinationFile.exists()) {
                    Log.d(LOGTAG, "Downloaded content does not exist anymore: " + content);

                    // This file does not exist even though it is marked as downloaded in the catalog. Scheduling a
                    // download to fetch it again.
                    catalog.scheduleDownload(content);
                    continue;
                }

                if (!verify(destinationFile, content.getChecksum())) {
                    catalog.scheduleDownload(content);
                    Log.d(LOGTAG, "Wrong checksum. Scheduling download: " + content);
                    continue;
                }

                Log.v(LOGTAG, "Content okay: " + content);
            } catch (UnrecoverableDownloadContentException e) {
                Log.w(LOGTAG, "Unrecoverable exception while verifying downloaded file", e);
            } catch (RecoverableDownloadContentException e) {
                // That's okay, we are just verifying already existing content. No log.
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
