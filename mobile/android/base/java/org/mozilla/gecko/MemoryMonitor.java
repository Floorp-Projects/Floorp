/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;
import org.mozilla.gecko.home.ImageLoader;
import org.mozilla.gecko.icons.storage.MemoryStorage;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks2;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;
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

    // Memory pressure levels. Keep these in sync with those in AndroidJavaWrappers.h
    private static final int MEMORY_PRESSURE_NONE = 0;
    private static final int MEMORY_PRESSURE_CLEANUP = 1;
    private static final int MEMORY_PRESSURE_LOW = 2;
    private static final int MEMORY_PRESSURE_MEDIUM = 3;
    private static final int MEMORY_PRESSURE_HIGH = 4;

    private static final MemoryMonitor sInstance = new MemoryMonitor();

    static MemoryMonitor getInstance() {
        return sInstance;
    }

    private Context mAppContext;
    private final PressureDecrementer mPressureDecrementer;
    private int mMemoryPressure;                  // Synchronized access only.
    private volatile boolean mStoragePressure;    // Accessed via UI thread intent, background runnables.
    private boolean mInited;

    private MemoryMonitor() {
        mPressureDecrementer = new PressureDecrementer();
        mMemoryPressure = MEMORY_PRESSURE_NONE;
    }

    public void init(final Context context) {
        if (mInited) {
            return;
        }

        mAppContext = context.getApplicationContext();
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_LOW);
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_OK);
        filter.addAction(ACTION_MEMORY_DUMP);
        filter.addAction(ACTION_FORCE_PRESSURE);
        mAppContext.registerReceiver(this, filter);
        mInited = true;
    }

    public void onLowMemory() {
        Log.d(LOGTAG, "onLowMemory() notification received");
        if (increaseMemoryPressure(MEMORY_PRESSURE_HIGH)) {
            // We need to wait on Gecko here, because if we haven't reduced
            // memory usage enough when we return from this, Android will kill us.
            if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
                GeckoThread.waitOnGecko();
            }
        }
    }

    public void onTrimMemory(int level) {
        Log.d(LOGTAG, "onTrimMemory() notification received with level " + level);
        if (level == ComponentCallbacks2.TRIM_MEMORY_COMPLETE) {
            // We seem to get this just by entering the task switcher or hitting the home button.
            // Seems bogus, because we are the foreground app, or at least not at the end of the LRU list.
            // Just ignore it, and if there is a real memory pressure event (CRITICAL, MODERATE, etc),
            // we'll respond appropriately.
            return;
        }

        switch (level) {
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL:
            case ComponentCallbacks2.TRIM_MEMORY_MODERATE:
                // TRIM_MEMORY_MODERATE is the highest level we'll respond to while backgrounded
                increaseMemoryPressure(MEMORY_PRESSURE_HIGH);
                break;
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE:
                increaseMemoryPressure(MEMORY_PRESSURE_MEDIUM);
                break;
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW:
                increaseMemoryPressure(MEMORY_PRESSURE_LOW);
                break;
            case ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN:
            case ComponentCallbacks2.TRIM_MEMORY_BACKGROUND:
                increaseMemoryPressure(MEMORY_PRESSURE_CLEANUP);
                break;
            default:
                Log.d(LOGTAG, "Unhandled onTrimMemory() level " + level);
                break;
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
            GeckoAppShell.notifyObservers("Memory:Dump", label);
        } else if (ACTION_FORCE_PRESSURE.equals(intent.getAction())) {
            increaseMemoryPressure(MEMORY_PRESSURE_HIGH);
        }
    }

    @WrapForJNI(calledFrom = "ui")
    private static native void dispatchMemoryPressure();

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

        Log.d(LOGTAG, "increasing memory pressure to " + level);

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
            if (GeckoThread.isRunning()) {
                dispatchMemoryPressure();
            }

            MemoryStorage.get().evictAll();
            ImageLoader.clearLruCache();
            LocalBroadcastManager.getInstance(mAppContext)
                    .sendBroadcast(new Intent(BrowserProvider.ACTION_SHRINK_MEMORY));
        }
        return true;
    }

    /**
     * Thread-safe due to mStoragePressure's volatility.
     */
    boolean isUnderStoragePressure() {
        return mStoragePressure;
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

    private static class StorageReducer implements Runnable {
        private final Context mContext;
        private final BrowserDB mDB;

        public StorageReducer(final Context context) {
            this.mContext = context;
            // Since this may be called while Fennec is in the background, we don't want to risk accidentally
            // using the wrong context. If the profile we get is a guest profile, use the default profile instead.
            GeckoProfile profile = GeckoProfile.get(mContext);
            if (profile.inGuestMode()) {
                // If it was the guest profile, switch to the default one.
                profile = GeckoProfile.get(mContext, GeckoProfile.DEFAULT_PROFILE);
            }

            mDB = profile.getDB();
        }

        @Override
        public void run() {
            // this might get run right on startup, if so wait 10 seconds and try again
            if (!GeckoThread.isRunning()) {
                ThreadUtils.getBackgroundHandler().postDelayed(this, 10000);
                return;
            }

            if (!MemoryMonitor.getInstance().isUnderStoragePressure()) {
                // Pressure is off, so we can abort.
                return;
            }

            final ContentResolver cr = mContext.getContentResolver();
            mDB.expireHistory(cr, BrowserContract.ExpirePriority.AGGRESSIVE);
            mDB.removeThumbnails(cr);

            // TODO: drop or shrink disk caches
        }
    }
}
