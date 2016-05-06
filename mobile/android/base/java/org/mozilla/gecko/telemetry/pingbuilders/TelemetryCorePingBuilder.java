/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.search.SearchEngine;
import org.mozilla.gecko.telemetry.TelemetryConstants;
import org.mozilla.gecko.telemetry.TelemetryPing;
import org.mozilla.gecko.util.Experiments;
import org.mozilla.gecko.util.StringUtils;

import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Builds a {@link TelemetryPing} representing a core ping.
 *
 * See https://gecko.readthedocs.org/en/latest/toolkit/components/telemetry/telemetry/core-ping.html
 * for details on the core ping.
 */
public class TelemetryCorePingBuilder extends TelemetryPingBuilder {

    private static final String NAME = "core";
    private static final int VERSION_VALUE = 4; // For version history, see toolkit/components/telemetry/docs/core-ping.rst
    private static final String OS_VALUE = "Android";

    private static final String ARCHITECTURE = "arch";
    private static final String CLIENT_ID = "clientId";
    private static final String DEFAULT_SEARCH_ENGINE = "defaultSearch";
    private static final String DEVICE = "device";
    private static final String DISTRIBUTION_ID = "distributionId";
    private static final String EXPERIMENTS = "experiments";
    private static final String LOCALE = "locale";
    private static final String OS_ATTR = "os";
    private static final String OS_VERSION = "osversion";
    private static final String PROFILE_CREATION_DATE = "profileDate";
    public static final String SEQ = "seq";
    private static final String VERSION_ATTR = "v";

    public TelemetryCorePingBuilder(final Context context, final int sequenceNumber) {
        super(sequenceNumber);
        setSequenceNumber(sequenceNumber);
        initPayloadConstants(context);
    }

    private void initPayloadConstants(final Context context) {
        payload.put(VERSION_ATTR, VERSION_VALUE);
        payload.put(OS_ATTR, OS_VALUE);

        // We limit the device descriptor to 32 characters because it can get long. We give fewer characters to the
        // manufacturer because we're less likely to have manufacturers with similar names than we are for a
        // manufacturer to have two devices with the similar names (e.g. Galaxy S6 vs. Galaxy Note 6).
        final String deviceDescriptor =
                StringUtils.safeSubstring(Build.MANUFACTURER, 0, 12) + '-' + StringUtils.safeSubstring(Build.MODEL, 0, 19);

        payload.put(ARCHITECTURE, AppConstants.ANDROID_CPU_ARCH);
        payload.put(DEVICE, deviceDescriptor);
        payload.put(LOCALE, Locales.getLanguageTag(Locale.getDefault()));
        payload.put(OS_VERSION, Integer.toString(Build.VERSION.SDK_INT)); // A String for cross-platform reasons.
        payload.putArray(EXPERIMENTS, Experiments.getActiveExperiments(context));
    }

    @Override
    public String getDocType() {
        return NAME;
    }

    @Override
    public String[] getMandatoryFields() {
        return new String[] {
                ARCHITECTURE,
                CLIENT_ID,
                DEFAULT_SEARCH_ENGINE,
                DEVICE,
                LOCALE,
                OS_ATTR,
                OS_VERSION,
                PROFILE_CREATION_DATE,
                SEQ,
                VERSION_ATTR,
        };
    }

    public TelemetryCorePingBuilder setClientID(@NonNull final String clientID) {
        if (clientID == null) {
            throw new IllegalArgumentException("Expected non-null clientID");
        }
        payload.put(CLIENT_ID, clientID);
        return this;
    }

    /**
     * @param engine the default search engine identifier, or null if there is an error.
     */
    public TelemetryCorePingBuilder setDefaultSearchEngine(@Nullable final String engine) {
        if (engine != null && engine.isEmpty()) {
            throw new IllegalArgumentException("Received empty string. Expected identifier or null.");
        }
        payload.put(DEFAULT_SEARCH_ENGINE, engine);
        return this;
    }

    public TelemetryCorePingBuilder setOptDistributionID(@NonNull final String distributionID) {
        if (distributionID == null) {
            throw new IllegalArgumentException("Expected non-null distribution ID");
        }
        payload.put(DISTRIBUTION_ID, distributionID);
        return this;
    }

    /**
     * @param date The profile creation date in days to the unix epoch (not millis!), or null if there is an error.
     */
    public TelemetryCorePingBuilder setProfileCreationDate(@Nullable final Long date) {
        if (date != null && date < 0) {
            throw new IllegalArgumentException("Expect positive date value. Received: " + date);
        }
        payload.put(PROFILE_CREATION_DATE, date);
        return this;
    }

    /**
     * @param seq a positive sequence number.
     */
    private void setSequenceNumber(final int seq) {
        if (seq < 0) {
            throw new IllegalArgumentException("Expected positive sequence number. Recived: " + seq);
        }
        payload.put(SEQ, seq);
    }

    /**
     * Gets the sequence number from shared preferences and increments it in the prefs. This method
     * is not thread safe.
     */
    @WorkerThread // synchronous shared prefs write.
    public static int getAndIncrementSequenceNumber(final SharedPreferences sharedPrefsForProfile) {
        final int seq = sharedPrefsForProfile.getInt(TelemetryConstants.PREF_SEQ_COUNT, 1);

        sharedPrefsForProfile.edit().putInt(TelemetryConstants.PREF_SEQ_COUNT, seq + 1).apply();
        return seq;
    }

    /**
     * @return the profile creation date in the format expected by
     *         {@link TelemetryCorePingBuilder#setProfileCreationDate(Long)}.
     */
    @WorkerThread
    public static Long getProfileCreationDate(final Context context, final GeckoProfile profile) {
        final long profileMillis = profile.getAndPersistProfileCreationDate(context);
        if (profileMillis < 0) {
            return null;
        }
        return (long) Math.floor((double) profileMillis / TimeUnit.DAYS.toMillis(1));
    }

    /**
     * @return the search engine identifier in the format expected by the core ping.
     */
    @Nullable
    public static String getEngineIdentifier(@Nullable final SearchEngine searchEngine) {
        if (searchEngine == null) {
            return null;
        }
        final String identifier = searchEngine.getIdentifier();
        return TextUtils.isEmpty(identifier) ? null : identifier;
    }
}
