/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.telephony.TelephonyManager;
import android.util.Log;

/*
 * A part of the work of GeckoNetworkManager is to give an estimation of the
 * download speed of the current connection. For known to be fast connection, we
 * simply use a predefined value (we don't care about being precise). For mobile
 * connections, we sort them in groups (generations) and estimate the average
 * real life download speed of that specific generation. This value comes from
 * researches (eg. Wikipedia articles) or is simply an arbitrary estimation.
 * Precision isn't important, we mostly need an order of magnitude.
 *
 * Each group is composed with networks represented by the constant from
 * Android's ConnectivityManager and the description comming from the same
 * class.
 *
 * 2G (15 bk/s):
 * int NETWORK_TYPE_IDEN     Current network is iDen
 * int NETWORK_TYPE_CDMA     Current network is CDMA: Either IS95A or IS95B
 *
 * 2.5G (60 kb/s)
 * int NETWORK_TYPE_GPRS     Current network is GPRS
 * int NETWORK_TYPE_1xRTT    Current network is 1xRTT
 *
 * 2.75G (200 kb/s)
 * int NETWORK_TYPE_EDGE     Current network is EDGE
 *
 * 3G (300 kb/s)
 * int NETWORK_TYPE_UMTS     Current network is UMTS
 * int NETWORK_TYPE_EVDO_0   Current network is EVDO revision 0
 *
 * 3.5G (7 Mb/s)
 * int NETWORK_TYPE_HSPA     Current network is HSPA
 * int NETWORK_TYPE_HSDPA    Current network is HSDPA
 * int NETWORK_TYPE_HSUPA    Current network is HSUPA
 * int NETWORK_TYPE_EVDO_A   Current network is EVDO revision A
 * int NETWORK_TYPE_EVDO_B   Current network is EVDO revision B
 * int NETWORK_TYPE_EHRPD    Current network is eHRPD
 *
 * 3.75G (20 Mb/s)
 * int NETWORK_TYPE_HSPAP    Current network is HSPA+
 *
 * 3.9G (50 Mb/s)
 * int NETWORK_TYPE_LTE      Current network is LTE
 */

