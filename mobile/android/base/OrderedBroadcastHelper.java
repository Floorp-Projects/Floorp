/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import android.util.Log;

/**
 * Helper class to send Android Ordered Broadcasts.
 */
public final class OrderedBroadcastHelper
             implements GeckoEventListener
{
    public static final String LOGTAG = "GeckoOrdBroadcast";

    public static final String SEND_EVENT = "OrderedBroadcast:Send";

    protected final Context mContext;

    public OrderedBroadcastHelper(Context context) {
        mContext = context;

        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.registerEventListener(SEND_EVENT, this);
    }

    public synchronized void uninit() {
        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.unregisterEventListener(SEND_EVENT, this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        if (!SEND_EVENT.equals(event)) {
            Log.e(LOGTAG, "OrderedBroadcastHelper got unexpected message " + event);
            return;
        }

        try {
            final String action = message.getString("action");
            if (action == null) {
                Log.e(LOGTAG, "action must not be null");
                return;
            }

            final String responseEvent = message.getString("responseEvent");
            if (responseEvent == null) {
                Log.e(LOGTAG, "responseEvent must not be null");
                return;
            }

            // It's fine if the caller-provided token is missing or null.
            final Object token = (message.has("token") && !message.isNull("token")) ?
                message.getJSONObject("token") : null;

            // And a missing or null permission means no permission.
            final String permission = (message.has("permission") && !message.isNull("permission")) ?
                message.getString("permission") : null;

            final BroadcastReceiver resultReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    int code = getResultCode();

                    if (code == Activity.RESULT_OK) {
                        String data = getResultData();

                        JSONObject res = new JSONObject();
                        try {
                            res.put("action", action);
                            res.put("token", token);
                            res.put("data", data);
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Got exception in onReceive handling action " + action, e);
                            return;
                        }

                        GeckoEvent event = GeckoEvent.createBroadcastEvent(responseEvent, res.toString());
                        GeckoAppShell.sendEventToGecko(event);
                    }
                }
            };

            Intent intent = new Intent(action);
            mContext.sendOrderedBroadcast(intent,
                                          permission,
                                          resultReceiver,
                                          null,
                                          Activity.RESULT_OK,
                                          null,
                                          null);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Got exception in handleMessage handling event " + event, e);
            return;
        }
    }
}
