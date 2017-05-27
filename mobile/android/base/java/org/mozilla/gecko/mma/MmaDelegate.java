//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Application;
import android.content.Context;

import org.mozilla.gecko.MmaConstants;


public class MmaDelegate {

    private static MmaInterface mmaHelper = MmaConstants.getMma();

    public static void init(Application application) {
        mmaHelper.init(application);
    }

    public void start(Context context) {
        mmaHelper.start(context);
    }

}
