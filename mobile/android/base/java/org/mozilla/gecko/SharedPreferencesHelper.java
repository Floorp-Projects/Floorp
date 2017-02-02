/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

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
             implements BundleEventListener
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

    private SharedPreferences getSharedPreferences(final GeckoBundle message) {
        final Scope scope = Scope.forKey(message.getString("scope"));
        switch (scope) {
            case APP:
                return GeckoSharedPrefs.forApp(mContext);
            case PROFILE:
                final String profileName = message.getString("profileName", null);
                if (profileName == null) {
                    return GeckoSharedPrefs.forProfile(mContext);
                } else {
                    return GeckoSharedPrefs.forProfileName(mContext, profileName);
                }
            case GLOBAL:
                final String branch = message.getString("branch", null);
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
    private void handleSet(final GeckoBundle message) {
        SharedPreferences.Editor editor = getSharedPreferences(message).edit();

        final GeckoBundle[] bundlePrefs = message.getBundleArray("preferences");

        for (int i = 0; i < bundlePrefs.length; i++) {
            final GeckoBundle pref = bundlePrefs[i];
            final String name = pref.getString("name");
            final String type = pref.getString("type");
            if ("bool".equals(type)) {
                editor.putBoolean(name, pref.getBoolean("value"));
            } else if ("int".equals(type)) {
                editor.putInt(name, pref.getInt("value"));
            } else if ("string".equals(type)) {
                editor.putString(name, pref.getString("value"));
            } else {
                Log.w(LOGTAG, "Unknown pref value type [" + type + "] for pref [" + name + "]");
            }
        }
        editor.apply();
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
    private GeckoBundle[] handleGet(final GeckoBundle message) {
        final SharedPreferences prefs = getSharedPreferences(message);
        final GeckoBundle[] bundlePrefs = message.getBundleArray("preferences");
        final GeckoBundle[] bundleValues = new GeckoBundle[bundlePrefs.length];

        for (int i = 0; i < bundlePrefs.length; i++) {
            final GeckoBundle pref = bundlePrefs[i];
            final String name = pref.getString("name");
            final String type = pref.getString("type");
            final GeckoBundle bundleValue = new GeckoBundle(3);
            bundleValue.putString("name", name);
            bundleValue.putString("type", type);
            try {
                if ("bool".equals(type)) {
                    bundleValue.putBoolean("value", prefs.getBoolean(name, false));
                } else if ("int".equals(type)) {
                    bundleValue.putInt("value", prefs.getInt(name, 0));
                } else if ("string".equals(type)) {
                    bundleValue.putString("value", prefs.getString(name, ""));
                } else {
                    Log.w(LOGTAG, "Unknown pref value type [" + type + "] for pref [" + name + "]");
                }
            } catch (final ClassCastException e) {
                // Thrown if there is a preference with the given name that is
                // not the right type.
                Log.w(LOGTAG, "Wrong pref value type [" + type + "] for pref [" + name + "]", e);
            }
            bundleValues[i] = bundleValue;
        }

        return bundleValues;
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

        private static void putSharedPreference(final GeckoBundle msg,
                                                final SharedPreferences sharedPreferences,
                                                final String key) {
            // Truly, this is awful, but the API impedance is strong: there is no way to
            // get a single untyped value from a SharedPreferences instance.

            try {
                msg.putBoolean("value", sharedPreferences.getBoolean(key, false));
                return;
            } catch (final ClassCastException e) {
            }

            try {
                msg.putInt("value", sharedPreferences.getInt(key, 0));
                return;
            } catch (final ClassCastException e) {
            }

            try {
                msg.putString("value", sharedPreferences.getString(key, ""));
                return;
            } catch (final ClassCastException e) {
            }
        }

        @Override
        public void onSharedPreferenceChanged(final SharedPreferences sharedPreferences,
                                              final String key) {
            if (logVerbose) {
                Log.v(LOGTAG, "Got onSharedPreferenceChanged");
            }

            final GeckoBundle msg = new GeckoBundle(5);
            msg.putString("scope", scope.key);
            msg.putString("branch", branch);
            msg.putString("profileName", profileName);
            msg.putString("key", key);
            putSharedPreference(msg, sharedPreferences, key);

            EventDispatcher.getInstance().dispatch("SharedPreferences:Changed", msg);
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
    private void handleObserve(final GeckoBundle message) {
        final SharedPreferences prefs = getSharedPreferences(message);
        final boolean enable = message.getBoolean("enable");

        final Scope scope = Scope.forKey(message.getString("scope"));
        final String profileName = message.getString("profileName", null);
        final String branch = getBranch(scope, profileName, message.getString("branch", null));

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

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        // Everything here is synchronous and serial, so we need not worry about
        // overwriting an in-progress response.
        if (event.equals("SharedPreferences:Set")) {
            if (logVerbose) {
                Log.v(LOGTAG, "Got SharedPreferences:Set message.");
            }
            handleSet(message);
        } else if (event.equals("SharedPreferences:Get")) {
            if (logVerbose) {
                Log.v(LOGTAG, "Got SharedPreferences:Get message.");
            }
            callback.sendSuccess(handleGet(message));
        } else if (event.equals("SharedPreferences:Observe")) {
            if (logVerbose) {
                Log.v(LOGTAG, "Got SharedPreferences:Observe message.");
            }
            handleObserve(message);
        } else {
            Log.e(LOGTAG, "SharedPreferencesHelper got unexpected message " + event);
            return;
        }
    }
}
