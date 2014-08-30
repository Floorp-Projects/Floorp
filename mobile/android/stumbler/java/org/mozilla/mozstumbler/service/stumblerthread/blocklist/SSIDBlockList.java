/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.blocklist;

import android.net.wifi.ScanResult;

public final class SSIDBlockList {
    private static String[] sPrefixList = new String[]{};
    private static String[] sSuffixList = new String[]{"_nomap"};

    private SSIDBlockList() {
    }

    public static void setFilterLists(String[] prefix, String[] suffix) {
        sPrefixList = prefix;
        sSuffixList = suffix;
    }

    public static boolean contains(ScanResult scanResult) {
        String SSID = scanResult.SSID;
        if (SSID == null) {
            return true; // no SSID?
        }

        for (String prefix : sPrefixList) {
            if (SSID.startsWith(prefix)) {
                return true; // blocked!
            }
        }

        for (String suffix : sSuffixList) {
            if (SSID.endsWith(suffix)) {
                return true; // blocked!
            }
        }

        return false; // OK
    }
}
