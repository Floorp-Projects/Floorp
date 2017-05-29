//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Application;
import android.content.Context;

import org.mozilla.gecko.MmaConstants;
import org.mozilla.gecko.PrefsHelper;


public class MmaDelegate {

    private static final String ENABLE_PREF = "mma.enabled";

    private static MmaInterface mmaHelper = MmaConstants.getMma();

    private static final String[] prefs = { ENABLE_PREF };


    public static void init(Application application) {
        setupPrefHandler(application);
    }

    public static void stop() {
        mmaHelper.stop();
    }

    private static void setupPrefHandler(final Application application) {
        PrefsHelper.PrefHandler handler = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (pref.equals(ENABLE_PREF)) {
                    if (value) {
                        mmaHelper.init(application);
                    } else {
                        mmaHelper.stop();
                    }

                }
            }
        };
        PrefsHelper.addObserver(prefs, handler);
    }
}
