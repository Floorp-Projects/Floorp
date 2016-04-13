/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.SysInfo;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.util.Log;
import android.view.ViewConfiguration;

public final class HardwareUtils {
    private static final String LOGTAG = "GeckoHardwareUtils";

    private static final boolean IS_AMAZON_DEVICE = Build.MANUFACTURER.equalsIgnoreCase("Amazon");
    public static final boolean IS_KINDLE_DEVICE = IS_AMAZON_DEVICE &&
                                                   (Build.MODEL.equals("Kindle Fire") ||
                                                    Build.MODEL.startsWith("KF"));

    private static volatile boolean sInited;

    // These are all set once, during init.
    private static volatile boolean sIsLargeTablet;
    private static volatile boolean sIsSmallTablet;
    private static volatile boolean sIsTelevision;

    private HardwareUtils() {
    }

    public static void init(Context context) {
        if (sInited) {
            // This is unavoidable, given that HardwareUtils is called from background services.
            Log.d(LOGTAG, "HardwareUtils already inited.");
            return;
        }

        // Pre-populate common flags from the context.
        final int screenLayoutSize = context.getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK;
        if (Build.VERSION.SDK_INT >= 11) {
            if (screenLayoutSize == Configuration.SCREENLAYOUT_SIZE_XLARGE) {
                sIsLargeTablet = true;
            } else if (screenLayoutSize == Configuration.SCREENLAYOUT_SIZE_LARGE) {
                sIsSmallTablet = true;
            }
            if (Build.VERSION.SDK_INT >= 16) {
                if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEVISION)) {
                    sIsTelevision = true;
                }
            }
        }

        sInited = true;
    }

    public static boolean isTablet() {
        return sIsLargeTablet || sIsSmallTablet;
    }

    public static boolean isLargeTablet() {
        return sIsLargeTablet;
    }

    public static boolean isSmallTablet() {
        return sIsSmallTablet;
    }

    public static boolean isTelevision() {
        return sIsTelevision;
    }

    public static int getMemSize() {
        return SysInfo.getMemSize();
    }

    /**
     * @return false if the current system is not supported (e.g. APK/system ABI mismatch).
     */
    public static boolean isSupportedSystem() {
        if (Build.VERSION.SDK_INT < AppConstants.Versions.MIN_SDK_VERSION ||
            Build.VERSION.SDK_INT > AppConstants.Versions.MAX_SDK_VERSION) {
            return false;
        }

        // See http://developer.android.com/ndk/guides/abis.html
        boolean isSystemARM = Build.CPU_ABI != null && Build.CPU_ABI.startsWith("arm");
        boolean isSystemX86 = Build.CPU_ABI != null && Build.CPU_ABI.startsWith("x86");

        boolean isAppARM = AppConstants.ANDROID_CPU_ARCH.startsWith("arm");
        boolean isAppX86 = AppConstants.ANDROID_CPU_ARCH.startsWith("x86");

        // Only reject known incompatible ABIs. Better safe than sorry.
        if ((isSystemX86 && isAppARM) || (isSystemARM && isAppX86)) {
            return false;
        }

        if ((isSystemX86 && isAppX86) || (isSystemARM && isAppARM)) {
            return true;
        }

        Log.w(LOGTAG, "Unknown app/system ABI combination: " + AppConstants.MOZ_APP_ABI + " / " + Build.CPU_ABI);
        return true;
    }
}
