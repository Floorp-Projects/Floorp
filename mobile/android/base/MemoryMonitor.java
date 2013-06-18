/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.util.Log;

/**
  * This is a utility class to keep track of how much memory and disk-space pressure
  * the system is under. It receives input from GeckoActivity via the onLowMemory() and
  * onTrimMemory() functions, and also listens for some system intents related to
  * disk-space notifications. Internally it will track how much memory and disk pressure
  * the system is under, and perform various actions to help alleviate the pressure.
  *
  * Note that since there is no notification for when the system has lots of free memory
  * again, this class also assumes that, over time, the system will free up memory. This
  * assumption is implemented using a timer that slowly lowers the internal memory
  * pressure state if no new low-memory notifications are received.
  *
  * Synchronization note: MemoryMonitor contains an inner class PressureDecrementer. Both
  * of these classes may be accessed from various threads, and have both been designed to
  * be thread-safe. In terms of lock ordering, code holding the PressureDecrementer lock
  * is allowed to pick up the MemoryMonitor lock, but not vice-versa.
  */
class MemoryMonitor extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoMemoryMonitor";
    private static final String ACTION_MEMORY_DUMP = "org.mozilla.gecko.MEMORY_DUMP";
    private static final String ACTION_FORCE_PRESSURE = "org.mozilla.gecko.FORCE_MEMORY_PRESSURE";

    // Memory pressue levels, keep in sync with those in AndroidJavaWrappers.h
    private static final int MEMORY_PRESSURE_NONE = 0;
    private static final int MEMORY_PRESSURE_CLEANUP = 1;
    private static final int MEMORY_PRESSURE_LOW = 2;
    private static final int MEMORY_PRESSURE_MEDIUM = 3;
    private static final int MEMORY_PRESSURE_HIGH = 4;

    private static MemoryMonitor sInstance = new MemoryMonitor();

    static MemoryMonitor getInstance() {
        return sInstance;
    }

    private final PressureDecrementer mPressureDecrementer;
    private int mMemoryPressure;
    private boolean mStoragePressure;

    private MemoryMonitor() {
        mPressureDecrementer = new PressureDecrementer();
        mMemoryPressure = MEMORY_PRESSURE_NONE;
        mStoragePressure = false;
    }

    public void init(Context context) {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_LOW);
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_OK);
        filter.addAction(ACTION_MEMORY_DUMP);
        filter.addAction(ACTION_FORCE_PRESSURE);
        context.getApplicationContext().registerReceiver(this, filter);
    }

    public void onLowMemory() {
        Log.d(LOGTAG, "onLowMemory() notification received");
        if (increaseMemoryPressure(MEMORY_PRESSURE_HIGH)) {
            // We need to wait on Gecko here, because if we haven't reduced
            // memory usage enough when we return from this, Android will kill us.
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createNoOpEvent());
        }
    }

    public void onTrimMemory(int level) {
        Log.d(LOGTAG, "onTrimMemory() notification received with level " + level);
        if (Build.VERSION.SDK_INT < 14) {
            // this won't even get called pre-ICS
            return;
        }

        if (level >= ComponentCallbacks2.TRIM_MEMORY_COMPLETE) {
            increaseMemoryPressure(MEMORY_PRESSURE_HIGH);
        } else if (level >= ComponentCallbacks2.TRIM_MEMORY_MODERATE) {
            increaseMemoryPressure(MEMORY_PRESSURE_MEDIUM);
        } else if (level >= ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN) {
            // includes TRIM_MEMORY_BACKGROUND
            increaseMemoryPressure(MEMORY_PRESSURE_CLEANUP);
        } else {
            // levels down here mean gecko is the foreground process so we
            // should be less aggressive with wiping memory as it may impact
            // user experience.
            increaseMemoryPressure(MEMORY_PRESSURE_LOW);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_DEVICE_STORAGE_LOW.equals(intent.getAction())) {
            Log.d(LOGTAG, "Device storage is low");
            mStoragePressure = true;
            ThreadUtils.postToBackgroundThread(new StorageReducer(context));
        } else if (Intent.ACTION_DEVICE_STORAGE_OK.equals(intent.getAction())) {
            Log.d(LOGTAG, "Device storage is ok");
            mStoragePressure = false;
        } else if (ACTION_MEMORY_DUMP.equals(intent.getAction())) {
            String label = intent.getStringExtra("label");
            if (label == null) {
                label = "default";
            }
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Memory:Dump", label));
        } else if (ACTION_FORCE_PRESSURE.equals(intent.getAction())) {
            increaseMemoryPressure(MEMORY_PRESSURE_HIGH);
        }
    }

    private boolean increaseMemoryPressure(int level) {
        int oldLevel;
        synchronized (this) {
            // bump up our level if we're not already higher
            if (mMemoryPressure > level) {
                return false;
            }
            oldLevel = mMemoryPressure;
            mMemoryPressure = level;
        }

        // since we don't get notifications for when memory pressure is off,
        // we schedule our own timer to slowly back off the memory pressure level.
        // note that this will reset the time to next decrement if the decrementer
        // is already running, which is the desired behaviour because we just got
        // a new low-mem notification.
        mPressureDecrementer.start();

        if (oldLevel == level) {
            // if we're not going to a higher level we probably don't
            // need to run another round of the same memory reductions
            // we did on the last memory pressure increase.
            return false;
        }

        // TODO hook in memory-reduction stuff for different levels here
        if (level >= MEMORY_PRESSURE_MEDIUM) {
            //Only send medium or higher events because that's all that is used right now
            if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
                GeckoAppShell.sendEventToGecko(GeckoEvent.createLowMemoryEvent(level));
            }

            Favicons.getInstance().clearMemCache();
        }
        return true;
    }

    private boolean decreaseMemoryPressure() {
        int newLevel;
        synchronized (this) {
            if (mMemoryPressure <= 0) {
                return false;
            }

            newLevel = --mMemoryPressure;
        }
        Log.d(LOGTAG, "Decreased memory pressure to " + newLevel);

        return true;
    }

    class PressureDecrementer implements Runnable {
        private static final int DECREMENT_DELAY = 5 * 60 * 1000; // 5 minutes

        private boolean mPosted;

        synchronized void start() {
            if (mPosted) {
                // cancel the old one before scheduling a new one
                ThreadUtils.getBackgroundHandler().removeCallbacks(this);
            }
            ThreadUtils.getBackgroundHandler().postDelayed(this, DECREMENT_DELAY);
            mPosted = true;
        }

        @Override
        public synchronized void run() {
            if (!decreaseMemoryPressure()) {
                // done decrementing, bail out
                mPosted = false;
                return;
            }

            // need to keep decrementing
            ThreadUtils.getBackgroundHandler().postDelayed(this, DECREMENT_DELAY);
        }
    }

    class StorageReducer implements Runnable {
        private final Context mContext;
        public StorageReducer(final Context context) {
            this.mContext = context;
        }

        @Override
        public void run() {
            // this might get run right on startup, if so wait 10 seconds and try again
            if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
                ThreadUtils.getBackgroundHandler().postDelayed(this, 10000);
                return;
            }

            if (!mStoragePressure) {
                // pressure is off, so we can abort
                return;
            }

            BrowserDB.expireHistory(mContext.getContentResolver(),
                                    BrowserContract.ExpirePriority.AGGRESSIVE);
            BrowserDB.removeThumbnails(mContext.getContentResolver());
            // TODO: drop or shrink disk caches
        }
    }
}
