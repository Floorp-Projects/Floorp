/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.UUID;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

class CrashHandler implements Thread.UncaughtExceptionHandler {

    private static final String LOGTAG = "GeckoCrashHandler";
    private static final Thread MAIN_THREAD = Thread.currentThread();
    private static final String DEFAULT_SERVER_URL =
        "https://crash-reports.mozilla.com/submit?id=%1$s&version=%2$s&buildid=%3$s";

    // Context for getting device information
    protected final Context appContext;
    // Thread that this handler applies to, or null for a global handler
    protected final Thread handlerThread;
    protected final Thread.UncaughtExceptionHandler systemUncaughtHandler;

    protected boolean crashing;
    protected boolean unregistered;

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
        this((Context) null);
    }

    /**
     * Create and register a CrashHandler for all threads and thread groups.
     *
     * @param appContext A Context for retrieving application information.
     */
    public CrashHandler(final Context appContext) {
        this.appContext = appContext;
        this.handlerThread = null;
        this.systemUncaughtHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(this);
    }

    /**
     * Create and register a CrashHandler for a particular thread.
     *
     * @param thread A thread to register the CrashHandler
     */
    public CrashHandler(final Thread thread) {
        this(thread, null);
    }

    /**
     * Create and register a CrashHandler for a particular thread.
     *
     * @param thread A thread to register the CrashHandler
     * @param appContext A Context for retrieving application information.
     */
    public CrashHandler(final Thread thread, final Context appContext) {
        this.appContext = appContext;
        this.handlerThread = thread;
        this.systemUncaughtHandler = thread.getUncaughtExceptionHandler();
        thread.setUncaughtExceptionHandler(this);
    }

    /**
     * Unregister this CrashHandler for exception handling.
     */
    public void unregister() {
        unregistered = true;

        // Restore the previous handler if we are still the topmost handler.
        // If not, we are part of a chain of handlers, and we cannot just restore the previous
        // handler, because that would replace whatever handler that's above us in the chain.

        if (handlerThread != null) {
            if (handlerThread.getUncaughtExceptionHandler() == this) {
                handlerThread.setUncaughtExceptionHandler(systemUncaughtHandler);
            }
        } else {
            if (Thread.getDefaultUncaughtExceptionHandler() == this) {
                Thread.setDefaultUncaughtExceptionHandler(systemUncaughtHandler);
            }
        }
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

    private static long getCrashTime() {
        return System.currentTimeMillis() / 1000;
    }

    private static long getStartupTime() {
        // Process start time is also the proc file modified time.
        final long uptimeMins = (new File("/proc/self/cmdline")).lastModified();
        if (uptimeMins == 0L) {
            return getCrashTime();
        }
        return uptimeMins / 1000;
    }

    private static String getJavaPackageName() {
        return CrashHandler.class.getPackage().getName();
    }

    protected String getAppPackageName() {
        if (appContext != null) {
            return appContext.getPackageName();
        }

        try {
            // Package name is also the command line string in most cases.
            final FileReader reader = new FileReader("/proc/self/cmdline");
            final char[] buffer = new char[64];
            try {
                if (reader.read(buffer) > 0) {
                    // cmdline is delimited by '\0', and we want the first token.
                    final int nul = Arrays.asList(buffer).indexOf('\0');
                    return (new String(buffer, 0, nul < 0 ? buffer.length : nul)).trim();
                }
            } finally {
                reader.close();
            }

        } catch (final IOException e) {
            Log.i(LOGTAG, "Error reading package name", e);
        }

        // Fallback to using CrashHandler's package name.
        return getJavaPackageName();
    }

    /**
     * Get the crash "extras" to be reported.
     *
     * @param thread The exception thread
     * @param exc An exception
     * @return "Extras" in the from of a Bundle
     */
    protected Bundle getCrashExtras(final Thread thread, final Throwable exc,
                                    final Context appContext) {
        final Bundle extras = new Bundle();
        final String pkgName = getAppPackageName();

        extras.putString("ProductName", pkgName);
        extras.putLong("CrashTime", getCrashTime());
        extras.putLong("StartupTime", getStartupTime());

        if (appContext != null) {
            final PackageManager pkgMgr = appContext.getPackageManager();
            try {
                final PackageInfo pkgInfo = pkgMgr.getPackageInfo(pkgName, 0);
                extras.putString("Version", pkgInfo.versionName);
                extras.putInt("BuildID", pkgInfo.versionCode);
                extras.putLong("InstallTime", pkgInfo.firstInstallTime / 1000);
            } catch (final PackageManager.NameNotFoundException e) {
                Log.i(LOGTAG, "Error getting package info", e);
            }
        }

        extras.putString("JavaStackTrace", getExceptionStackTrace(exc));
        return extras;
    }

    /**
     * Get the crash minidump content to be reported.
     *
     * @param thread The exception thread
     * @param exc An exception
     * @return Minidump content
     */
    protected byte[] getCrashDump(final Thread thread, final Throwable exc) {
        return new byte[0]; // No minidump.
    }

    protected static String normalizeUrlString(final String str) {
        if (str == null) {
            return "";
        }
        return Uri.encode(str);
    }

    /**
     * Get the server URL to send the crash report to.
     *
     * @param extras The crash extras Bundle
     */
    protected String getServerUrl(final Bundle extras) {
        return String.format(DEFAULT_SERVER_URL,
            normalizeUrlString(extras.getString("ProductID")),
            normalizeUrlString(extras.getString("Version")),
            normalizeUrlString(extras.getString("BuildID")));
    }

    /**
     * Launch the crash reporter activity that sends the crash report to the server.
     *
     * @param dumpFile Path for the minidump file
     * @param extraFile Path for the crash extra file
     * @return Whether the crash reporter was successfully launched
     */
    protected boolean launchCrashReporter(final String dumpFile, final String extraFile) {
        try {
            final String javaPkg = getJavaPackageName();
            final String pkg = getAppPackageName();
            final String component = javaPkg + ".CrashReporter";
            final String action = javaPkg + ".reportCrash";
            final ProcessBuilder pb;

            if (appContext != null) {
                final Intent intent = new Intent(action);
                intent.setComponent(new ComponentName(pkg, component));
                intent.putExtra("minidumpPath", dumpFile);
                appContext.startActivity(intent);
                return true;
            }

            // Avoid AppConstants dependency for SDK version constants,
            // because CrashHandler could be used outside of Fennec code.
            if (Build.VERSION.SDK_INT < 17) {
                pb = new ProcessBuilder(
                    "/system/bin/am", "start",
                    "-a", action,
                    "-n", pkg + '/' + component,
                    "--es", "minidumpPath", dumpFile);
            } else {
                pb = new ProcessBuilder(
                    "/system/bin/am", "start",
                    "--user", /* USER_CURRENT_OR_SELF */ "-3",
                    "-a", action,
                    "-n", pkg + '/' + component,
                    "--es", "minidumpPath", dumpFile);
            }

            pb.start().waitFor();

        } catch (final IOException e) {
            Log.e(LOGTAG, "Error launching crash reporter", e);
            return false;

        } catch (final InterruptedException e) {
            Log.i(LOGTAG, "Interrupted while waiting to launch crash reporter", e);
            // Fall-through
        }
        return true;
    }

    /**
     * Report an exception to Socorro.
     *
     * @param thread The exception thread
     * @param exc An exception
     * @return Whether the exception was successfully reported
     */
    protected boolean reportException(final Thread thread, final Throwable exc) {
        final String id = UUID.randomUUID().toString();

        // Use the cache directory under the app directory to store crash files.
        final File dir;
        if (appContext != null) {
            dir = appContext.getCacheDir();
        } else {
            dir = new File("/data/data/" + getAppPackageName() + "/cache");
        }

        dir.mkdirs();
        if (!dir.exists()) {
            return false;
        }

        final File dmpFile = new File(dir, id + ".dmp");
        final File extraFile = new File(dir, id + ".extra");

        try {
            // Write out minidump file as binary.

            final byte[] minidump = getCrashDump(thread, exc);
            final FileOutputStream dmpStream = new FileOutputStream(dmpFile);
            try {
                dmpStream.write(minidump);
            } finally {
                dmpStream.close();
            }

        } catch (final IOException e) {
            Log.e(LOGTAG, "Error writing minidump file", e);
            return false;
        }

        try {
            // Write out crash extra file as text.

            final Bundle extras = getCrashExtras(thread, exc, appContext);
            final String url = getServerUrl(extras);
            extras.putString("ServerURL", url);

            final BufferedWriter extraWriter = new BufferedWriter(new FileWriter(extraFile));
            try {
                for (String key : extras.keySet()) {
                    // Each extra line is in the format, key=value, with newlines escaped.
                    extraWriter.write(key);
                    extraWriter.write('=');
                    extraWriter.write(String.valueOf(extras.get(key)).replace("\n", "\\n"));
                    extraWriter.write('\n');
                }
            } finally {
                extraWriter.close();
            }

        } catch (final IOException e) {
            Log.e(LOGTAG, "Error writing extra file", e);
            return false;
        }

        return launchCrashReporter(dmpFile.getAbsolutePath(), extraFile.getAbsolutePath());
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
            if (!this.unregistered) {
                // Only process crash ourselves if we have not been unregistered.

                this.crashing = true;
                exc = getRootException(exc);
                logException(thread, exc);

                if (reportException(thread, exc)) {
                    // Reporting succeeded; we can terminate our process now.
                    return;
                }
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
