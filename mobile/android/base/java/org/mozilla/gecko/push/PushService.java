/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoService;
import org.mozilla.gecko.GeckoServicesCreatorService;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.devices.FxAccountDeviceRegistrator;
import org.mozilla.gecko.fxa.FxAccountPushHandler;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.gcm.GcmTokenClient;
import org.mozilla.gecko.push.autopush.AutopushClientException;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.IOException;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * Class that handles messages used in the Google Cloud Messaging and DOM push API integration.
 * <p/>
 * This singleton services Gecko messages from dom/push/PushServiceAndroidGCM.jsm and Google Cloud
 * Messaging requests.
 * <p/>
 * It is expected that Gecko is started (if not already running) soon after receiving GCM messages
 * otherwise there is a greater risk that pending messages that have not been handle by Gecko will
 * be lost if this service is killed.
 * <p/>
 * It's worth noting that we allow the DOM push API in restricted profiles.
 */
@ReflectionTarget
public class PushService implements BundleEventListener {
    private static final String LOG_TAG = "GeckoPushService";

    public static final String SERVICE_WEBPUSH = "webpush";
    public static final String SERVICE_FXA = "fxa";

    public static final double ERROR_GCM_DISABLED = 2154627078L; // = NS_ERROR_DOM_PUSH_GCM_DISABLED

    public static final String REPLY_BUNDLE_KEY_ERROR = "error";
    public static final String ERROR_BUNDLE_KEY_MESSAGE = "message";
    public static final String ERROR_BUNDLE_KEY_RESULT = "result";

    private static PushService sInstance;

    private static final String[] GECKO_EVENTS = new String[] {
            "PushServiceAndroidGCM:Configure",
            "PushServiceAndroidGCM:DumpRegistration",
            "PushServiceAndroidGCM:DumpSubscriptions",
            "PushServiceAndroidGCM:Initialized",
            "PushServiceAndroidGCM:Uninitialized",
            "PushServiceAndroidGCM:RegisterUserAgent",
            "PushServiceAndroidGCM:UnregisterUserAgent",
            "PushServiceAndroidGCM:SubscribeChannel",
            "PushServiceAndroidGCM:UnsubscribeChannel",
            "FxAccountsPush:Initialized",
            "FxAccountsPush:ReceivedPushMessageToDecode:Response",
            "History:GetPrePathLastVisitedTimeMilliseconds",
    };

    private enum GeckoComponent {
        FxAccountsPush,
        PushServiceAndroidGCM
    }

    public static synchronized PushService getInstance(Context context) {
        if (sInstance == null) {
            onCreate(context);
        }
        return sInstance;
    }

    @ReflectionTarget
    public static synchronized boolean onCreate(Context context) {
        if (sInstance != null) {
            return false;
        }
        sInstance = new PushService(context);

        sInstance.registerGeckoEventListener();
        sInstance.onStartup();
        return true;
    }

    protected final PushManager pushManager;

    // NB: These are not thread-safe, we're depending on these being access from the same background thread.
    private boolean isReadyPushServiceAndroidGCM = false;
    private boolean isReadyFxAccountsPush = false;
    private final List<JSONObject> pendingPushMessages;

    // NB, on context use in AccountManager and AndroidFxAccount:
    // We are not going to register any listeners, or surface any UI out of AccountManager.
    // It should be fine to use a potentially short-lived context then, as opposed to a long-lived
    // application context, contrary to what AndroidFxAccount docs ask for.
    private final Context context;

    public PushService(Context context) {
        this.context = context;
        pushManager = new PushManager(new PushState(context, "GeckoPushState.json"), new GcmTokenClient(context), new PushManager.PushClientFactory() {
            @Override
            public PushClient getPushClient(String autopushEndpoint, boolean debug) {
                return new PushClient(autopushEndpoint);
            }
        });

        pendingPushMessages = new LinkedList<>();
    }

