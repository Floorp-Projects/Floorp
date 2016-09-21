/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.R;
import org.mozilla.gecko.util.EventCallback;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.CastRemoteDisplayLocalService;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.api.Status;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.util.Log;

public class ChromeCastDisplay implements GeckoPresentationDisplay {

    static final String REMOTE_DISPLAY_APP_ID = "4574A331";

    private static final String LOGTAG = "GeckoChromeCastDisplay";
    private final RouteInfo route;
    private CastDevice castDevice;

    public ChromeCastDisplay(Context context, RouteInfo route) {
        int status =  GooglePlayServicesUtil.isGooglePlayServicesAvailable(context);
        if (status != ConnectionResult.SUCCESS) {
            throw new IllegalStateException("Play services are required for Chromecast support (got status code " + status + ")");
        }

        this.route = route;
        this.castDevice = CastDevice.getFromBundle(route.getExtras());
    }

    public JSONObject toJSON() {
        final JSONObject obj = new JSONObject();
        try {
            if (castDevice == null) {
                return null;
            }
            obj.put("uuid", route.getId());
            obj.put("friendlyName", castDevice.getFriendlyName());
            obj.put("type", "chromecast");
        } catch (JSONException ex) {
            Log.d(LOGTAG, "Error building route", ex);
        }

        return obj;
    }

    @Override
    public void start(EventCallback callback) { }

    @Override
    public void stop(EventCallback callback) { }
}
