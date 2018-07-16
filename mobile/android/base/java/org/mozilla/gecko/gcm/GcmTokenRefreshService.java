/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gcm;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;
import android.util.Log;

import org.mozilla.gecko.JobIdsConstants;
import org.mozilla.gecko.push.PushService;

/**
 * As per <a href="https://developers.google.com/cloud-messaging/android/client">
 *     Google recommendations for setting a GCM Client</a>
 * we should use a JobIntentService to handle the token refresh.
 */
public class GcmTokenRefreshService extends JobIntentService {
    private static final String LOGTAG = "GeckoPushGCM";
    private static final boolean DEBUG = false;

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        if (DEBUG) {
            Log.d(LOGTAG, "Processing token refresh on in background");
        }

        PushService.getInstance(this).onRefresh();
    }

    public static void enqueueWork(@NonNull final Context context) {
        enqueueWork(context, GcmTokenRefreshService.class, JobIdsConstants.getIdForGcmTokenRefresher(), new Intent());
    }
}
