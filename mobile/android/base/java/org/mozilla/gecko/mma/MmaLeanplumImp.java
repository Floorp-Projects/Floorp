//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.app.Notification;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.v4.app.NotificationCompat;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.LeanplumPushNotificationCustomizer;
import com.leanplum.LeanplumPushService;
import com.leanplum.internal.Constants;
import com.leanplum.internal.LeanplumInternal;
import com.leanplum.internal.VarCache;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.MmaConstants;

import java.util.Map;
import java.util.UUID;


public class MmaLeanplumImp implements MmaInterface {


    @Override
    public void init(final Activity activity, Map<String, ?> attributes) {
        if (activity == null) {
            return;
        }

        // Need to call this in the eventuality that in this app session stop() has been called.
        // It will allow LeanPlum to communicate again with the servers.
        Leanplum.setIsTestModeEnabled(false);

        Leanplum.setApplicationContext(activity.getApplicationContext());

        LeanplumActivityHelper.enableLifecycleCallbacks(activity.getApplication());

        if (AppConstants.MOZILLA_OFFICIAL) {
            Leanplum.setAppIdForProductionMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        } else {
            Leanplum.setAppIdForDevelopmentMode(MmaConstants.MOZ_LEANPLUM_SDK_CLIENTID, MmaConstants.MOZ_LEANPLUM_SDK_KEY);
        }

        LeanplumPushService.setGcmSenderId(AppConstants.MOZ_ANDROID_GCM_SENDERIDS);

        if (attributes != null) {
            Leanplum.start(activity, attributes);
        } else {
            Leanplum.start(activity);
        }

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
    public void setCustomIcon(@DrawableRes final int iconResId) {
        LeanplumPushService.setCustomizer(new LeanplumPushNotificationCustomizer() {
            @Override
            public void customize(NotificationCompat.Builder builder, Bundle notificationPayload) {
                builder.setSmallIcon(iconResId);
                builder.setDefaults(Notification.DEFAULT_SOUND);
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

    // This method digs deep into LeanPlum internals.
    // It's currently the only way to achieve what we needed:
    //      - fully stop LeanPlum: tracking events and displaying messages
    //      - allow LP to be restarted in the same app session
    // Did extensive testing to ensure all will work as intended
    // but although this method uses LP public methods it should be maintained in accordance with LP SDK updates.
    @Override
    public void stop() {
        // This will just inform LeanPlum server that we stopped current LeanPlum session
        Leanplum.stop();

        // As written in LeanPlum SDK documentation, "This prevents Leanplum from communicating with the server."
        // as this "isTestMode" flag is checked before LeanPlum SDK does anything.
        // Also has the benefit effect of blocking the display of already downloaded messages.
        //
        // The reverse of this - setIsTestModeEnabled(false) must be called before trying to init LP in the same session.
        Leanplum.setIsTestModeEnabled(true);

        // This is just to allow restarting LP and it's functionality in the same app session
        // as LP stores it's state internally and check against it.
        LeanplumInternal.setCalledStart(false);
        LeanplumInternal.setHasStarted(false);
    }

    @Override
    public boolean handleGcmMessage(Context context, String from, Bundle bundle) {
        if (from != null && from.equals(MmaConstants.MOZ_MMA_SENDER_ID) && bundle.containsKey(Constants.Keys.PUSH_MESSAGE_TEXT)) {
            LeanplumPushService.handleNotification(context, bundle);
            return true;
        }
        return false;
    }

    @Override
    public void setDeviceId(@NonNull String deviceId) {
        Leanplum.setDeviceId(deviceId);
    }

}
