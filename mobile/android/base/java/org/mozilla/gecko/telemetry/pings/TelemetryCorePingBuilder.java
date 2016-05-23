/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pings;

import android.content.Context;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.util.DateUtil;
import org.mozilla.gecko.util.Experiments;
import org.mozilla.gecko.util.StringUtils;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * Builds a {@link TelemetryPing} representing a core ping.
 *
 * See https://gecko.readthedocs.org/en/latest/toolkit/components/telemetry/telemetry/core-ping.html
 * for details on the core ping.
 */
public class TelemetryCorePingBuilder extends TelemetryPingBuilder {

    private static final String NAME = "core";
    private static final int VERSION_VALUE = 5; // For version history, see toolkit/components/telemetry/docs/core-ping.rst
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
    private static final String PING_CREATION_DATE = "created";
    private static final String PROFILE_CREATION_DATE = "profileDate";
    private static final String SEQ = "seq";
    private static final String TIMEZONE_OFFSET = "tz";
    private static final String VERSION_ATTR = "v";

    public TelemetryCorePingBuilder(final Context context, final String serverURLSchemeHostPort) {
        super(serverURLSchemeHostPort);
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

        final Calendar nowCalendar = Calendar.getInstance();
        final DateFormat pingCreationDateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.US);

        payload.put(ARCHITECTURE, AppConstants.ANDROID_CPU_ARCH);
        payload.put(DEVICE, deviceDescriptor);
        payload.put(LOCALE, Locales.getLanguageTag(Locale.getDefault()));
        payload.put(OS_VERSION, Integer.toString(Build.VERSION.SDK_INT)); // A String for cross-platform reasons.
        payload.put(PING_CREATION_DATE, pingCreationDateFormat.format(nowCalendar.getTime()));
        payload.put(TIMEZONE_OFFSET, DateUtil.getTimezoneOffsetInMinutesForGivenDate(nowCalendar));
        payload.putArray(EXPERIMENTS, Experiments.getActiveExperiments(context));
    }

    @Override
    String getDocType() {
        return NAME;
    }

    @Override
    String[] getMandatoryFields() {
        return new String[] {
                ARCHITECTURE,
                CLIENT_ID,
                DEFAULT_SEARCH_ENGINE,
                DEVICE,
                LOCALE,
                OS_ATTR,
                OS_VERSION,
                PING_CREATION_DATE,
                PROFILE_CREATION_DATE,
                SEQ,
                TIMEZONE_OFFSET,
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
     * @param date a positive date value, or null if there is an error.
     */
    public TelemetryCorePingBuilder setProfileCreationDate(@Nullable final Long date) {
        if (date != null && date < 0) {
            throw new IllegalArgumentException("Expect positive date value. Received: " + date);
        }
        payload.put(PROFILE_CREATION_DATE, date);
        return this;
    }

    // TODO (mcomella): We can potentially build two pings with the same seq no if we leave seq as an argument.
    /**
     * @param seq a positive sequence number.
     */
    public TelemetryCorePingBuilder setSequenceNumber(final int seq) {
        if (seq < 0) {
            throw new IllegalArgumentException("Expected positive sequence number. Recived: " + seq);
        }
        payload.put(SEQ, seq);
        return this;
    }
}
