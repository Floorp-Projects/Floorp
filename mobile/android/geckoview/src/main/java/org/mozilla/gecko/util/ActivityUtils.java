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
import android.view.WindowManager;

public class ActivityUtils {
    private ActivityUtils() {
    }

    public static void setFullScreen(Activity activity, boolean fullscreen) {
        // Hide/show the system notification bar
        Window window = activity.getWindow();

        if (Build.VERSION.SDK_INT >= 16) {
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
                newVis = View.SYSTEM_UI_FLAG_VISIBLE;
            }

            window.getDecorView().setSystemUiVisibility(newVis);
        } else {
            window.setFlags(fullscreen ?
                            WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }

    public static boolean isFullScreen(final Activity activity) {
        final Window window = activity.getWindow();

        if (Build.VERSION.SDK_INT >= 16) {
            final int vis = window.getDecorView().getSystemUiVisibility();
            return (vis & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0;
        }

        final int flags = window.getAttributes().flags;
        return ((flags & WindowManager.LayoutParams.FLAG_FULLSCREEN) != 0);
    }

    /**
     * Finish this activity and launch the default home screen activity.
     */
    public static void goToHomeScreen(Activity activity) {
        Intent intent = new Intent(Intent.ACTION_MAIN);

        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity.startActivity(intent);
    }

    public static Activity getActivityFromContext(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }
}
