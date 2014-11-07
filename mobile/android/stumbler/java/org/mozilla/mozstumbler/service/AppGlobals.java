/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service;

import java.util.concurrent.ConcurrentLinkedQueue;

public class AppGlobals {
    public static final String LOG_PREFIX = "Stumbler:";

    /* All intent actions start with this string. Only locally broadcasted. */
    public static final String ACTION_NAMESPACE = "org.mozilla.mozstumbler.intent.action";

    /* Handle this for logging reporter info. */
    public static final String ACTION_GUI_LOG_MESSAGE = AppGlobals.ACTION_NAMESPACE + ".LOG_MESSAGE";
    public static final String ACTION_GUI_LOG_MESSAGE_EXTRA = ACTION_GUI_LOG_MESSAGE + ".MESSAGE";

    /* Defined here so that the Reporter class can access the time of an Intent in a generic fashion.
     * Classes should have their own constant that is assigned to this, for example,
     * WifiScanner has ACTION_WIFIS_SCANNED_ARG_TIME = ACTION_ARG_TIME.
     * This member definition in the broadcaster makes it clear what the extra Intent args are for that class. */
    public static final String ACTION_ARG_TIME = "time";

    /* Location constructor requires a named origin, these are created in the app. */
    public static final String LOCATION_ORIGIN_INTERNAL = "internal";

    public enum ActiveOrPassiveStumbling { ACTIVE_STUMBLING, PASSIVE_STUMBLING }

    /* In passive mode, only scan this many times for each gps. */
    public static final int PASSIVE_MODE_MAX_SCANS_PER_GPS = 3;

    /* These are set on startup. The appVersionName and code are not used in the service-only case. */
    public static String appVersionName = "0.0.0";
    public static int appVersionCode = 0;
    public static String appName = "StumblerService";
    public static boolean isDebug;

    /* The log activity will clear this periodically, and display the messages.
     * Always null when the stumbler service is used stand-alone. */
    public static volatile ConcurrentLinkedQueue<String> guiLogMessageBuffer;

    public static void guiLogError(String msg) {
        guiLogInfo(msg, "red", true);
    }

    public static void guiLogInfo(String msg) {
        guiLogInfo(msg, "white", false);
    }

    public static void guiLogInfo(String msg, String color, boolean isBold) {
        if (guiLogMessageBuffer != null) {
            if (isBold) {
                msg = "<b>" + msg + "</b>";
            }
            guiLogMessageBuffer.add("<font color='" + color +"'>" + msg + "</font>");
        }
    }

    public static final String ACTION_TEST_SETTING_ENABLED = "stumbler-test-setting-enabled";
    public static final String ACTION_TEST_SETTING_DISABLED = "stumbler-test-setting-disabled";
}

