/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * Receiver only used in local notifications posted by an {@link UpdaterService} after which it finishes.<br>
 * Used to start another variant of {@link UpdaterService}.
 */
public class UpdateServiceReceiver extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoUpdateServiceRcv";   // must be <= 23 characters
    private static final boolean DEBUG = false;

    public UpdateServiceReceiver() {
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();

        if (DEBUG) {
            Log.d(LOGTAG, String.format("Will enqueue \"%s\" work", action));
        }

        switch (action) {
            case UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE:
                UpdateServiceHelper.enqueueUpdateCheck(context, intent);
                break;
            case UpdateServiceHelper.ACTION_APPLY_UPDATE:
                UpdateServiceHelper.enqueueUpdateApply(context, intent);
                break;
            case UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE:
                UpdateServiceHelper.enqueueUpdateDownload(context, intent);
                break;
            default:
                // no-op, we only listen for the above
        }
    }
}
