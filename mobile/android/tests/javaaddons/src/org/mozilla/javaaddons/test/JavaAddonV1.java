/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.javaaddons.test;

import android.content.Context;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.javaaddons.JavaAddonInterfaceV1.EventCallback;
import org.mozilla.javaaddons.JavaAddonInterfaceV1.EventDispatcher;
import org.mozilla.javaaddons.JavaAddonInterfaceV1.EventListener;
import org.mozilla.javaaddons.JavaAddonInterfaceV1.RequestCallback;

public class JavaAddonV1 implements EventListener, RequestCallback {
    protected final EventDispatcher mDispatcher;

    public JavaAddonV1(Context context, EventDispatcher dispatcher) {
        mDispatcher = dispatcher;
        mDispatcher.registerEventListener(this, "JavaAddon:V1");
    }

    @Override
    public void handleMessage(Context context, String event, JSONObject message, EventCallback callback) {
        Log.i("JavaAddon", "handleMessage: " + event + ", " + message.toString());
        final JSONObject output = new JSONObject();
        try {
            output.put("outputStringKey", "inputStringKey=" + message.getString("inputStringKey"));
            output.put("outputIntKey", 1 + message.getInt("inputIntKey"));
        } catch (JSONException e) {
            // Should never happen; ignore.
        }
        // Respond.
        if (callback != null) {
            callback.sendSuccess(output);
        }

        // And send an independent Gecko event.
        final JSONObject input = new JSONObject();
        try {
            input.put("inputStringKey", "raw");
            input.put("inputIntKey", 3);
        } catch (JSONException e) {
            // Should never happen; ignore.
        }
        mDispatcher.sendRequestToGecko("JavaAddon:V1:Request", input, this);
    }

    @Override
    public void onResponse(Context context, JSONObject jsonObject) {
        Log.i("JavaAddon", "onResponse: " + jsonObject.toString());
        // Unregister event listener, so that the JavaScript side can send a test message and
        // check it is not handled.
        mDispatcher.unregisterEventListener(this);
        mDispatcher.sendRequestToGecko("JavaAddon:V1:VerificationRequest", jsonObject, null);

        mDispatcher.registerEventListener(new EventListener() {
            @Override
            public void handleMessage(Context context, String event, JSONObject message, EventCallback callback) {
                mDispatcher.unregisterEventListener(this);
                callback.sendSuccess(null);
            }
        }, "JavaAddon:V1:Finish");
    }
}
