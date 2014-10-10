/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.JSONUtils;
import org.mozilla.gecko.util.WebActivityMapper;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.util.Log;

import java.util.Arrays;
import java.util.List;

public final class IntentHelper implements GeckoEventListener {
    private static final String LOGTAG = "GeckoIntentHelper";
    private static final String[] EVENTS = {
        "Intent:GetHandlers",
        "Intent:Open",
        "Intent:OpenForResult",
        "WebActivity:Open"
    };
    private static IntentHelper instance;

    private final Activity activity;

    private IntentHelper(Activity activity) {
        this.activity = activity;
        EventDispatcher.getInstance().registerGeckoThreadListener(this, EVENTS);
    }

    public static IntentHelper init(Activity activity) {
        if (instance == null) {
            instance = new IntentHelper(activity);
        } else {
            Log.w(LOGTAG, "IntentHelper.init() called twice, ignoring.");
        }

        return instance;
    }

    public static void destroy() {
        if (instance != null) {
            EventDispatcher.getInstance().unregisterGeckoThreadListener(instance, EVENTS);
            instance = null;
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Intent:GetHandlers")) {
                getHandlers(message);
            } else if (event.equals("Intent:Open")) {
                open(message);
            } else if (event.equals("Intent:OpenForResult")) {
                openForResult(message);
            } else if (event.equals("WebActivity:Open")) {
                openWebActivity(message);
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    private void getHandlers(JSONObject message) throws JSONException {
        final Intent intent = GeckoAppShell.getOpenURIIntent(activity,
                                                             message.optString("url"),
                                                             message.optString("mime"),
                                                             message.optString("action"),
                                                             message.optString("title"));
        final List<String> appList = Arrays.asList(GeckoAppShell.getHandlersForIntent(intent));

        final JSONObject response = new JSONObject();
        response.put("apps", new JSONArray(appList));
        EventDispatcher.sendResponse(message, response);
    }

    private void open(JSONObject message) throws JSONException {
        GeckoAppShell.openUriExternal(message.optString("url"),
                                      message.optString("mime"),
                                      message.optString("packageName"),
                                      message.optString("className"),
                                      message.optString("action"),
                                      message.optString("title"));
    }

    private void openForResult(final JSONObject message) throws JSONException {
        Intent intent = GeckoAppShell.getOpenURIIntent(activity,
                                                       message.optString("url"),
                                                       message.optString("mime"),
                                                       message.optString("action"),
                                                       message.optString("title"));
        intent.setClassName(message.optString("packageName"), message.optString("className"));
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        ActivityHandlerHelper.startIntentForActivity(activity, intent, new ResultHandler(message));
    }

    private void openWebActivity(JSONObject message) throws JSONException {
        final Intent intent = WebActivityMapper.getIntentForWebActivity(message.getJSONObject("activity"));
        ActivityHandlerHelper.startIntentForActivity(activity, intent, new ResultHandler(message));
    }

    private static class ResultHandler implements ActivityResultHandler {
        private final JSONObject message;

        public ResultHandler(JSONObject message) {
            this.message = message;
        }

        @Override
        public void onActivityResult(int resultCode, Intent data) {
            JSONObject response = new JSONObject();

            try {
                if (data != null) {
                    response.put("extras", JSONUtils.bundleToJSON(data.getExtras()));
                    response.put("uri", data.getData().toString());
                }

                response.put("resultCode", resultCode);
            } catch (JSONException e) {
                Log.w(LOGTAG, "Error building JSON response.", e);
            }

            EventDispatcher.sendResponse(message, response);
        }
    }
}
