/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.Location;
import android.os.AsyncTask;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.R;
import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.Prefs;
import org.mozilla.mozstumbler.service.stumblerthread.blocklist.WifiBlockListInterface;
import org.mozilla.mozstumbler.service.stumblerthread.datahandling.DataStorageManager;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.ScanManager;
import org.mozilla.mozstumbler.service.uploadthread.UploadAlarmReceiver;
import org.mozilla.mozstumbler.service.utils.PersistentIntentService;

// In stand-alone service mode (a.k.a passive scanning mode), this is created from PassiveServiceReceiver (by calling startService).
// The StumblerService is a sticky unbound service in this usage.
//
public class StumblerService extends PersistentIntentService
        implements DataStorageManager.StorageIsEmptyTracker {
    private static final String LOG_TAG = AppGlobals.makeLogTag(StumblerService.class.getSimpleName());
    public static final String ACTION_BASE = AppGlobals.ACTION_NAMESPACE;
    public static final String ACTION_START_PASSIVE = ACTION_BASE + ".START_PASSIVE";
    public static final String ACTION_EXTRA_MOZ_API_KEY = ACTION_BASE + ".MOZKEY";
    public static final String ACTION_EXTRA_USER_AGENT = ACTION_BASE + ".USER_AGENT";
    public static final String ACTION_NOT_FROM_HOST_APP = ACTION_BASE + ".NOT_FROM_HOST";
    public static final AtomicBoolean sFirefoxStumblingEnabled = new AtomicBoolean();
    protected final ScanManager mScanManager = new ScanManager();
    protected final Reporter mReporter = new Reporter();

    // This is a delay before the single-shot upload is attempted. The number is arbitrary
    // and used to avoid startup tasks bunching up.
    private static final int DELAY_IN_SEC_BEFORE_STARTING_UPLOAD_IN_PASSIVE_MODE = 2;

    // This is the frequency of the repeating upload alarm in active scanning mode.
    private static final int FREQUENCY_IN_SEC_OF_UPLOAD_IN_ACTIVE_MODE = 5 * 60;

    // Used to guard against attempting to upload too frequently in passive mode.
    private static final long PASSIVE_UPLOAD_FREQ_GUARD_MSEC = 5 * 60 * 1000;

    public StumblerService() {
        this("StumblerService");
    }

    public StumblerService(String name) {
        super(name);
    }

    public boolean isScanning() {
        return mScanManager.isScanning();
    }

    public void startScanning() {
        mScanManager.startScanning(this);
    }

    // This is optional, not used in Fennec, and is for clients to specify a (potentially long) list
    // of blocklisted SSIDs/BSSIDs
    public void setWifiBlockList(WifiBlockListInterface list) {
        mScanManager.setWifiBlockList(list);
    }

    public Prefs getPrefs(Context c) {
        return Prefs.getInstance(c);
    }

    public void checkPrefs() {
        mScanManager.checkPrefs();
    }

    public int getLocationCount() {
        return mScanManager.getLocationCount();
    }

    public double getLatitude() {
        return mScanManager.getLatitude();
    }

    public double getLongitude() {
        return mScanManager.getLongitude();
    }

    public Location getLocation() {
        return mScanManager.getLocation();
    }

    public int getWifiStatus() {
        return mScanManager.getWifiStatus();
    }

    public int getAPCount() {
        return mScanManager.getAPCount();
    }

    public int getVisibleAPCount() {
        return mScanManager.getVisibleAPCount();
    }

    public int getCellInfoCount() {
        return mScanManager.getCellInfoCount();
    }

    public boolean isGeofenced () {
        return mScanManager.isGeofenced();
    }

    // Previously this was done in onCreate(). Moved out of that so that in the passive standalone service
    // use (i.e. Fennec), init() can be called from this class's dedicated thread.
    // Safe to call more than once, ensure added code complies with that intent.
    protected void init() {
        // Ensure Prefs is created, so internal utility code can use getInstanceWithoutContext
        Prefs.getInstance(this);
        DataStorageManager.createGlobalInstance(this, this);

        mReporter.startup(this);
    }

    // Called from the main thread.
    @Override
    public void onCreate() {
        super.onCreate();
        setIntentRedelivery(true);
    }

    @Override
    @SuppressLint("NewApi")
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (!AppConstants.Versions.preO) {
            final Notification notification = new NotificationCompat.Builder(this, GeckoApplication.getDefaultNotificationChannel().getId())
                    .setSmallIcon(R.drawable.ic_status_logo)
                    .setContentTitle(getString(R.string.datareporting_stumbler_notification_title))
                    .setOngoing(true)
                    .setShowWhen(false)
                    .setWhen(0)
                    .build();

            startForeground(R.id.stumblerNotification, notification);
        }
        return START_STICKY;
    }

    // Called from the main thread
    @Override
    public void onDestroy() {
        super.onDestroy();

        if (!mScanManager.isScanning()) {
            return;
        }

        // Used to move these disk I/O ops off the calling thread. The current operations here are synchronized,
        // however instead of creating another thread (if onDestroy grew to have concurrency complications)
        // we could be messaging the stumbler thread to perform a shutdown function.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                if (AppGlobals.isDebug) {
                    Log.d(LOG_TAG, "onDestroy");
                }

                if (!sFirefoxStumblingEnabled.get()) {
                    Prefs.getInstance(StumblerService.this).setFirefoxScanEnabled(false);
                }

                if (DataStorageManager.getInstance() != null) {
                    try {
                        DataStorageManager.getInstance().saveCurrentReportsToDisk();
                    } catch (IOException ex) {
                        AppGlobals.guiLogInfo(ex.toString());
                        Log.e(LOG_TAG, "Exception in onDestroy saving reports" + ex.toString());
                    }
                }
                return null;
            }
        }.execute();

        mReporter.shutdown();
        mScanManager.stopScanning();
    }

    // This is the entry point for the stumbler thread.
    @Override
    protected void onHandleIntent(Intent intent) {
        // Do init() in all cases, there is no cost, whereas it is easy to add code that depends on this.
        init();

        // Post-init(), set the mode to passive.
        mScanManager.setPassiveMode(true);

        if (intent == null) {
            return;
        }

        final boolean isScanEnabledInPrefs = Prefs.getInstance(this).getFirefoxScanEnabled();

        if (!isScanEnabledInPrefs && intent.getBooleanExtra(ACTION_NOT_FROM_HOST_APP, false)) {
            stopSelf();
            return;
        }

        if (!hasLocationPermission()) {
            Log.d(LOG_TAG, "Location permission not granted. Aborting.");
            return;
        }

        boolean hasFilesWaiting = !DataStorageManager.getInstance().isDirEmpty();
        if (AppGlobals.isDebug) {
            Log.d(LOG_TAG, "Files waiting:" + hasFilesWaiting);
        }
        if (hasFilesWaiting) {
            // non-empty on startup, schedule an upload
            // This is the only upload trigger in Firefox mode
            // Firefox triggers this ~4 seconds after startup (after Gecko is loaded), add a small delay to avoid
            // clustering with other operations that are triggered at this time.
            final long lastAttemptedTime = Prefs.getInstance(this).getLastAttemptedUploadTime();
            final long timeNow = System.currentTimeMillis();

            if (timeNow - lastAttemptedTime < PASSIVE_UPLOAD_FREQ_GUARD_MSEC) {
                // TODO Consider telemetry to track this.
                if (AppGlobals.isDebug) {
                    Log.d(LOG_TAG, "Upload attempt too frequent.");
                }
            } else {
                Prefs.getInstance(this).setLastAttemptedUploadTime(timeNow);
                UploadAlarmReceiver.scheduleAlarm(this, DELAY_IN_SEC_BEFORE_STARTING_UPLOAD_IN_PASSIVE_MODE, false /* no repeat*/);
            }
        }

        if (!isScanEnabledInPrefs) {
            Prefs.getInstance(this).setFirefoxScanEnabled(true);
        }

        String apiKey = intent.getStringExtra(ACTION_EXTRA_MOZ_API_KEY);
        if (apiKey != null && !apiKey.equals(Prefs.getInstance(this).getMozApiKey())) {
            Prefs.getInstance(this).setMozApiKey(apiKey);
        }

        String userAgent = intent.getStringExtra(ACTION_EXTRA_USER_AGENT);
        if (userAgent != null && !userAgent.equals(Prefs.getInstance(this).getUserAgent())) {
            Prefs.getInstance(this).setUserAgent(userAgent);
        }

        if (!mScanManager.isScanning()) {
            startScanning();
        }
    }

    // Note that in passive mode, having data isn't an upload trigger, it is triggered by the start intent
    @Override
    public void notifyStorageStateEmpty(boolean isEmpty) {
        if (isEmpty) {
            UploadAlarmReceiver.cancelAlarm(this, !mScanManager.isPassiveMode());
        } else if (!mScanManager.isPassiveMode()) {
            UploadAlarmReceiver.scheduleAlarm(this, FREQUENCY_IN_SEC_OF_UPLOAD_IN_ACTIVE_MODE, true /* repeating */);
        }
    }

    private boolean hasLocationPermission() {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }
}
