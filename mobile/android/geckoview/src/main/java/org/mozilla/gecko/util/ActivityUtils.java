/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.Build;
import android.view.View;
import android.view.Window;

public class ActivityUtils {
    private ActivityUtils() {
    }

    public static void setFullScreen(final Activity activity, final boolean fullscreen) {
        // Hide/show the system notification bar
        Window window = activity.getWindow();

        int newVis;
        if (fullscreen) {
            newVis = View.SYSTEM_UI_FLAG_FULLSCREEN;
            if (Build.VERSION.SDK_INT >= 19) {
                newVis |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            } else {
                newVis |= View.SYSTEM_UI_FLAG_LOW_PROFILE;
            }
        } else {
            // no need to prevent status bar to appear when exiting full screen
            preventDisplayStatusbar(activity, false);
            newVis = View.SYSTEM_UI_FLAG_VISIBLE;
        }

        if (Build.VERSION.SDK_INT >= 23) {
            // We also have to set SYSTEM_UI_FLAG_LIGHT_STATUS_BAR with to current system ui status
            // to support both light and dark status bar.
            final int oldVis = window.getDecorView().getSystemUiVisibility();
            newVis |= (oldVis & View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
        }

        window.getDecorView().setSystemUiVisibility(newVis);
    }

    public static boolean isFullScreen(final Activity activity) {
        final Window window = activity.getWindow();

        final int vis = window.getDecorView().getSystemUiVisibility();
        return (vis & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0;
    }

    /**
     * Finish this activity and launch the default home screen activity.
     */
    public static void goToHomeScreen(final Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);

        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    public static Activity getActivityFromContext(final Context outerContext) {
        Context context = outerContext;
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }

    public static void preventDisplayStatusbar(final Activity activity,
                                               final boolean registering) {
        final View decorView = activity.getWindow().getDecorView();
        if (registering) {
            decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
                @Override
                public void onSystemUiVisibilityChange(final int visibility) {
                    if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                        setFullScreen(activity, true);
                    }
                }
            });
        } else {
            decorView.setOnSystemUiVisibilityChangeListener(null);
        }

    }
}
