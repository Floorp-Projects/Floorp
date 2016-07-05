/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gcm;

import android.util.Log;

import com.google.android.gms.iid.InstanceIDListenerService;

import org.mozilla.gecko.push.PushService;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * This service is notified by the on-device Google Play Services library if an
 * in-use token needs to be updated.  We simply pass through to AndroidPushService.
 */
public class GcmInstanceIDListenerService extends InstanceIDListenerService {
    /**
     * Called if InstanceID token is updated. This may occur if the security of
     * the previous token had been compromised. This call is initiated by the
     * InstanceID provider.
     */
    @Override
    public void onTokenRefresh() {
        Log.d("GeckoPushGCM", "Token refresh request received.  Processing on background thread.");
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                PushService.getInstance(GcmInstanceIDListenerService.this).onRefresh();
            }
        });
    }
}
