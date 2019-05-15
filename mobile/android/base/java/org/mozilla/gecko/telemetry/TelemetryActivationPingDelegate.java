package org.mozilla.gecko.telemetry;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.WorkerThread;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.delegates.BrowserAppDelegate;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetryActivationPingBuilder;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.lang.reflect.Method;

/**
 * An activity-lifecycle delegate for uploading the activation ping.
 */
public class TelemetryActivationPingDelegate extends BrowserAppDelegate {
    private static final String LOGTAG = StringUtils.safeSubstring(
            "Gecko" + TelemetryActivationPingDelegate.class.getSimpleName(), 0, 23);

    private TelemetryDispatcher telemetryDispatcher; // lazy


    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        super.onCreate(browserApp, savedInstanceState);
        uploadActivationPing(browserApp);
    }

    private void uploadActivationPing(final BrowserApp activity) {
        if (!AppConstants.MOZ_ANDROID_GCM) {
            return;
        }

        if (TelemetryActivationPingBuilder.activationPingAlreadySent(activity)) {
            return;
        }

        ThreadUtils.postToBackgroundThread(() -> {
            if (activity == null) {
                return;
            }

            if (!TelemetryUploadService.isUploadEnabledByAppConfig(activity)) {
                Log.d(LOGTAG, "Activation ping upload disabled by app config. Returning.");
                return;
            }

            String identifier = null;

            try {
                final Class<?> clazz = Class.forName("org.mozilla.gecko.advertising.AdvertisingUtil");
                final Method getAdvertisingId = clazz.getMethod("getAdvertisingId", Context.class);
                identifier = (String) getAdvertisingId.invoke(null, activity);
            } catch (Exception e) {
                Log.w(LOGTAG, "Unable to get identifier: " + e);
            }

            final GeckoProfile profile = GeckoThread.getActiveProfile();
            String clientID = null;
            try {
                clientID = profile.getClientId();
            } catch (final IOException e) {
                Log.w(LOGTAG, "Unable to get client ID: " + e);
                if (identifier == null) {
                    //Activation ping is mandatory to be sent with either the identifier or the clientID.
                    Log.d(LOGTAG, "Activation ping failed to send - both identifier and clientID were unable to be retrieved.");
                    return;
                }
            }

            final TelemetryActivationPingBuilder pingBuilder = new TelemetryActivationPingBuilder(activity);
            if (identifier != null) {
                pingBuilder.setIdentifier(identifier);
            } else {
                pingBuilder.setClientID(clientID);
            }

            getTelemetryDispatcher().queuePingForUpload(activity, pingBuilder);
        });
    }

    @WorkerThread // via constructor
    private TelemetryDispatcher getTelemetryDispatcher() {
        if (telemetryDispatcher == null) {
            final GeckoProfile profile = GeckoThread.getActiveProfile();
            final String profilePath = profile.getDir().getAbsolutePath();
            final String profileName = profile.getName();
            telemetryDispatcher = new TelemetryDispatcher(profilePath, profileName);
        }
        return telemetryDispatcher;
    }
}
