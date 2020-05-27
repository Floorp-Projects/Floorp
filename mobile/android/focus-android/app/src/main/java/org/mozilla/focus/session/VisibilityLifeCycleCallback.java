/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;

import org.mozilla.focus.FocusApplication;

/**
 * This ActivityLifecycleCallbacks implementations tracks if there is at least one activity in the
 * STARTED state (meaning some part of our application is visible).
 * Based on this information the current task can be removed if the app is not visible.
 */
public class VisibilityLifeCycleCallback implements Application.ActivityLifecycleCallbacks {
    /**
     * If all activities of this app are in the background then finish and remove all tasks. After
     * that the app won't show up in "recent apps" anymore.
     */
    /* package */ static void finishAndRemoveTaskIfInBackground(Context context) {
        ((FocusApplication) context.getApplicationContext())
                .getVisibilityLifeCycleCallback()
                .finishAndRemoveTaskIfInBackground();
    }

    /* package */ static boolean isInBackground(Context context) {
        return ((FocusApplication) context.getApplicationContext())
                .getVisibilityLifeCycleCallback()
                .activitiesInStartedState == 0;
    }

    private final Context context;

    /**
     * Activities are not stopped/started in an ordered way. So we are using
     */
    private int activitiesInStartedState;

    public VisibilityLifeCycleCallback(Context context) {
        this.context = context;
    }

    private void finishAndRemoveTaskIfInBackground() {
        if (activitiesInStartedState == 0) {
            final ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (activityManager == null) {
                return;
            }

            for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                task.finishAndRemoveTask();
            }
        }
    }

    @Override
    public void onActivityStarted(Activity activity) {
        activitiesInStartedState++;
    }

    @Override
    public void onActivityStopped(Activity activity) {
        activitiesInStartedState--;
    }

    @Override
    public void onActivityResumed(Activity activity) {}

    @Override
    public void onActivityPaused(Activity activity) {}

    @Override
    public void onActivityCreated(Activity activity, Bundle bundle) {}

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle bundle) {}

    @Override
    public void onActivityDestroyed(Activity activity) {}
}
