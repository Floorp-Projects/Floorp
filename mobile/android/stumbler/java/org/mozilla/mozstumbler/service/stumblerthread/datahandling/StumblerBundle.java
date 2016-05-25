/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.datahandling;

import android.location.Location;
import android.net.wifi.ScanResult;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.telephony.TelephonyManager;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.mozstumbler.service.stumblerthread.scanners.cellscanner.CellInfo;

public final class StumblerBundle implements Parcelable {
    private final int mPhoneType;
    private final Location mGpsPosition;
    private final Map<String, ScanResult> mWifiData;
    private final Map<String, CellInfo> mCellData;
    private float mPressureHPA;


    public void wasSent() {
        mGpsPosition.setTime(System.currentTimeMillis());
        mWifiData.clear();
        mCellData.clear();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        Bundle wifiBundle = new Bundle(ScanResult.class.getClassLoader());
        Collection<String> scans = mWifiData.keySet();
        for (String s : scans) {
            wifiBundle.putParcelable(s, mWifiData.get(s));
        }

        Bundle cellBundle = new Bundle(CellInfo.class.getClassLoader());
        Collection<String> cells = mCellData.keySet();
        for (String c : cells) {
            cellBundle.putParcelable(c, mCellData.get(c));
        }

        out.writeBundle(wifiBundle);
        out.writeBundle(cellBundle);
        out.writeParcelable(mGpsPosition, 0);
        out.writeInt(mPhoneType);
    }

    public static final Parcelable.Creator<StumblerBundle> CREATOR
        = new Parcelable.Creator<StumblerBundle>() {
        @Override
        public StumblerBundle createFromParcel(Parcel in) {
            return new StumblerBundle(in);
        }

        @Override
        public StumblerBundle[] newArray(int size) {
            return new StumblerBundle[size];
        }
    };

    private StumblerBundle(Parcel in) {
        mWifiData = new HashMap<String, ScanResult>();
        mCellData = new HashMap<String, CellInfo>();

        Bundle wifiBundle = in.readBundle(ScanResult.class.getClassLoader());
        Bundle cellBundle = in.readBundle(CellInfo.class.getClassLoader());

        Collection<String> scans = wifiBundle.keySet();
        for (String s : scans) {
            mWifiData.put(s, (ScanResult) wifiBundle.get(s));
        }

        Collection<String> cells = cellBundle.keySet();
        for (String c : cells) {
            mCellData.put(c, (CellInfo) cellBundle.get(c));
        }

        mGpsPosition = in.readParcelable(Location.class.getClassLoader());
        mPhoneType = in.readInt();
    }

    public StumblerBundle(Location position, int phoneType) {
        mGpsPosition = position;
        mPhoneType = phoneType;
        mWifiData = new HashMap<String, ScanResult>();
        mCellData = new HashMap<String, CellInfo>();
    }

    public Location getGpsPosition() {
        return mGpsPosition;
    }

    public Map<String, ScanResult> getWifiData() {
        return mWifiData;
    }

    public Map<String, CellInfo> getCellData() {
        return mCellData;
    }

    public JSONObject toMLSJSON() throws JSONException {
        JSONObject item = new JSONObject();

        item.put(DataStorageContract.ReportsColumns.TIME, mGpsPosition.getTime());
        item.put(DataStorageContract.ReportsColumns.LAT, Math.floor(mGpsPosition.getLatitude() * 1.0E6) / 1.0E6);
        item.put(DataStorageContract.ReportsColumns.LON, Math.floor(mGpsPosition.getLongitude() * 1.0E6) / 1.0E6);

        item.put(DataStorageContract.ReportsColumns.HEADING, mGpsPosition.getBearing());
        item.put(DataStorageContract.ReportsColumns.SPEED,  mGpsPosition.getSpeed());
        if (mPressureHPA != 0.0) {
            item.put(DataStorageContract.ReportsColumns.PRESSURE,  mPressureHPA);
        }


        if (mGpsPosition.hasAccuracy()) {
            item.put(DataStorageContract.ReportsColumns.ACCURACY, (int) Math.ceil(mGpsPosition.getAccuracy()));
        }

        if (mGpsPosition.hasAltitude()) {
            item.put(DataStorageContract.ReportsColumns.ALTITUDE, Math.round(mGpsPosition.getAltitude()));
        }

        if (mPhoneType == TelephonyManager.PHONE_TYPE_GSM) {
            item.put(DataStorageContract.ReportsColumns.RADIO, "gsm");
        } else if (mPhoneType == TelephonyManager.PHONE_TYPE_CDMA) {
            item.put(DataStorageContract.ReportsColumns.RADIO, "cdma");
        } else {
            // issue #598. investigate this case further in future
            item.put(DataStorageContract.ReportsColumns.RADIO, "");
        }

        JSONArray cellJSON = new JSONArray();
        for (CellInfo c : mCellData.values()) {
            JSONObject obj = c.toJSONObject();
            cellJSON.put(obj);
        }

        item.put(DataStorageContract.ReportsColumns.CELL, cellJSON);
        item.put(DataStorageContract.ReportsColumns.CELL_COUNT, cellJSON.length());

        JSONArray wifis = new JSONArray();

        long gpsTimeSinceBootInMS = 0;

        if (Build.VERSION.SDK_INT >= 17) {
            gpsTimeSinceBootInMS = mGpsPosition.getElapsedRealtimeNanos() / 1000000;
        }

        for (ScanResult s : mWifiData.values()) {
            JSONObject wifiEntry = new JSONObject();
            wifiEntry.put("key", s.BSSID);
            wifiEntry.put("frequency", s.frequency);
            wifiEntry.put("signal", s.level);

            if (Build.VERSION.SDK_INT >= 17) {
                long wifiTimeSinceBootInMS = (s.timestamp / 1000);
                long ageMS =  wifiTimeSinceBootInMS - gpsTimeSinceBootInMS;
                wifiEntry.put("age", ageMS);
            }

            wifis.put(wifiEntry);
        }
        item.put(DataStorageContract.ReportsColumns.WIFI, wifis);
        item.put(DataStorageContract.ReportsColumns.WIFI_COUNT, wifis.length());

        return item;
    }


    public void addPressure(float hPa) {
        mPressureHPA = hPa;
    }

}
