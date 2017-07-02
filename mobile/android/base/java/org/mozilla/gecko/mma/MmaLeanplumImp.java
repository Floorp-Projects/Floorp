//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.MmaConstants;

import java.util.HashMap;
import java.util.Map;
import java.util.UUID;


public class MmaLeanplumImp implements MmaInterface {

    private static final String KEY_ANDROID_PREF_STRING_LEANPLUM_DEVICE_ID = "android.not_a_preference.leanplum.device_id";

    @Override
    public void init(final Activity activity) {
        if (activity == null) {
            return;
        }
        Leanplum.setApplicationContext(activity.getApplicationContext());

        LeanplumActivityHelper.enableLifecycleCallbacks(activity.getApplication());

        if (AppConstants.MOZILLA_OFFICIAL) {
            Leanplum.setAppIdForProductionMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        } else {
            Leanplum.setAppIdForDevelopmentMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        }

        Map<String, Object> attributes = new HashMap<>();
        boolean installedFocus = isPackageInstalled(activity, "org.mozilla.focus");
        boolean installedKlar = isPackageInstalled(activity, "org.mozilla.klar");
        if (installedFocus || installedKlar) {
            attributes.put("focus", true);
        } else {
            attributes.put("focus", false);
        }

        final SharedPreferences sharedPreferences = activity.getPreferences(0);
        String deviceId = sharedPreferences.getString(KEY_ANDROID_PREF_STRING_LEANPLUM_DEVICE_ID, null);
        if (deviceId == null) {
            deviceId = UUID.randomUUID().toString();
            sharedPreferences.edit().putString(KEY_ANDROID_PREF_STRING_LEANPLUM_DEVICE_ID, deviceId).apply();
        }
        Leanplum.setDeviceId(deviceId);
        Leanplum.start(activity, attributes);

        // this is special to Leanplum. Since we defer LeanplumActivityHelper's onResume call till
        // switchboard completes loading. We miss the call to LeanplumActivityHelper.onResume.
        // So I manually call it here.
        //
        // There's a risk that if this is called after activity's onPause(Although I've
        // tested it's seems okay). We  should require their SDK to separate activity call back with
        // SDK initialization and Activity lifecycle in the future.
        //
        // I put it under runOnUiThread because in current Leanplum's SDK design, this should be run in main thread.
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                LeanplumActivityHelper.onResume(activity);
            }
        });
    }

    @Override
    public void start(Context context) {

    }

    @Override
    public void event(String leanplumEvent) {
        Leanplum.track(leanplumEvent);

    }

    @Override
    public void event(String leanplumEvent, double value) {
        Leanplum.track(leanplumEvent, value);

    }

    @Override
    public void stop() {
        Leanplum.stop();
    }

    private static boolean isPackageInstalled(final Context context, String packageName) {
        try {
            PackageManager pm = context.getPackageManager();
            pm.getPackageInfo(packageName, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }
}
