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

    private GeckoApplication appContext;
    private WeakReference<Activity> currentActivity = new WeakReference<>(null);
    private boolean mInitialized;

    public static GeckoActivityMonitor getInstance() {
        return instance;
    }

    private GeckoActivityMonitor() { }

    public Activity getCurrentActivity() {
        return currentActivity.get();
    }

    public synchronized void initialize(final Context context) {
        if (mInitialized) {
            return;
        }

        appContext = (GeckoApplication) context.getApplicationContext();

        appContext.registerActivityLifecycleCallbacks(this);
        mInitialized = true;
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) { }

    @Override
    public void onActivityStarted(Activity activity) {
        if (currentActivity.get() == null) {
            appContext.onApplicationForeground();
        }
        currentActivity = new WeakReference<>(activity);
    }

    @Override
    public void onActivityResumed(Activity activity) { }

    @Override
    public void onActivityPaused(Activity activity) { }

    @Override
    public void onActivityStopped(Activity activity) {
        // onStop for the previous activity is called after onStart for the new activity, so if
        // we're switching activities within our app, currentActivity should already refer to the
        // next activity at this point.
        // If it doesn't, it means we've been backgrounded.
        if (currentActivity.get() == activity) {
            currentActivity.clear();
            appContext.onApplicationBackground();
        }
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) { }

    @Override
    public void onActivityDestroyed(Activity activity) { }
}
