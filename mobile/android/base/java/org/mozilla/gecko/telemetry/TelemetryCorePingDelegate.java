/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.Log;
import android.view.accessibility.AccessibilityManager;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.adjust.AttributionHelperListener;
import org.mozilla.gecko.telemetry.measurements.CampaignIdMeasurements;
import org.mozilla.gecko.delegates.BrowserAppDelegateWithReference;
import org.mozilla.gecko.distribution.DistributionStoreCallback;
import org.mozilla.gecko.search.SearchEngineManager;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.measurements.SearchCountMeasurements;
import org.mozilla.gecko.telemetry.measurements.SessionMeasurements;
import org.mozilla.gecko.telemetry.pingbuilders.TelemetryCorePingBuilder;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.IOException;
import java.util.List;

/**
 * An activity-lifecycle delegate for uploading the core ping.
 */
public class TelemetryCorePingDelegate extends BrowserAppDelegateWithReference
        implements SearchEngineManager.SearchEngineCallback, AttributionHelperListener {
    private static final String LOGTAG = StringUtils.safeSubstring(
            "Gecko" + TelemetryCorePingDelegate.class.getSimpleName(), 0, 23);

    private static final String TELEMETRY_EXTRA_ONRESUME_ALREADY_CALLED = "onResumeAlreadyCalled";
    private static final String TELEMETRY_EXTRA_ONPAUSE_CALLED_BEFORE_ONRESUME = "onPauseCalledBeforeOnResume";

    private boolean isOnResumeCalled = false;
    private TelemetryDispatcher telemetryDispatcher; // lazy
    private final SessionMeasurements sessionMeasurements = new SessionMeasurements();

    @Override
    public void onStart(final BrowserApp browserApp) {
        TelemetryPreferences.initPreferenceObserver(browserApp, GeckoThread.getActiveProfile().getName());

        // We don't upload in onCreate because that's only called when the Activity needs to be instantiated
        // and it's possible the system will never free the Activity from memory.
        //
        // We don't upload in onResume/onPause because that will be called each time the Activity is obscured,
        // including by our own Activities/dialogs, and there is no reason to upload each time we're unobscured.
        //
        // We're left with onStart/onStop and we upload in onStart because onStop is not guaranteed to be called
        // and we want to upload the first run ASAP (e.g. to get install data before the app may crash).
        uploadPing(browserApp);
    }

    @Override
    public void onStop(final BrowserApp browserApp) {
        // We've decided to upload primarily in onStart (see note there). However, if it's the first run,
        // it's possible a user used fennec and decided never to return to it again - it'd be great to get
        // their session information before they decided to give it up so we upload here on first run.
        //
        // Caveats:
        //   * onStop is not guaranteed to be called in low memory conditions so it's possible we won't upload,
        // but it's better than it was before.
        //   * Besides first run (because of this call), we can never get the user's *last* session data.
        //
        // If we are really interested in the user's last session data, we could consider uploading in onStop
        // but it's less robust (see discussion in bug 1277091).
        final SharedPreferences sharedPrefs = getSharedPreferences(browserApp);
        if (sharedPrefs.getBoolean(GeckoApp.PREFS_IS_FIRST_RUN, true)) {
            // GeckoApp will set this pref to false afterwards.
            uploadPing(browserApp);
        }
    }

    private void uploadPing(final BrowserApp browserApp) {
        final SearchEngineManager searchEngineManager = browserApp.getSearchEngineManager();
        searchEngineManager.getEngine(this);
    }

    @Override
    public void onResume(BrowserApp browserApp) {
        if (isOnResumeCalled) {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SYSTEM, TELEMETRY_EXTRA_ONRESUME_ALREADY_CALLED);
            return;
        }
        isOnResumeCalled = true;
        sessionMeasurements.recordSessionStart();
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        if (!isOnResumeCalled) {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.SYSTEM, TELEMETRY_EXTRA_ONPAUSE_CALLED_BEFORE_ONRESUME);
            return;
        }
        isOnResumeCalled = false;
        // onStart/onStop is ideal over onResume/onPause. However, onStop is not guaranteed to be called and
        // dealing with that possibility adds a lot of complexity that we don't want to handle at this point.
        sessionMeasurements.recordSessionEnd(browserApp);
    }

    @WorkerThread // via constructor
    private TelemetryDispatcher getTelemetryDispatcher(final BrowserApp browserApp) {
        if (telemetryDispatcher == null) {
            final GeckoProfile profile = GeckoThread.getActiveProfile();
            final String profilePath = profile.getDir().getAbsolutePath();
            final String profileName = profile.getName();
            telemetryDispatcher = new TelemetryDispatcher(profilePath, profileName);
        }
        return telemetryDispatcher;
    }

    private SharedPreferences getSharedPreferences(final BrowserApp activity) {
        return GeckoSharedPrefs.forProfileName(activity, GeckoThread.getActiveProfile().getName());
    }

    // via SearchEngineCallback - may be called from any thread.
    @Override
    public void execute(@Nullable final org.mozilla.gecko.search.SearchEngine engine) {
        // Don't waste resources queueing to the background thread if we don't have a reference.
        if (getBrowserApp() == null) {
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
                final BrowserApp activity = getBrowserApp();
                if (activity == null) {
                    return;
                }

                final GeckoProfile profile = GeckoThread.getActiveProfile();
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
                final SharedPreferences sharedPrefs = getSharedPreferences(activity);
                final SessionMeasurements.SessionMeasurementsContainer sessionMeasurementsContainer =
                        sessionMeasurements.getAndResetSessionMeasurements(activity);
                final TelemetryCorePingBuilder pingBuilder = new TelemetryCorePingBuilder(activity)
                        .setClientID(clientID)
                        .setDefaultSearchEngine(TelemetryCorePingBuilder.getEngineIdentifier(engine))
                        .setProfileCreationDate(TelemetryCorePingBuilder.getProfileCreationDate(activity, profile))
                        .setSequenceNumber(TelemetryCorePingBuilder.getAndIncrementSequenceNumber(sharedPrefs))
                        .setSessionCount(sessionMeasurementsContainer.sessionCount)
                        .setSessionDuration(sessionMeasurementsContainer.elapsedSeconds);
                maybeSetOptionalMeasurements(activity, sharedPrefs, pingBuilder);

                getTelemetryDispatcher(activity).queuePingForUpload(activity, pingBuilder);
            }
        });
    }

    private void maybeSetOptionalMeasurements(final Context context, final SharedPreferences sharedPrefs,
                                              final TelemetryCorePingBuilder pingBuilder) {
        final String distributionId = sharedPrefs.getString(DistributionStoreCallback.PREF_DISTRIBUTION_ID, null);
        if (distributionId != null) {
            pingBuilder.setOptDistributionID(distributionId);
        }

        final ExtendedJSONObject searchCounts = SearchCountMeasurements.getAndZeroSearch(sharedPrefs);
        if (searchCounts.size() > 0) {
            pingBuilder.setOptSearchCounts(searchCounts);
        }

        final String campaignId = CampaignIdMeasurements.getCampaignIdFromPrefs(context);
        if (campaignId != null) {
            pingBuilder.setOptCampaignId(campaignId);
        }

        final AccessibilityManager accessibilityManager = (AccessibilityManager)
            context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (accessibilityManager != null) {
            final List<AccessibilityServiceInfo> enabledServices =
                accessibilityManager.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
            if (enabledServices.size() > 0) {
                pingBuilder.setOptAccessibility(enabledServices);
            }
        }
    }

    @Override
    public void onCampaignIdChanged(String campaignId) {
        CampaignIdMeasurements.updateCampaignIdPref(getBrowserApp(), campaignId);
    }
}
