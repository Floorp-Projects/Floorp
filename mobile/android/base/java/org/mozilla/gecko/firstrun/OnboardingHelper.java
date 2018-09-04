/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.StrictMode;
import android.support.annotation.MainThread;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.ViewStub;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.mma.MmaDelegate;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.switchboard.SwitchBoard;
import org.mozilla.gecko.util.NetworkUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.lang.ref.WeakReference;
import java.util.UUID;

/**
 * Helper class of an an {@link AppCompatActivity} for managing showing the Onboarding screens.
 * <br>The user class will have to implement {@link OnboardingListener}.
 */
public class OnboardingHelper implements MmaDelegate.MmaVariablesChangedListener,
                                         SwitchBoard.ConfigStatusListener {
    private static final String LOGTAG = "OnboardingHelper";
    private static final boolean DEBUG = false;

    @RobocopTarget
    public static final String EXTRA_SKIP_STARTPANE = "skipstartpane";

    /** Be aware of {@link org.mozilla.gecko.fxa.EnvironmentUtils#GECKO_PREFS_FIRSTRUN_UUID}. */
    private static final String FIRSTRUN_UUID = "firstrun_uuid";

    // Speculative timeout for showing the Onboarding screens with the default local values.
    public static final int DELAY_SHOW_DEFAULT_ONBOARDING = 3 * 1000;

    private WeakReference<AppCompatActivity> activityRef;
    private OnboardingListener listener;
    private SafeIntent activityStartingIntent;
    private FirstrunAnimationContainer firstrunAnimationContainer;
    private Runnable showOnboarding;
    private boolean onboardingIsPreparing;
    private boolean abortOnboarding;
    private long startTimeForCheckingOnlineVariables;

    public OnboardingHelper(
            @NonNull final AppCompatActivity activity,
            @NonNull final SafeIntent activityStartingIntent)
            throws IllegalArgumentException {

        if (!(activity instanceof OnboardingListener)) {
            final String activityClass = activity.getClass().getSimpleName();
            final String listenerInterface = OnboardingListener.class.getSimpleName();
            throw new IllegalArgumentException(
                    String.format("%s does not implement %s", activityClass, listenerInterface));
        }

        this.activityRef = new WeakReference<>(activity);
        this.listener = (OnboardingListener) activity;
        this.activityStartingIntent = activityStartingIntent;
    }

    /**
     * Check and show the firstrun pane if the browser has never been launched and
     * is not opening an external link from another application.
     */
    public void checkFirstRun() {
        if (GeckoThread.getActiveProfile().inGuestMode()) {
            // We do not want to show any first run tour for guest profiles.
            return;
        }

        if (activityStartingIntent.getBooleanExtra(EXTRA_SKIP_STARTPANE, false)) {
            // Note that we don't set the pref, so subsequent launches can result
            // in the firstrun pane being shown.
            return;
        }

        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();

        try {
            AppCompatActivity activity = activityRef.get();
            if (activity == null) {
                return;
            }
            final SharedPreferences prefs = GeckoSharedPrefs.forProfile(activity);

            if (prefs.getBoolean(FirstrunAnimationContainer.PREF_FIRSTRUN_ENABLED_OLD, true) &&
                    prefs.getBoolean(FirstrunAnimationContainer.PREF_FIRSTRUN_ENABLED, true)) {

                onboardingIsPreparing = true;
                listener.onOnboardingProcessStarted();

                // Allow the activity to be gc'ed while waiting for the distribution
                activity = null;

                if (!Intent.ACTION_VIEW.equals(activityStartingIntent.getAction())) {
                    // Check to see if a distribution has turned off the first run pager.
                    final Distribution distribution = Distribution.getInstance(activityRef.get());
                    if (!distribution.shouldWaitForSystemDistribution()) {
                        checkFirstrunInternal();
                    } else {
                        distribution.addOnDistributionReadyCallback(new Distribution.ReadyCallback() {
                            @Override
                            public void distributionNotFound() {
                                ThreadUtils.postToUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        checkFirstrunInternal();
                                    }
                                });
                            }

                            @Override
                            public void distributionFound(final Distribution distribution) {
                                // Check preference again in case distribution turned it off.
                                if (prefs.getBoolean(FirstrunAnimationContainer.PREF_FIRSTRUN_ENABLED, true)) {
                                    ThreadUtils.postToUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            checkFirstrunInternal();
                                        }
                                    });
                                }
                            }

                            @Override
                            public void distributionArrivedLate(final Distribution distribution) {
                            }
                        });
                    }
                }

                // We have no intention of stopping this session. The FIRSTRUN session
                // ends when the browsing session/activity has ended. All events
                // during firstrun will be tagged as FIRSTRUN.
                Telemetry.startUISession(TelemetryContract.Session.FIRSTRUN);
            }
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }
    }

    /**
     * Call this to prevent or finish displaying of the Onboarding process.<br>
     * If it has not yet been shown to the user and now it has been prevented to,
     * showing the Onboarding screens will be retried at the next app start.
     *
     * @return whether Onboarding was prevented / finished early or not.
     */
    public boolean hideOnboarding() {
        abortOnboarding = true;

        if (DEBUG) {
            Log.d(LOGTAG, "hideOnboarding");
        }

        if (isPreparing()) {
            onboardingIsPreparing = false;
            // Cancel showing Onboarding. Will retry automatically at the next app startup.
            ThreadUtils.removeCallbacksFromUiThread(showOnboarding);
            return true;
        }

        if (isOnboardingVisible()) {
            onboardingIsPreparing = false;
            firstrunAnimationContainer.registerOnFinishListener(null);
            firstrunAnimationContainer.hide();
            return true;
        }

        return false;
    }

    private boolean isOnboardingVisible() {
        return firstrunAnimationContainer != null && firstrunAnimationContainer.isVisible();
    }

    /**
     * Get if we are in the process of preparing the Onboarding screens.<br>
     * If the Onboarding screens should be shown to the user, they will be so after a small delay -
     * up to {@link #DELAY_SHOW_DEFAULT_ONBOARDING} necessary for downloading the data
     * needed to populate the screens.
     *
     * @return <code>true</code> - we are preparing for showing Onboarding but haven't yet done
     *         <code>false</code> - Onboarding has been displayed
     */
    public boolean isPreparing() {
        return onboardingIsPreparing;
    }

    /**
     * Code to actually show the first run pager, separated for distribution purposes.<br>
     * If network is available it will first try to use server values for populating the
     * onboarding screens. If that isn't possible the default local values will be used.
     */
    @UiThread
    private void checkFirstrunInternal() {
        final AppCompatActivity activity = activityRef.get();
        if (activity == null) {
            return;
        }

        if (abortOnboarding) {
            return;
        }

        if (NetworkUtils.isConnected(activity)) {
            showOnboarding = new Runnable() {
                @Override
                public void run() {
                    showFirstrunPager(true);
                }
            };

            if (DEBUG) {
                startTimeForCheckingOnlineVariables = System.currentTimeMillis();
            }

            ThreadUtils.postDelayedToUiThread(showOnboarding, DELAY_SHOW_DEFAULT_ONBOARDING);
        } else {
            showFirstrunPager(true);
        }
    }

    private void showFirstrunPager(final boolean useLocalValues) {
        final AppCompatActivity activity = activityRef.get();
        if (activity == null) {
            return;
        }

        onboardingIsPreparing = false;

        if (firstrunAnimationContainer == null) {
            final ViewStub firstrunPagerStub = (ViewStub) activity.findViewById(R.id.firstrun_pager_stub);
            firstrunAnimationContainer = (FirstrunAnimationContainer) firstrunPagerStub.inflate();
        }

        if (DEBUG) {
            final StringBuilder logMessage =
                    new StringBuilder("Will show Onboarding using ")
                            .append((useLocalValues ? "local" : "server"))
                            .append(" values");
            Log.d(LOGTAG, logMessage.toString());
        }

        firstrunAnimationContainer.load
                (activity.getApplicationContext(), activity.getSupportFragmentManager(), useLocalValues);
        firstrunAnimationContainer.registerOnFinishListener(new FirstrunAnimationContainer.OnFinishListener() {
            @Override
            public void onFinish() {
                listener.onFinishedOnboarding(firstrunAnimationContainer.showBrowserHint());
            }
        });

        listener.onOnboardingScreensVisible();
        saveOnboardingShownStatus();
    }

    // The Onboarding screens should only be shown one time.
    private void saveOnboardingShownStatus() {
        // The method is called serially from showFirstrunPager()
        // which stores a hard reference to the activity so it's safe to use it directly
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(activityRef.get());

        prefs.edit()
                // Don't bother trying again to show the v1 minimal first run.
                .putBoolean(FirstrunAnimationContainer.PREF_FIRSTRUN_ENABLED, false)
                // Generate a unique identifier for the current first run.
                // See Bug 1429735 for why we care to do this.
                .putString(FIRSTRUN_UUID, UUID.randomUUID().toString())
                .apply();
    }

    /**
     * Try showing the Onboarding screens even before #DELAY_SHOW_DEFAULT_ONBOARDING.<br>
     * If they have already been shown calling this method has no effect.
     */
    private void tryShowOnboarding(final boolean shouldUseLocalValues) {
        final AppCompatActivity activity = activityRef.get();
        if (activity == null) {
            return;
        }

        if (isPreparing()) {
            ThreadUtils.removeCallbacksFromUiThread(showOnboarding);
            showFirstrunPager(shouldUseLocalValues);
        }
    }

    @Override
    @MainThread
    public void onRemoteVariablesChanged() {
        if (DEBUG) {
            final long timeElapsed = System.currentTimeMillis() - startTimeForCheckingOnlineVariables;
            Log.d(LOGTAG, String.format("Got online variables after: %d millis", timeElapsed));
        }

        tryShowOnboarding(false);
    }

    @Override
    @MainThread
    public void onRemoteVariablesUnavailable() {
        tryShowOnboarding(true);
    }

    @Override
    @MainThread
    public void onExperimentsConfigLoaded() {
        final AppCompatActivity activity = activityRef.get();
        if (activity == null) {
            return;
        }

        // Only if the Mma experiment is available we should continue to wait for server values.
        if (!MmaDelegate.isMmaExperimentEnabled(activity)) {
            tryShowOnboarding(true);
        }
    }

    @Override
    @MainThread
    public void onExperimentsConfigLoadFailed() {
        tryShowOnboarding(true);
    }

    /**
     * Informs about the status of the onboarding process.
     */
    public interface OnboardingListener {

        void onOnboardingProcessStarted();

        void onOnboardingScreensVisible();

        void onFinishedOnboarding(final boolean showBrowserHint);
    }
}
