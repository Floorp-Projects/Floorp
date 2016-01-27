/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.StrictMode;
import android.util.Log;

import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import java.util.regex.Pattern;

/**
 * A collection of system info values, broadly mirroring a subset of
 * nsSystemInfo. See also the constants in AppConstants, which reflect
 * much of nsIXULAppInfo.
 */
// Normally, we'd annotate with @RobocopTarget.  Since SysInfo is compiled
// before RobocopTarget, we instead add o.m.g.SysInfo directly to the Proguard
// configuration.
public final class SysInfo {
    private static final String LOG_TAG = "GeckoSysInfo";

    // Number of bytes of /proc/meminfo to read in one go.
    private static final int MEMINFO_BUFFER_SIZE_BYTES = 256;

    // We don't mind an instant of possible duplicate work, we only wish to
    // avoid inconsistency, so we don't bother with synchronization for
    // these.
    private static volatile int cpuCount = -1;

    private static volatile int totalRAM = -1;

    /**
     * Get the number of cores on the device.
     *
     * We can't use a nice tidy API call, because they're all wrong:
     *
     * <http://stackoverflow.com/questions/7962155/how-can-you-detect-a-dual-core-
     * cpu-on-an-android-device-from-code>
     *
     * This method is based on that code.
     *
     * @return the number of CPU cores, or 1 if the number could not be
     *         determined.
     */
    public static int getCPUCount() {
        if (cpuCount > 0) {
            return cpuCount;
        }

        // Avoid a strict mode warning.
        StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        try {
            return readCPUCount();
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }
    }

    private static int readCPUCount() {
        class CpuFilter implements FileFilter {
            @Override
            public boolean accept(File pathname) {
                return Pattern.matches("cpu[0-9]+", pathname.getName());
            }
        }
        try {
            final File dir = new File("/sys/devices/system/cpu/");
            return cpuCount = dir.listFiles(new CpuFilter()).length;
        } catch (Exception e) {
            Log.w(LOG_TAG, "Assuming 1 CPU; got exception.", e);
            return cpuCount = 1;
        }
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
        if (totalRAM >= 0) {
            return totalRAM;
        }

        // This is the string "MemTotal" that we're searching for in the buffer.
        final byte[] MEMTOTAL = {'M', 'e', 'm', 'T', 'o', 't', 'a', 'l'};

        // `/proc/meminfo` is not a real file and thus safe to read on the main thread.
        final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
        try {
            final byte[] buffer = new byte[MEMINFO_BUFFER_SIZE_BYTES];
            final FileInputStream is = new FileInputStream("/proc/meminfo");
            try {
                final int length = is.read(buffer);

                for (int i = 0; i < length; i++) {
                    if (matchMemText(buffer, i, length, MEMTOTAL)) {
                        i += 8;
                        totalRAM = extractMemValue(buffer, i, length) / 1024;
                        Log.d(LOG_TAG, "System memory: " + totalRAM + "MB.");
                        return totalRAM;
                    }
                }
            } finally {
                is.close();
            }

            Log.w(LOG_TAG, "Did not find MemTotal line in /proc/meminfo.");
            return totalRAM = 0;
        } catch (FileNotFoundException f) {
            return totalRAM = 0;
        } catch (IOException e) {
            return totalRAM = 0;
        } finally {
            StrictMode.setThreadPolicy(savedPolicy);
        }
    }

    /**
     * @return the SDK version supported by this device, such as '16'.
     */
    public static int getVersion() {
        return android.os.Build.VERSION.SDK_INT;
    }

    /**
     * @return the release version string, such as "4.1.2".
     */
    public static String getReleaseVersion() {
        return android.os.Build.VERSION.RELEASE;
    }

    /**
     * @return the kernel version string, such as "3.4.10-geb45596".
     */
    public static String getKernelVersion() {
        return System.getProperty("os.version", "");
    }

    /**
     * @return the device manufacturer, such as "HTC".
     */
    public static String getManufacturer() {
        return android.os.Build.MANUFACTURER;
    }

    /**
     * @return the device name, such as "HTC One".
     */
    public static String getDevice() {
        // No, not android.os.Build.DEVICE.
        return android.os.Build.MODEL;
    }

    /**
     * @return the Android "hardware" identifier, such as "m7".
     */
    public static String getHardware() {
        return android.os.Build.HARDWARE;
    }

    /**
     * @return the system OS name. Hardcoded to "Android".
     */
    public static String getName() {
        // We deliberately differ from PR_SI_SYSNAME, which is "Linux".
        return "Android";
    }

    /**
     * @return the Android architecture string, including ABI.
     */
    public static String getArchABI() {
        // Android likes to include the ABI, too ("armeabiv7"), so we
        // differ to add value.
        return android.os.Build.CPU_ABI;
    }
}
