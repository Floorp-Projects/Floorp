/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.AppConstants.Versions;

import android.content.Context;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import java.lang.reflect.Method;

public class WindowUtils {
    private static final String LOGTAG = "Gecko" + WindowUtils.class.getSimpleName();

    private WindowUtils() { /* To prevent instantiation */ }

    /**
     * Returns the best-guess physical device dimensions, including the system status bars. Note
     * that DisplayMetrics.height/widthPixels does not include the system bars.
     *
     * via http://stackoverflow.com/a/23861333
     *
     * @param context the calling Activity's Context
     * @return The number of pixels of the device's largest dimension, ignoring software status bars
     */
    public static int getLargestDimension(final Context context) {
        final Display display = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();

        if (Versions.feature17Plus) {
            final DisplayMetrics realMetrics = new DisplayMetrics();
            display.getRealMetrics(realMetrics);
            return Math.max(realMetrics.widthPixels, realMetrics.heightPixels);

        } else if (Versions.feature14Plus) {
            int tempWidth;
            int tempHeight;
            try {
                final Method getRawH = Display.class.getMethod("getRawHeight");
                final Method getRawW = Display.class.getMethod("getRawWidth");
                tempWidth = (Integer) getRawW.invoke(display);
                tempHeight = (Integer) getRawH.invoke(display);
            } catch (Exception e) {
                // This is the best we can do.
                tempWidth = display.getWidth();
                tempHeight = display.getHeight();
                Log.w(LOGTAG, "Couldn't use reflection to get the real display metrics.");
            }

            return Math.max(tempWidth, tempHeight);

        } else {
            // This should be close, as lower API devices should not have window navigation bars.
            return Math.max(display.getWidth(), display.getHeight());
        }
    }
}
