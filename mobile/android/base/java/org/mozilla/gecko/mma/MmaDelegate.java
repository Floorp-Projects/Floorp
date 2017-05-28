//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.util.Log;

import org.mozilla.gecko.MmaConstants;
import org.mozilla.gecko.PrefsHelper;


public class MmaDelegate {

    private static final String TAG = "MmaDelegate";
    private static final String KEY_PREF_BOOLEAN_MMA_ENABLED = "mma.enabled";
    private static final String[] PREFS = { KEY_PREF_BOOLEAN_MMA_ENABLED };


    private static boolean isGeckoPrefOn = false;
    private static MmaInterface mmaHelper = MmaConstants.getMma();


    public static void init(Activity activity) {
        setupPrefHandler(activity);
    }

    public static void stop() {
        mmaHelper.stop();
    }

    private static void setupPrefHandler(final Activity activity) {
        PrefsHelper.PrefHandler handler = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (pref.equals(KEY_PREF_BOOLEAN_MMA_ENABLED)) {
                    Log.d(TAG, "prefValue() called with: pref = [" + pref + "], value = [" + value + "]");
                    if (value) {
                        mmaHelper.init(activity);
                        isGeckoPrefOn = true;
                    } else {
                        isGeckoPrefOn = false;
                    }
                }
            }
        };
        PrefsHelper.addObserver(PREFS, handler);
    }

    public static void track(String event) {
        if (isGeckoPrefOn) {
            mmaHelper.track(event);
        }
    }

    public static void track(String event, long value) {
        if (isGeckoPrefOn) {
            mmaHelper.track(event, value);
        }
    }
}
