/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.system.Os;
import android.util.Log;

import org.mozilla.geckoview.BuildConfig;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

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

    private static volatile File sLibDir;
    private static volatile int sMachineType = -1;

    private HardwareUtils() {
    }

    public static void init(final Context context) {
        if (sInited) {
            return;
        }

        // Pre-populate common flags from the context.
        final int screenLayoutSize = context.getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK;
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

        sLibDir = new File(context.getApplicationInfo().nativeLibraryDir);
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

    private static String getPreferredAbi() {
        String abi = null;
        if (Build.VERSION.SDK_INT >= 21) {
            abi = Build.SUPPORTED_ABIS[0];
        }
        if (abi == null) {
            abi = Build.CPU_ABI;
        }
        return abi;
    }

    public static boolean isARMSystem() {
        return "armeabi-v7a".equals(getPreferredAbi());
    }

    public static boolean isARM64System() {
        // 64-bit support was introduced in 21.
        return "arm64-v8a".equals(getPreferredAbi());
    }

    public static boolean isX86System() {
        if ("x86".equals(getPreferredAbi())) {
            return true;
        }
        if (Build.VERSION.SDK_INT >= 21) {
            // On some devices we have to look into the kernel release string.
            try {
                return Os.uname().release.contains("-x86_");
            } catch (final Exception e) {
                Log.w(LOGTAG, "Cannot get uname", e);
            }
        }
        return false;
    }

    public static String getRealAbi() {
        if (isX86System() && isARMSystem()) {
            // Some x86 devices try to make us believe we're ARM,
            // in which case CPU_ABI is not reliable.
            return "x86";
        }
        return getPreferredAbi();
    }

    private static final int ELF_MACHINE_UNKNOWN = 0;
    private static final int ELF_MACHINE_X86 = 0x03;
    private static final int ELF_MACHINE_X86_64 = 0x3e;
    private static final int ELF_MACHINE_ARM = 0x28;
    private static final int ELF_MACHINE_AARCH64 = 0xb7;

    private static int readElfMachineType(final File file) {
        try (final FileInputStream is = new FileInputStream(file)) {
            final byte[] buf = new byte[19];
            int count = 0;
            while (count != buf.length) {
                count += is.read(buf, count, buf.length - count);
            }

            int machineType = buf[18];
            if (machineType < 0) {
                machineType += 256;
            }

            return machineType;
        } catch (FileNotFoundException e) {
            Log.w(LOGTAG, String.format("Failed to open %s", file.getAbsolutePath()));
            return ELF_MACHINE_UNKNOWN;
        } catch (IOException e) {
            Log.w(LOGTAG, "Failed to read library", e);
            return ELF_MACHINE_UNKNOWN;
        }
    }

    private static String machineTypeToString(final int machineType) {
        switch (machineType) {
            case ELF_MACHINE_X86:
                return "x86";
            case ELF_MACHINE_X86_64:
                return "x86_64";
            case ELF_MACHINE_ARM:
                return "arm";
            case ELF_MACHINE_AARCH64:
                return "aarch64";
            case ELF_MACHINE_UNKNOWN:
            default:
                return String.format("unknown (0x%x)", machineType);
        }
    }

    private static void initMachineType() {
        if (sMachineType >= 0) {
            return;
        }

        sMachineType = readElfMachineType(new File(sLibDir, System.mapLibraryName("mozglue")));
    }

    /**
     * @return The ABI of the libraries installed for this app.
     */
    public static String getLibrariesABI() {
        initMachineType();

        return machineTypeToString(sMachineType);
    }

    /**
     * @return false if the current system is not supported (e.g. APK/system ABI mismatch).
     */
    public static boolean isSupportedSystem() {
        // We've had crash reports from users on API 10 (with minSDK==15). That shouldn't even install,
        // but since it does we need to protect against it:
        if (Build.VERSION.SDK_INT < BuildConfig.MIN_SDK_VERSION) {
            return false;
        }

        initMachineType();

        // See http://developer.android.com/ndk/guides/abis.html
        final boolean isSystemX86 = isX86System();
        final boolean isSystemARM = !isSystemX86 && isARMSystem();
        final boolean isSystemARM64 = isARM64System();

        final boolean isAppARM = sMachineType == ELF_MACHINE_ARM;
        final boolean isAppARM64 = sMachineType == ELF_MACHINE_AARCH64;
        final boolean isAppX86 = sMachineType == ELF_MACHINE_X86;

        // Only reject known incompatible ABIs. Better safe than sorry.
        if ((isSystemX86 && isAppARM) || (isSystemARM && isAppX86)) {
            return false;
        }

        if ((isSystemX86 && isAppX86) || (isSystemARM && isAppARM) ||
            (isSystemARM64 && (isAppARM64 || isAppARM))) {
            return true;
        }

        Log.w(LOGTAG, "Unknown app/system ABI combination: " +
                      machineTypeToString(sMachineType) + " / " + getRealAbi());
        return true;
    }
}
