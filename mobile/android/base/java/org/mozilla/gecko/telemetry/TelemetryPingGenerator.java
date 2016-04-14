/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.content.Context;
import android.os.Build;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryConstants.CorePing;
import org.mozilla.gecko.util.Experiments;
import org.mozilla.gecko.util.StringUtils;

import java.io.IOException;
import java.util.Locale;

/**
 * A class with static methods to generate the various Java-created Telemetry pings to upload to the telemetry server.
 */
public class TelemetryPingGenerator {

    // In the server url, the initial path directly after the "scheme://host:port/"
    private static final String SERVER_INITIAL_PATH = "submit/telemetry";

    /**
     * Returns a url of the format:
     *   http://hostname/submit/telemetry/docId/docType/appName/appVersion/appUpdateChannel/appBuildID
     *
     * @param docId A unique document ID for the ping associated with the upload to this server
     * @param serverURLSchemeHostPort The server url with the scheme, host, and port (e.g. "http://mozilla.org:80")
     * @param docType The name of the ping (e.g. "main")
     * @return a url at which to POST the telemetry data to
     */
    private static String getTelemetryServerURL(final String docId, final String serverURLSchemeHostPort,
            final String docType) {
        final String appName = AppConstants.MOZ_APP_BASENAME;
        final String appVersion = AppConstants.MOZ_APP_VERSION;
        final String appUpdateChannel = AppConstants.MOZ_UPDATE_CHANNEL;
        final String appBuildId = AppConstants.MOZ_APP_BUILDID;

        // The compiler will optimize a single String concatenation into a StringBuilder statement.
        // If you change this `return`, be sure to keep it as a single statement to keep it optimized!
        return serverURLSchemeHostPort + '/' +
                SERVER_INITIAL_PATH + '/' +
                docId + '/' +
                docType + '/' +
                appName + '/' +
                appVersion + '/' +
                appUpdateChannel + '/' +
                appBuildId;
    }

    /**
     * @param docId A unique document ID for the ping associated with the upload to this server
     * @param clientId The client ID of this profile (from Gecko)
     * @param serverURLSchemeHostPort The server url with the scheme, host, and port (e.g. "http://mozilla.org:80")
     * @param profileCreationDateDays The profile creation date in days to the UNIX epoch, NOT MILLIS.
     * @throws IOException when client ID could not be created
     */
    public static TelemetryPing createCorePing(final Context context, final String docId, final String clientId,
            final String serverURLSchemeHostPort, final int seq, final long profileCreationDateDays,
            @Nullable final String distributionId, @Nullable final String defaultSearchEngine) {
        final String serverURL = getTelemetryServerURL(docId, serverURLSchemeHostPort, CorePing.NAME);
        final ExtendedJSONObject payload =
                createCorePingPayload(context, clientId, seq, profileCreationDateDays, distributionId, defaultSearchEngine);
        return new TelemetryPing(serverURL, payload);
    }

    private static ExtendedJSONObject createCorePingPayload(final Context context, final String clientId,
            final int seq, final long profileCreationDate, @Nullable final String distributionId,
            @Nullable final String defaultSearchEngine) {
        final ExtendedJSONObject ping = new ExtendedJSONObject();
        ping.put(CorePing.VERSION_ATTR, CorePing.VERSION_VALUE);
        ping.put(CorePing.OS_ATTR, CorePing.OS_VALUE);

        // We limit the device descriptor to 32 characters because it can get long. We give fewer characters to the
        // manufacturer because we're less likely to have manufacturers with similar names than we are for a
        // manufacturer to have two devices with the similar names (e.g. Galaxy S6 vs. Galaxy Note 6).
        final String deviceDescriptor =
                StringUtils.safeSubstring(Build.MANUFACTURER, 0, 12) + '-' + StringUtils.safeSubstring(Build.MODEL, 0, 19);

        ping.put(CorePing.ARCHITECTURE, AppConstants.ANDROID_CPU_ARCH);
        ping.put(CorePing.CLIENT_ID, clientId);
        ping.put(CorePing.DEFAULT_SEARCH_ENGINE, TextUtils.isEmpty(defaultSearchEngine) ? null : defaultSearchEngine);
        ping.put(CorePing.DEVICE, deviceDescriptor);
        ping.put(CorePing.LOCALE, Locales.getLanguageTag(Locale.getDefault()));
        ping.put(CorePing.OS_VERSION, Integer.toString(Build.VERSION.SDK_INT)); // A String for cross-platform reasons.
        ping.put(CorePing.SEQ, seq);
        ping.putArray(CorePing.EXPERIMENTS, Experiments.getActiveExperiments(context));

        // Optional.
        if (distributionId != null) {
            ping.put(CorePing.DISTRIBUTION_ID, distributionId);
        }

        // `null` indicates failure more clearly than < 0.
        final Long finalProfileCreationDate = (profileCreationDate < 0) ? null : profileCreationDate;
        ping.put(CorePing.PROFILE_CREATION_DATE, finalProfileCreationDate);
        return ping;
    }
}
