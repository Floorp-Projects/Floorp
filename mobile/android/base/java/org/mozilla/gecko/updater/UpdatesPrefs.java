/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.updater.UpdateServiceHelper.AutoDownloadPolicy;
import org.mozilla.gecko.updater.UpdateServiceHelper.UpdateInfo;

import java.io.File;
import java.net.URI;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.TimeZone;

public final class UpdatesPrefs {
    private static final String PREFS_NAME = "UpdateService";
    private static final String KEY_LAST_BUILDID = "UpdateService.lastBuildID";
    private static final String KEY_LAST_HASH_FUNCTION = "UpdateService.lastHashFunction";
    private static final String KEY_LAST_HASH_VALUE = "UpdateService.lastHashValue";
    private static final String KEY_LAST_FILE_NAME = "UpdateService.lastFileName";
    private static final String KEY_LAST_ATTEMPT_DATE = "UpdateService.lastAttemptDate";
    private static final String KEY_AUTODOWNLOAD_POLICY = "UpdateService.autoDownloadPolicy";
    private static final String KEY_UPDATE_URL = "UpdateService.updateUrl";

    private SharedPreferences sharedPrefs;

    UpdatesPrefs(@NonNull final Context context) {
        sharedPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    String getPreferenceName() {
        return PREFS_NAME;
    }

    String getLastBuildID() {
        return sharedPrefs.getString(KEY_LAST_BUILDID, null);
    }

    String getLastHashFunction() {
        return sharedPrefs.getString(KEY_LAST_HASH_FUNCTION, null);
    }

    String getLastHashValue() {
        return sharedPrefs.getString(KEY_LAST_HASH_VALUE, null);
    }

    String getLastFileName() {
        return sharedPrefs.getString(KEY_LAST_FILE_NAME, null);
    }

    Calendar getLastAttemptDate() {
        long lastAttempt = sharedPrefs.getLong(KEY_LAST_ATTEMPT_DATE, -1);
        if (lastAttempt < 0)
            return null;

        GregorianCalendar cal = new GregorianCalendar(TimeZone.getTimeZone("GMT"));
        cal.setTimeInMillis(lastAttempt);
        return cal;
    }

    void setLastAttemptDate() {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putLong(KEY_LAST_ATTEMPT_DATE, System.currentTimeMillis());
        editor.commit();
    }

    AutoDownloadPolicy getAutoDownloadPolicy() {
        return AutoDownloadPolicy.get(sharedPrefs.getInt(KEY_AUTODOWNLOAD_POLICY, AutoDownloadPolicy.WIFI.value));
    }

    void setAutoDownloadPolicy(AutoDownloadPolicy policy) {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putInt(KEY_AUTODOWNLOAD_POLICY, policy.value);
        editor.commit();
    }

    URI getUpdateURI(boolean force) {
        return UpdateServiceHelper.expandUpdateURI(GeckoAppShell.getApplicationContext(),
                sharedPrefs.getString(KEY_UPDATE_URL, null), force);
    }

    void setUpdateUrl(String url) {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putString(KEY_UPDATE_URL, url);
        editor.commit();
    }

    void saveUpdateInfo(UpdateInfo info, File downloaded) {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.putString(KEY_LAST_BUILDID, info.buildID);
        editor.putString(KEY_LAST_HASH_FUNCTION, info.hashFunction);
        editor.putString(KEY_LAST_HASH_VALUE, info.hashValue);
        editor.putString(KEY_LAST_FILE_NAME, downloaded.toString());
        editor.commit();
    }
}
