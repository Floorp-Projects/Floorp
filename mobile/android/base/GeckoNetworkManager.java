/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.JNITarget;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.telephony.TelephonyManager;
import android.util.Log;

/*
 * A part of the work of GeckoNetworkManager is to give an general connection
 * type based on the current connection. According to spec of NetworkInformation
 * API version 3, connection types include: bluetooth, cellular, ethernet, none,
 * wifi and other. The objective of providing such general connection is due to
 * some securtiy concerns. In short, we don't want to expose the information of
 * exact network type, especially the cellular network type.
 *
 * Currnet connection is firstly obtained from Android's ConnectivityManager,
 * which is represented by the constant, and then will be mapped into the
 * connection type defined in Network Information API version 3.
 */

public class GeckoNetworkManager extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoNetworkManager";

    static private final GeckoNetworkManager sInstance = new GeckoNetworkManager();

    // Connection Type defined in Network Information API v3.
    private enum ConnectionType {
        CELLULAR(0),
        BLUETOOTH(1),
        ETHERNET(2),
        WIFI(3),
        OTHER(4),
        NONE(5);

        public final int value;

        private ConnectionType(int value) {
            this.value = value;
        }
    }

    private enum InfoType {
        MCC,
        MNC
    }

    static private final ConnectionType kDefaultConnectionType = ConnectionType.NONE;

    private static Context getApplicationContext() {
        Context context = GeckoAppShell.getContext();
        if (null == context)
            return null;
        return context.getApplicationContext();
    }
    private ConnectionType mConnectionType = ConnectionType.NONE;
    private final IntentFilter mNetworkFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);

    // Whether the manager should be listening to Network Information changes.
    private boolean mShouldBeListening = false;

    // Whether the manager should notify Gecko that a change in Network
    // Information happened.
    private boolean mShouldNotify = false;

    public static GeckoNetworkManager getInstance() {
        return sInstance;
    }

    @Override
    public void onReceive(Context aContext, Intent aIntent) {
        updateConnectionType();
    }

    public void start(final Context context) {
        // Note that this initialization clause only runs once.
        if (mConnectionType == ConnectionType.NONE) {
            mConnectionType = getConnectionType();
        }

        mShouldBeListening = true;
        updateConnectionType();

        if (mShouldNotify) {
            startListening();
        }
    }

    private void startListening() {
        if (null !=getApplicationContext())
            getApplicationContext().registerReceiver(sInstance, mNetworkFilter);
    }

    public void stop() {
        mShouldBeListening = false;

        if (mShouldNotify) {
            stopListening();
        }
    }

    private void stopListening() {
        if (null != getApplicationContext())
            getApplicationContext().unregisterReceiver(sInstance);
    }

    private int wifiDhcpGatewayAddress() {
        if (mConnectionType != ConnectionType.WIFI) {
            return 0;
        }

        if (null == getApplicationContext()) {
            return 0;
        }

        try {
            WifiManager mgr = (WifiManager)getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            DhcpInfo d = mgr.getDhcpInfo();
            if (d == null) {
                return 0;
            }

            return d.gateway;

        } catch (Exception ex) {
            // getDhcpInfo() is not documented to require any permissions, but on some devices
            // requires android.permission.ACCESS_WIFI_STATE. Just catching the generic exeption
            // here and returning 0. Not logging because this could be noisy
            return 0;
        }
    }

    private void updateConnectionType() {
        ConnectionType previousConnectionType = mConnectionType;
        mConnectionType = getConnectionType();

        if (mConnectionType == previousConnectionType || !mShouldNotify) {
            return;
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createNetworkEvent(
                                       mConnectionType.value,
                                       mConnectionType == ConnectionType.WIFI,
                                       wifiDhcpGatewayAddress()));
    }

    public double[] getCurrentInformation() {
        return new double[] { mConnectionType.value,
                              (mConnectionType == ConnectionType.WIFI) ? 1.0 : 0.0,
                              wifiDhcpGatewayAddress()};
    }

    public void enableNotifications() {
        // We set mShouldNotify *after* calling updateConnectionType() to make sure we
        // don't notify an eventual change in mConnectionType.
        mConnectionType = ConnectionType.NONE; // force a notification
        updateConnectionType();
        mShouldNotify = true;

        if (mShouldBeListening) {
            startListening();
        }
    }

    public void disableNotifications() {
        mShouldNotify = false;

        if (mShouldBeListening) {
            stopListening();
        }
    }

    private static ConnectionType getConnectionType() {
        if (null == getApplicationContext()) {
            return ConnectionType.NONE;
        }

        ConnectivityManager cm =
            (ConnectivityManager)getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) {
            Log.e(LOGTAG, "Connectivity service does not exist");
            return ConnectionType.NONE;
        }

        NetworkInfo ni = null;
        try {
            ni = cm.getActiveNetworkInfo();
        } catch (SecurityException se) {} // if we don't have the permission, fall through to null check

        if (ni == null) {
            return ConnectionType.NONE;
        }

        switch (ni.getType()) {
        case ConnectivityManager.TYPE_BLUETOOTH:
            return ConnectionType.BLUETOOTH;
        case ConnectivityManager.TYPE_ETHERNET:
            return ConnectionType.ETHERNET;
        case ConnectivityManager.TYPE_MOBILE:
        case ConnectivityManager.TYPE_WIMAX:
            return ConnectionType.CELLULAR;
        case ConnectivityManager.TYPE_WIFI:
            return ConnectionType.WIFI;
        default:
            Log.w(LOGTAG, "Ignoring the current network type.");
            return ConnectionType.OTHER;
        }
    }

    private static int getNetworkOperator(InfoType type) {
        if (null == getApplicationContext())
            return -1;

        TelephonyManager tel = (TelephonyManager)getApplicationContext().getSystemService(Context.TELEPHONY_SERVICE);
        if (tel == null) {
            Log.e(LOGTAG, "Telephony service does not exist");
            return -1;
        }

        String networkOperator = tel.getNetworkOperator();
        if (networkOperator == null || networkOperator.length() <= 3) {
            return -1;
        }
        if (type == InfoType.MNC) {
            return Integer.parseInt(networkOperator.substring(3));
        } else if (type == InfoType.MCC) {
            return Integer.parseInt(networkOperator.substring(0, 3));
        }

        return -1;
    }

    /* These are called from javascript c-types. Avoid letting pro-guard delete them */
    @JNITarget
    public static int getMCC() {
        return getNetworkOperator(InfoType.MCC);
    }

    @JNITarget
    public static int getMNC() {
        return getNetworkOperator(InfoType.MNC);
    }
}
