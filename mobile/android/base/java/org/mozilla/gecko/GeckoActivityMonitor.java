/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

public class GeckoActivityMonitor implements Application.ActivityLifecycleCallbacks {
    private static final String LOGTAG = "GeckoActivityMonitor";

    // We only hold a reference to the currently running activity - when this activity pauses,
    // the reference is released or else overwritten by the next activity.
    @SuppressLint("StaticFieldLeak")
    private static final GeckoActivityMonitor instance = new GeckoActivityMonitor();

    private Activity currentActivity;

    public static GeckoActivityMonitor getInstance() {
        return instance;
    }

    private GeckoActivityMonitor() { }

    public Activity getCurrentActivity() {
        return currentActivity;
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        currentActivity = activity;
    }

    // onNewIntent happens in-between a pause/resume cycle, which means that we wouldn't have
    // a current activity to report if we were using only the official ActivityLifecycleCallbacks.
    // For code that wants to know the current activity even at this point we therefore have to
    // handle this ourselves.
    public void onActivityNewIntent(Activity activity) {
        currentActivity = activity;
    }

    @Override
    public void onActivityStarted(Activity activity) {
        currentActivity = activity;
    }

    @Override
    public void onActivityResumed(Activity activity) {
        currentActivity = activity;
    }

    /**
     * Intended to be used if the current activity is required to be up-to-date for code that
     * executes in onCreate/onStart/... before calling the corresponding superclass method.
     */
    public void setCurrentActivity(Activity activity) {
        currentActivity = activity;
    }

    @Override
    public void onActivityPaused(Activity activity) {
        releaseIfCurrentActivity(activity);
    }

    @Override
    public void onActivityStopped(Activity activity) {
        releaseIfCurrentActivity(activity);
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) { }

    @Override
    public void onActivityDestroyed(Activity activity) {
        releaseIfCurrentActivity(activity);
    }

    private void releaseIfCurrentActivity(Activity activity) {
        // If the next activity has already started by the time the previous activity is being
        // stopped/destroyed, we no longer need to clear the previous activity.
        if (currentActivity == activity) {
            currentActivity = null;
        }
    }
}
