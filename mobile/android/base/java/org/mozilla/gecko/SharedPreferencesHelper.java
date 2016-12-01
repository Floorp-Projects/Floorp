/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;
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
             implements GeckoEventListener
{
    public static final String LOGTAG = "GeckoAndSharedPrefs";

    // Calculate this once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static final boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);

    private enum Scope {
        APP("app"),
        PROFILE("profile"),
        GLOBAL("global");

        public final String key;

        private Scope(String key) {
            this.key = key;
        }

        public static Scope forKey(String key) {
            for (Scope scope : values()) {
                if (scope.key.equals(key)) {
                    return scope;
                }
            }

            throw new IllegalStateException("SharedPreferences scope must be valid.");
        }
    }

    protected final Context mContext;

    // mListeners is not synchronized because it is only updated in
    // handleObserve, which is called from Gecko serially.
    protected final Map<String, SharedPreferences.OnSharedPreferenceChangeListener> mListeners;

    public SharedPreferencesHelper(Context context) {
        mContext = context;

        mListeners = new HashMap<String, SharedPreferences.OnSharedPreferenceChangeListener>();

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "SharedPreferences:Set",
            "SharedPreferences:Get",
            "SharedPreferences:Observe");
    }

    public synchronized void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "SharedPreferences:Set",
            "SharedPreferences:Get",
            "SharedPreferences:Observe");
    }

    private SharedPreferences getSharedPreferences(JSONObject message) throws JSONException {
        final Scope scope = Scope.forKey(message.getString("scope"));
        switch (scope) {
            case APP:
                return GeckoSharedPrefs.forApp(mContext);
            case PROFILE:
                final String profileName = message.optString("profileName", null);
                if (profileName == null) {
                    return GeckoSharedPrefs.forProfile(mContext);
                } else {
                    return GeckoSharedPrefs.forProfileName(mContext, profileName);
                }
            case GLOBAL:
                final String branch = message.optString("branch", null);
                if (branch == null) {
                    return PreferenceManager.getDefaultSharedPreferences(mContext);
                } else {
                    return mContext.getSharedPreferences(branch, Context.MODE_PRIVATE);
                }
        }

        return null;
    }

    private String getBranch(Scope scope, String profileName, String branch) {
        switch (scope) {
            case APP:
                return GeckoSharedPrefs.APP_PREFS_NAME;
            case PROFILE:
                if (profileName == null) {
                    profileName = GeckoProfile.get(mContext).getName();
                }

                return GeckoSharedPrefs.PROFILE_PREFS_NAME_PREFIX + profileName;
            case GLOBAL:
                return branch;
        }

        return null;
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
        SharedPreferences.Editor editor = getSharedPreferences(message).edit();

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
            editor.apply();
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
    private JSONArray handleGet(JSONObject message) throws JSONException {
        SharedPreferences prefs = getSharedPreferences(message);
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

        return jsonValues;
    }

    private static class ChangeListener
        implements SharedPreferences.OnSharedPreferenceChangeListener {
        public final Scope scope;
        public final String branch;
        public final String profileName;

        public ChangeListener(final Scope scope, final String branch, final String profileName) {
            this.scope = scope;
            this.branch = branch;
            this.profileName = profileName;
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            if (logVerbose) {
                Log.v(LOGTAG, "Got onSharedPreferenceChanged");
            }
            try {
                final JSONObject msg = new JSONObject();
                msg.put("scope", this.scope.key);
                msg.put("branch", this.branch);
                msg.put("profileName", this.profileName);
                msg.put("key", key);

                // Truly, this is awful, but the API impedance is strong: there
                // is no way to get a single untyped value from a
                // SharedPreferences instance.
                msg.put("value", sharedPreferences.getAll().get(key));

                GeckoAppShell.notifyObservers("SharedPreferences:Changed", msg.toString());
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
        final SharedPreferences prefs = getSharedPreferences(message);
        final boolean enable = message.getBoolean("enable");

        final Scope scope = Scope.forKey(message.getString("scope"));
        final String profileName = message.optString("profileName", null);
        final String branch = getBranch(scope, profileName, message.optString("branch", null));

        if (branch == null) {
            Log.e(LOGTAG, "No branch specified for SharedPreference:Observe; aborting.");
            return;
        }

        // mListeners is only modified in this one observer, which is called
        // from Gecko serially.
        if (enable && !this.mListeners.containsKey(branch)) {
            SharedPreferences.OnSharedPreferenceChangeListener listener
                = new ChangeListener(scope, branch, profileName);
            this.mListeners.put(branch, listener);
            prefs.registerOnSharedPreferenceChangeListener(listener);
        }
        if (!enable && this.mListeners.containsKey(branch)) {
            SharedPreferences.OnSharedPreferenceChangeListener listener
                = this.mListeners.remove(branch);
            prefs.unregisterOnSharedPreferenceChangeListener(listener);
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        // Everything here is synchronous and serial, so we need not worry about
        // overwriting an in-progress response.
        try {
            if (event.equals("SharedPreferences:Set")) {
                if (logVerbose) {
                    Log.v(LOGTAG, "Got SharedPreferences:Set message.");
                }
                handleSet(message);
            } else if (event.equals("SharedPreferences:Get")) {
                if (logVerbose) {
                    Log.v(LOGTAG, "Got SharedPreferences:Get message.");
                }
                JSONObject obj = new JSONObject();
                obj.put("values", handleGet(message));
                EventDispatcher.sendResponse(message, obj);
            } else if (event.equals("SharedPreferences:Observe")) {
                if (logVerbose) {
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
}
