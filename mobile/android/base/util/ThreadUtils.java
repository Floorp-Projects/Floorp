/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

public final class ThreadUtils {
    private static Thread sUiThread;
    private static Thread sGeckoThread;
    private static Thread sBackgroundThread;

    public static void setUiThread(Thread thread) {
        sUiThread = thread;
    }

    public static void setGeckoThread(Thread thread) {
        sGeckoThread = thread;
    }

    public static void setBackgroundThread(Thread thread) {
        sBackgroundThread = thread;
    }

    public static Thread getUiThread() {
        return sUiThread;
    }

    public static Thread getGeckoThread() {
        return sGeckoThread;
    }

    public static Thread getBackgroundThread() {
        return sBackgroundThread;
    }

    public static void assertOnUiThread() {
        assertOnThread(getUiThread());
    }

    public static void assertOnGeckoThread() {
        assertOnThread(getGeckoThread());
    }

    public static void assertOnBackgroundThread() {
        assertOnThread(getBackgroundThread());
    }

    public static void assertOnThread(Thread expectedThread) {
        Thread currentThread = Thread.currentThread();
        long currentThreadId = currentThread.getId();
        long expectedThreadId = expectedThread.getId();

        if (currentThreadId != expectedThreadId) {
            throw new IllegalThreadStateException("Expected thread " + expectedThreadId + " (\""
                                                  + expectedThread.getName()
                                                  + "\"), but running on thread " + currentThreadId
                                                  + " (\"" + currentThread.getName() + ")");
        }
    }
}
