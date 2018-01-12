/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.gcm.GcmTokenClient;
import org.mozilla.gecko.mma.MmaDelegate;
import org.mozilla.gecko.push.autopush.AutopushClientException;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.util.Collections;
import java.util.Iterator;
import java.util.Map;

/**
 * The push manager advances push registrations, ensuring that the upstream autopush endpoint has
 * a fresh GCM token.  It brokers channel subscription requests to the upstream and maintains
 * local state.
 * <p/>
 * This class is not thread safe.  An individual instance should be accessed on a single
 * (background) thread.
 */
public class PushManager {
    public static final long TIME_BETWEEN_AUTOPUSH_UAID_REGISTRATION_IN_MILLIS = 7 * 24 * 60 * 60 * 1000L; // One week.

    public static class ProfileNeedsConfigurationException extends Exception {
        private static final long serialVersionUID = 3326738888L;

        public ProfileNeedsConfigurationException() {
            super();
        }
    }

    private static final String LOG_TAG = "GeckoPushManager";

    protected final @NonNull PushState state;
    protected final @NonNull GcmTokenClient gcmClient;
    protected final @NonNull PushClientFactory pushClientFactory;

    // For testing only.
    public interface PushClientFactory {
        PushClient getPushClient(String autopushEndpoint, boolean debug);
    }

    public PushManager(@NonNull PushState state, @NonNull GcmTokenClient gcmClient, @NonNull PushClientFactory pushClientFactory) {
        this.state = state;
        this.gcmClient = gcmClient;
        this.pushClientFactory = pushClientFactory;
    }

    public PushRegistration registrationForSubscription(String chid) {
        // chids are globally unique, so we're not concerned about finding a chid associated to
        // any particular profile.
        for (Map.Entry<String, PushRegistration> entry : state.getRegistrations().entrySet()) {
            final PushSubscription subscription = entry.getValue().getSubscription(chid);
            if (subscription != null) {
                return entry.getValue();
            }
        }
        return null;
    }

    public Map<String, PushSubscription> allSubscriptionsForProfile(String profileName) {
        final PushRegistration registration = state.getRegistration(profileName);
        if (registration == null) {
            return Collections.emptyMap();
        }
        return Collections.unmodifiableMap(registration.subscriptions);
    }

    public PushRegistration registerUserAgent(final @NonNull String profileName, final long now) throws ProfileNeedsConfigurationException, AutopushClientException, PushClient.LocalException, GcmTokenClient.NeedsGooglePlayServicesException, IOException {
        Log.i(LOG_TAG, "Registering user agent for profile named: " + profileName);
        return advanceRegistration(profileName, now);
    }

    public PushRegistration unregisterUserAgent(final @NonNull String profileName, final long now) throws ProfileNeedsConfigurationException {
        Log.i(LOG_TAG, "Unregistering user agent for profile named: " + profileName);

        final PushRegistration registration = state.getRegistration(profileName);
        if (registration == null) {
            Log.w(LOG_TAG, "Cannot find registration corresponding to subscription; not unregistering remote uaid for profileName: " + profileName);
            return null;
        }

        final String uaid = registration.uaid.value;
        final String secret = registration.secret;
        if (uaid == null || secret == null) {
            Log.e(LOG_TAG, "Cannot unregisterUserAgent with null registration uaid or secret!");
            return null;
        }

        unregisterUserAgentOnBackgroundThread(registration);
        return registration;
    }

    public PushSubscription subscribeChannel(final @NonNull String profileName, final @NonNull String service, final @NonNull JSONObject serviceData, @Nullable String appServerKey, final long now) throws ProfileNeedsConfigurationException, AutopushClientException, PushClient.LocalException, GcmTokenClient.NeedsGooglePlayServicesException, IOException {
        Log.i(LOG_TAG, "Subscribing to channel for service: " + service + "; for profile named: " + profileName);
        final PushRegistration registration = advanceRegistration(profileName, now);
        final PushSubscription subscription = subscribeChannel(registration, profileName, service, serviceData, appServerKey, System.currentTimeMillis());
        return subscription;
    }

    protected PushSubscription subscribeChannel(final @NonNull PushRegistration registration, final @NonNull String profileName, final @NonNull String service, final @NonNull JSONObject serviceData, @Nullable String appServerKey, final long now) throws AutopushClientException, PushClient.LocalException {
        final String uaid = registration.uaid.value;
        final String secret = registration.secret;
        if (uaid == null || secret == null) {
            throw new IllegalStateException("Cannot subscribeChannel with null uaid or secret!");
        }

        // Verify endpoint is not null?
        final PushClient pushClient = pushClientFactory.getPushClient(registration.autopushEndpoint, registration.debug);

        final SubscribeChannelResponse result = pushClient.subscribeChannel(uaid, secret, appServerKey);
        if (registration.debug) {
            Log.i(LOG_TAG, "Got chid: " + result.channelID + " and endpoint: " + result.endpoint);
        } else {
            Log.i(LOG_TAG, "Got chid and endpoint.");
        }

        final PushSubscription subscription = new PushSubscription(result.channelID, profileName, result.endpoint, service, serviceData);
        registration.putSubscription(result.channelID, subscription);
        state.checkpoint();

        return subscription;
    }

