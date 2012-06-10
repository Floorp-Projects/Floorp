/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;

public class GeckoActionBar {

    public static View getCustomView(Activity activity) {
        return activity.getActionBar().getCustomView();
    }
 
    public static void hide(Activity activity) {
        activity.getActionBar().hide();
    }

    public static void setBackgroundDrawable(Activity activity, Drawable drawable) {
         activity.getActionBar().setBackgroundDrawable(drawable);
    }

    public static void setDisplayOptions(Activity activity, int options, int mask) {
         activity.getActionBar().setDisplayOptions(options, mask);
    }

    public static void setCustomView(Activity activity, View view) {
         activity.getActionBar().setCustomView(view);
    }

    public static void setDisplayHomeAsUpEnabled(Activity activity, boolean enabled) {
         activity.getActionBar().setDisplayHomeAsUpEnabled(enabled);
    } 
 
    public static void show(Activity activity) {
        activity.getActionBar().show();
    }
}
