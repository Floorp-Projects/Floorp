/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventResponder;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;

import java.util.Map;
import java.util.HashMap;

/**
 * Helper class to get, set, and observe Android Shared Preferences.
 */
public final class SharedPreferencesHelper
             implements GeckoEventResponder
{
    public static final String LOGTAG = "GeckoAndSharedPrefs";

    protected final Context mContext;

    protected String mResponse;

    // mListeners is not synchronized because it is only updated in
    // handleObserve, which is called from Gecko serially.
    protected final Map<String, SharedPreferences.OnSharedPreferenceChangeListener> mListeners;

    public SharedPreferencesHelper(Context context) {
        mContext = context;

        mListeners = new HashMap<String, SharedPreferences.OnSharedPreferenceChangeListener>();

        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }
        dispatcher.registerEventListener("SharedPreferences:Set", this);
        dispatcher.registerEventListener("SharedPreferences:Get", this);
        dispatcher.registerEventListener("SharedPreferences:Observe", this);
    }

    public synchronized void uninit() {
        EventDispatcher dispatcher = GeckoAppShell.getEventDispatcher();
        if (dispatcher == null) {
            Log.e(LOGTAG, "Gecko event dispatcher must not be null", new RuntimeException());
            return;
        }

        dispatcher.unregisterEventListener("SharedPreferences:Set", this);
        dispatcher.unregisterEventListener("SharedPreferences:Get", this);
        dispatcher.unregisterEventListener("SharedPreferences:Observe", this);
    }

    private SharedPreferences getSharedPreferences(String branch) {
        if (branch == null) {
            return PreferenceManager.getDefaultSharedPreferences(mContext);
        } else {
            return mContext.getSharedPreferences(branch, Context.MODE_PRIVATE);
        }
    }

    /**
     * Set many SharedPreferences in Android.
     *
     * message.branch must exist, and should be a String SharedPreferences
     * branch name, or null for the default branch.
     * message.preferences should be an array of preferences.  Each preference
     * must include a String name, a String type in ["bool", "int", "string"],
     * and an Object value.
     */
    private void handleSet(JSONObject message) throws JSONException {
        if (!message.has("branch")) {
            Log.e(LOGTAG, "No branch specified for SharedPreference:Set; aborting.");
            return;
        }

        String branch = message.isNull("branch") ? null : message.getString("branch");
        SharedPreferences.Editor editor = getSharedPreferences(branch).edit();

        JSONArray jsonPrefs = message.getJSONArray("preferences");

        for (int i = 0; i < jsonPrefs.length(); i++) {
            JSONObject pref = jsonPrefs.getJSONObject(i);
            String name = pref.getString("name");
            String type = pref.getString("type");
            if ("bool".equals(type)) {
                editor.putBoolean(name, pref.getBoolean("value"));
            } else if ("int".equals(type)) {
                editor.putInt(name, pref.getInt("value"));
            } else if ("string".equals(type)) {
                editor.putString(name, pref.getString("value"));
            } else {
                Log.w(LOGTAG, "Unknown pref value type [" + type + "] for pref [" + name + "]");
            }
            editor.commit();
        }
    }

    /**
     * Get many SharedPreferences from Android.
     *
     * message.branch must exist, and should be a String SharedPreferences
     * branch name, or null for the default branch.
     * message.preferences should be an array of preferences.  Each preference
     * must include a String name, and a String type in ["bool", "int",
     * "string"].
     */
    private String handleGet(JSONObject message) throws JSONException {
        if (!message.has("branch")) {
            Log.e(LOGTAG, "No branch specified for SharedPreference:Get; aborting.");
            return null;
        }

        String branch = message.isNull("branch") ? null : message.getString("branch");
        SharedPreferences prefs = getSharedPreferences(branch);
        JSONArray jsonPrefs = message.getJSONArray("preferences");
        JSONArray jsonValues = new JSONArray();

        for (int i = 0; i < jsonPrefs.length(); i++) {
            JSONObject pref = jsonPrefs.getJSONObject(i);
            String name = pref.getString("name");
            String type = pref.getString("type");
            JSONObject jsonValue = new JSONObject();
            jsonValue.put("name", name);
            jsonValue.put("type", type);
            try {
                if ("bool".equals(type)) {
                    boolean value = prefs.getBoolean(name, false);
                    jsonValue.put("value", value);
                } else if ("int".equals(type)) {
                    int value = prefs.getInt(name, 0);
                    jsonValue.put("value", value);
                } else if ("string".equals(type)) {
                    String value = prefs.getString(name, "");
                    jsonValue.put("value", value);
                } else {
                    Log.w(LOGTAG, "Unknown pref value type [" + type + "] for pref [" + name + "]");
                }
            } catch (ClassCastException e) {
                // Thrown if there is a preference with the given name that is
                // not the right type.
                Log.w(LOGTAG, "Wrong pref value type [" + type + "] for pref [" + name + "]");
            }
            jsonValues.put(jsonValue);
        }

        return jsonValues.toString();
    }

    private static class ChangeListener
        implements SharedPreferences.OnSharedPreferenceChangeListener {
        public final String branch;

        public ChangeListener(final String branch) {
            this.branch = branch;
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                Log.v(LOGTAG, "Got onSharedPreferenceChanged");
            }
            try {
                final JSONObject msg = new JSONObject();
                msg.put("branch", this.branch);
                msg.put("key", key);

                // Truly, this is awful, but the API impedence is strong: there
                // is no way to get a single untyped value from a
                // SharedPreferences instance.
                msg.put("value", sharedPreferences.getAll().get(key));

                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SharedPreferences:Changed", msg.toString()));
            } catch (JSONException e) {
                Log.e(LOGTAG, "Got exception creating JSON object", e);
                return;
            }
        }
    }

    /**
     * Register or unregister a SharedPreferences.OnSharedPreferenceChangeListener.
     *
     * message.branch must exist, and should be a String SharedPreferences
     * branch name, or null for the default branch.
     * message.enable should be a boolean: true to enable listening, false to
     * disable listening.
     */
    private void handleObserve(JSONObject message) throws JSONException {
        if (!message.has("branch")) {
            Log.e(LOGTAG, "No branch specified for SharedPreference:Observe; aborting.");
            return;
        }

        String branch = message.isNull("branch") ? null : message.getString("branch");
        SharedPreferences prefs = getSharedPreferences(branch);
        boolean enable = message.getBoolean("enable");

        // mListeners is only modified in this one observer, which is called
        // from Gecko serially.
        if (enable && !this.mListeners.containsKey(branch)) {
            SharedPreferences.OnSharedPreferenceChangeListener listener = new ChangeListener(branch);
            this.mListeners.put(branch, listener);
            prefs.registerOnSharedPreferenceChangeListener(listener);
        }
        if (!enable && this.mListeners.containsKey(branch)) {
            SharedPreferences.OnSharedPreferenceChangeListener listener = this.mListeners.remove(branch);
            prefs.unregisterOnSharedPreferenceChangeListener(listener);
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        // Everything here is synchronous and serial, so we need not worry about
        // overwriting an in-progress response.
        mResponse = null;

        try {
            if (event.equals("SharedPreferences:Set")) {
                if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                    Log.v(LOGTAG, "Got SharedPreferences:Set message.");
                }
                handleSet(message);
            } else if (event.equals("SharedPreferences:Get")) {
                if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                    Log.v(LOGTAG, "Got SharedPreferences:Get message.");
                }
                // Synchronous and serial, so we are the only consumer of
                // mResponse and can write to it freely.
                mResponse = handleGet(message);
            } else if (event.equals("SharedPreferences:Observe")) {
                if (Log.isLoggable(LOGTAG, Log.VERBOSE)) {
                    Log.v(LOGTAG, "Got SharedPreferences:Observe message.");
                }
                handleObserve(message);
            } else {
                Log.e(LOGTAG, "SharedPreferencesHelper got unexpected message " + event);
                return;
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Got exception in handleMessage handling event " + event, e);
            return;
        }
    }

    @Override
    public String getResponse() {
        return mResponse;
    }
}
