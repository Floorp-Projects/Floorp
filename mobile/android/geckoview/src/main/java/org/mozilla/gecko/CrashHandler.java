/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.geckoview.BuildConfig;
import org.mozilla.geckoview.GeckoRuntime;

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

public class CrashHandler implements Thread.UncaughtExceptionHandler {

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

    protected final Class<? extends Service> handlerService;

    /**
     * Get the root exception from the 'cause' chain of an exception.
     *
     * @param exc An exception
     * @return The root exception
     */
    public static Throwable getRootException(final Throwable exc) {
        Throwable cause;
        Throwable result = exc;
        for (cause = exc; cause != null; cause = cause.getCause()) {
            result = cause;
        }

        return result;
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
    public CrashHandler(final Class<? extends Service> handlerService) {
        this((Context) null, handlerService);
    }

    /**
     * Create and register a CrashHandler for all threads and thread groups.
     *
     * @param appContext A Context for retrieving application information.
     */
    public CrashHandler(final Context appContext, final Class<? extends Service> handlerService) {
        this.appContext = appContext;
        this.handlerThread = null;
        this.handlerService = handlerService;
        this.systemUncaughtHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(this);
    }

    /**
     * Create and register a CrashHandler for a particular thread.
     *
     * @param thread A thread to register the CrashHandler
     */
    public CrashHandler(final Thread thread, final Class<? extends Service> handlerService) {
        this(thread, null, handlerService);
    }

    /**
     * Create and register a CrashHandler for a particular thread.
     *
     * @param thread A thread to register the CrashHandler
     * @param appContext A Context for retrieving application information.
     */
    public CrashHandler(final Thread thread, final Context appContext,
                        final Class<? extends Service> handlerService) {
        this.appContext = appContext;
        this.handlerThread = thread;
        this.handlerService = handlerService;
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
    public static void logException(final Thread thread, final Throwable exc) {
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

    private static String getProcessName() {
        try {
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
        }

        return null;
    }

    protected String getAppPackageName() {
        final Context context = getAppContext();

        if (context != null) {
            return context.getPackageName();
        }

        // Package name is also the process name in most cases.
        String processName = getProcessName();
        if (processName != null) {
            return processName;
        }

        // Fallback to using CrashHandler's package name.
        return getJavaPackageName();
    }

    protected Context getAppContext() {
        return appContext;
    }

    /**
     * Get the crash "extras" to be reported.
     *
     * @param thread The exception thread
     * @param exc An exception
     * @return "Extras" in the from of a Bundle
     */
    protected Bundle getCrashExtras(final Thread thread, final Throwable exc) {
        final Context context = getAppContext();
        final Bundle extras = new Bundle();
        final String pkgName = getAppPackageName();

        extras.putLong("CrashTime", getCrashTime());
        extras.putLong("StartupTime", getStartupTime());
        extras.putString("Android_ProcessName", getProcessName());
        extras.putString("Android_PackageName", pkgName);

        if (context != null) {
            final PackageManager pkgMgr = context.getPackageManager();
            try {
                final PackageInfo pkgInfo = pkgMgr.getPackageInfo(pkgName, 0);
                extras.putString("Version", pkgInfo.versionName);
                extras.putInt("BuildID", pkgInfo.versionCode);
                extras.putLong("InstallTime", pkgInfo.lastUpdateTime / 1000);
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
            final Context context = getAppContext();
            final ProcessBuilder pb;

            if (handlerService == null) {
                Log.w(LOGTAG, "No crash handler service defined, unable to report crash");
                return false;
            }

            if (context != null) {
                final Intent intent = new Intent(GeckoRuntime.ACTION_CRASHED);
                intent.putExtra(GeckoRuntime.EXTRA_MINIDUMP_PATH, dumpFile);
                intent.putExtra(GeckoRuntime.EXTRA_EXTRAS_PATH, extraFile);
                intent.putExtra(GeckoRuntime.EXTRA_CRASH_FATAL, true);
                intent.setClass(context, handlerService);

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(intent);
                } else {
                    context.startService(intent);
                }
                return true;
            }

            final int deviceSdkVersion = Build.VERSION.SDK_INT;
            if (deviceSdkVersion < 17) {
                pb = new ProcessBuilder(
                        "/system/bin/am",
                        "startservice",
                        "-a", GeckoRuntime.ACTION_CRASHED,
                        "-n", getAppPackageName() + '/' + handlerService.getName(),
                        "--es", GeckoRuntime.EXTRA_MINIDUMP_PATH, dumpFile,
                        "--es", GeckoRuntime.EXTRA_EXTRAS_PATH, extraFile,
                        "--ez", GeckoRuntime.EXTRA_CRASH_FATAL, "true");
            } else {
                final String startServiceCommand;
                if (deviceSdkVersion >= 26) {
                    startServiceCommand = "start-foreground-service";
                } else {
                    startServiceCommand = "startservice";
                }

                pb = new ProcessBuilder(
                        "/system/bin/am",
                        startServiceCommand,
                        "--user", /* USER_CURRENT_OR_SELF */ "-3",
                        "-a", GeckoRuntime.ACTION_CRASHED,
                        "-n", getAppPackageName() + '/' + handlerService.getName(),
                        "--es", GeckoRuntime.EXTRA_MINIDUMP_PATH, dumpFile,
                        "--es", GeckoRuntime.EXTRA_EXTRAS_PATH, extraFile,
                        "--ez", GeckoRuntime.EXTRA_CRASH_FATAL, "true");
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
    @SuppressLint("SdCardPath")
    protected boolean reportException(final Thread thread, final Throwable exc) {
        final Context context = getAppContext();
        final String id = UUID.randomUUID().toString();

        // Use the cache directory under the app directory to store crash files.
        final File dir;
        if (context != null) {
            dir = context.getCacheDir();
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

            final Bundle extras = getCrashExtras(thread, exc);
            final String url = getServerUrl(extras);
            extras.putString("ServerURL", url);

            JSONObject json = new JSONObject();
            for (String key : extras.keySet()) {
                json.put(key, extras.get(key));
            }

            final BufferedWriter extraWriter = new BufferedWriter(new FileWriter(extraFile));
            try {
                extraWriter.write(json.toString());
            } finally {
                extraWriter.close();
            }
        } catch (final IOException | JSONException e) {
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
    public void uncaughtException(final Thread thread, final Throwable exc) {
        if (this.crashing) {
            // Prevent possible infinite recusions.
            return;
        }

        Thread resolvedThread = thread;
        if (resolvedThread == null) {
            // Gecko may pass in null for thread to denote the current thread.
            resolvedThread = Thread.currentThread();
        }

        try {
            Throwable rootException = exc;
            if (!this.unregistered) {
                // Only process crash ourselves if we have not been unregistered.

                this.crashing = true;
                rootException = getRootException(exc);
                logException(resolvedThread, rootException);

                if (reportException(resolvedThread, rootException)) {
                    // Reporting succeeded; we can terminate our process now.
                    return;
                }
            }

            if (systemUncaughtHandler != null) {
                // Follow the chain of uncaught handlers.
                systemUncaughtHandler.uncaughtException(resolvedThread, rootException);
            }
        } finally {
            terminateProcess();
        }
    }

    public static CrashHandler createDefaultCrashHandler(final Context context) {
        return new CrashHandler(context, null) {
            @Override
            protected Bundle getCrashExtras(final Thread thread, final Throwable exc) {
                final Bundle extras = super.getCrashExtras(thread, exc);

                extras.putString("ProductName", BuildConfig.MOZ_APP_BASENAME);
                extras.putString("ProductID", BuildConfig.MOZ_APP_ID);
                extras.putString("Version", BuildConfig.MOZ_APP_VERSION);
                extras.putString("BuildID", BuildConfig.MOZ_APP_BUILDID);
                extras.putString("Vendor", BuildConfig.MOZ_APP_VENDOR);
                extras.putString("ReleaseChannel", BuildConfig.MOZ_UPDATE_CHANNEL);
                return extras;
            }

            @Override
            public boolean reportException(final Thread thread, final Throwable exc) {
                if (BuildConfig.MOZ_CRASHREPORTER && BuildConfig.MOZILLA_OFFICIAL) {
                    // Only use Java crash reporter if enabled on official build.
                    return super.reportException(thread, exc);
                }
                return false;
            }
        };
    }
}
