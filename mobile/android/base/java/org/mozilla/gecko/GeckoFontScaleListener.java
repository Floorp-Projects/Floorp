/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.preferences.GeckoPreferences;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.ContentObserver;
import android.net.Uri;
import android.provider.Settings;
import android.support.annotation.UiThread;
import android.util.Log;

class GeckoFontScaleListener
        extends ContentObserver
        implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String LOGTAG = "GeckoFontScaleListener";

    private static final String PREF_SYSTEM_FONT_SCALE = "font.size.systemFontScale";
    private static final String PREF_FONT_INFLATION = "font.size.inflation.minTwips";
    private static final int FONT_INFLATION_OFF = 0;
    private static final int FONT_INFLATION_ON_DEFAULT_VALUE = 120;
    private static final float DEFAULT_FONT_SCALE = 1.0f;

    // We're referencing the *application* context, so this is in fact okay.
    @SuppressLint("StaticFieldLeak")
    private static final GeckoFontScaleListener sInstance = new GeckoFontScaleListener();

    private Context mApplicationContext;
    private boolean mInitialized;
    private boolean mRunning;

    public static GeckoFontScaleListener getInstance() {
        return sInstance;
    }

    private GeckoFontScaleListener() {
        super(null);
    }

    public synchronized void initialize(final Context context) {
        if (mInitialized) {
            Log.w(LOGTAG, "Already initialized!");
            return;
        }

        mApplicationContext = context.getApplicationContext();
        SharedPreferences prefs = GeckoSharedPrefs.forApp(mApplicationContext);
        prefs.registerOnSharedPreferenceChangeListener(this);
        onPrefChange(prefs);
        mInitialized = true;
    }

    public synchronized void shutdown() {
        if (!mInitialized) {
            Log.w(LOGTAG, "Already shut down!");
            return;
        }

        GeckoSharedPrefs.forApp(mApplicationContext).unregisterOnSharedPreferenceChangeListener(this);
        stop();
        mApplicationContext = null;
        mInitialized = false;
    }

    private synchronized void start() {
        if (mRunning) {
            return;
        }

        ContentResolver contentResolver = mApplicationContext.getContentResolver();
        Uri fontSizeSetting = Settings.System.getUriFor(Settings.System.FONT_SCALE);
        contentResolver.registerContentObserver(fontSizeSetting, false, this);
        onSystemFontScaleChange(contentResolver, false);

        mRunning = true;
    }

    private synchronized void stop() {
        if (!mRunning) {
            return;
        }

        ContentResolver contentResolver = mApplicationContext.getContentResolver();
        contentResolver.unregisterContentObserver(this);
        onSystemFontScaleChange(contentResolver, /*stopping*/ true);

        mRunning = false;
    }

    private void onSystemFontScaleChange(final ContentResolver contentResolver, boolean stopping) {
        float fontScale;
        int fontInflation;

        if (!stopping) { // Pref was flipped to "On" or system font scale changed.
            fontScale = Settings.System.getFloat(contentResolver, Settings.System.FONT_SCALE, DEFAULT_FONT_SCALE);
            fontInflation = Math.round(FONT_INFLATION_ON_DEFAULT_VALUE * fontScale);
        } else { // Pref was flipped to "Off".
            fontScale = DEFAULT_FONT_SCALE;
            fontInflation = FONT_INFLATION_OFF;
        }

        PrefsHelper.setPref(PREF_FONT_INFLATION, fontInflation);
        PrefsHelper.setPref(PREF_SYSTEM_FONT_SCALE, Math.round(fontScale * 100));
    }

    private void onPrefChange(final SharedPreferences prefs) {
        boolean useSystemFontScale = prefs.getBoolean(GeckoPreferences.PREFS_SYSTEM_FONT_SIZE, false);

        if (useSystemFontScale) {
            start();
        } else {
            stop();
        }
    }

    @Override
    public void onChange(boolean selfChange) {
        onSystemFontScaleChange(mApplicationContext.getContentResolver(), false);
    }

    @UiThread // According to the docs.
    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (!GeckoPreferences.PREFS_SYSTEM_FONT_SIZE.equals(key)) {
            return;
        }

        onPrefChange(sharedPreferences);
    }
}
