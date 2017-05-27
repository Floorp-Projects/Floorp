//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.app.Application;
import android.content.Context;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.annotations.Parser;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.MmaConstants;
import org.mozilla.gecko.util.ContextUtils;

import java.util.HashMap;
import java.util.Map;


public class MmaLeanplumImp implements MmaInterface {
    @Override
    public void init(Activity activity) {
        Leanplum.setApplicationContext(activity.getApplicationContext());

        LeanplumActivityHelper.enableLifecycleCallbacks(activity.getApplication());

        if (AppConstants.MOZILLA_OFFICIAL) {
            Leanplum.setAppIdForProductionMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        } else {
            Leanplum.setAppIdForDevelopmentMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        }


        Map<String, Object> attributes = new HashMap<>();
        boolean installedFocus = ContextUtils.isPackageInstalled(activity, "org.mozilla.focus");
        boolean installedKlar = ContextUtils.isPackageInstalled(activity, "org.mozilla.klar");
        if (installedFocus || installedKlar) {
            attributes.put("focus", "installed");
        }
        Leanplum.start(activity, attributes);
        Leanplum.track("Launch");
        // this is special to Leanplum. Since we defer LeanplumActivityHelper's onResume call till
        // switchboard completes loading, we manually call it here.
        LeanplumActivityHelper.onResume(activity);
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
