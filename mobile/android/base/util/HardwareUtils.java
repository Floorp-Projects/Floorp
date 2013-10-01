/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.util.Log;
import android.view.ViewConfiguration;

import java.io.RandomAccessFile;
import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class HardwareUtils {
    private static final String LOGTAG = "GeckoHardwareUtils";

    // Minimum memory threshold for a device to be considered
    // a low memory platform (see sIsLowMemoryPlatform). This value
    // has be in sync with Gecko's equivalent threshold (defined in
    // xpcom/base/nsMemoryImpl.cpp) and should only be used in cases
    // where we can't depend on Gecko to be up and running e.g. show/hide
    // reading list capabilities in HomePager.
    private static final int LOW_MEMORY_THRESHOLD_KB = 384 * 1024;

    private static final String PROC_MEMINFO_FILE = "/proc/meminfo";

    private static final Pattern PROC_MEMTOTAL_FORMAT =
          Pattern.compile("^MemTotal:[ \t]*([0-9]*)[ \t]kB");

    private static Context sContext;

    private static Boolean sIsLargeTablet;
    private static Boolean sIsSmallTablet;
    private static Boolean sIsTelevision;
    private static Boolean sHasMenuButton;
    private static Boolean sIsLowMemoryPlatform;

    private HardwareUtils() {
    }

    public static void init(Context context) {
        if (sContext != null) {
            Log.w(LOGTAG, "HardwareUtils.init called twice!");
        }
        sContext = context;
    }

    public static boolean isTablet() {
        return isLargeTablet() || isSmallTablet();
    }

    public static boolean isLargeTablet() {
        if (sIsLargeTablet == null) {
            int screenLayout = sContext.getResources().getConfiguration().screenLayout;
            sIsLargeTablet = (Build.VERSION.SDK_INT >= 11 &&
                              ((screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) == Configuration.SCREENLAYOUT_SIZE_XLARGE));
        }
        return sIsLargeTablet;
    }

    public static boolean isSmallTablet() {
        if (sIsSmallTablet == null) {
            int screenLayout = sContext.getResources().getConfiguration().screenLayout;
            sIsSmallTablet = (Build.VERSION.SDK_INT >= 11 &&
                              ((screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) == Configuration.SCREENLAYOUT_SIZE_LARGE));
        }
        return sIsSmallTablet;
    }

    public static boolean isTelevision() {
        if (sIsTelevision == null) {
            sIsTelevision = sContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEVISION);
        }
        return sIsTelevision;
    }

    public static boolean hasMenuButton() {
        if (sHasMenuButton == null) {
            sHasMenuButton = Boolean.TRUE;
            if (Build.VERSION.SDK_INT >= 11) {
                sHasMenuButton = Boolean.FALSE;
            }
            if (Build.VERSION.SDK_INT >= 14) {
                sHasMenuButton = ViewConfiguration.get(sContext).hasPermanentMenuKey();
            }
        }
        return sHasMenuButton;
    }

    public static boolean isLowMemoryPlatform() {
        if (sIsLowMemoryPlatform == null) {
            RandomAccessFile fileReader = null;
            try {
                fileReader = new RandomAccessFile(PROC_MEMINFO_FILE, "r");

                // Defaults to false
                long totalMem = LOW_MEMORY_THRESHOLD_KB;

                String line = null;
                while ((line = fileReader.readLine()) != null) {
                    final Matcher matcher = PROC_MEMTOTAL_FORMAT.matcher(line);
                    if (matcher.find()) {
                        totalMem = Long.parseLong(matcher.group(1));
                        break;
                    }
                }

                sIsLowMemoryPlatform = (totalMem < LOW_MEMORY_THRESHOLD_KB);
            } catch (IOException e) {
                // Fallback to false if we fail to read meminfo
                // for some reason.
                Log.w(LOGTAG, "Could not read " + PROC_MEMINFO_FILE + "." +
                              "Falling back to isLowMemoryPlatform = false", e);
                sIsLowMemoryPlatform = false;
            } finally {
                if (fileReader != null) {
                    try {
                        fileReader.close();
                    } catch (IOException e) {
                        // Do nothing
                    }
                }
            }
        }

        return sIsLowMemoryPlatform;
    }
}
