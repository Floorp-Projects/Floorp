/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gcm;

import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.gcm.GcmListenerService;

import org.mozilla.gecko.push.PushService;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * This service actually handles messages directed from the on-device Google
 * Play Services package.  We simply route them to the AndroidPushService.
 */
public class GcmMessageListenerService extends GcmListenerService {
    /**
     * Called when message is received.
     *
     * @param from SenderID of the sender.
     * @param bundle Data bundle containing message data as key/value pairs.
     */
    @Override
    public void onMessageReceived(final String from, final Bundle bundle) {
        Log.d("GeckoPushGCM", "Message received.  Processing on background thread.");
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                PushService.getInstance(GcmMessageListenerService.this).onMessageReceived(
                        GcmMessageListenerService.this, bundle);
            }
        });
    }
}
