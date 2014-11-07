/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.Location;
import android.net.wifi.ScanResult;
import android.support.v4.content.LocalBroadcastManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import java.io.IOException;
import java.util.List;
import java.util.Map;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.stumblerthread.datahandling.DataStorageContract;
import org.mozilla.mozstumbler.service.stumblerthread.datahandling.DataStorageManager;
import org.mozilla.mozstumbler.service.stumblerthread.datahandling.StumblerBundle;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.cellscanner.CellInfo;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.cellscanner.CellScanner;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.GPSScanner;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.WifiScanner;

public final class Reporter extends BroadcastReceiver {
    private static final String LOG_TAG = AppGlobals.makeLogTag(Reporter.class.getSimpleName());
    public static final String ACTION_FLUSH_TO_BUNDLE = AppGlobals.ACTION_NAMESPACE + ".FLUSH";
    private boolean mIsStarted;

    /* The maximum number of Wi-Fi access points in a single observation. */
    private static final int MAX_WIFIS_PER_LOCATION = 200;

    /* The maximum number of cells in a single observation */
    private static final int MAX_CELLS_PER_LOCATION  = 50;

    private Context mContext;
    private int mPhoneType;

    private StumblerBundle mBundle;

    Reporter() {}

    private void resetData() {
        mBundle = null;
    }

    public void flush() {
        reportCollectedLocation();
    }

    void startup(Context context) {
        if (mIsStarted) {
            return;
        }

        mContext = context.getApplicationContext();
        TelephonyManager tm = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        mPhoneType = tm.getPhoneType();

        mIsStarted = true;

        resetData();
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiScanner.ACTION_WIFIS_SCANNED);
        intentFilter.addAction(CellScanner.ACTION_CELLS_SCANNED);
        intentFilter.addAction(GPSScanner.ACTION_GPS_UPDATED);
        intentFilter.addAction(ACTION_FLUSH_TO_BUNDLE);
        LocalBroadcastManager.getInstance(mContext).registerReceiver(this,
                intentFilter);
    }

    void shutdown() {
        if (mContext == null) {
            return;
        }

        mIsStarted = false;

        Log.d(LOG_TAG, "shutdown");
        flush();
        LocalBroadcastManager.getInstance(mContext).unregisterReceiver(this);
    }

    private void receivedWifiMessage(Intent intent) {
        List<ScanResult> results = intent.getParcelableArrayListExtra(WifiScanner.ACTION_WIFIS_SCANNED_ARG_RESULTS);
        putWifiResults(results);
    }

    private void receivedCellMessage(Intent intent) {
        List<CellInfo> results = intent.getParcelableArrayListExtra(CellScanner.ACTION_CELLS_SCANNED_ARG_CELLS);
        putCellResults(results);
    }

    private void receivedGpsMessage(Intent intent) {
        String subject = intent.getStringExtra(Intent.EXTRA_SUBJECT);
        if (subject.equals(GPSScanner.SUBJECT_NEW_LOCATION)) {
            reportCollectedLocation();
            Location newPosition = intent.getParcelableExtra(GPSScanner.NEW_LOCATION_ARG_LOCATION);
            mBundle = (newPosition != null) ? new StumblerBundle(newPosition, mPhoneType) : mBundle;
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        switch (action) {
            case ACTION_FLUSH_TO_BUNDLE:
                flush();
                return;
            case WifiScanner.ACTION_WIFIS_SCANNED:
                receivedWifiMessage(intent);
                break;
            case CellScanner.ACTION_CELLS_SCANNED:
                receivedCellMessage(intent);
                break;
            case GPSScanner.ACTION_GPS_UPDATED:
                // Calls reportCollectedLocation, this is the ideal case
                receivedGpsMessage(intent);
                break;
        }

        if (mBundle != null &&
            (mBundle.getWifiData().size() > MAX_WIFIS_PER_LOCATION ||
             mBundle.getCellData().size() > MAX_CELLS_PER_LOCATION)) {
            // no gps for a while, have too much data, just bundle it
            reportCollectedLocation();
        }
    }

    private void putWifiResults(List<ScanResult> results) {
        if (mBundle == null) {
            return;
        }

        Map<String, ScanResult> currentWifiData = mBundle.getWifiData();
        for (ScanResult result : results) {
            if (currentWifiData.size() > MAX_WIFIS_PER_LOCATION) {
                return;
            }

            String key = result.BSSID;
            if (!currentWifiData.containsKey(key)) {
                currentWifiData.put(key, result);
            }
        }
    }

    private void putCellResults(List<CellInfo> cells) {
        if (mBundle == null) {
            return;
        }

        Map<String, CellInfo> currentCellData = mBundle.getCellData();
        for (CellInfo result : cells) {
            if (currentCellData.size() > MAX_CELLS_PER_LOCATION) {
                return;
            }
            String key = result.getCellIdentity();
            if (!currentCellData.containsKey(key)) {
                currentCellData.put(key, result);
            }
        }
    }

    private void reportCollectedLocation() {
        if (mBundle == null) {
            return;
        }

        storeBundleAsJSON(mBundle);

        mBundle.wasSent();
    }

    private void storeBundleAsJSON(StumblerBundle bundle) {
        JSONObject mlsObj;
        int wifiCount = 0;
        int cellCount = 0;
        try {
            mlsObj = bundle.toMLSJSON();
            wifiCount = mlsObj.getInt(DataStorageContract.ReportsColumns.WIFI_COUNT);
            cellCount = mlsObj.getInt(DataStorageContract.ReportsColumns.CELL_COUNT);

        } catch (JSONException e) {
            Log.w(LOG_TAG, "Failed to convert bundle to JSON: " + e);
            return;
        }

        if (AppGlobals.isDebug) {
            // PII: do not log the bundle without obfuscating it
            Log.d(LOG_TAG, "Received bundle");
        }

        if (wifiCount + cellCount < 1) {
            return;
        }

        try {
            DataStorageManager.getInstance().insert(mlsObj.toString(), wifiCount, cellCount);
        } catch (IOException e) {
            Log.w(LOG_TAG, e.toString());
        }
    }
}

