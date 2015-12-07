/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;

public class GeckoBatteryManager extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoBatteryManager";

    // Those constants should be keep in sync with the ones in:
    // dom/battery/Constants.h
    private final static double  kDefaultLevel         = 1.0;
    private final static boolean kDefaultCharging      = true;
    private final static double  kDefaultRemainingTime = 0.0;
    private final static double  kUnknownRemainingTime = -1.0;

    private static long    sLastLevelChange;
    private static boolean sNotificationsEnabled;
    private static double  sLevel                      = kDefaultLevel;
    private static boolean sCharging                   = kDefaultCharging;
    private static double  sRemainingTime              = kDefaultRemainingTime;

    private static final GeckoBatteryManager sInstance = new GeckoBatteryManager();

    private final IntentFilter mFilter;
    private Context mApplicationContext;
    private boolean mIsEnabled;

    public static GeckoBatteryManager getInstance() {
        return sInstance;
    }

    private GeckoBatteryManager() {
        mFilter = new IntentFilter();
        mFilter.addAction(Intent.ACTION_BATTERY_CHANGED);
    }

    public synchronized void start(final Context context) {
        if (mIsEnabled) {
            Log.w(LOGTAG, "Already started!");
            return;
        }

        mApplicationContext = context.getApplicationContext();
        // registerReceiver will return null if registering fails.
        if (mApplicationContext.registerReceiver(this, mFilter) == null) {
            Log.e(LOGTAG, "Registering receiver failed");
        } else {
            mIsEnabled = true;
        }
    }

    public synchronized void stop() {
        if (!mIsEnabled) {
            Log.w(LOGTAG, "Already stopped!");
            return;
        }

        mApplicationContext.unregisterReceiver(this);
        mApplicationContext = null;
        mIsEnabled = false;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!intent.getAction().equals(Intent.ACTION_BATTERY_CHANGED)) {
            Log.e(LOGTAG, "Got an unexpected intent!");
            return;
        }

        boolean previousCharging = isCharging();
        double previousLevel = getLevel();

        // NOTE: it might not be common (in 2012) but technically, Android can run
        // on a device that has no battery so we want to make sure it's not the case
        // before bothering checking for battery state.
        // However, the Galaxy Nexus phone advertises itself as battery-less which
        // force us to special-case the logic.
        // See the Google bug: https://code.google.com/p/android/issues/detail?id=22035
        if (intent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false) ||
                Build.MODEL.equals("Galaxy Nexus")) {
            int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
            if (plugged == -1) {
                sCharging = kDefaultCharging;
                Log.e(LOGTAG, "Failed to get the plugged status!");
            } else {
                // Likely, if plugged > 0, it's likely plugged and charging but the doc
                // isn't clear about that.
                sCharging = plugged != 0;
            }

            if (sCharging != previousCharging) {
                sRemainingTime = kUnknownRemainingTime;
                // The new remaining time is going to take some time to show up but
                // it's the best way to show a not too wrong value.
                sLastLevelChange = 0;
            }

            // We need two doubles because sLevel is a double.
            double current = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            double max = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
            if (current == -1 || max == -1) {
                Log.e(LOGTAG, "Failed to get battery level!");
                sLevel = kDefaultLevel;
            } else {
                sLevel = current / max;
            }

            if (sLevel == 1.0 && sCharging) {
                sRemainingTime = kDefaultRemainingTime;
            } else if (sLevel != previousLevel) {
                // Estimate remaining time.
                if (sLastLevelChange != 0) {
                    // Use elapsedRealtime() because we want to track time across device sleeps.
                    long currentTime = SystemClock.elapsedRealtime();
                    long dt = (currentTime - sLastLevelChange) / 1000;
                    double dLevel = sLevel - previousLevel;

                    if (sCharging) {
                        if (dLevel < 0) {
                            Log.w(LOGTAG, "When charging, level should increase!");
                            sRemainingTime = kUnknownRemainingTime;
                        } else {
                            sRemainingTime = Math.round(dt / dLevel * (1.0 - sLevel));
                        }
                    } else {
                        if (dLevel > 0) {
                            Log.w(LOGTAG, "When discharging, level should decrease!");
                            sRemainingTime = kUnknownRemainingTime;
                        } else {
                            sRemainingTime = Math.round(dt / -dLevel * sLevel);
                        }
                    }

                    sLastLevelChange = currentTime;
                } else {
                    // That's the first time we got an update, we can't do anything.
                    sLastLevelChange = SystemClock.elapsedRealtime();
                }
            }
        } else {
            sLevel = kDefaultLevel;
            sCharging = kDefaultCharging;
            sRemainingTime = kDefaultRemainingTime;
        }

        /*
         * We want to inform listeners if the following conditions are fulfilled:
         *  - we have at least one observer;
         *  - the charging state or the level has changed.
         *
         * Note: no need to check for a remaining time change given that it's only
         * updated if there is a level change or a charging change.
         *
         * The idea is to prevent doing all the way to the DOM code in the child
         * process to finally not send an event.
         */
        if (sNotificationsEnabled &&
                (previousCharging != isCharging() || previousLevel != getLevel())) {
            GeckoAppShell.notifyBatteryChange(getLevel(), isCharging(), getRemainingTime());
        }
    }

    public static boolean isCharging() {
        return sCharging;
    }

    public static double getLevel() {
        return sLevel;
    }

    public static double getRemainingTime() {
        return sRemainingTime;
    }

    public static void enableNotifications() {
        sNotificationsEnabled = true;
    }

    public static void disableNotifications() {
        sNotificationsEnabled = false;
    }

    public static double[] getCurrentInformation() {
        return new double[] { getLevel(), isCharging() ? 1.0 : 0.0, getRemainingTime() };
    }
}
