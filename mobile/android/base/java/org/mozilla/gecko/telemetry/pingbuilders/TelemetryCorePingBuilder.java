/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.mma.MmaDelegate;
import org.mozilla.gecko.search.SearchEngine;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryOutgoingPing;
import org.mozilla.gecko.util.DateUtil;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.StringUtils;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Builds a {@link TelemetryOutgoingPing} representing a core ping.
 *
 * See https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/core-ping.html
 * for details on the core ping.
 */
public class TelemetryCorePingBuilder extends TelemetryPingBuilder {
    private static final String LOGTAG = StringUtils.safeSubstring(TelemetryCorePingBuilder.class.getSimpleName(), 0, 23);

    // For legacy reasons, this preference key is not namespaced with "core".
    private static final String PREF_SEQ_COUNT = "telemetry-seqCount";

    private static final String NAME = "core";
    private static final int VERSION_VALUE = 9; // For version history, see toolkit/components/telemetry/docs/core-ping.rst

    private static final String DEFAULT_BROWSER = "defaultBrowser";
    private static final String ARCHITECTURE = "arch";
    private static final String CAMPAIGN_ID = "campaignId";
    private static final String CLIENT_ID = "clientId";
    private static final String DEFAULT_SEARCH_ENGINE = "defaultSearch";
    private static final String DEVICE = "device";
    private static final String DISTRIBUTION_ID = "distributionId";
    private static final String DISPLAY_VERSION = "displayVersion";
    private static final String EXPERIMENTS = "experiments";
    private static final String LOCALE = "locale";
    private static final String OS_ATTR = "os";
    private static final String OS_VERSION = "osversion";
    private static final String PING_CREATION_DATE = "created";
    private static final String PROFILE_CREATION_DATE = "profileDate";
    private static final String SEARCH_COUNTS = "searches";
    private static final String SEQ = "seq";
    private static final String SESSION_COUNT = "sessions";
    private static final String SESSION_DURATION = "durations";
    private static final String TIMEZONE_OFFSET = "tz";
    private static final String VERSION_ATTR = "v";
    private static final String FLASH_USAGE = "flashUsage";
    private static final String ACCESSIBILITY_SERVICES = "accessibilityServices";

    public TelemetryCorePingBuilder(final Context context) {
        initPayloadConstants(context);
    }

    private void initPayloadConstants(final Context context) {
        payload.put(VERSION_ATTR, VERSION_VALUE);
        payload.put(OS_ATTR, TelemetryPingBuilder.OS_NAME);

        // We limit the device descriptor to 32 characters because it can get long. We give fewer characters to the
        // manufacturer because we're less likely to have manufacturers with similar names than we are for a
        // manufacturer to have two devices with the similar names (e.g. Galaxy S6 vs. Galaxy Note 6).
        final String deviceDescriptor =
                StringUtils.safeSubstring(Build.MANUFACTURER, 0, 12) + '-' + StringUtils.safeSubstring(Build.MODEL, 0, 19);

        final Calendar nowCalendar = Calendar.getInstance();
        final DateFormat pingCreationDateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.US);

        payload.put(DEFAULT_BROWSER, MmaDelegate.isDefaultBrowser(context));
        payload.put(ARCHITECTURE, HardwareUtils.getRealAbi());
        payload.put(DEVICE, deviceDescriptor);
        payload.put(LOCALE, Locales.getLanguageTag(Locale.getDefault()));
        payload.put(OS_VERSION, Integer.toString(Build.VERSION.SDK_INT)); // A String for cross-platform reasons.
        payload.put(PING_CREATION_DATE, pingCreationDateFormat.format(nowCalendar.getTime()));
        payload.put(TIMEZONE_OFFSET, DateUtil.getTimezoneOffsetInMinutesForGivenDate(nowCalendar));
        payload.put(DISPLAY_VERSION, AppConstants.MOZ_APP_VERSION_DISPLAY);
        payload.putArray(EXPERIMENTS, Experiments.getActiveExperiments(context));
        synchronized (this) {
            SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
            final int count = prefs.getInt(GeckoApp.PREFS_FLASH_USAGE, 0);
            payload.put(FLASH_USAGE, count);
            prefs.edit().putInt(GeckoApp.PREFS_FLASH_USAGE, 0).apply();
        }
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
     * @param searchCounts non-empty JSON with {"engine.where": <int-count>}
     */
    public TelemetryCorePingBuilder setOptSearchCounts(@NonNull final ExtendedJSONObject searchCounts) {
        if (searchCounts == null) {
            throw new IllegalStateException("Expected non-null search counts");
        } else if (searchCounts.size() == 0) {
            throw new IllegalStateException("Expected non-empty search counts");
        }

        payload.put(SEARCH_COUNTS, searchCounts);
        return this;
    }

    /**
     * Get enabled accessibility services that might start gecko accessibility.
     */
    public TelemetryCorePingBuilder setOptAccessibility(@NonNull final List<AccessibilityServiceInfo> enabledServices) {
        // Some services, like TalkBack, register themselves several times. We
        // only record unique services.
        final Set<String> services = new HashSet<String>();
        for (AccessibilityServiceInfo service : enabledServices) {
            services.add(service.getId());
        }

        payload.putArray(ACCESSIBILITY_SERVICES, new ArrayList<String>(services));
        return this;
    }

    public TelemetryCorePingBuilder setOptCampaignId(final String campaignId) {
        if (campaignId == null) {
            throw new IllegalStateException("Expected non-null campaign ID.");
        }
        payload.put(CAMPAIGN_ID, campaignId);
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
    public TelemetryCorePingBuilder setSequenceNumber(final int seq) {
        if (seq < 0) {
            // Since this is an increasing value, it's possible we can overflow into negative values and get into a
            // crash loop so we don't crash on invalid arg - we can investigate if we see negative values on the server.
            Log.w(LOGTAG, "Expected positive sequence number. Received: " + seq);
        }
        payload.put(SEQ, seq);
        return this;
    }

    public TelemetryCorePingBuilder setSessionCount(final int sessionCount) {
        if (sessionCount < 0) {
            // Since this is an increasing value, it's possible we can overflow into negative values and get into a
            // crash loop so we don't crash on invalid arg - we can investigate if we see negative values on the server.
            Log.w(LOGTAG, "Expected positive session count. Received: " + sessionCount);
        }
        payload.put(SESSION_COUNT, sessionCount);
        return this;
    }

    public TelemetryCorePingBuilder setSessionDuration(final long sessionDuration) {
        if (sessionDuration < 0) {
            // Since this is an increasing value, it's possible we can overflow into negative values and get into a
            // crash loop so we don't crash on invalid arg - we can investigate if we see negative values on the server.
            Log.w(LOGTAG, "Expected positive session duration. Received: " + sessionDuration);
        }
        payload.put(SESSION_DURATION, sessionDuration);
        return this;
    }

    /**
     * Gets the sequence number from shared preferences and increments it in the prefs. This method
     * is not thread safe.
     */
    @WorkerThread // synchronous shared prefs write.
    public static int getAndIncrementSequenceNumber(final SharedPreferences sharedPrefsForProfile) {
        final int seq = sharedPrefsForProfile.getInt(PREF_SEQ_COUNT, 1);

        sharedPrefsForProfile.edit().putInt(PREF_SEQ_COUNT, seq + 1).apply();
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
