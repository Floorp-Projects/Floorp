/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.EventCallback;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.DhcpInfo;
import android.net.NetworkInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.telephony.TelephonyManager;
import android.text.format.Formatter;
import android.util.Log;

/*
 * A part of the work of GeckoNetworkManager is to give an general connection
 * type based on the current connection. According to spec of NetworkInformation
 * API version 3, connection types include: bluetooth, cellular, ethernet, none,
 * wifi and other. The objective of providing such general connection is due to
 * some security concerns. In short, we don't want to expose the information of
 * exact network type, especially the cellular network type.
 *
 * Current connection is firstly obtained from Android's ConnectivityManager,
 * which is represented by the constant, and then will be mapped into the
 * connection type defined in Network Information API version 3.
 */

public class GeckoNetworkManager extends BroadcastReceiver implements NativeEventListener {
    private static final String LOGTAG = "GeckoNetworkManager";

    private static GeckoNetworkManager sInstance;

    public static void destroy() {
        if (sInstance != null) {
            sInstance.onDestroy();
            sInstance = null;
        }
    }

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

    private GeckoNetworkManager() {
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "Wifi:Enable",
                "Wifi:GetIPAddress");
    }

    private void onDestroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                "Wifi:Enable",
                "Wifi:GetIPAddress");
    }

    private volatile ConnectionType mConnectionType = ConnectionType.NONE;
    private final IntentFilter mNetworkFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);

    // Whether the manager should be listening to Network Information changes.
    private boolean mShouldBeListening;

    // Whether the manager should notify Gecko that a change in Network
    // Information happened.
    private boolean mShouldNotify;

    // The application context used for registering receivers, so
    // we can unregister them again later.
    private volatile Context mApplicationContext;
    private boolean mIsListening;

    public static GeckoNetworkManager getInstance() {
        if (sInstance == null) {
            sInstance = new GeckoNetworkManager();
        }

        return sInstance;
    }

    @Override
    public void onReceive(Context aContext, Intent aIntent) {
        updateConnectionType();
    }

    public void start(final Context context) {
        // Note that this initialization clause only runs once.
        mApplicationContext = context.getApplicationContext();
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
        if (mIsListening) {
            Log.w(LOGTAG, "Already started!");
            return;
        }

        final Context appContext = mApplicationContext;
        if (appContext == null) {
            Log.w(LOGTAG, "Not registering receiver: no context!");
            return;
        }

        // registerReceiver will return null if registering fails.
        if (appContext.registerReceiver(this, mNetworkFilter) == null) {
            Log.e(LOGTAG, "Registering receiver failed");
        } else {
            mIsListening = true;
        }
    }

    public void stop() {
        mShouldBeListening = false;

        if (mShouldNotify) {
            stopListening();
        }
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        switch (event) {
            case "Wifi:Enable": {
                final WifiManager mgr = (WifiManager) mApplicationContext.getSystemService(Context.WIFI_SERVICE);

                if (!mgr.isWifiEnabled()) {
                    mgr.setWifiEnabled(true);
                } else {
                    // If Wifi is enabled, maybe you need to select a network
                    Intent intent = new Intent(android.provider.Settings.ACTION_WIFI_SETTINGS);
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    mApplicationContext.startActivity(intent);
                }
                break;
            }
            case "Wifi:GetIPAddress": {
                getWifiIPAddress(callback);
                break;
            }
        }
    }

    // This function only works for IPv4
    private void getWifiIPAddress(final EventCallback callback) {
        final WifiManager mgr = (WifiManager) mApplicationContext.getSystemService(Context.WIFI_SERVICE);

        if (mgr == null) {
            callback.sendError("Cannot get WifiManager");
            return;
        }

        final WifiInfo info = mgr.getConnectionInfo();
        if (info == null) {
            callback.sendError("Cannot get connection info");
            return;
        }

        int ip = info.getIpAddress();
        if (ip == 0) {
            callback.sendError("Cannot get IPv4 address");
            return;
        }
        callback.sendSuccess(Formatter.formatIpAddress(ip));
    }

    private void stopListening() {
        if (null == mApplicationContext) {
            return;
        }

        if (!mIsListening) {
            Log.w(LOGTAG, "Already stopped!");
            return;
        }

        mApplicationContext.unregisterReceiver(this);
        mIsListening = false;
    }

    private int wifiDhcpGatewayAddress() {
        if (mConnectionType != ConnectionType.WIFI) {
            return 0;
        }

        if (null == mApplicationContext) {
            return 0;
        }

        try {
            WifiManager mgr = (WifiManager) mApplicationContext.getSystemService(Context.WIFI_SERVICE);
            DhcpInfo d = mgr.getDhcpInfo();
            if (d == null) {
                return 0;
            }

            return d.gateway;

        } catch (Exception ex) {
            // getDhcpInfo() is not documented to require any permissions, but on some devices
            // requires android.permission.ACCESS_WIFI_STATE. Just catch the generic exception
            // here and returning 0. Not logging because this could be noisy.
            return 0;
        }
    }

    private void updateConnectionType() {
        final ConnectionType previousConnectionType = mConnectionType;
        final ConnectionType newConnectionType = getConnectionType();
        if (newConnectionType == previousConnectionType) {
            return;
        }

        mConnectionType = newConnectionType;

        if (!mShouldNotify) {
            return;
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createNetworkEvent(
                                       newConnectionType.value,
                                       newConnectionType == ConnectionType.WIFI,
                                       wifiDhcpGatewayAddress()));
    }

    public double[] getCurrentInformation() {
        final ConnectionType connectionType = mConnectionType;
        return new double[] { connectionType.value,
                              connectionType == ConnectionType.WIFI ? 1.0 : 0.0,
                              wifiDhcpGatewayAddress() };
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

    private ConnectionType getConnectionType() {
        final Context appContext = mApplicationContext;

        if (null == appContext) {
            return ConnectionType.NONE;
        }

        ConnectivityManager cm = (ConnectivityManager) appContext.getSystemService(Context.CONNECTIVITY_SERVICE);
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

    private static int getNetworkOperator(InfoType type, Context context) {
        if (null == context) {
            return -1;
        }

        TelephonyManager tel = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
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
        }

        if (type == InfoType.MCC) {
            return Integer.parseInt(networkOperator.substring(0, 3));
        }

        return -1;
    }

    /**
     * These are called from JavaScript ctypes. Avoid letting ProGuard delete them.
     *
     * Note that these methods must only be called after GeckoAppShell has been
     * initialized: they depend on access to the context.
     */
    @JNITarget
    public static int getMCC() {
        return getNetworkOperator(InfoType.MCC, GeckoAppShell.getContext().getApplicationContext());
    }

    @JNITarget
    public static int getMNC() {
        return getNetworkOperator(InfoType.MNC, GeckoAppShell.getContext().getApplicationContext());
    }
}
