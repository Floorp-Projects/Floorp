/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.gcm.GcmTokenClient;
import org.mozilla.gecko.push.autopush.AutopushClientException;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.util.Map;

/**
 * Class that handles messages used in the Google Cloud Messaging and DOM push API integration.
 * <p/>
 * This singleton services Gecko messages from dom/push/PushServiceAndroidGCM.jsm and Google Cloud
 * Messaging requests.
 * <p/>
 * It's worth noting that we allow the DOM push API in restricted profiles.
 */
public class PushService {
    private static final String LOG_TAG = "GeckoPushService";

    public static final String SERVICE_WEBPUSH = "webpush";

    private static PushService sInstance;

    public static synchronized PushService getInstance() {
        if (sInstance == null) {
            throw new IllegalStateException("PushService not yet created!");
        }
        return sInstance;
    }

    public static synchronized PushService createInstance(Context context) {
        if (sInstance != null) {
            throw new IllegalStateException("PushService already created!");
        }
        sInstance = new PushService(context);
        return sInstance;
    }

    protected final PushManager pushManager;

    public PushService(Context context) {
        pushManager = new PushManager(new PushState(context, "GeckoPushState.json"), new GcmTokenClient(context), new PushManager.PushClientFactory() {
            @Override
            public PushClient getPushClient(String autopushEndpoint, boolean debug) {
                return new PushClient(autopushEndpoint);
            }
        });
    }

    public void onStartup() {
        Log.i(LOG_TAG, "Starting up.");
        ThreadUtils.assertOnBackgroundThread();

        try {
            pushManager.startup(System.currentTimeMillis());
        } catch (Exception e) {
            Log.e(LOG_TAG, "Got exception during startup; ignoring.", e);
            return;
        }
    }

    public void onRefresh() {
        Log.i(LOG_TAG, "Google Play Services requested GCM token refresh; invalidating GCM token and running startup again.");
        ThreadUtils.assertOnBackgroundThread();

        pushManager.invalidateGcmToken();
        try {
            pushManager.startup(System.currentTimeMillis());
        } catch (Exception e) {
            Log.e(LOG_TAG, "Got exception during startup; ignoring.", e);
            return;
        }
    }

    public void onMessageReceived(final @NonNull Bundle bundle) {
        Log.i(LOG_TAG, "Google Play Services GCM message received; delivering.");
        ThreadUtils.assertOnBackgroundThread();

        final String chid = bundle.getString("chid");
        if (chid == null) {
            Log.w(LOG_TAG, "No chid found; ignoring message.");
            return;
        }

        final PushRegistration registration = pushManager.registrationForSubscription(chid);
        if (registration == null) {
            Log.w(LOG_TAG, "Cannot find registration corresponding to subscription for chid: " + chid + "; ignoring message.");
            return;
        }

        final PushSubscription subscription = registration.getSubscription(chid);
        if (subscription == null) {
            // This should never happen.  There's not much to be done; in the future, perhaps we
            // could try to drop the remote subscription?
            Log.e(LOG_TAG, "No subscription found for chid: " + chid + "; ignoring message.");
            return;
        }

        Log.i(LOG_TAG, "Message directed to service: " + subscription.service);

        if (SERVICE_WEBPUSH.equals(subscription.service)) {
            // Nothing yet.
            Log.i(LOG_TAG, "Message directed to unimplemented service; ignoring: " + subscription.service);
            return;
        } else {
            Log.e(LOG_TAG, "Message directed to unknown service; dropping: " + subscription.service);
        }
    }
}
