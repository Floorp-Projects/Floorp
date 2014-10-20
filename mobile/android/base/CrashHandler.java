/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.PrintWriter;
import java.io.StringWriter;

import android.os.Process;
import android.util.Log;

class CrashHandler implements Thread.UncaughtExceptionHandler {

    private static final String LOGTAG = "GeckoCrashHandler";
    private static final Thread MAIN_THREAD = Thread.currentThread();

    protected final Thread.UncaughtExceptionHandler systemUncaughtHandler;

    protected boolean crashing;

    /**
     * Get the root exception from the 'cause' chain of an exception.
     *
     * @param exc An exception
     * @return The root exception
     */
    public static Throwable getRootException(Throwable exc) {
        for (Throwable cause = exc; cause != null; cause = cause.getCause()) {
            exc = cause;
        }
        return exc;
    }

    /**
     * Get the standard stack trace string of an exception.
     *
     * @param exc An exception
     * @return The exception stack trace.
     */
    public static String getExceptionStackTrace(final Throwable exc) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        exc.printStackTrace(pw);
        pw.flush();
        return sw.toString();
    }

    /**
     * Terminate the current process.
     */
    public static void terminateProcess() {
        Process.killProcess(Process.myPid());
    }

    /**
     * Create and register a CrashHandler for all threads and thread groups.
     */
    public CrashHandler() {
        systemUncaughtHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(this);
    }

    /**
     * Create and register a CrashHandler for a particular thread.
     *
     * @param thread A thread to register the CrashHandler
     */
    public CrashHandler(final Thread thread) {
        systemUncaughtHandler = thread.getUncaughtExceptionHandler();
        thread.setUncaughtExceptionHandler(this);
    }

    /**
     * Record an exception stack in logs.
     *
     * @param thread The exception thread
     * @param exc An exception
     */
    protected void logException(final Thread thread, final Throwable exc) {
        try {
            Log.e(LOGTAG, ">>> REPORTING UNCAUGHT EXCEPTION FROM THREAD "
                          + thread.getId() + " (\"" + thread.getName() + "\")", exc);

            if (MAIN_THREAD != thread) {
                Log.e(LOGTAG, "Main thread (" + MAIN_THREAD.getId() + ") stack:");
                for (StackTraceElement ste : MAIN_THREAD.getStackTrace()) {
                    Log.e(LOGTAG, "    " + ste.toString());
                }
            }
        } catch (final Throwable e) {
            // If something throws here, we want to continue to report the exception,
            // so we catch all exceptions and ignore them.
        }
    }

    /**
     * Report an exception to Socorro.
     *
     * @param thread The exception thread
     * @param exc An exception
     * @return Whether the exception was successfully reported
     */
    protected boolean reportException(final Thread thread, final Throwable exc) {
        // TODO implement reporting to Socorro without Breakpad.
    }

    /**
     * Implements the default behavior for handling uncaught exceptions.
     *
     * @param thread The exception thread
     * @param exc An uncaught exception
     */
    @Override
    public void uncaughtException(Thread thread, Throwable exc) {
        if (this.crashing) {
            // Prevent possible infinite recusions.
            return;
        }

        if (thread == null) {
            // Gecko may pass in null for thread to denote the current thread.
            thread = Thread.currentThread();
        }

        try {
            this.crashing = true;
            exc = getRootException(exc);
            logException(thread, exc);

            if (reportException(thread, exc)) {
                // Reporting succeeded; we can terminate our process now.
                return;
            }

            if (systemUncaughtHandler != null) {
                // Follow the chain of uncaught handlers.
                systemUncaughtHandler.uncaughtException(thread, exc);
            }
        } finally {
            terminateProcess();
        }
    }
}
