/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.NumberFormat;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Environment;
import java.util.ArrayList;
import android.util.Log;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.geckoview.BuildConfig;

public final class GeckoLoader {
    private static final String LOGTAG = "GeckoLoader";

    private static volatile SafeIntent sIntent;
    private static File sCacheFile;
    private static File sGREDir;

    /* Synchronized on GeckoLoader.class. */
    private static boolean sSQLiteLibsLoaded;
    private static boolean sNSSLibsLoaded;
    private static boolean sMozGlueLoaded;
    private static String[] sEnvList;

    private GeckoLoader() {
        // prevent instantiation
    }

    public static File getCacheDir(Context context) {
        if (sCacheFile == null) {
            sCacheFile = context.getCacheDir();
        }
        return sCacheFile;
    }

    public static File getGREDir(Context context) {
        if (sGREDir == null) {
            sGREDir = new File(context.getApplicationInfo().dataDir);
        }
        return sGREDir;
    }

    private static void setupPluginEnvironment(Context context, String[] pluginDirs) {
        // setup plugin path directories
        try {
            // Check to see if plugins were blocked.
            if (pluginDirs == null) {
                putenv("MOZ_PLUGINS_BLOCKED=1");
                putenv("MOZ_PLUGIN_PATH=");
                return;
            }

            StringBuilder pluginSearchPath = new StringBuilder();
            for (int i = 0; i < pluginDirs.length; i++) {
                pluginSearchPath.append(pluginDirs[i]);
                pluginSearchPath.append(":");
            }
            putenv("MOZ_PLUGIN_PATH=" + pluginSearchPath);

            File pluginDataDir = context.getDir("plugins", 0);
            putenv("ANDROID_PLUGIN_DATADIR=" + pluginDataDir.getPath());

            File pluginPrivateDataDir = context.getDir("plugins_private", 0);
            putenv("ANDROID_PLUGIN_DATADIR_PRIVATE=" + pluginPrivateDataDir.getPath());

        } catch (Exception ex) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", ex);
        }
    }

    private static void setupDownloadEnvironment(final Context context) {
        try {
            File downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
            File updatesDir  = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
            if (downloadDir == null) {
                downloadDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");
            }
            if (updatesDir == null) {
                updatesDir = downloadDir;
            }
            putenv("DOWNLOADS_DIRECTORY=" + downloadDir.getPath());
            putenv("UPDATES_DIRECTORY="   + updatesDir.getPath());
        } catch (Exception e) {
            Log.w(LOGTAG, "No download directory found.", e);
        }
    }

    private static void delTree(File file) {
        if (file.isDirectory()) {
            File children[] = file.listFiles();
            for (File child : children) {
                delTree(child);
            }
        }
        file.delete();
    }

    private static File getTmpDir(Context context) {
        File tmpDir = context.getDir("tmpdir", Context.MODE_PRIVATE);
        // check if the old tmp dir is there
        File oldDir = new File(tmpDir.getParentFile(), "app_tmp");
        if (oldDir.exists()) {
            delTree(oldDir);
        }
        return tmpDir;
    }

    public static void setLastIntent(SafeIntent intent) {
        sIntent = intent;
    }

    public static void addEnvironmentToIntent(Intent intent) {
        if (sEnvList != null) {
            for (int ix = 0; ix < sEnvList.length; ix++) {
                intent.putExtra("env" + ix, sEnvList[ix]);
            }
        }
    }

    public static void setupGeckoEnvironment(Context context, String[] pluginDirs, String profilePath) {
        // if we have an intent (we're being launched by an activity)
        // read in any environmental variables from it here
        final SafeIntent intent = sIntent;
        if (intent != null) {
            final ArrayList<String> envList = new ArrayList<String>();
            String env = intent.getStringExtra("env0");
            Log.d(LOGTAG, "Gecko environment env0: " + env);
            for (int c = 1; env != null; c++) {
                envList.add(env);
                putenv(env);
                env = intent.getStringExtra("env" + c);
                Log.d(LOGTAG, "env" + c + ": " + env);
            }
            if (envList.size() > 0) {
              sEnvList = envList.toArray(new String[envList.size()]);
            }
        }

        putenv("MOZ_ANDROID_PACKAGE_NAME=" + context.getPackageName());

        setupPluginEnvironment(context, pluginDirs);
        setupDownloadEnvironment(context);

        // profile home path
        putenv("HOME=" + profilePath);

        // setup the tmp path
        File f = getTmpDir(context);
        if (!f.exists()) {
            f.mkdirs();
        }
        putenv("TMPDIR=" + f.getPath());

        // setup the downloads path
        f = Environment.getDownloadCacheDirectory();
        putenv("EXTERNAL_STORAGE=" + f.getPath());

        // setup the app-specific cache path
        f = context.getCacheDir();
        putenv("CACHE_DIRECTORY=" + f.getPath());

        if (Build.VERSION.SDK_INT >= 17) {
            android.os.UserManager um = (android.os.UserManager)context.getSystemService(Context.USER_SERVICE);
            if (um != null) {
                putenv("MOZ_ANDROID_USER_SERIAL_NUMBER=" + um.getSerialNumberForUser(android.os.Process.myUserHandle()));
            } else {
                Log.d(LOGTAG, "Unable to obtain user manager service on a device with SDK version " + Build.VERSION.SDK_INT);
            }
        }
        setupLocaleEnvironment();

        // We don't need this any more.
        sIntent = null;
    }

    private static void loadLibsSetupLocked(Context context) {
        // The package data lib directory isn't placed in ld.so's
        // search path, so we have to manually load libraries that
        // libxul will depend on.  Not ideal.

        File cacheFile = getCacheDir(context);
        putenv("GRE_HOME=" + getGREDir(context).getPath());

        // setup the libs cache
        String linkerCache = System.getenv("MOZ_LINKER_CACHE");
        if (linkerCache == null) {
            linkerCache = cacheFile.getPath();
            putenv("MOZ_LINKER_CACHE=" + linkerCache);
        }

        // Disable on-demand decompression of the linker on devices where it
        // is known to cause crashes.
        String forced_ondemand = System.getenv("MOZ_LINKER_ONDEMAND");
        if (forced_ondemand == null) {
            if ("HTC".equals(android.os.Build.MANUFACTURER) &&
                "HTC Vision".equals(android.os.Build.MODEL)) {
                putenv("MOZ_LINKER_ONDEMAND=0");
            }
        }

        putenv("MOZ_LINKER_EXTRACT=1");
    }

    @RobocopTarget
    public synchronized static void loadSQLiteLibs(final Context context, final String apkName) {
        if (sSQLiteLibsLoaded) {
            return;
        }

        loadMozGlue(context);
        loadLibsSetupLocked(context);
        loadSQLiteLibsNative(apkName);
        sSQLiteLibsLoaded = true;
    }

    public synchronized static void loadNSSLibs(final Context context, final String apkName) {
        if (sNSSLibsLoaded) {
            return;
        }

        loadMozGlue(context);
        loadLibsSetupLocked(context);
        loadNSSLibsNative(apkName);
        sNSSLibsLoaded = true;
    }

    @SuppressWarnings("deprecation")
    private static final String getCPUABI() {
        return android.os.Build.CPU_ABI;
    }

    /**
     * Copy a library out of our APK.
     *
     * @param context a Context.
     * @param lib the name of the library; e.g., "mozglue".
     * @param outDir the output directory for the .so. No trailing slash.
     * @return true on success, false on failure.
     */
    private static boolean extractLibrary(final Context context, final String lib, final String outDir) {
        final String apkPath = context.getApplicationInfo().sourceDir;

        // Sanity check.
        if (!apkPath.endsWith(".apk")) {
            Log.w(LOGTAG, "sourceDir is not an APK.");
            return false;
        }

        // Try to extract the named library from the APK.
        File outDirFile = new File(outDir);
        if (!outDirFile.isDirectory()) {
            if (!outDirFile.mkdirs()) {
                Log.e(LOGTAG, "Couldn't create " + outDir);
                return false;
            }
        }

        if (Build.VERSION.SDK_INT >= 21) {
            String[] abis = Build.SUPPORTED_ABIS;
            for (String abi : abis) {
                if (tryLoadWithABI(lib, outDir, apkPath, abi)) {
                    return true;
                }
            }
            return false;
        } else {
            final String abi = getCPUABI();
            return tryLoadWithABI(lib, outDir, apkPath, abi);
        }
    }

    private static boolean tryLoadWithABI(String lib, String outDir, String apkPath, String abi) {
        try {
            final ZipFile zipFile = new ZipFile(new File(apkPath));
            try {
                final String libPath = "lib/" + abi + "/lib" + lib + ".so";
                final ZipEntry entry = zipFile.getEntry(libPath);
                if (entry == null) {
                    Log.w(LOGTAG, libPath + " not found in APK " + apkPath);
                    return false;
                }

                final InputStream in = zipFile.getInputStream(entry);
                try {
                    final String outPath = outDir + "/lib" + lib + ".so";
                    final FileOutputStream out = new FileOutputStream(outPath);
                    final byte[] bytes = new byte[1024];
                    int read;

                    Log.d(LOGTAG, "Copying " + libPath + " to " + outPath);
                    boolean failed = false;
                    try {
                        while ((read = in.read(bytes, 0, 1024)) != -1) {
                            out.write(bytes, 0, read);
                        }
                    } catch (Exception e) {
                        Log.w(LOGTAG, "Failing library copy.", e);
                        failed = true;
                    } finally {
                        out.close();
                    }

                    if (failed) {
                        // Delete the partial copy so we don't fail to load it.
                        // Don't bother to check the return value -- there's nothing
                        // we can do about a failure.
                        new File(outPath).delete();
                    } else {
                        // Mark the file as executable. This doesn't seem to be
                        // necessary for the loader, but it's the normal state of
                        // affairs.
                        Log.d(LOGTAG, "Marking " + outPath + " as executable.");
                        new File(outPath).setExecutable(true);
                    }

                    return !failed;
                } finally {
                    in.close();
                }
            } finally {
                zipFile.close();
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to extract lib from APK.", e);
            return false;
        }
    }

    private static String getLoadDiagnostics(final Context context, final String lib) {
        final String androidPackageName = context.getPackageName();

        final StringBuilder message = new StringBuilder("LOAD ");
        message.append(lib);

        // These might differ. If so, we know why the library won't load!
        message.append(": ABI: " + BuildConfig.MOZ_APP_ABI + ", " + getCPUABI());
        message.append(": Data: " + context.getApplicationInfo().dataDir);
        try {
            final boolean appLibExists = new File("/data/app-lib/" + androidPackageName + "/lib" + lib + ".so").exists();
            final boolean dataDataExists = new File("/data/data/" + androidPackageName + "/lib/lib" + lib + ".so").exists();
            message.append(", ax=" + appLibExists);
            message.append(", ddx=" + dataDataExists);
        } catch (Throwable e) {
            message.append(": ax/ddx fail, ");
        }

        try {
            final String dashOne = "/data/data/" + androidPackageName + "-1";
            final String dashTwo = "/data/data/" + androidPackageName + "-2";
            final boolean dashOneExists = new File(dashOne).exists();
            final boolean dashTwoExists = new File(dashTwo).exists();
            message.append(", -1x=" + dashOneExists);
            message.append(", -2x=" + dashTwoExists);
        } catch (Throwable e) {
            message.append(", dash fail, ");
        }

        try {
            if (Build.VERSION.SDK_INT >= 9) {
                final String nativeLibPath = context.getApplicationInfo().nativeLibraryDir;
                final boolean nativeLibDirExists = new File(nativeLibPath).exists();
                final boolean nativeLibLibExists = new File(nativeLibPath + "/lib" + lib + ".so").exists();

                message.append(", nativeLib: " + nativeLibPath);
                message.append(", dirx=" + nativeLibDirExists);
                message.append(", libx=" + nativeLibLibExists);
            } else {
                message.append(", <pre-9>");
            }
        } catch (Throwable e) {
            message.append(", nativeLib fail.");
        }

        return message.toString();
    }

    private static final boolean attemptLoad(final String path) {
        try {
            System.load(path);
            return true;
        } catch (Throwable e) {
            Log.wtf(LOGTAG, "Couldn't load " + path + ": " + e);
        }

        return false;
    }

    /**
     * The first two attempts at loading a library: directly, and
     * then using the app library path.
     *
     * Returns null or the cause exception.
     */
    private static final Throwable doLoadLibraryExpected(final Context context, final String lib) {
        try {
            // Attempt 1: the way that should work.
            System.loadLibrary(lib);
            return null;
        } catch (Throwable e) {
            Log.wtf(LOGTAG, "Couldn't load " + lib + ". Trying native library dir.");

            if (Build.VERSION.SDK_INT < 9) {
                // We can't use nativeLibraryDir.
                return e;
            }

            // Attempt 2: use nativeLibraryDir, which should also work.
            final String libDir = context.getApplicationInfo().nativeLibraryDir;
            final String libPath = libDir + "/lib" + lib + ".so";

            // Does it even exist?
            if (new File(libPath).exists()) {
                if (attemptLoad(libPath)) {
                    // Success!
                    return null;
                }
                Log.wtf(LOGTAG, "Library exists but couldn't load!");
            } else {
                Log.wtf(LOGTAG, "Library doesn't exist when it should.");
            }

            // We failed. Return the original cause.
            return e;
        }
    }

    public static void doLoadLibrary(final Context context, final String lib) {
        final Throwable e = doLoadLibraryExpected(context, lib);
        if (e == null) {
            // Success.
            return;
        }

        // If we're in a mismatched UID state (Bug 1042935 Comment 16) there's really
        // nothing we can do.
        if (Build.VERSION.SDK_INT >= 9) {
            final String nativeLibPath = context.getApplicationInfo().nativeLibraryDir;
            if (nativeLibPath.contains("mismatched_uid")) {
                throw new RuntimeException("Fatal: mismatched UID: cannot load.");
            }
        }

        // Attempt 3: try finding the path the pseudo-supported way using .dataDir.
        final String dataLibPath = context.getApplicationInfo().dataDir + "/lib/lib" + lib + ".so";
        if (attemptLoad(dataLibPath)) {
            return;
        }

        // Attempt 4: use /data/app-lib directly. This is a last-ditch effort.
        final String androidPackageName = context.getPackageName();
        if (attemptLoad("/data/app-lib/" + androidPackageName + "/lib" + lib + ".so")) {
            return;
        }

        // Attempt 5: even more optimistic.
        if (attemptLoad("/data/data/" + androidPackageName + "/lib/lib" + lib + ".so")) {
            return;
        }

        // Look in our files directory, copying from the APK first if necessary.
        final String filesLibDir = context.getFilesDir() + "/lib";
        final String filesLibPath = filesLibDir + "/lib" + lib + ".so";
        if (new File(filesLibPath).exists()) {
            if (attemptLoad(filesLibPath)) {
                return;
            }
        } else {
            // Try copying.
            if (extractLibrary(context, lib, filesLibDir)) {
                // Let's try it!
                if (attemptLoad(filesLibPath)) {
                    return;
                }
            }
        }

        // Give up loudly, leaking information to debug the failure.
        final String message = getLoadDiagnostics(context, lib);
        Log.e(LOGTAG, "Load diagnostics: " + message);

        // Throw the descriptive message, using the original library load
        // failure as the cause.
        throw new RuntimeException(message, e);
    }

    public synchronized static void loadMozGlue(final Context context) {
        if (sMozGlueLoaded) {
            return;
        }

        doLoadLibrary(context, "mozglue");
        sMozGlueLoaded = true;
    }

    public synchronized static void loadGeckoLibs(final Context context, final String apkName) {
        loadLibsSetupLocked(context);
        loadGeckoLibsNative(apkName);
    }

    private static void setupLocaleEnvironment() {
        putenv("LANG=" + Locale.getDefault().toString());
        NumberFormat nf = NumberFormat.getInstance();
        if (nf instanceof DecimalFormat) {
            DecimalFormat df = (DecimalFormat)nf;
            DecimalFormatSymbols dfs = df.getDecimalFormatSymbols();

            putenv("LOCALE_DECIMAL_POINT=" + dfs.getDecimalSeparator());
            putenv("LOCALE_THOUSANDS_SEP=" + dfs.getGroupingSeparator());
            putenv("LOCALE_GROUPING=" + (char)df.getGroupingSize());
        }
    }

    @SuppressWarnings("serial")
    public static class AbortException extends Exception {
        public AbortException(String msg) {
            super(msg);
        }
    }

    @JNITarget
    public static void abort(final String msg) {
        final Thread thread = Thread.currentThread();
        final Thread.UncaughtExceptionHandler uncaughtHandler =
            thread.getUncaughtExceptionHandler();
        if (uncaughtHandler != null) {
            uncaughtHandler.uncaughtException(thread, new AbortException(msg));
        }
    }

    // These methods are implemented in mozglue/android/nsGeckoUtils.cpp
    private static native void putenv(String map);

    // These methods are implemented in mozglue/android/APKOpen.cpp
    public static native void nativeRun(String[] args, int crashFd, int ipcFd);
    private static native void loadGeckoLibsNative(String apkName);
    private static native void loadSQLiteLibsNative(String apkName);
    private static native void loadNSSLibsNative(String apkName);
    public static native boolean neonCompatible();
    public static native void suppressCrashDialog();
}