public class GeckoNetworkManager extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoNetworkManager";

    static private final GeckoNetworkManager sInstance = new GeckoNetworkManager();

    static private final double  kDefaultBandwidth    = -1.0;
    static private final boolean kDefaultCanBeMetered = false;

    static private final double  kMaxBandwidth = 20.0;

    static private final double  kNetworkSpeedEthernet = 20.0;           // 20 Mb/s
    static private final double  kNetworkSpeedWifi     = 20.0;           // 20 Mb/s
    static private final double  kNetworkSpeedWiMax    = 40.0;           // 40 Mb/s
    static private final double  kNetworkSpeed_2_G     = 15.0 / 1024.0;  // 15 kb/s
    static private final double  kNetworkSpeed_2_5_G   = 60.0 / 1024.0;  // 60 kb/s
    static private final double  kNetworkSpeed_2_75_G  = 200.0 / 1024.0; // 200 kb/s
    static private final double  kNetworkSpeed_3_G     = 300.0 / 1024.0; // 300 kb/s
    static private final double  kNetworkSpeed_3_5_G   = 7.0;            // 7 Mb/s
    static private final double  kNetworkSpeed_3_75_G  = 20.0;           // 20 Mb/s
    static private final double  kNetworkSpeed_3_9_G   = 50.0;           // 50 Mb/s

    private enum NetworkType {
        NETWORK_NONE,
        NETWORK_ETHERNET,
        NETWORK_WIFI,
        NETWORK_WIMAX,
        NETWORK_2_G,    // 2G
        NETWORK_2_5_G,  // 2.5G
        NETWORK_2_75_G, // 2.75G
        NETWORK_3_G,    // 3G
        NETWORK_3_5_G,  // 3.5G
        NETWORK_3_75_G, // 3.75G
        NETWORK_3_9_G,  // 3.9G
        NETWORK_UNKNOWN
    }

    private Context mApplicationContext;
    private NetworkType  mNetworkType = NetworkType.NETWORK_NONE;
    private IntentFilter mNetworkFilter = new IntentFilter();
    // Whether the manager should be listening to Network Information changes.
    private boolean mShouldBeListening = false;
    // Whether the manager should notify Gecko that a change in Network
    // Information happened.
    private boolean mShouldNotify      = false;

    public static GeckoNetworkManager getInstance() {
        return sInstance;
    }

    @Override
    public void onReceive(Context aContext, Intent aIntent) {
        updateNetworkType();
    }

    public void init(Context context) {
        mApplicationContext = context.getApplicationContext();
        mNetworkFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        mNetworkType = getNetworkType();
    }

    public void start() {
        mShouldBeListening = true;
        updateNetworkType();

        if (mShouldNotify) {
            startListening();
        }
    }

    private void startListening() {
        mApplicationContext.registerReceiver(sInstance, mNetworkFilter);
    }

    public void stop() {
        mShouldBeListening = false;

        if (mShouldNotify) {
        stopListening();
        }
    }

    private void stopListening() {
        mApplicationContext.unregisterReceiver(sInstance);
    }

    private void updateNetworkType() {
        NetworkType previousNetworkType = mNetworkType;
        mNetworkType = getNetworkType();

        if (mNetworkType == previousNetworkType || !mShouldNotify) {
            return;
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createNetworkEvent(
                                       getNetworkSpeed(mNetworkType),
                                       isNetworkUsuallyMetered(mNetworkType)));
    }

    public double[] getCurrentInformation() {
        return new double[] { getNetworkSpeed(mNetworkType),
                              isNetworkUsuallyMetered(mNetworkType) ? 1.0 : 0.0 };
    }

    public void enableNotifications() {
        // We set mShouldNotify *after* calling updateNetworkType() to make sure we
        // don't notify an eventual change in mNetworkType.
        updateNetworkType();
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

    private static NetworkType getNetworkType() {
        ConnectivityManager cm =
            (ConnectivityManager)sInstance.mApplicationContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) {
            Log.e(LOGTAG, "Connectivity service does not exist");
            return NetworkType.NETWORK_NONE;
        }

        NetworkInfo ni = cm.getActiveNetworkInfo();
        if (ni == null) {
            return NetworkType.NETWORK_NONE;
        }

        switch (ni.getType()) {
        case ConnectivityManager.TYPE_ETHERNET:
            return NetworkType.NETWORK_ETHERNET;
        case ConnectivityManager.TYPE_WIFI:
            return NetworkType.NETWORK_WIFI;
        case ConnectivityManager.TYPE_WIMAX:
            return NetworkType.NETWORK_WIMAX;
        case ConnectivityManager.TYPE_MOBILE:
            break; // We will handle sub-types after the switch.
        default:
            Log.w(LOGTAG, "Ignoring the current network type.");
            return NetworkType.NETWORK_UNKNOWN;
        }

        TelephonyManager tm =
            (TelephonyManager)sInstance.mApplicationContext.getSystemService(Context.TELEPHONY_SERVICE);
        if (tm == null) {
            Log.e(LOGTAG, "Telephony service does not exist");
            return NetworkType.NETWORK_UNKNOWN;
        }

        switch (tm.getNetworkType()) {
        case TelephonyManager.NETWORK_TYPE_IDEN:
        case TelephonyManager.NETWORK_TYPE_CDMA:
            return NetworkType.NETWORK_2_G;
        case TelephonyManager.NETWORK_TYPE_GPRS:
        case TelephonyManager.NETWORK_TYPE_1xRTT:
            return NetworkType.NETWORK_2_5_G;
        case TelephonyManager.NETWORK_TYPE_EDGE:
            return NetworkType.NETWORK_2_75_G;
        case TelephonyManager.NETWORK_TYPE_UMTS:
        case TelephonyManager.NETWORK_TYPE_EVDO_0:
            return NetworkType.NETWORK_3_G;
        case TelephonyManager.NETWORK_TYPE_HSPA:
        case TelephonyManager.NETWORK_TYPE_HSDPA:
        case TelephonyManager.NETWORK_TYPE_HSUPA:
        case TelephonyManager.NETWORK_TYPE_EVDO_A:
        case TelephonyManager.NETWORK_TYPE_EVDO_B:
        case TelephonyManager.NETWORK_TYPE_EHRPD:
            return NetworkType.NETWORK_3_5_G;
        case TelephonyManager.NETWORK_TYPE_HSPAP:
            return NetworkType.NETWORK_3_75_G;
        case TelephonyManager.NETWORK_TYPE_LTE:
            return NetworkType.NETWORK_3_9_G;
        case TelephonyManager.NETWORK_TYPE_UNKNOWN:
        default:
            Log.w(LOGTAG, "Connected to an unknown mobile network!");
            return NetworkType.NETWORK_UNKNOWN;
        }
    }

    private static double getNetworkSpeed(NetworkType aType) {
        switch (aType) {
        case NETWORK_NONE:
            return 0.0;
        case NETWORK_ETHERNET:
            return kNetworkSpeedEthernet;
        case NETWORK_WIFI:
            return kNetworkSpeedWifi;
        case NETWORK_WIMAX:
            return kNetworkSpeedWiMax;
        case NETWORK_2_G:
            return kNetworkSpeed_2_G;
        case NETWORK_2_5_G:
            return kNetworkSpeed_2_5_G;
        case NETWORK_2_75_G:
            return kNetworkSpeed_2_75_G;
        case NETWORK_3_G:
            return kNetworkSpeed_3_G;
        case NETWORK_3_5_G:
            return kNetworkSpeed_3_5_G;
        case NETWORK_3_75_G:
            return kNetworkSpeed_3_75_G;
        case NETWORK_3_9_G:
            return kNetworkSpeed_3_9_G;
        case NETWORK_UNKNOWN:
        default:
            return kDefaultBandwidth;
        }
    }

    private static boolean isNetworkUsuallyMetered(NetworkType aType) {
        switch (aType) {
        case NETWORK_NONE:
        case NETWORK_UNKNOWN:
        case NETWORK_ETHERNET:
        case NETWORK_WIFI:
        case NETWORK_WIMAX:
            return false;
        case NETWORK_2_G:
        case NETWORK_2_5_G:
        case NETWORK_2_75_G:
        case NETWORK_3_G:
        case NETWORK_3_5_G:
        case NETWORK_3_75_G:
        case NETWORK_3_9_G:
            return true;
        default:
            Log.e(LOGTAG, "Got an unexpected network type!");
            return false;
        }
    }
}
