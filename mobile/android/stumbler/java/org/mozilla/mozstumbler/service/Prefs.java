/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.location.Location;
import android.os.Build.VERSION;
import android.text.TextUtils;
import android.util.Log;

public  final class Prefs {
    private static final String LOG_TAG = AppGlobals.makeLogTag(Prefs.class.getSimpleName());
    private static final String NICKNAME_PREF = "nickname";
    private static final String USER_AGENT_PREF = "user-agent";
    private static final String VALUES_VERSION_PREF = "values_version";
    private static final String WIFI_ONLY = "wifi_only";
    private static final String LAT_PREF = "lat_pref";
    private static final String LON_PREF = "lon_pref";
    private static final String GEOFENCE_HERE = "geofence_here";
    private static final String GEOFENCE_SWITCH = "geofence_switch";
    private static final String FIREFOX_SCAN_ENABLED = "firefox_scan_on";
    private static final String MOZ_API_KEY = "moz_api_key";
    private static final String WIFI_SCAN_ALWAYS = "wifi_scan_always";
    private static final String LAST_ATTEMPTED_UPLOAD_TIME = "last_attempted_upload_time";
    // Public for MozStumbler to use for manual upgrade of old prefs.
    public static final String PREFS_FILE = Prefs.class.getSimpleName();

    private final SharedPreferences mSharedPrefs;
    static private Prefs sInstance;

    private Prefs(Context context) {
        mSharedPrefs = context.getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);
        if (getPrefs().getInt(VALUES_VERSION_PREF, -1) != AppGlobals.appVersionCode) {
            Log.i(LOG_TAG, "Version of the application has changed. Updating default values.");
            // Remove old keys
            getPrefs().edit()
                    .remove("reports")
                    .remove("power_saving_mode")
                    .commit();

            getPrefs().edit().putInt(VALUES_VERSION_PREF, AppGlobals.appVersionCode).commit();
            getPrefs().edit().commit();
        }
    }

    public static Prefs getInstance(Context c) {
        if (sInstance == null) {
            sInstance = new Prefs(c);
        }
        return sInstance;
    }

    // Allows code without a context handle to grab the prefs. The caller must null check the return value.
    public static Prefs getInstanceWithoutContext() {
        return sInstance;
    }

    ///
    /// Setters
    ///
    public synchronized void setUserAgent(String userAgent) {
        setStringPref(USER_AGENT_PREF, userAgent);
    }

    public synchronized void setUseWifiOnly(boolean state) {
        setBoolPref(WIFI_ONLY, state);
    }

    public synchronized void setGeofenceEnabled(boolean state) {
        setBoolPref(GEOFENCE_SWITCH, state);
    }

    public synchronized void setGeofenceHere(boolean flag) {
        setBoolPref(GEOFENCE_HERE, flag);
    }

    public synchronized void setGeofenceLocation(Location location) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putFloat(LAT_PREF, (float) location.getLatitude());
        editor.putFloat(LON_PREF, (float) location.getLongitude());
        apply(editor);
    }

    public synchronized void setMozApiKey(String s) {
        setStringPref(MOZ_API_KEY, s);
    }

    ///
    /// Getters
    ///
    public synchronized String getUserAgent() {
        String s = getStringPref(USER_AGENT_PREF);
        return (s == null)? AppGlobals.appName + "/" + AppGlobals.appVersionName : s;
    }

    public synchronized boolean getFirefoxScanEnabled() {
        return getBoolPrefWithDefault(FIREFOX_SCAN_ENABLED, false);
    }

    public synchronized String getMozApiKey() {
        String s = getStringPref(MOZ_API_KEY);
        return (s == null)? "no-mozilla-api-key" : s;
    }

    public synchronized boolean getGeofenceEnabled() {
        return getBoolPrefWithDefault(GEOFENCE_SWITCH, false);
    }

    public synchronized boolean getGeofenceHere() {
        return getBoolPrefWithDefault(GEOFENCE_HERE, false);
    }

    public synchronized Location getGeofenceLocation() {
        Location loc = new Location(AppGlobals.LOCATION_ORIGIN_INTERNAL);
        loc.setLatitude(getPrefs().getFloat(LAT_PREF, 0));
        loc.setLongitude(getPrefs().getFloat(LON_PREF,0));
        return loc;
    }

    // This is the time an upload was last attempted, not necessarily successful.
    // Used to ensure upload attempts aren't happening too frequently.
    public synchronized long getLastAttemptedUploadTime() {
        return getPrefs().getLong(LAST_ATTEMPTED_UPLOAD_TIME, 0);
    }

    public synchronized String getNickname() {
        String nickname = getStringPref(NICKNAME_PREF);
        if (nickname != null) {
            nickname = nickname.trim();
        }
        return TextUtils.isEmpty(nickname) ? null : nickname;
    }

    public synchronized void setFirefoxScanEnabled(boolean on) {
        setBoolPref(FIREFOX_SCAN_ENABLED, on);
    }

    public synchronized void setLastAttemptedUploadTime(long time) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putLong(LAST_ATTEMPTED_UPLOAD_TIME, time);
        apply(editor);
    }

    public synchronized void setNickname(String nick) {
        if (nick != null) {
            nick = nick.trim();
            if (nick.length() > 0) {
                setStringPref(NICKNAME_PREF, nick);
            }
        }
    }

    public synchronized boolean getUseWifiOnly() {
        return getBoolPrefWithDefault(WIFI_ONLY, true);
    }

    public synchronized boolean getWifiScanAlways() {
        return getBoolPrefWithDefault(WIFI_SCAN_ALWAYS, false);
    }

    public synchronized void setWifiScanAlways(boolean b) {
        setBoolPref(WIFI_SCAN_ALWAYS, b);
    }

    ///
    /// Privates
    ///

    private String getStringPref(String key) {
        return getPrefs().getString(key, null);
    }

    private boolean getBoolPrefWithDefault(String key, boolean def) {
        return getPrefs().getBoolean(key, def);
    }

    private void setBoolPref(String key, Boolean state) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putBoolean(key,state);
        apply(editor);
    }

    private void setStringPref(String key, String value) {
        SharedPreferences.Editor editor = getPrefs().edit();
        editor.putString(key, value);
        apply(editor);
    }

    @TargetApi(9)
    private static void apply(SharedPreferences.Editor editor) {
        if (VERSION.SDK_INT >= 9) {
            editor.apply();
        } else if (!editor.commit()) {
            Log.e(LOG_TAG, "", new IllegalStateException("commit() failed?!"));
        }
    }

    private SharedPreferences getPrefs() {
        return mSharedPrefs;
    }
}
