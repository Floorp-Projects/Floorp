/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to get/set gecko prefs.
 */
public final class PrefsHelper {
    private static final String LOGTAG = "GeckoPrefsHelper";

    private static boolean sRegistered = false;
    private static final Map<Integer, PrefHandler> sCallbacks = new HashMap<Integer, PrefHandler>();
    private static int sUniqueRequestId = 1;

    public static void getPref(String prefName, PrefHandler callback) {
        JSONArray prefs = new JSONArray();
        prefs.put(prefName);
        getPrefs(prefs, callback);
    }

    public static void getPrefs(String[] prefNames, PrefHandler callback) {
        JSONArray prefs = new JSONArray();
        for (String p : prefNames) {
            prefs.put(p);
        }
        getPrefs(prefs, callback);
    }

    public static void getPrefs(JSONArray prefNames, PrefHandler callback) {
        int requestId;
        synchronized (PrefsHelper.class) {
            ensureRegistered();

            requestId = sUniqueRequestId++;
            sCallbacks.put(requestId, callback);
        }

        GeckoEvent event;
        try {
            JSONObject message = new JSONObject();
            message.put("requestId", requestId);
            message.put("preferences", prefNames);
            event = GeckoEvent.createBroadcastEvent("Preferences:Get", message.toString());
            GeckoAppShell.sendEventToGecko(event);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error while composing Preferences:Get message", e);

            // if we failed to send the message, drop our reference to the callback because
            // otherwise it will leak since we will never get the response
            synchronized (PrefsHelper.class) {
                sCallbacks.remove(requestId);
            }
        }
    }

    private static void ensureRegistered() {
        if (sRegistered) {
            return;
        }

        GeckoAppShell.getEventDispatcher().registerEventListener("Preferences:Data", new GeckoEventListener() {
            @Override public void handleMessage(String event, JSONObject message) {
                try {
                    PrefHandler callback;
                    synchronized (PrefsHelper.class) {
                        try {
                            callback = sCallbacks.remove(message.getInt("requestId"));
                        } catch (Exception e) {
                            callback = null;
                        }
                    }
                    if (callback == null) {
                        Log.d(LOGTAG, "Preferences:Data message had an unknown requestId; ignoring");
                        return;
                    }

                    JSONArray jsonPrefs = message.getJSONArray("preferences");
                    for (int i = 0; i < jsonPrefs.length(); i++) {
                        JSONObject pref = jsonPrefs.getJSONObject(i);
                        String name = pref.getString("name");
                        String type = pref.getString("type");
                        try {
                            if ("bool".equals(type)) {
                                callback.prefValue(name, pref.getBoolean("value"));
                            } else if ("int".equals(type)) {
                                callback.prefValue(name, pref.getInt("value"));
                            } else if ("string".equals(type)) {
                                callback.prefValue(name, pref.getString("value"));
                            } else {
                                Log.e(LOGTAG, "Unknown pref value type [" + type + "] for pref [" + name + "]");
                            }
                        } catch (Exception e) {
                            Log.e(LOGTAG, "Handler for preference [" + name + "] threw exception", e);
                        }
                    }
                    callback.finish();
                } catch (Exception e) {
                    Log.e(LOGTAG, "Error handling Preferences:Data message", e);
                }
            }
        });
        sRegistered = true;
    }

    public static void setPref(String pref, Object value) {
        if (pref == null || pref.length() == 0) {
            throw new IllegalArgumentException("Pref name must be non-empty");
        }

        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", pref);
            if (value instanceof Boolean) {
                jsonPref.put("type", "bool");
                jsonPref.put("value", ((Boolean)value).booleanValue());
            } else if (value instanceof Integer) {
                jsonPref.put("type", "int");
                jsonPref.put("value", ((Integer)value).intValue());
            } else {
                jsonPref.put("type", "string");
                jsonPref.put("value", String.valueOf(value));
            }

            GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Set", jsonPref.toString());
            GeckoAppShell.sendEventToGecko(event);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error setting pref [" + pref + "]", e);
        }
    }

    public interface PrefHandler {
        void prefValue(String pref, boolean value);
        void prefValue(String pref, int value);
        void prefValue(String pref, String value);
        void finish();
    }

    public static abstract class PrefHandlerBase implements PrefHandler {
        public void prefValue(String pref, boolean value) {
            Log.w(LOGTAG, "Unhandled boolean value for pref [" + pref + "]");
        }

        public void prefValue(String pref, int value) {
            Log.w(LOGTAG, "Unhandled int value for pref [" + pref + "]");
        }

        public void prefValue(String pref, String value) {
            Log.w(LOGTAG, "Unhandled String value for pref [" + pref + "]");
        }

        public void finish() {
        }
    }
}