    public void onStartup() {
        Log.i(LOG_TAG, "Starting up.");
        ThreadUtils.assertOnBackgroundThread();

        try {
            pushManager.startup(System.currentTimeMillis());

            // Determine if we need to renew our FxA Push Subscription. Unused subscriptions expire
            // once a month, and so we do a simple check on startup to determine if it's time to get
            // a new one. Note that this is sub-optimal, as we might have a perfectly valid (but old)
            // subscription which we'll nevertheless unsubscribe in lieu of a new one. Improvements
            // to this will be addressed as part of a larger Bug 1345651.

            // From the Android permission docs:
            // Prior to API 23, GET_ACCOUNTS permission was necessary to get access to information
            // about any account. Beginning with API 23, if an app shares the signature of the
            // authenticator that manages an account, it does not need "GET_ACCOUNTS" permission to
            // read information about that account.
            // We list GET_ACCOUNTS in our manifest for pre-23 devices.
            final AccountManager accountManager = AccountManager.get(context);
            final Account[] fxAccounts = accountManager.getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);

            // Nothing to renew if there isn't an account.
            if (fxAccounts.length == 0) {
                return;
            }

            // Defensively obtain account state. We are in a startup situation: try to not crash.
            final AndroidFxAccount fxAccount = new AndroidFxAccount(context, fxAccounts[0]);
            final State fxAccountState;
            try {
                fxAccountState = fxAccount.getState();
            } catch (IllegalStateException e) {
                Log.e(LOG_TAG, "Failed to obtain FxA account state while renewing registration", e);
                return;
            }

            // This decision will be re-addressed as part of Bug 1346061.
            if (!State.StateLabel.Married.equals(fxAccountState.getStateLabel())) {
                Log.i(LOG_TAG, "FxA account not in Married state, not proceeding with registration renewal");
                return;
            }

            // We'll obtain a new subscription as part of device registration.
            if (FxAccountDeviceRegistrator.shouldRenewRegistration(fxAccount)) {
                Log.i(LOG_TAG, "FxA device needs registration renewal");
                FxAccountDeviceRegistrator.renewRegistration(context);
            }
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
            Log.e(LOG_TAG, "Got exception during refresh; ignoring.", e);
            return;
        }
    }

    public void onMessageReceived(final @NonNull Context context, final @NonNull Bundle bundle) {
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

        boolean isWebPush = SERVICE_WEBPUSH.equals(subscription.service);
        boolean isFxAPush = SERVICE_FXA.equals(subscription.service);
        if (!isWebPush && !isFxAPush) {
            Log.e(LOG_TAG, "Message directed to unknown service; dropping: " + subscription.service);
            return;
        }

        Log.i(LOG_TAG, "Message directed to service: " + subscription.service);

        if (subscription.serviceData == null) {
            Log.e(LOG_TAG, "No serviceData found for chid: " + chid + "; ignoring dom/push message.");
            return;
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SERVICE, "dom-push-api");

        final String profileName = subscription.serviceData.optString("profileName", null);
        final String profilePath = subscription.serviceData.optString("profilePath", null);
        if (profileName == null || profilePath == null) {
            Log.e(LOG_TAG, "Corrupt serviceData found for chid: " + chid + "; ignoring dom/push message.");
            return;
        }

        if (canSendPushMessagesToGecko()) {
            if (!GeckoThread.canUseProfile(profileName, new File(profilePath))) {
                Log.e(LOG_TAG, "Mismatched profile for chid: " + chid + "; ignoring dom/push message.");
                return;
            }
        } else {
            final Intent intent = GeckoService.getIntentToCreateServices("android-push-service");
            GeckoService.setIntentProfile(intent, profileName, profilePath);
            GeckoServicesCreatorService.enqueueWork(context, intent);
        }

        final JSONObject data = new JSONObject();
        try {
            data.put("channelID", chid);
            data.put("con", bundle.getString("con"));
            data.put("enc", bundle.getString("enc"));
            // Only one of cryptokey (newer) and enckey (deprecated) should be set, but the
            // Gecko handler will verify this.
            data.put("cryptokey", bundle.getString("cryptokey"));
            data.put("enckey", bundle.getString("enckey"));
            data.put("message", bundle.getString("body"));

            if (!canSendPushMessagesToGecko()) {
                data.put("profileName", profileName);
                data.put("profilePath", profilePath);
                data.put("service", subscription.service);
            }
        } catch (JSONException e) {
            Log.e(LOG_TAG, "Got exception delivering dom/push message to Gecko!", e);
            return;
        }

        if (!canSendPushMessagesToGecko()) {
            Log.i(LOG_TAG, "Required service not initialized, adding message to queue.");
            pendingPushMessages.add(data);
            return;
        }

        if (isWebPush) {
            sendMessageToGeckoService(data);
        } else {
            sendMessageToDecodeToGeckoService(data);
        }
    }

    protected static void sendMessageToGeckoService(final @NonNull JSONObject message) {
        Log.i(LOG_TAG, "Delivering dom/push message to Gecko!");
        GeckoAppShell.notifyObservers("PushServiceAndroidGCM:ReceivedPushMessage",
                                      message.toString(),
                                      GeckoThread.State.PROFILE_READY);
    }

    protected static void sendMessageToDecodeToGeckoService(final @NonNull JSONObject message) {
        Log.i(LOG_TAG, "Delivering dom/push message to decode to Gecko!");
        GeckoAppShell.notifyObservers("FxAccountsPush:ReceivedPushMessageToDecode",
                                      message.toString(),
                                      GeckoThread.State.PROFILE_READY);
    }

    protected void registerGeckoEventListener() {
        Log.d(LOG_TAG, "Registered Gecko event listener.");
        EventDispatcher.getInstance().registerBackgroundThreadListener(this, GECKO_EVENTS);
    }

    protected void unregisterGeckoEventListener() {
        Log.d(LOG_TAG, "Unregistered Gecko event listener.");
        EventDispatcher.getInstance().unregisterBackgroundThreadListener(this, GECKO_EVENTS);
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message, final EventCallback callback) {
        Log.i(LOG_TAG, "Handling event: " + event);
        ThreadUtils.assertOnBackgroundThread();

        final Context context = GeckoAppShell.getApplicationContext();
        // We're invoked in response to a Gecko message on a background thread.  We should always
        // be able to safely retrieve the current Gecko profile.
        final GeckoProfile geckoProfile = GeckoProfile.get(context);

        if (callback == null) {
            Log.e(LOG_TAG, "callback must not be null in " + event);
            return;
        }

        try {
            if ("PushServiceAndroidGCM:Initialized".equals(event)) {
                processComponentState(GeckoComponent.PushServiceAndroidGCM, true);
                callback.sendSuccess(null);
                return;
            }
            if ("PushServiceAndroidGCM:Uninitialized".equals(event)) {
                processComponentState(GeckoComponent.PushServiceAndroidGCM, false);
                callback.sendSuccess(null);
                return;
            }
            if ("FxAccountsPush:Initialized".equals(event)) {
                processComponentState(GeckoComponent.FxAccountsPush, true);
                callback.sendSuccess(null);
                return;
            }
            if ("PushServiceAndroidGCM:Configure".equals(event)) {
                final String endpoint = message.getString("endpoint");
                if (endpoint == null) {
                    callback.sendError("endpoint must not be null in " + event);
                    return;
                }
                final boolean debug = message.getBoolean("debug", false);
                pushManager.configure(geckoProfile.getName(), endpoint, debug, System.currentTimeMillis()); // For side effects.
                callback.sendSuccess(null);
                return;
            }
            if ("PushServiceAndroidGCM:DumpRegistration".equals(event)) {
                // In the future, this might be used to interrogate the Java Push Manager
                // registration state from JavaScript.
                callback.sendError("Not yet implemented!");
                return;
            }
            if ("PushServiceAndroidGCM:DumpSubscriptions".equals(event)) {
                try {
                    final Map<String, PushSubscription> result = pushManager.allSubscriptionsForProfile(geckoProfile.getName());

                    final JSONObject json = new JSONObject();
                    for (Map.Entry<String, PushSubscription> entry : result.entrySet()) {
                        json.put(entry.getKey(), entry.getValue().toJSONObject());
                    }
                    callback.sendSuccess(json.toString());
                } catch (JSONException e) {
                    callback.sendError("Got exception handling message [" + event + "]: " + e.toString());
                }
                return;
            }
            if ("PushServiceAndroidGCM:RegisterUserAgent".equals(event)) {
                try {
                    pushManager.registerUserAgent(geckoProfile.getName(), System.currentTimeMillis()); // For side-effects.
                    callback.sendSuccess(null);
                } catch (PushManager.ProfileNeedsConfigurationException | AutopushClientException | PushClient.LocalException | IOException e) {
                    Log.e(LOG_TAG, "Got exception in " + event, e);
                    callback.sendError("Got exception handling message [" + event + "]: " + e.toString());
                }
                return;
            }
            if ("PushServiceAndroidGCM:UnregisterUserAgent".equals(event)) {
                // In the future, this might be used to tell the Java Push Manager to unregister
                // a User Agent entirely from JavaScript.  Right now, however, everything is
                // subscription based; there's no concept of unregistering all subscriptions
                // simultaneously.
                callback.sendError("Not yet implemented!");
                return;
            }
            if ("PushServiceAndroidGCM:SubscribeChannel".equals(event)) {
                final String service = SERVICE_FXA.equals(message.getString("service")) ?
                                       SERVICE_FXA :
                                       SERVICE_WEBPUSH;
                final JSONObject serviceData;
                final String appServerKey = message.getString("appServerKey");
                try {
                    serviceData = new JSONObject();
                    serviceData.put("profileName", geckoProfile.getName());
                    serviceData.put("profilePath", geckoProfile.getDir().getAbsolutePath());
                } catch (JSONException e) {
                    Log.e(LOG_TAG, "Got exception in " + event, e);
                    callback.sendError("Got exception handling message [" + event + "]: " + e.toString());
                    return;
                }

                final PushSubscription subscription;
                try {
                    subscription = pushManager.subscribeChannel(geckoProfile.getName(), service, serviceData, appServerKey, System.currentTimeMillis());
                } catch (PushManager.ProfileNeedsConfigurationException | AutopushClientException | PushClient.LocalException | IOException e) {
                    Log.e(LOG_TAG, "Got exception in " + event, e);
                    callback.sendError("Got exception handling message [" + event + "]: " + e.toString());
                    return;
                }

                final JSONObject json = new JSONObject();
                try {
                    json.put("channelID", subscription.chid);
                    json.put("endpoint", subscription.webpushEndpoint);
                } catch (JSONException e) {
                    Log.e(LOG_TAG, "Got exception in " + event, e);
                    callback.sendError("Got exception handling message [" + event + "]: " + e.toString());
                    return;
                }

                Telemetry.sendUIEvent(TelemetryContract.Event.SAVE, TelemetryContract.Method.SERVICE, "dom-push-api");
                callback.sendSuccess(json.toString());
                return;
            }
            if ("PushServiceAndroidGCM:UnsubscribeChannel".equals(event)) {
                final String channelID = message.getString("channelID");
                if (channelID == null) {
                    callback.sendError("channelID must not be null in " + event);
                    return;
                }

                // Fire and forget.  See comments in the function itself.
                final PushSubscription pushSubscription = pushManager.unsubscribeChannel(channelID);
                if (pushSubscription != null) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.UNSAVE, TelemetryContract.Method.SERVICE, "dom-push-api");
                    callback.sendSuccess(null);
                    return;
                }

                callback.sendError("Could not unsubscribe from channel: " + channelID);
                return;
            }
            if ("FxAccountsPush:ReceivedPushMessageToDecode:Response".equals(event)) {
                FxAccountPushHandler.handleFxAPushMessage(context, message);
                return;
            }
            if ("History:GetPrePathLastVisitedTimeMilliseconds".equals(event)) {
                final String prePath = message.getString("prePath");
                if (prePath == null) {
                    callback.sendError("prePath must not be null in " + event);
                    return;
                }
                // We're on a background thread, so we can be synchronous.
                final long millis = BrowserDB.from(geckoProfile).getPrePathLastVisitedTimeMilliseconds(
                        context.getContentResolver(), prePath);
                callback.sendSuccess(millis);
                return;
            }
        } catch (GcmTokenClient.NeedsGooglePlayServicesException e) {
            // TODO: improve this.  Can we find a point where the user is *definitely* interacting
            // with the WebPush?  Perhaps we can show a dialog when interacting with the Push
            // permissions, and then be more aggressive showing this notification when we have
            // registrations and subscriptions that can't be advanced.
            String msg = "To handle event [" + event + "], user interaction is needed to enable Google Play Services.";
            GeckoBundle reply = new GeckoBundle();
            GeckoBundle error = new GeckoBundle();
            error.putString(ERROR_BUNDLE_KEY_MESSAGE, msg);
            error.putDouble(ERROR_BUNDLE_KEY_RESULT, ERROR_GCM_DISABLED);
            reply.putBundle(REPLY_BUNDLE_KEY_ERROR, error);
            callback.sendError(reply);
        }
    }

    private void processComponentState(@NonNull GeckoComponent component, boolean isReady) {
        if (component == GeckoComponent.FxAccountsPush) {
            isReadyFxAccountsPush = isReady;

        } else if (component == GeckoComponent.PushServiceAndroidGCM) {
            isReadyPushServiceAndroidGCM = isReady;
        }

        // Send all pending messages to Gecko.
        if (canSendPushMessagesToGecko()) {
            sendPushMessagesToGecko(pendingPushMessages);
            pendingPushMessages.clear();
        }
    }

    private boolean canSendPushMessagesToGecko() {
        return isReadyFxAccountsPush && isReadyPushServiceAndroidGCM;
    }

    private static void sendPushMessagesToGecko(@NonNull List<JSONObject> messages) {
        for (JSONObject pushMessage : messages) {
            final String profileName = pushMessage.optString("profileName", null);
            final String profilePath = pushMessage.optString("profilePath", null);
            final String service = pushMessage.optString("service", null);
            if (profileName == null || profilePath == null ||
                    !GeckoThread.canUseProfile(profileName, new File(profilePath))) {
                Log.e(LOG_TAG, "Mismatched profile for chid: " +
                        pushMessage.optString("channelID") +
                        "; ignoring dom/push message.");
                continue;
            }
            if (SERVICE_WEBPUSH.equals(service)) {
                sendMessageToGeckoService(pushMessage);
            } else if (SERVICE_FXA.equals(service)) {
                sendMessageToDecodeToGeckoService(pushMessage);
            }
        }
    }
}
