/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry;

import android.content.SharedPreferences;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.Log;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.distribution.DistributionStoreCallback;
import org.mozilla.gecko.search.SearchEngineManager;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.measurements.SearchCountMeasurements;
import org.mozilla.gecko.telemetry.measurements.SessionMeasurements.SessionMeasurementsContainer;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetryCorePingBuilder;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.lang.ref.WeakReference;

/**
 * A search engine callback that will attempt to upload the core ping.
 */
public class UploadTelemetryCorePingCallback implements SearchEngineManager.SearchEngineCallback {
    private static final String LOGTAG = StringUtils.safeSubstring(
            "Gecko" + UploadTelemetryCorePingCallback.class.getSimpleName(), 0, 23);

    private final WeakReference<BrowserApp> activityWeakReference;

    public UploadTelemetryCorePingCallback(final BrowserApp activity) {
        this.activityWeakReference = new WeakReference<>(activity);
    }

    // May be called from any thread.
    @Override
    public void execute(@Nullable final org.mozilla.gecko.search.SearchEngine engine) {
        // Don't waste resources queueing to the background thread if we don't have a reference.
        if (this.activityWeakReference.get() == null) {
            return;
        }

        // The containing method can be called from onStart: queue this work so that
        // the first launch of the activity doesn't trigger profile init too early.
        //
        // Additionally, getAndIncrementSequenceNumber must be called from a worker thread.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @WorkerThread
            @Override
            public void run() {
                final BrowserApp activity = activityWeakReference.get();
                if (activity == null) {
                    return;
                }

                final GeckoProfile profile = activity.getProfile();
                if (!TelemetryUploadService.isUploadEnabledByProfileConfig(activity, profile)) {
                    Log.d(LOGTAG, "Core ping upload disabled by profile config. Returning.");
                    return;
                }

                final String clientID;
                try {
                    clientID = profile.getClientId();
                } catch (final IOException e) {
                    Log.w(LOGTAG, "Unable to get client ID to generate core ping: " + e);
                    return;
                }

                // Each profile can have different telemetry data so we intentionally grab the shared prefs for the profile.
                final SharedPreferences sharedPrefs = GeckoSharedPrefs.forProfileName(activity, profile.getName());
                final SessionMeasurementsContainer sessionMeasurementsContainer =
                        activity.getSessionMeasurementDelegate().getAndResetSessionMeasurements(activity);
                final TelemetryCorePingBuilder pingBuilder = new TelemetryCorePingBuilder(activity)
                        .setClientID(clientID)
                        .setDefaultSearchEngine(TelemetryCorePingBuilder.getEngineIdentifier(engine))
                        .setProfileCreationDate(TelemetryCorePingBuilder.getProfileCreationDate(activity, profile))
                        .setSequenceNumber(TelemetryCorePingBuilder.getAndIncrementSequenceNumber(sharedPrefs))
                        .setSessionCount(sessionMeasurementsContainer.sessionCount)
                        .setSessionDuration(sessionMeasurementsContainer.elapsedSeconds);
                maybeSetOptionalMeasurements(sharedPrefs, pingBuilder);

                activity.getTelemetryDispatcher().queuePingForUpload(activity, pingBuilder);
            }
        });
    }

    private static void maybeSetOptionalMeasurements(final SharedPreferences sharedPrefs, final TelemetryCorePingBuilder pingBuilder) {
        final String distributionId = sharedPrefs.getString(DistributionStoreCallback.PREF_DISTRIBUTION_ID, null);
        if (distributionId != null) {
            pingBuilder.setOptDistributionID(distributionId);
        }

        final ExtendedJSONObject searchCounts = SearchCountMeasurements.getAndZeroSearch(sharedPrefs);
        if (searchCounts.size() > 0) {
            pingBuilder.setOptSearchCounts(searchCounts);
        }
    }
}
