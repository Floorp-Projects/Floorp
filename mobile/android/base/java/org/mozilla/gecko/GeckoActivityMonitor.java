/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;

import java.lang.ref.WeakReference;

public class GeckoActivityMonitor implements Application.ActivityLifecycleCallbacks {
    private static final String LOGTAG = "GeckoActivityMonitor";

    private static final GeckoActivityMonitor instance = new GeckoActivityMonitor();

    private WeakReference<Activity> currentActivity = new WeakReference<>(null);
    private boolean currentActivityInBackground = true;

    public static GeckoActivityMonitor getInstance() {
        return instance;
    }

    private GeckoActivityMonitor() { }

    private void updateActivity(final Activity activity) {
        if (currentActivityInBackground) {
            ((GeckoApplication) activity.getApplication()).onApplicationForeground();
            currentActivityInBackground = false;
        }
        currentActivity = new WeakReference<>(activity);
    }

    private void checkAppGoingIntoBackground(final Activity activity, boolean clearActivity) {
        // For the previous activity, this is called after onStart/onResume for the
        // new/resumed activity, so if we're switching activities within our app,
        // currentActivity should already refer to the next activity at this point.
        if (currentActivity.get() != activity) {
            // Some other activity of ours is in the process of starting,
            // so we remain the active app.
            return;
        }
        // No one wants to replace us - we're still the current activity within our app and are
        // therefore going into the background.

        if (clearActivity) {
            currentActivity.clear();
        }
        if (!currentActivityInBackground) {
            ((GeckoApplication) activity.getApplication()).onApplicationBackground();
            currentActivityInBackground = true;
        }
    }

    public Activity getCurrentActivity() {
        return currentActivity.get();
    }

    public synchronized void initialize(final GeckoApplication app) {
        app.registerActivityLifecycleCallbacks(this);
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) { }

    @Override
    public void onActivityStarted(Activity activity) {
        updateActivity(activity);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        updateActivity(activity);
    }

    @Override
    public void onActivityPaused(Activity activity) { }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
        // We need to trigger our application-background handling here already, so that we have a
        // chance to update the session store data for private tabs before it is saved as part of
        // the saved instance state, but at the same time we don't yet want to clear the current
        // activity until we're stopping for real.
        checkAppGoingIntoBackground(activity, false);
    }

    @Override
    public void onActivityStopped(Activity activity) {
        checkAppGoingIntoBackground(activity, true);
    }

    @Override
    public void onActivityDestroyed(Activity activity) { }
}
