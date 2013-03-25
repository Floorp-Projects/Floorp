/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;

public final class ThreadUtils {
    private static Thread sUiThread;
    private static Thread sGeckoThread;
    private static Thread sBackgroundThread;

    private static Handler sUiHandler;

    public static void setUiThread(Thread thread, Handler handler) {
        sUiThread = thread;
        sUiHandler = handler;
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

    public static Handler getUiHandler() {
        return sUiHandler;
    }

    public static void postToUiThread(Runnable runnable) {
        sUiHandler.post(runnable);
    }

    public static Thread getGeckoThread() {
        return sGeckoThread;
    }

    public static Thread getBackgroundThread() {
        return sBackgroundThread;
    }

    public static Handler getBackgroundHandler() {
        return GeckoBackgroundThread.getHandler();
    }

    public static void postToBackgroundThread(Runnable runnable) {
        GeckoBackgroundThread.post(runnable);
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

    public static boolean isOnUiThread() {
        return isOnThread(getUiThread());
    }

    public static boolean isOnBackgroundThread() {
        return isOnThread(sBackgroundThread);
    }

    public static boolean isOnThread(Thread thread) {
        return (Thread.currentThread().getId() == thread.getId());
    }
}
