//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Application;
import android.content.Context;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.annotations.Parser;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.MmaConstants;


public class MmaLeanplumImp implements MmaInterface {
    @Override
    public void init(Application application) {
        Leanplum.setApplicationContext(application);
        Parser.parseVariables(application);
        LeanplumActivityHelper.enableLifecycleCallbacks(application);

        if (AppConstants.MOZILLA_OFFICIAL) {
            Leanplum.setAppIdForProductionMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        } else {
            Leanplum.setAppIdForDevelopmentMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        }

        Leanplum.start(application);
    }

    @Override
    public void start(Context context) {

    }

    @Override
    public void track(String leanplumEvent) {
        Leanplum.track(leanplumEvent);

    }

    @Override
    public void track(String leanplumEvent, double value) {
        Leanplum.track(leanplumEvent, value);

    }

    @Override
    public void stop() {
        Leanplum.stop();
    }
}
