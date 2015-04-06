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
import android.util.SparseArray;

import java.util.ArrayList;

/**
 * Helper class to get/set gecko prefs.
 */
public final class PrefsHelper {
    private static final String LOGTAG = "GeckoPrefsHelper";

    private static boolean sRegistered;
    private static int sUniqueRequestId = 1;
    static final SparseArray<PrefHandler> sCallbacks = new SparseArray<PrefHandler>();

    public static int getPref(String prefName, PrefHandler callback) {
        return getPrefsInternal(new String[] { prefName }, callback);
    }

    public static int getPrefs(String[] prefNames, PrefHandler callback) {
        return getPrefsInternal(prefNames, callback);
    }

    public static int getPrefs(ArrayList<String> prefNames, PrefHandler callback) {
        return getPrefsInternal(prefNames.toArray(new String[prefNames.size()]), callback);
    }

    private static int getPrefsInternal(String[] prefNames, PrefHandler callback) {
        int requestId;
        synchronized (PrefsHelper.class) {
            ensureRegistered();

            requestId = sUniqueRequestId++;
            sCallbacks.put(requestId, callback);
        }

        GeckoEvent event;
        if (callback.isObserver()) {
            event = GeckoEvent.createPreferencesObserveEvent(requestId, prefNames);
        } else {
            event = GeckoEvent.createPreferencesGetEvent(requestId, prefNames);
        }
        GeckoAppShell.sendEventToGecko(event);

        return requestId;
    }

    private static void ensureRegistered() {
        if (sRegistered) {
            return;
        }

        GeckoEventListener listener = new GeckoEventListener() {
            @Override
            public void handleMessage(String event, JSONObject message) {
                try {
                    PrefHandler callback;
                    synchronized (PrefsHelper.class) {
                        try {
                            int requestId = message.getInt("requestId");
                            callback = sCallbacks.get(requestId);
                            if (callback != null && !callback.isObserver()) {
                                sCallbacks.delete(requestId);
                            }
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
        };
        EventDispatcher.getInstance().registerGeckoThreadListener(listener, "Preferences:Data");
        sRegistered = true;
    }

    public static void setPref(String pref, Object value) {
        setPref(pref, value, false);
    }

    public static void setPref(String pref, Object value, boolean flush) {
        if (pref == null || pref.length() == 0) {
            throw new IllegalArgumentException("Pref name must be non-empty");
        }

        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", pref);
            jsonPref.put("flush", flush);

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

    public static void removeObserver(int requestId) {
        if (requestId < 0) {
            throw new IllegalArgumentException("Invalid request ID");
        }

        synchronized (PrefsHelper.class) {
            PrefHandler callback = sCallbacks.get(requestId);
            sCallbacks.delete(requestId);

            if (callback == null) {
                Log.e(LOGTAG, "Unknown request ID " + requestId);
                return;
            }
        }

        GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:RemoveObserver",
                                                           Integer.toString(requestId));
        GeckoAppShell.sendEventToGecko(event);
    }

    public interface PrefHandler {
        void prefValue(String pref, boolean value);
        void prefValue(String pref, int value);
        void prefValue(String pref, String value);
        boolean isObserver();
        void finish();
    }

    public static abstract class PrefHandlerBase implements PrefHandler {
        @Override
        public void prefValue(String pref, boolean value) {
            Log.w(LOGTAG, "Unhandled boolean value for pref [" + pref + "]");
        }

        @Override
        public void prefValue(String pref, int value) {
            Log.w(LOGTAG, "Unhandled int value for pref [" + pref + "]");
        }

        @Override
        public void prefValue(String pref, String value) {
            Log.w(LOGTAG, "Unhandled String value for pref [" + pref + "]");
        }

        @Override
        public void finish() {
        }

        @Override
        public boolean isObserver() {
            return false;
        }
    }
}
