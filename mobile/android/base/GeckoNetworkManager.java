/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.lang.Math;

import android.util.Log;

import android.content.Context;

import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import android.telephony.TelephonyManager;

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

public class GeckoNetworkManager
{
  static private final double  kDefaultBandwidth    = -1.0;
  static private final boolean kDefaultCanBeMetered = false;

  static private final double  kMaxBandwidth = 20.0;

  static private final double  kNetworkSpeed_2_G    = 15.0 / 1024.0;  // 15 kb/s
  static private final double  kNetworkSpeed_2_5_G  = 60.0 / 1024.0;  // 60 kb/s
  static private final double  kNetworkSpeed_2_75_G = 200.0 / 1024.0; // 200 kb/s
  static private final double  kNetworkSpeed_3_G    = 300.0 / 1024.0; // 300 kb/s
  static private final double  kNetworkSpeed_3_5_G  = 7.0;            // 7 Mb/s
  static private final double  kNetworkSpeed_3_75_G = 20.0;           // 20 Mb/s
  static private final double  kNetworkSpeed_3_9_G  = 50.0;           // 50 Mb/s

  private enum MobileNetworkType {
    NETWORK_2_G,    // 2G
    NETWORK_2_5_G,  // 2.5G
    NETWORK_2_75_G, // 2.75G
    NETWORK_3_G,    // 3G
    NETWORK_3_5_G,  // 3.5G
    NETWORK_3_75_G, // 3.75G
    NETWORK_3_9_G,  // 3.9G
    NETWORK_UNKNOWN
  }

  private static MobileNetworkType getMobileNetworkType() {
    TelephonyManager tm =
      (TelephonyManager)GeckoApp.mAppContext.getSystemService(Context.TELEPHONY_SERVICE);

    switch (tm.getNetworkType()) {
      case TelephonyManager.NETWORK_TYPE_IDEN:
      case TelephonyManager.NETWORK_TYPE_CDMA:
        return MobileNetworkType.NETWORK_2_G;
      case TelephonyManager.NETWORK_TYPE_GPRS:
      case TelephonyManager.NETWORK_TYPE_1xRTT:
        return MobileNetworkType.NETWORK_2_5_G;
      case TelephonyManager.NETWORK_TYPE_EDGE:
        return MobileNetworkType.NETWORK_2_75_G;
      case TelephonyManager.NETWORK_TYPE_UMTS:
      case TelephonyManager.NETWORK_TYPE_EVDO_0:
        return MobileNetworkType.NETWORK_3_G;
      case TelephonyManager.NETWORK_TYPE_HSPA:
      case TelephonyManager.NETWORK_TYPE_HSDPA:
      case TelephonyManager.NETWORK_TYPE_HSUPA:
      case TelephonyManager.NETWORK_TYPE_EVDO_A:
      case TelephonyManager.NETWORK_TYPE_EVDO_B:
      case TelephonyManager.NETWORK_TYPE_EHRPD:
        return MobileNetworkType.NETWORK_3_5_G;
      case TelephonyManager.NETWORK_TYPE_HSPAP:
        return MobileNetworkType.NETWORK_3_75_G;
      case TelephonyManager.NETWORK_TYPE_LTE:
        return MobileNetworkType.NETWORK_3_9_G;
      case TelephonyManager.NETWORK_TYPE_UNKNOWN:
      default:
        Log.w("GeckoNetworkManager", "Connected to an unknown mobile network!");
        return MobileNetworkType.NETWORK_UNKNOWN;
    }
  }

  public static double getMobileNetworkSpeed(MobileNetworkType aType) {
    switch (aType) {
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

  public static double[] getCurrentInformation() {
    ConnectivityManager cm =
      (ConnectivityManager)GeckoApp.mAppContext.getSystemService(Context.CONNECTIVITY_SERVICE);

    if (cm.getActiveNetworkInfo() == null) {
      return new double[] { 0.0, 1.0 };
    }

    int type = cm.getActiveNetworkInfo().getType();
    double bandwidth = kDefaultBandwidth;
    boolean canBeMetered = kDefaultCanBeMetered;

    switch (type) {
      case ConnectivityManager.TYPE_ETHERNET:
      case ConnectivityManager.TYPE_WIFI:
      case ConnectivityManager.TYPE_WIMAX:
        bandwidth = kMaxBandwidth;
        canBeMetered = false;
        break;
      case ConnectivityManager.TYPE_MOBILE:
        bandwidth = Math.min(getMobileNetworkSpeed(getMobileNetworkType()), kMaxBandwidth);
        canBeMetered = true;
        break;
      default:
        Log.w("GeckoNetworkManager", "Ignoring the current network type.");
        break;
    }

    return new double[] { bandwidth, canBeMetered ? 1.0 : 0.0 };
  }
}
