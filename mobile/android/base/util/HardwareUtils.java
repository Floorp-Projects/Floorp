/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.util.Log;
import android.view.ViewConfiguration;

public final class HardwareUtils {
    private static final String LOGTAG = "GeckoHardwareUtils";

    // Minimum memory threshold for a device to be considered
    // a low memory platform (see isLowMemoryPlatform). This value
    // has be in sync with Gecko's equivalent threshold (defined in
    // xpcom/base/nsMemoryImpl.cpp) and should only be used in cases
    // where we can't depend on Gecko to be up and running e.g. show/hide
    // reading list capabilities in HomePager.
    private static final int LOW_MEMORY_THRESHOLD_MB = 384;

    // Number of bytes of /proc/meminfo to read in one go.
    private static final int MEMINFO_BUFFER_SIZE_BYTES = 256;

    private static final boolean IS_AMAZON_DEVICE = Build.MANUFACTURER.equalsIgnoreCase("Amazon");
    public static final boolean IS_KINDLE_DEVICE = IS_AMAZON_DEVICE &&
                                                   (Build.MODEL.equals("Kindle Fire") ||
                                                    Build.MODEL.startsWith("KF"));
    private static volatile int sTotalRAM = -1;

    private static volatile boolean sInited;

    // These are all set once, during init.
    private static volatile boolean sIsLargeTablet;
    private static volatile boolean sIsSmallTablet;
    private static volatile boolean sIsTelevision;
    private static volatile boolean sHasMenuButton;

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
            sHasMenuButton = false;
            if (screenLayoutSize == Configuration.SCREENLAYOUT_SIZE_XLARGE) {
                sIsLargeTablet = true;
            } else if (screenLayoutSize == Configuration.SCREENLAYOUT_SIZE_LARGE) {
                sIsSmallTablet = true;
            }
            if (Build.VERSION.SDK_INT >= 14) {
                sHasMenuButton = ViewConfiguration.get(context).hasPermanentMenuKey();

                if (Build.VERSION.SDK_INT >= 16) {
                    if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEVISION)) {
                        sIsTelevision = true;
                    }
                }
            }
        } else {
            sHasMenuButton = true;
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

    public static boolean hasMenuButton() {
        return sHasMenuButton;
    }

    /**
    * Helper functions used to extract key/value data from /proc/meminfo
    * Pulled from:
    * http://androidxref.com/4.2_r1/xref/frameworks/base/core/java/com/android/internal/util/MemInfoReader.java
    */

    private static boolean matchMemText(byte[] buffer, int index, int bufferLength, byte[] text) {
        final int N = text.length;
        if ((index + N) >= bufferLength) {
            return false;
        }
        for (int i = 0; i < N; i++) {
            if (buffer[index + i] != text[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Parses a line like:
     *
     *  MemTotal: 1605324 kB
     *
     * into 1605324.
     *
     * @return the first uninterrupted sequence of digits following the
     *         specified index, parsed as an integer value in KB.
     */
    private static int extractMemValue(byte[] buffer, int offset, int length) {
        if (offset >= length) {
            return 0;
        }

        while (offset < length && buffer[offset] != '\n') {
            if (buffer[offset] >= '0' && buffer[offset] <= '9') {
                int start = offset++;
                while (offset < length &&
                       buffer[offset] >= '0' &&
                       buffer[offset] <= '9') {
                    ++offset;
                }
                return Integer.parseInt(new String(buffer, start, offset - start), 10);
            }
            ++offset;
        }
        return 0;
    }

    /**
     * Fetch the total memory of the device in MB by parsing /proc/meminfo.
     *
     * Of course, Android doesn't have a neat and tidy way to find total
     * RAM, so we do it by parsing /proc/meminfo.
     *
     * @return 0 if a problem occurred, or memory size in MB.
     */
    public static int getMemSize() {
        if (sTotalRAM >= 0) {
            return sTotalRAM;
        }

        // This is the string "MemTotal" that we're searching for in the buffer.
        final byte[] MEMTOTAL = {'M', 'e', 'm', 'T', 'o', 't', 'a', 'l'};
        try {
            final byte[] buffer = new byte[MEMINFO_BUFFER_SIZE_BYTES];
            final FileInputStream is = new FileInputStream("/proc/meminfo");
            try {
                final int length = is.read(buffer);

                for (int i = 0; i < length; i++) {
                    if (matchMemText(buffer, i, length, MEMTOTAL)) {
                        i += 8;
                        sTotalRAM = extractMemValue(buffer, i, length) / 1024;
                        Log.d(LOGTAG, "System memory: " + sTotalRAM + "MB.");
                        return sTotalRAM;
                    }
                }
            } finally {
                is.close();
            }

            Log.w(LOGTAG, "Did not find MemTotal line in /proc/meminfo.");
            return sTotalRAM = 0;
        } catch (FileNotFoundException f) {
            return sTotalRAM = 0;
        } catch (IOException e) {
            return sTotalRAM = 0;
        }
    }

    public static boolean isLowMemoryPlatform() {
        final int memSize = getMemSize();

        // Fallback to false if we fail to read meminfo
        // for some reason.
        if (memSize == 0) {
            Log.w(LOGTAG, "Could not compute system memory. Falling back to isLowMemoryPlatform = false.");
            return false;
        }

        return memSize < LOW_MEMORY_THRESHOLD_MB;
    }
}