    public PushSubscription unsubscribeChannel(final @NonNull String chid) {
        Log.i(LOG_TAG, "Unsubscribing from channel with chid: " + chid);

        final PushRegistration registration = registrationForSubscription(chid);
        if (registration == null) {
            Log.w(LOG_TAG, "Cannot find registration corresponding to subscription; not unregistering remote subscription: " + chid);
            return null;
        }

        // We remove the local subscription before the remote subscription:  without the local
        // subscription we'll ignoring incoming messages, and after some amount of time the
        // server will expire the channel due to non-activity.  This is also Desktop's approach.
        final PushSubscription subscription = registration.removeSubscription(chid);
        state.checkpoint();

        if (subscription == null) {
            // This should never happen.
            Log.e(LOG_TAG, "Subscription did not exist: " + chid);
            return null;
        }

        final String uaid = registration.uaid.value;
        final String secret = registration.secret;
        if (uaid == null || secret == null) {
            Log.e(LOG_TAG, "Cannot unsubscribeChannel with null registration uaid or secret!");
            return null;
        }

        final PushClient pushClient = pushClientFactory.getPushClient(registration.autopushEndpoint, registration.debug);
        // Fire and forget.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                try {
                    pushClient.unsubscribeChannel(registration.uaid.value, registration.secret, chid);
                    Log.i(LOG_TAG, "Unsubscribed from channel with chid: " + chid);
                } catch (PushClient.LocalException | AutopushClientException e) {
                    Log.w(LOG_TAG, "Failed to unsubscribe from channel with chid; ignoring: " + chid, e);
                }
            }
        });

        return subscription;
    }

    public PushRegistration configure(final @NonNull String profileName, final @NonNull String endpoint, final boolean debug, final long now) {
        Log.i(LOG_TAG, "Updating configuration.");
        final PushRegistration registration = state.getRegistration(profileName);
        final PushRegistration newRegistration;
        if (registration != null) {
            if (!endpoint.equals(registration.autopushEndpoint)) {
                if (debug) {
                    Log.i(LOG_TAG, "Push configuration autopushEndpoint changed! Was: " + registration.autopushEndpoint + "; now: " + endpoint);
                } else {
                    Log.i(LOG_TAG, "Push configuration autopushEndpoint changed!");
                }

                newRegistration = new PushRegistration(endpoint, debug, Fetched.now(null), null);

                if (registration.uaid.value != null) {
                    // New endpoint!  All registrations and subscriptions have been dropped, and
                    // should be removed remotely.
                    unregisterUserAgentOnBackgroundThread(registration);
                }
            } else if (debug != registration.debug) {
                Log.i(LOG_TAG, "Push configuration debug changed: " + debug);
                newRegistration = registration.withDebug(debug);
            } else {
                newRegistration = registration;
            }
        } else {
            if (debug) {
                Log.i(LOG_TAG, "Push configuration set: " + endpoint + "; debug: " + debug);
            } else {
                Log.i(LOG_TAG, "Push configuration set!");
            }
            newRegistration = new PushRegistration(endpoint, debug, new Fetched(null, now), null);
        }

        if (newRegistration != registration) {
            state.putRegistration(profileName, newRegistration);
            state.checkpoint();
        }

        return newRegistration;
    }

    private void unregisterUserAgentOnBackgroundThread(final PushRegistration registration) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                try {
                    pushClientFactory.getPushClient(registration.autopushEndpoint, registration.debug).unregisterUserAgent(registration.uaid.value, registration.secret);
                    Log.i(LOG_TAG, "Unregistered user agent with uaid: " + registration.uaid.value);
                } catch (PushClient.LocalException | AutopushClientException e) {
                    Log.w(LOG_TAG, "Failed to unregister user agent with uaid; ignoring: " + registration.uaid.value, e);
                }
            }
        });
    }

    protected @NonNull PushRegistration advanceRegistration(final @NonNull String profileName, final long now) throws ProfileNeedsConfigurationException, AutopushClientException, PushClient.LocalException, GcmTokenClient.NeedsGooglePlayServicesException, IOException {
        final PushRegistration registration = state.getRegistration(profileName);
        if (registration == null || registration.autopushEndpoint == null) {
            Log.i(LOG_TAG, "Cannot advance to registered: registration needs configuration.");
            throw new ProfileNeedsConfigurationException();
        }
        return advanceRegistration(registration, profileName, now);
    }

    protected @NonNull PushRegistration advanceRegistration(final PushRegistration registration, final @NonNull String profileName, final long now) throws AutopushClientException, PushClient.LocalException, GcmTokenClient.NeedsGooglePlayServicesException, IOException {
        final Fetched gcmToken = gcmClient.getToken(AppConstants.MOZ_ANDROID_GCM_SENDERIDS, registration.debug);

        final PushClient pushClient = pushClientFactory.getPushClient(registration.autopushEndpoint, registration.debug);

        if (registration.uaid.value == null) {
            if (registration.debug) {
                Log.i(LOG_TAG, "No uaid; requesting from autopush endpoint: " + registration.autopushEndpoint);
            } else {
                Log.i(LOG_TAG, "No uaid: requesting from autopush endpoint.");
            }
            final RegisterUserAgentResponse result = pushClient.registerUserAgent(gcmToken.value);
            if (registration.debug) {
                Log.i(LOG_TAG, "Got uaid: " + result.uaid + " and secret: " + result.secret);
            } else {
                Log.i(LOG_TAG, "Got uaid and secret.");
            }
            final long nextNow = System.currentTimeMillis();
            final PushRegistration nextRegistration = registration.withUserAgentID(result.uaid, result.secret, nextNow);
            state.putRegistration(profileName, nextRegistration);
            state.checkpoint();
            return advanceRegistration(nextRegistration, profileName, nextNow);
        }

        if (registration.uaid.timestamp + TIME_BETWEEN_AUTOPUSH_UAID_REGISTRATION_IN_MILLIS < now
                || registration.uaid.timestamp < gcmToken.timestamp) {
            if (registration.debug) {
                Log.i(LOG_TAG, "Stale uaid; re-registering with autopush endpoint: " + registration.autopushEndpoint);
            } else {
                Log.i(LOG_TAG, "Stale uaid: re-registering with autopush endpoint.");
            }

            pushClient.reregisterUserAgent(registration.uaid.value, registration.secret, gcmToken.value);

            Log.i(LOG_TAG, "Re-registered uaid and secret.");
            final long nextNow = System.currentTimeMillis();
            final PushRegistration nextRegistration = registration.withUserAgentID(registration.uaid.value, registration.secret, nextNow);
            state.putRegistration(profileName, nextRegistration);
            state.checkpoint();
            return advanceRegistration(nextRegistration, profileName, nextNow);
        }

        Log.d(LOG_TAG, "Existing uaid is fresh; no need to request from autopush endpoint.");
        return registration;
    }

    public void invalidateGcmToken() {
        gcmClient.invalidateToken();
    }

    public void startup(long now) {
        try {
            Log.i(LOG_TAG, "Startup: requesting GCM token.");
            gcmClient.getToken(AppConstants.MOZ_ANDROID_GCM_SENDERIDS, false); // For side-effects.
        } catch (GcmTokenClient.NeedsGooglePlayServicesException e) {
            // Requires user intervention.  At App startup, we don't want to address this.  In
            // response to user activity, we do want to try to have the user address this.
            Log.w(LOG_TAG, "Startup: needs Google Play Services.  Ignoring until GCM is requested in response to user activity.");
            return;
        } catch (IOException e) {
            // We're temporarily unable to get a GCM token.  There's nothing to be done; we'll
            // try to advance the App's state in response to user activity or at next startup.
            Log.w(LOG_TAG, "Startup: Google Play Services is available, but we can't get a token; ignoring.", e);
            return;
        }

        Log.i(LOG_TAG, "Startup: advancing all registrations.");
        final Map<String, PushRegistration> registrations = state.getRegistrations();

        // Now advance all registrations.
        try {
            final Iterator<Map.Entry<String, PushRegistration>> it = registrations.entrySet().iterator();
            while (it.hasNext()) {
                final Map.Entry<String, PushRegistration> entry = it.next();
                final String profileName = entry.getKey();
                final PushRegistration registration = entry.getValue();
                if (registration.subscriptions.isEmpty()) {
                    Log.i(LOG_TAG, "Startup: no subscriptions for profileName; not advancing registration: " + profileName);
                    continue;
                }

                try {
                    advanceRegistration(profileName, now); // For side-effects.
                    Log.i(LOG_TAG, "Startup: advanced registration for profileName: " + profileName);
                } catch (ProfileNeedsConfigurationException e) {
                    Log.i(LOG_TAG, "Startup: cannot advance registration for profileName: " + profileName + "; profile needs configuration from Gecko.");
                } catch (AutopushClientException e) {
                    if (e.isTransientError()) {
                        Log.w(LOG_TAG, "Startup: cannot advance registration for profileName: " + profileName + "; got transient autopush error.  Ignoring; will advance on demand.", e);
                    } else {
                        Log.w(LOG_TAG, "Startup: cannot advance registration for profileName: " + profileName + "; got permanent autopush error.  Removing registration entirely.", e);
                        it.remove();
                    }
                } catch (PushClient.LocalException e) {
                    Log.w(LOG_TAG, "Startup: cannot advance registration for profileName: " + profileName + "; got local exception.  Ignoring; will advance on demand.", e);
                }
            }
        } catch (GcmTokenClient.NeedsGooglePlayServicesException e) {
            Log.w(LOG_TAG, "Startup: cannot advance any registrations; need Google Play Services!", e);
            return;
        } catch (IOException e) {
            Log.w(LOG_TAG, "Startup: cannot advance any registrations; intermittent Google Play Services exception; ignoring, will advance on demand.", e);
            return;
        }

        // We may have removed registrations above.  Checkpoint just to be safe!
        state.checkpoint();
    }
}
