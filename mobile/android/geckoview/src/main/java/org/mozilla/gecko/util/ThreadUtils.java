/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.RobocopTarget;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

public final class ThreadUtils {
    private static final String LOGTAG = "ThreadUtils";

    /**
     * Controls the action taken when a method like
     * {@link ThreadUtils#assertOnUiThread(AssertBehavior)} detects a problem.
     */
    public enum AssertBehavior {
        NONE,
        THROW,
    }

    private static final Thread sUiThread = Looper.getMainLooper().getThread();
    private static final Handler sUiHandler = new Handler(Looper.getMainLooper());

    private static volatile Thread sBackgroundThread;

    // Referenced directly from GeckoAppShell in highly performance-sensitive code (The extra
    // function call of the getter was harming performance. (Bug 897123))
    // Once Bug 709230 is resolved we should reconsider this as ProGuard should be able to optimise
    // this out at compile time.
    public static Handler sGeckoHandler;
    public static volatile Thread sGeckoThread;

    public static void setBackgroundThread(final Thread thread) {
        sBackgroundThread = thread;
    }

    public static Thread getUiThread() {
        return sUiThread;
    }

    public static Handler getUiHandler() {
        return sUiHandler;
    }

    /**
     * Runs the provided runnable on the UI thread. If this method is called on the UI thread
     * the runnable will be executed synchronously.
     *
     * @param runnable the runnable to be executed.
     */
    public static void runOnUiThread(final Runnable runnable) {
        // We're on the UI thread already, let's just run this
        if (isOnUiThread()) {
            runnable.run();
            return;
        }

        postToUiThread(runnable);
    }

    public static void postToUiThread(final Runnable runnable) {
        sUiHandler.post(runnable);
    }

    public static Thread getBackgroundThread() {
        return sBackgroundThread;
    }

    public static Handler getBackgroundHandler() {
        return GeckoBackgroundThread.getHandler();
    }

    public static void postToBackgroundThread(final Runnable runnable) {
        GeckoBackgroundThread.post(runnable);
    }

    public static void assertOnUiThread(final AssertBehavior assertBehavior) {
        assertOnThread(getUiThread(), assertBehavior);
    }

    public static void assertOnUiThread() {
        assertOnThread(getUiThread(), AssertBehavior.THROW);
    }

    public static void assertNotOnUiThread() {
        assertNotOnThread(getUiThread(), AssertBehavior.THROW);
    }

    @RobocopTarget
    public static void assertOnGeckoThread() {
        assertOnThread(sGeckoThread, AssertBehavior.THROW);
    }

    public static void assertOnThread(final Thread expectedThread, final AssertBehavior behavior) {
        assertOnThreadComparison(expectedThread, behavior, true);
    }

    public static void assertNotOnThread(final Thread expectedThread,
                                         final AssertBehavior behavior) {
        assertOnThreadComparison(expectedThread, behavior, false);
    }

    private static void assertOnThreadComparison(final Thread expectedThread,
                                                 final AssertBehavior behavior,
                                                 final boolean expected) {
        final Thread currentThread = Thread.currentThread();
        final long currentThreadId = currentThread.getId();
        final long expectedThreadId = expectedThread.getId();

        if ((currentThreadId == expectedThreadId) == expected) {
            return;
        }

        final String message;
        if (expected) {
            message = "Expected thread " + expectedThreadId +
                      " (\"" + expectedThread.getName() + "\"), but running on thread " +
                      currentThreadId + " (\"" + currentThread.getName() + "\")";
        } else {
            message = "Expected anything but " + expectedThreadId +
                      " (\"" + expectedThread.getName() + "\"), but running there.";
        }

        final IllegalThreadStateException e = new IllegalThreadStateException(message);

        switch (behavior) {
            case THROW:
                throw e;
            default:
                Log.e(LOGTAG, "Method called on wrong thread!", e);
        }
    }

    public static boolean isOnGeckoThread() {
        if (sGeckoThread != null) {
            return isOnThread(sGeckoThread);
        }
        return false;
    }

    public static boolean isOnUiThread() {
        return isOnThread(getUiThread());
    }

    @RobocopTarget
    public static boolean isOnBackgroundThread() {
        if (sBackgroundThread == null) {
            return false;
        }

        return isOnThread(sBackgroundThread);
    }

    @RobocopTarget
    public static boolean isOnThread(final Thread thread) {
        return (Thread.currentThread().getId() == thread.getId());
    }
}
