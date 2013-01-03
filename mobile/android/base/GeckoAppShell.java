/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.GeckoLayerClient;
import org.mozilla.gecko.gfx.GfxInfoThread;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.InputConnectionHandler;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.TouchEventHandler;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoBackgroundThread;
import org.mozilla.gecko.util.GeckoEventListener;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationManager;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.StatFs;
import android.os.Vibrator;
import android.provider.Settings;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.HapticFeedbackConstants;
import android.view.Surface;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.webkit.MimeTypeMap;
import android.widget.Toast;
import android.text.TextUtils;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;
import java.net.URL;
import java.nio.ByteBuffer;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.NumberFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.NoSuchElementException;
import java.util.StringTokenizer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.SynchronousQueue;
import java.net.ProxySelector;
import java.net.Proxy;
import java.net.URI;

public class GeckoAppShell
{
    private static final String LOGTAG = "GeckoAppShell";

    // static members only
    private GeckoAppShell() { }

    static private LinkedList<GeckoEvent> gPendingEvents =
        new LinkedList<GeckoEvent>();

    static private boolean gRestartScheduled = false;

    static private GeckoEditableListener mEditableListener = null;

    static private final HashMap<Integer, AlertNotification>
        mAlertNotifications = new HashMap<Integer, AlertNotification>();

    /* Keep in sync with constants found here:
      http://mxr.mozilla.org/mozilla-central/source/uriloader/base/nsIWebProgressListener.idl
    */
    static public final int WPL_STATE_START = 0x00000001;
    static public final int WPL_STATE_STOP = 0x00000010;
    static public final int WPL_STATE_IS_DOCUMENT = 0x00020000;
    static public final int WPL_STATE_IS_NETWORK = 0x00040000;

    public static final String SHORTCUT_TYPE_WEBAPP = "webapp";
    public static final String SHORTCUT_TYPE_BOOKMARK = "bookmark";

    static private final boolean LOGGING = false;

    static private File sCacheFile = null;
    static private int sFreeSpace = -1;
    static File sHomeDir = null;
    static private int sDensityDpi = 0;
    private static Boolean sSQLiteLibsLoaded = false;
    private static Boolean sNSSLibsLoaded = false;
    private static Boolean sLibsSetup = false;
    private static File sGREDir = null;
    private static final EventDispatcher sEventDispatcher = new EventDispatcher();

    /* Default colors. */
    private static final float[] DEFAULT_LAUNCHER_ICON_HSV = { 32.0f, 1.0f, 1.0f };

    /* Is the value in sVibrationEndTime valid? */
    private static boolean sVibrationMaybePlaying = false;

    /* Time (in System.nanoTime() units) when the currently-playing vibration
     * is scheduled to end.  This value is valid only when
     * sVibrationMaybePlaying is true. */
    private static long sVibrationEndTime = 0;

    /* Default value of how fast we should hint the Android sensors. */
    private static int sDefaultSensorHint = 100;

    private static Sensor gAccelerometerSensor = null;
    private static Sensor gLinearAccelerometerSensor = null;
    private static Sensor gGyroscopeSensor = null;
    private static Sensor gOrientationSensor = null;
    private static Sensor gProximitySensor = null;
    private static Sensor gLightSensor = null;

    private static boolean mLocationHighAccuracy = false;

    private static Handler sGeckoHandler;

    public static GfxInfoThread sGfxInfoThread = null;

    static ActivityHandlerHelper sActivityHelper = new ActivityHandlerHelper();

    /* The Android-side API: API methods that Android calls */

    // Initialization methods
    public static native void nativeInit();
    public static native void nativeRun(String args);

    // helper methods
    //    public static native void setSurfaceView(GeckoSurfaceView sv);
    public static native void setLayerClient(GeckoLayerClient client);
    public static native void putenv(String map);
    public static native void onResume();
    public static native void onLowMemory();
    public static native void callObserver(String observerKey, String topic, String data);
    public static native void removeObserver(String observerKey);
    public static native void loadGeckoLibsNative(String apkName);
    public static native void loadSQLiteLibsNative(String apkName, boolean shouldExtract);
    public static native void loadNSSLibsNative(String apkName, boolean shouldExtract);
    public static native void onChangeNetworkLinkStatus(String status);
    public static native Message getNextMessageFromQueue(MessageQueue queue);
    public static native void onSurfaceTextureFrameAvailable(Object surfaceTexture, int id);

    public static void registerGlobalExceptionHandler() {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            public void uncaughtException(Thread thread, Throwable e) {
                Log.e(LOGTAG, ">>> REPORTING UNCAUGHT EXCEPTION FROM THREAD "
                              + thread.getId() + " (\"" + thread.getName() + "\")", e);

                // If the uncaught exception was rethrown, walk the exception `cause` chain to find
                // the original exception so Socorro can correctly collate related crash reports.
                Throwable cause;
                while ((cause = e.getCause()) != null) {
                    e = cause;
                }

                if (e instanceof java.lang.OutOfMemoryError) {
                    SharedPreferences prefs =
                        GeckoApp.mAppContext.getSharedPreferences(GeckoApp.PREFS_NAME, 0);
                    SharedPreferences.Editor editor = prefs.edit();
                    editor.putBoolean(GeckoApp.PREFS_OOM_EXCEPTION, true);
                    editor.commit();
                }

                reportJavaCrash(getStackTraceString(e));
            }
        });
    }

    private static String getStackTraceString(Throwable e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        pw.flush();
        return sw.toString();
    }

    private static native void reportJavaCrash(String stackTrace);

    public static void notifyUriVisited(String uri) {
        sendEventToGecko(GeckoEvent.createVisitedEvent(uri));
    }

    public static native void processNextNativeEvent();

    public static native void notifyBatteryChange(double aLevel, boolean aCharging, double aRemainingTime);

    public static native void notifySmsReceived(String aSender, String aBody, int aMessageClass, long aTimestamp);
    public static native void notifySmsSent(int aId, String aReceiver, String aBody, long aTimestamp, int aRequestId);
    public static native void notifySmsDelivery(int aId, int aDeliveryStatus, String aReceiver, String aBody, long aTimestamp);
    public static native void notifySmsSendFailed(int aError, int aRequestId);
    public static native void notifyGetSms(int aId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
    public static native void notifyGetSmsFailed(int aError, int aRequestId);
    public static native void notifySmsDeleted(boolean aDeleted, int aRequestId);
    public static native void notifySmsDeleteFailed(int aError, int aRequestId);
    public static native void notifyNoMessageInList(int aRequestId);
    public static native void notifyListCreated(int aListId, int aMessageId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
    public static native void notifyGotNextMessage(int aMessageId, int aDeliveryStatus, String aReceiver, String aSender, String aBody, long aTimestamp, int aRequestId);
    public static native void notifyReadingMessageListFailed(int aError, int aRequestId);

    public static native void scheduleComposite();
    public static native void schedulePauseComposition();
    public static native void scheduleResumeComposition(int width, int height);
    public static native float computeRenderIntegrity();

    public static native SurfaceBits getSurfaceBits(Surface surface);

    public static native void onFullScreenPluginHidden(View view);

    private static class GeckoMediaScannerClient implements MediaScannerConnectionClient {
        private String mFile = "";
        private String mMimeType = "";
        private MediaScannerConnection mScanner = null;

        public GeckoMediaScannerClient(Context aContext, String aFile, String aMimeType) {
            mFile = aFile;
            mMimeType = aMimeType;
            mScanner = new MediaScannerConnection(aContext, this);
            if (mScanner != null)
                mScanner.connect();
        }

        public void onMediaScannerConnected() {
            mScanner.scanFile(mFile, mMimeType);
        }

        public void onScanCompleted(String path, Uri uri) {
            if(path.equals(mFile)) {
                mScanner.disconnect();
                mScanner = null;
            }
        }
    }

    // Get a Handler for the main java thread
    public static Handler getMainHandler() {
        return GeckoApp.mAppContext.mMainHandler;
    }

    public static Handler getGeckoHandler() {
        return sGeckoHandler;
    }

    public static Handler getHandler() {
        return GeckoBackgroundThread.getHandler();
    }

    public static File getCacheDir(Context context) {
        if (sCacheFile == null)
            sCacheFile = context.getCacheDir();
        return sCacheFile;
    }

    public static long getFreeSpace(Context context) {
        try {
            if (sFreeSpace == -1) {
                File cacheDir = getCacheDir(context);
                if (cacheDir != null) {
                    StatFs cacheStats = new StatFs(cacheDir.getPath());
                    sFreeSpace = cacheStats.getFreeBlocks() *
                        cacheStats.getBlockSize();
                } else {
                    Log.w(LOGTAG, "Unable to get cache dir.");
                }
            }
        } catch (Exception e) {
            Log.w(LOGTAG, "Caught exception while stating cache dir.", e);
        }
        return sFreeSpace;
    }

    public static File getGREDir(Context context) {
        if (sGREDir == null)
            sGREDir = new File(context.getApplicationInfo().dataDir);
        return sGREDir;
    }

    // java-side stuff
    public static void loadLibsSetup(Context context) {
        if (sLibsSetup)
            return;

        // The package data lib directory isn't placed in ld.so's
        // search path, so we have to manually load libraries that
        // libxul will depend on.  Not ideal.
        GeckoProfile profile = GeckoProfile.get(context);

        File cacheFile = getCacheDir(context);
        putenv("GRE_HOME=" + getGREDir(context).getPath());

        // setup the libs cache
        String linkerCache = System.getenv("MOZ_LINKER_CACHE");
        if (linkerCache == null) {
            linkerCache = cacheFile.getPath();
            GeckoAppShell.putenv("MOZ_LINKER_CACHE=" + linkerCache);
        }

        if (GeckoApp.mAppContext != null &&
            GeckoApp.mAppContext.linkerExtract()) {
            GeckoAppShell.putenv("MOZ_LINKER_EXTRACT=1");
            // Ensure that the cache dir is world-writable
            File cacheDir = new File(linkerCache);
            if (cacheDir.isDirectory()) {
                cacheDir.setWritable(true, false);
                cacheDir.setExecutable(true, false);
                cacheDir.setReadable(true, false);
            }
        }

        sLibsSetup = true;
    }

    private static void setupPluginEnvironment(GeckoApp context) {
        // setup plugin path directories
        try {
            String[] dirs = context.getPluginDirectories();
            // Check to see if plugins were blocked.
            if (dirs == null) {
                GeckoAppShell.putenv("MOZ_PLUGINS_BLOCKED=1");
                GeckoAppShell.putenv("MOZ_PLUGIN_PATH=");
                return;
            }

            StringBuffer pluginSearchPath = new StringBuffer();
            for (int i = 0; i < dirs.length; i++) {
                pluginSearchPath.append(dirs[i]);
                pluginSearchPath.append(":");
            }
            GeckoAppShell.putenv("MOZ_PLUGIN_PATH="+pluginSearchPath);

            File pluginDataDir = context.getDir("plugins", 0);
            GeckoAppShell.putenv("ANDROID_PLUGIN_DATADIR=" + pluginDataDir.getPath());

        } catch (Exception ex) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", ex);
        }
    }

    private static void setupDownloadEnvironment(GeckoApp context) {
        try {
            File downloadDir = null;
            File updatesDir  = null;
            if (Build.VERSION.SDK_INT >= 8) {
                downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
                updatesDir  = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
            }
            if (downloadDir == null) {
                downloadDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");
            }
            if (updatesDir == null) {
                updatesDir = downloadDir;
            }
            GeckoAppShell.putenv("DOWNLOADS_DIRECTORY=" + downloadDir.getPath());
            GeckoAppShell.putenv("UPDATES_DIRECTORY="   + updatesDir.getPath());
        }
        catch (Exception e) {
            Log.w(LOGTAG, "No download directory found.", e);
        }
    }

    public static void setupGeckoEnvironment(Context context) {
        GeckoProfile profile = GeckoProfile.get(context);

        setupPluginEnvironment((GeckoApp) context);
        setupDownloadEnvironment((GeckoApp) context);

        // profile home path
        GeckoAppShell.putenv("HOME=" + profile.getFilesDir().getPath());

        // setup the tmp path
        File f = context.getDir("tmp", Context.MODE_WORLD_READABLE |
                                 Context.MODE_WORLD_WRITEABLE );
        if (!f.exists())
            f.mkdirs();
        GeckoAppShell.putenv("TMPDIR=" + f.getPath());

        // setup the downloads path
        f = Environment.getDownloadCacheDirectory();
        GeckoAppShell.putenv("EXTERNAL_STORAGE=" + f.getPath());

        // setup the app-specific cache path
        f = context.getCacheDir();
        GeckoAppShell.putenv("CACHE_DIRECTORY=" + f.getPath());

        /* We really want to use this code, but it requires bumping up the SDK to 17 so for now
           we will use reflection. See https://bugzilla.mozilla.org/show_bug.cgi?id=811763#c11
        
        if (Build.VERSION.SDK_INT >= 17) {
            android.os.UserManager um = (android.os.UserManager)context.getSystemService(Context.USER_SERVICE);
            if (um != null) {
                GeckoAppShell.putenv("MOZ_ANDROID_USER_SERIAL_NUMBER=" + um.getSerialNumberForUser(android.os.Process.myUserHandle()));
            } else {
                Log.d(LOGTAG, "Unable to obtain user manager service on a device with SDK version " + Build.VERSION.SDK_INT);
            }
        }
        */
        try {
            Object userManager = context.getSystemService("user");
            if (userManager != null) {
                // if userManager is non-null that means we're running on 4.2+ and so the rest of this
                // should just work
                Object userHandle = android.os.Process.class.getMethod("myUserHandle", (Class[])null).invoke(null);
                Object userSerial = userManager.getClass().getMethod("getSerialNumberForUser", userHandle.getClass()).invoke(userManager, userHandle);
                GeckoAppShell.putenv("MOZ_ANDROID_USER_SERIAL_NUMBER=" + userSerial.toString());
            }
        } catch (Exception e) {
            // Guard against any unexpected failures
            Log.d(LOGTAG, "Unable to set the user serial number", e);
        }

        putLocaleEnv();
    }

    /* This method is referenced by Robocop via reflection. */
    public static void loadSQLiteLibs(Context context, String apkName) {
        if (sSQLiteLibsLoaded)
            return;
        synchronized(sSQLiteLibsLoaded) {
            if (sSQLiteLibsLoaded)
                return;
            loadMozGlue(context);
            // the extract libs parameter is being removed in bug 732069
            loadLibsSetup(context);
            loadSQLiteLibsNative(apkName, false);
            sSQLiteLibsLoaded = true;
        }
    }

    public static void loadNSSLibs(Context context, String apkName) {
        if (sNSSLibsLoaded)
            return;
        synchronized(sNSSLibsLoaded) {
            if (sNSSLibsLoaded)
                return;
            loadMozGlue(context);
            loadLibsSetup(context);
            loadNSSLibsNative(apkName, false);
            sNSSLibsLoaded = true;
        }
    }

    public static void loadMozGlue(Context context) {
        System.loadLibrary("mozglue");

        // When running TestPasswordProvider, we're being called with
        // a GeckoApplication, which is not an Activity
        if (!(context instanceof Activity))
            return;

        Intent i = null;
        i = ((Activity)context).getIntent();

        // if we have an intent (we're being launched by an activity)
        // read in any environmental variables from it here
        String env = i.getStringExtra("env0");
        Log.d(LOGTAG, "Gecko environment env0: "+ env);
        for (int c = 1; env != null; c++) {
            GeckoAppShell.putenv(env);
            env = i.getStringExtra("env" + c);
            Log.d(LOGTAG, "env" + c + ": " + env);
        }
    }

    public static void loadGeckoLibs(String apkName) {
        loadLibsSetup(GeckoApp.mAppContext);
        loadGeckoLibsNative(apkName);
    }

    private static void putLocaleEnv() {
        GeckoAppShell.putenv("LANG=" + Locale.getDefault().toString());
        NumberFormat nf = NumberFormat.getInstance();
        if (nf instanceof DecimalFormat) {
            DecimalFormat df = (DecimalFormat)nf;
            DecimalFormatSymbols dfs = df.getDecimalFormatSymbols();

            GeckoAppShell.putenv("LOCALE_DECIMAL_POINT=" + dfs.getDecimalSeparator());
            GeckoAppShell.putenv("LOCALE_THOUSANDS_SEP=" + dfs.getGroupingSeparator());
            GeckoAppShell.putenv("LOCALE_GROUPING=" + (char)df.getGroupingSize());
        }
    }

    public static void runGecko(String apkPath, String args, String url, String type) {
        WebAppAllocator.getInstance();

        Looper.prepare();
        sGeckoHandler = new Handler();

        // run gecko -- it will spawn its own thread
        GeckoAppShell.nativeInit();

        // Tell Gecko where the target byte buffer is for rendering
        GeckoAppShell.setLayerClient(GeckoApp.mAppContext.getLayerView().getLayerClient());

        // First argument is the .apk path
        String combinedArgs = apkPath + " -greomni " + apkPath;
        if (args != null)
            combinedArgs += " " + args;
        if (url != null)
            combinedArgs += " -url " + url;
        if (type != null)
            combinedArgs += " " + type;

        DisplayMetrics metrics = GeckoApp.mAppContext.getResources().getDisplayMetrics();
        combinedArgs += " -width " + metrics.widthPixels + " -height " + metrics.heightPixels;

        GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                public void run() {
                    geckoLoaded();
                }
            });

        // and go
        GeckoAppShell.nativeRun(combinedArgs);
    }

    // Called on the UI thread after Gecko loads.
    private static void geckoLoaded() {
        GeckoEditable editable = new GeckoEditable();
        // install the gecko => editable listener
        mEditableListener = editable;
    }

    static void sendPendingEventsToGecko() {
        try {
            while (!gPendingEvents.isEmpty()) {
                GeckoEvent e = gPendingEvents.removeFirst();
                notifyGeckoOfEvent(e);
            }
        } catch (NoSuchElementException e) {}
    }

    /* This method is referenced by Robocop via reflection. */
    public static void sendEventToGecko(GeckoEvent e) {
        if (GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning)) {
            notifyGeckoOfEvent(e);
        } else {
            gPendingEvents.addLast(e);
        }
    }

    public static void sendEventToGeckoSync(GeckoEvent e) {
        sendEventToGecko(e);
        geckoEventSync();
    }

    // Tell the Gecko event loop that an event is available.
    public static native void notifyGeckoOfEvent(GeckoEvent event);

    /*
     *  The Gecko-side API: API methods that Gecko calls
     */
    public static void notifyIME(int type, int state) {
        if (mEditableListener != null) {
            mEditableListener.notifyIME(type, state);
        }
    }

    public static void notifyIMEEnabled(int state, String typeHint,
                                        String modeHint, String actionHint,
                                        boolean landscapeFS) {
        // notifyIMEEnabled() still needs the landscapeFS parameter
        // because it is called from JNI code that assumes it has the
        // same signature as XUL Fennec's (which does use landscapeFS).
        // Bug 807124 will eliminate the need for landscapeFS
        if (mEditableListener != null) {
            mEditableListener.notifyIMEEnabled(state, typeHint,
                                               modeHint, actionHint);
        }
    }

    public static void notifyIMEChange(String text, int start, int end, int newEnd) {
        if (newEnd < 0) { // Selection change
            mEditableListener.onSelectionChange(start, end);
        } else { // Text change
            mEditableListener.onTextChange(text, start, end, newEnd);
        }
    }

    private static CountDownLatch sGeckoPendingAcks = null;

    // Block the current thread until the Gecko event loop is caught up
    synchronized public static void geckoEventSync() {
        sGeckoPendingAcks = new CountDownLatch(1);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createSyncEvent());
        while (sGeckoPendingAcks.getCount() != 0) {
            try {
                sGeckoPendingAcks.await();
            } catch(InterruptedException e) {}
        }
        sGeckoPendingAcks = null;
    }

    // Signal the Java thread that it's time to wake up
    public static void acknowledgeEventSync() {
        CountDownLatch tmp = sGeckoPendingAcks;
        if (tmp != null)
            tmp.countDown();
    }

    public static void enableLocation(final boolean enable) {
        getMainHandler().post(new Runnable() { 
                public void run() {
                    LocationManager lm = (LocationManager)
                        GeckoApp.mAppContext.getSystemService(Context.LOCATION_SERVICE);

                    if (enable) {
                        Criteria criteria = new Criteria();
                        criteria.setSpeedRequired(false);
                        criteria.setBearingRequired(false);
                        criteria.setAltitudeRequired(false);
                        if (mLocationHighAccuracy) {
                            criteria.setAccuracy(Criteria.ACCURACY_FINE);
                            criteria.setCostAllowed(true);
                            criteria.setPowerRequirement(Criteria.POWER_HIGH);
                        } else {
                            criteria.setAccuracy(Criteria.ACCURACY_COARSE);
                            criteria.setCostAllowed(false);
                            criteria.setPowerRequirement(Criteria.POWER_LOW);
                        }

                        String provider = lm.getBestProvider(criteria, true);
                        if (provider == null)
                            return;

                        Looper l = Looper.getMainLooper();
                        Location loc = lm.getLastKnownLocation(provider);
                        if (loc != null) {
                            GeckoApp.mAppContext.onLocationChanged(loc);
                        }
                        lm.requestLocationUpdates(provider, 100, (float).5, GeckoApp.mAppContext, l);
                    } else {
                        lm.removeUpdates(GeckoApp.mAppContext);
                    }
                }
            });
    }

    public static void enableLocationHighAccuracy(final boolean enable) {
        mLocationHighAccuracy = enable;
    }

    public static void enableSensor(int aSensortype) {
        SensorManager sm = (SensorManager)
            GeckoApp.mAppContext.getSystemService(Context.SENSOR_SERVICE);

        switch(aSensortype) {
        case GeckoHalDefines.SENSOR_ORIENTATION:
            if(gOrientationSensor == null)
                gOrientationSensor = sm.getDefaultSensor(Sensor.TYPE_ORIENTATION);
            if (gOrientationSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gOrientationSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_ACCELERATION:
            if(gAccelerometerSensor == null)
                gAccelerometerSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            if (gAccelerometerSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gAccelerometerSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_PROXIMITY:
            if(gProximitySensor == null)
                gProximitySensor = sm.getDefaultSensor(Sensor.TYPE_PROXIMITY);
            if (gProximitySensor != null)
                sm.registerListener(GeckoApp.mAppContext, gProximitySensor, SensorManager.SENSOR_DELAY_NORMAL);
            break;

        case GeckoHalDefines.SENSOR_LIGHT:
            if(gLightSensor == null)
                gLightSensor = sm.getDefaultSensor(Sensor.TYPE_LIGHT);
            if (gLightSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gLightSensor, SensorManager.SENSOR_DELAY_NORMAL);
            break;

        case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
            if(gLinearAccelerometerSensor == null)
                gLinearAccelerometerSensor = sm.getDefaultSensor(10 /* API Level 9 - TYPE_LINEAR_ACCELERATION */);
            if (gLinearAccelerometerSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gLinearAccelerometerSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_GYROSCOPE:
            if(gGyroscopeSensor == null)
                gGyroscopeSensor = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            if (gGyroscopeSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gGyroscopeSensor, sDefaultSensorHint);
            break;
        default:
            Log.w(LOGTAG, "Error! Can't enable unknown SENSOR type " + aSensortype);
        }
    }

    public static void disableSensor(int aSensortype) {
        SensorManager sm = (SensorManager)
            GeckoApp.mAppContext.getSystemService(Context.SENSOR_SERVICE);

        switch (aSensortype) {
        case GeckoHalDefines.SENSOR_ORIENTATION:
            if (gOrientationSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gOrientationSensor);
            break;

        case GeckoHalDefines.SENSOR_ACCELERATION:
            if (gAccelerometerSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gAccelerometerSensor);
            break;

        case GeckoHalDefines.SENSOR_PROXIMITY:
            if (gProximitySensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gProximitySensor);
            break;

        case GeckoHalDefines.SENSOR_LIGHT:
            if (gLightSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gLightSensor);
            break;

        case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
            if (gLinearAccelerometerSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gLinearAccelerometerSensor);
            break;

        case GeckoHalDefines.SENSOR_GYROSCOPE:
            if (gGyroscopeSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gGyroscopeSensor);
            break;
        default:
            Log.w(LOGTAG, "Error! Can't disable unknown SENSOR type " + aSensortype);
        }
    }

    public static void moveTaskToBack() {
        GeckoApp.mAppContext.moveTaskToBack(true);
    }

    public static void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        // This method may be called from JNI to report Gecko's current selection indexes, but
        // Native Fennec doesn't care because the Java code already knows the selection indexes.
    }

    static void onXreExit() {
        // mLaunchState can only be Launched or GeckoRunning at this point
        GeckoApp.setLaunchState(GeckoApp.LaunchState.GeckoExiting);
        if (gRestartScheduled) {
            GeckoApp.mAppContext.doRestart();
        } else {
            GeckoApp.mAppContext.finish();
        }

        Log.d(LOGTAG, "Killing via System.exit()");
        System.exit(0);
    }

    static void scheduleRestart() {
        gRestartScheduled = true;
    }

    public static File installWebApp(String aTitle, String aURI, String aUniqueURI, String aIconURL) {
        int index = WebAppAllocator.getInstance(GeckoApp.mAppContext).findAndAllocateIndex(aUniqueURI, aTitle, aIconURL);
        GeckoProfile profile = GeckoProfile.get(GeckoApp.mAppContext, "webapp" + index);
        createShortcut(aTitle, aURI, aUniqueURI, aIconURL, "webapp");
        return profile.getDir();
    }

    public static Intent getWebAppIntent(String aURI, String aUniqueURI, String aTitle, Bitmap aIcon) {
        int index;

        if (aIcon != null && !TextUtils.isEmpty(aTitle))
            index = WebAppAllocator.getInstance(GeckoApp.mAppContext).findAndAllocateIndex(aUniqueURI, aTitle, aIcon);
        else
            index = WebAppAllocator.getInstance(GeckoApp.mAppContext).getIndexForApp(aUniqueURI);

        if (index == -1)
            return null;

        return getWebAppIntent(index, aURI);
    }

    public static Intent getWebAppIntent(int aIndex, String aURI) {
        Intent intent = new Intent();
        intent.setAction(GeckoApp.ACTION_WEBAPP_PREFIX + aIndex);
        intent.setData(Uri.parse(aURI));
        intent.setClassName(GeckoApp.mAppContext, GeckoApp.mAppContext.getPackageName() + ".WebApps$WebApp" + aIndex);
        return intent;
    }

    // "Installs" an application by creating a shortcut
    // This is the entry point from AndroidBridge.h
    static void createShortcut(String aTitle, String aURI, String aIconData, String aType) {
        if ("webapp".equals(aType)) {
            Log.w(LOGTAG, "createShortcut with no unique URI should not be used for aType = webapp!");
        }

        createShortcut(aTitle, aURI, aURI, aIconData, aType);
    }

    // internal, for non-webapps
    static void createShortcut(String aTitle, String aURI, Bitmap aBitmap, String aType) {
        createShortcut(aTitle, aURI, aURI, aBitmap, aType);
    }

    // internal, for webapps
    static void createShortcut(String aTitle, String aURI, String aUniqueURI, String aIconData, String aType) {
        createShortcut(aTitle, aURI, aUniqueURI, BitmapUtils.getBitmapFromDataURI(aIconData), aType);
    }

    public static void createShortcut(final String aTitle, final String aURI, final String aUniqueURI,
                                      final Bitmap aIcon, final String aType)
    {
        getHandler().post(new Runnable() {
            public void run() {
                // the intent to be launched by the shortcut
                Intent shortcutIntent;
                if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP)) {
                    shortcutIntent = getWebAppIntent(aURI, aUniqueURI, aTitle, aIcon);
                } else {
                    shortcutIntent = new Intent();
                    shortcutIntent.setAction(GeckoApp.ACTION_BOOKMARK);
                    shortcutIntent.setData(Uri.parse(aURI));
                    shortcutIntent.setClassName(GeckoApp.mAppContext,
                                                GeckoApp.mAppContext.getPackageName() + ".App");
                }
        
                Intent intent = new Intent();
                intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
                if (aTitle != null)
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
                else
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);
                intent.putExtra(Intent.EXTRA_SHORTCUT_ICON, getLauncherIcon(aIcon, aType));

                // Do not allow duplicate items
                intent.putExtra("duplicate", false);

                intent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
                GeckoApp.mAppContext.sendBroadcast(intent);
            }
        });
    }

    public static void removeShortcut(final String aTitle, final String aURI, final String aType) {
        removeShortcut(aTitle, aURI, null, aType);
    }

    public static void removeShortcut(final String aTitle, final String aURI, final String aUniqueURI, final String aType) {
        getHandler().post(new Runnable() {
            public void run() {
                // the intent to be launched by the shortcut
                Intent shortcutIntent;
                if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP)) {
                    int index = WebAppAllocator.getInstance(GeckoApp.mAppContext).getIndexForApp(aUniqueURI);
                    shortcutIntent = getWebAppIntent(aURI, aUniqueURI, "", null);
                    if (shortcutIntent == null)
                        return;
                } else {
                    shortcutIntent = new Intent();
                    shortcutIntent.setAction(GeckoApp.ACTION_BOOKMARK);
                    shortcutIntent.setClassName(GeckoApp.mAppContext,
                                                GeckoApp.mAppContext.getPackageName() + ".App");
                    shortcutIntent.setData(Uri.parse(aURI));
                }
        
                Intent intent = new Intent();
                intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
                if (aTitle != null)
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
                else
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);

                intent.setAction("com.android.launcher.action.UNINSTALL_SHORTCUT");
                GeckoApp.mAppContext.sendBroadcast(intent);
            }
        });
    }

    public static void uninstallWebApp(final String uniqueURI) {
        // On uninstall, we need to do a couple of things:
        //   1. nuke the running app process.
        //   2. nuke the profile that was assigned to that webapp
        getHandler().post(new Runnable() {
            public void run() {
                int index = WebAppAllocator.getInstance(GeckoApp.mAppContext).releaseIndexForApp(uniqueURI);

                // if -1, nothing to do; we didn't think it was installed anyway
                if (index == -1)
                    return;

                // kill the app if it's running
                String targetProcessName = GeckoApp.mAppContext.getPackageName();
                targetProcessName = targetProcessName + ":" + targetProcessName + ".WebApp" + index;

                ActivityManager am = (ActivityManager) GeckoApp.mAppContext.getSystemService(Context.ACTIVITY_SERVICE);
                List<ActivityManager.RunningAppProcessInfo> procs = am.getRunningAppProcesses();
                if (procs != null) {
                    for (ActivityManager.RunningAppProcessInfo proc : procs) {
                        if (proc.processName.equals(targetProcessName)) {
                            android.os.Process.killProcess(proc.pid);
                            break;
                        }
                    }
                }

                // then nuke the profile
                GeckoProfile.removeProfile(GeckoApp.mAppContext, "webapp" + index);
            }
        });
    }

    static public int getPreferredIconSize() {
        if (android.os.Build.VERSION.SDK_INT >= 11) {
            ActivityManager am = (ActivityManager)GeckoApp.mAppContext.getSystemService(Context.ACTIVITY_SERVICE);
            return am.getLauncherLargeIconSize();
        } else {
            switch (getDpi()) {
                case DisplayMetrics.DENSITY_MEDIUM:
                    return 48;
                case DisplayMetrics.DENSITY_XHIGH:
                    return 96;
                case DisplayMetrics.DENSITY_HIGH:
                default:
                    return 72;
            }
        }
    }

    static private Bitmap getLauncherIcon(Bitmap aSource, String aType) {
        final int kOffset = 6;
        final int kRadius = 5;
        int size = getPreferredIconSize();
        int insetSize = aSource != null ? size * 2 / 3 : size;

        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP)) {
            Rect iconBounds = new Rect(0, 0, size, size);
            canvas.drawBitmap(aSource, null, iconBounds, null);
            return bitmap;
        }

        // draw a base color
        Paint paint = new Paint();
        if (aSource == null) {
            // If we aren't drawing a favicon, just use an orange color.
            paint.setColor(Color.HSVToColor(DEFAULT_LAUNCHER_ICON_HSV));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
        } else {
            // otherwise use the dominant color from the icon + a layer of transparent white to lighten it somewhat
            int color = BitmapUtils.getDominantColor(aSource);
            paint.setColor(color);
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
            paint.setColor(Color.argb(100, 255, 255, 255));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
        }

        // draw the overlay
        Bitmap overlay = BitmapFactory.decodeResource(GeckoApp.mAppContext.getResources(), R.drawable.home_bg);
        canvas.drawBitmap(overlay, null, new Rect(0, 0, size, size), null);

        // draw the favicon
        if (aSource == null)
            aSource = BitmapFactory.decodeResource(GeckoApp.mAppContext.getResources(), R.drawable.home_star);

        // by default, we scale the icon to this size
        int sWidth = insetSize / 2;
        int sHeight = sWidth;

        if (aSource.getWidth() > insetSize || aSource.getHeight() > insetSize) {
            // however, if the icon is larger than our minimum, we allow it to be drawn slightly larger
            // (but not necessarily at its full resolution)
            sWidth = Math.min(size / 3, aSource.getWidth() / 2);
            sHeight = Math.min(size / 3, aSource.getHeight() / 2);
        }

        int halfSize = size / 2;
        canvas.drawBitmap(aSource,
                          null,
                          new Rect(halfSize - sWidth,
                                   halfSize - sHeight,
                                   halfSize + sWidth,
                                   halfSize + sHeight),
                          null);

        return bitmap;
    }

    static String[] getHandlersForMimeType(String aMimeType, String aAction) {
        Intent intent = getIntentForActionString(aAction);
        if (aMimeType != null && aMimeType.length() > 0)
            intent.setType(aMimeType);
        return getHandlersForIntent(intent);
    }

    static String[] getHandlersForURL(String aURL, String aAction) {
        // aURL may contain the whole URL or just the protocol
        Uri uri = aURL.indexOf(':') >= 0 ? Uri.parse(aURL) : new Uri.Builder().scheme(aURL).build();
        Intent intent = getIntentForActionString(aAction);
        intent.setData(uri);
        return getHandlersForIntent(intent);
    }

    static String[] getHandlersForIntent(Intent intent) {
        PackageManager pm = GeckoApp.mAppContext.getPackageManager();
        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        int numAttr = 4;
        String[] ret = new String[list.size() * numAttr];
        for (int i = 0; i < list.size(); i++) {
            ResolveInfo resolveInfo = list.get(i);
            ret[i * numAttr] = resolveInfo.loadLabel(pm).toString();
            if (resolveInfo.isDefault)
                ret[i * numAttr + 1] = "default";
            else
                ret[i * numAttr + 1] = "";
            ret[i * numAttr + 2] = resolveInfo.activityInfo.applicationInfo.packageName;
            ret[i * numAttr + 3] = resolveInfo.activityInfo.name;
        }
        return ret;
    }

    static Intent getIntentForActionString(String aAction) {
        // Default to the view action if no other action as been specified.
        if (aAction != null && aAction.length() > 0)
            return new Intent(aAction);
        else
            return new Intent(Intent.ACTION_VIEW);
    }

    static String getExtensionFromMimeType(String aMimeType) {
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(aMimeType);
    }

    static String getMimeTypeFromExtensions(String aFileExt) {
        MimeTypeMap mtm = MimeTypeMap.getSingleton();
        StringTokenizer st = new StringTokenizer(aFileExt, ".,; ");
        String type = null;
        String subType = null;
        while (st.hasMoreElements()) {
            String ext = st.nextToken();
            String mt = mtm.getMimeTypeFromExtension(ext);
            if (mt == null)
                continue;
            int slash = mt.indexOf('/');
            String tmpType = mt.substring(0, slash);
            if (!tmpType.equalsIgnoreCase(type))
                type = type == null ? tmpType : "*";
            String tmpSubType = mt.substring(slash + 1);
            if (!tmpSubType.equalsIgnoreCase(subType))
                subType = subType == null ? tmpSubType : "*";
        }
        if (type == null)
            type = "*";
        if (subType == null)
            subType = "*";
        return type + "/" + subType;
    }

    static void safeStreamClose(Closeable stream) {
        try {
            if (stream != null)
                stream.close();
        } catch (IOException e) {}
    }

    static void shareImage(String aSrc, String aType) {

        Intent intent = new Intent(Intent.ACTION_SEND);
        boolean isDataURI = aSrc.startsWith("data:");
        OutputStream os = null;
        File dir = GeckoApp.getTempDirectory();

        if (dir == null) {
            showImageShareFailureToast();
            return;
        }

        GeckoApp.deleteTempFiles();

        try {
            // Create a temporary file for the image
            File imageFile = File.createTempFile("image",
                                                 "." + aType.replace("image/",""),
                                                 dir);
            os = new FileOutputStream(imageFile);

            if (isDataURI) {
                // We are dealing with a Data URI
                int dataStart = aSrc.indexOf(',');
                byte[] buf = Base64.decode(aSrc.substring(dataStart+1), Base64.DEFAULT);
                os.write(buf);
            } else {
                // We are dealing with a URL
                InputStream is = null;
                try {
                    URL url = new URL(aSrc);
                    is = url.openStream();
                    byte[] buf = new byte[2048];
                    int length;

                    while ((length = is.read(buf)) != -1) {
                        os.write(buf, 0, length);
                    }
                } finally {
                    safeStreamClose(is);
                }
            }
            intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(imageFile));

            // If we were able to determine the image type, send that in the intent. Otherwise,
            // use a generic type.
            if (aType.startsWith("image/")) {
                intent.setType(aType);
            } else {
                intent.setType("image/*");
            }
        } catch (IOException e) {
            if (!isDataURI) {
               // If we failed, at least send through the URL link
               intent.putExtra(Intent.EXTRA_TEXT, aSrc);
               intent.setType("text/plain");
            } else {
               showImageShareFailureToast();
               return;
            }
        } finally {
            safeStreamClose(os);
        }
        GeckoApp.mAppContext.startActivity(Intent.createChooser(intent,
                GeckoApp.mAppContext.getResources().getString(R.string.share_title)));
    }

    // Don't fail silently, tell the user that we weren't able to share the image
    private static final void showImageShareFailureToast() {
        Toast toast = Toast.makeText(GeckoApp.mAppContext,
                                     GeckoApp.mAppContext.getResources().getString(R.string.share_image_failed),
                                     Toast.LENGTH_SHORT);
        toast.show();
    }

    static boolean isUriSafeForScheme(Uri aUri) {
        // Bug 794034 - We don't want to pass MWI or USSD codes to the
        // dialer, and ensure the Uri class doesn't parse a URI
        // containing a fragment ('#')
        final String scheme = aUri.getScheme();
        if ("tel".equals(scheme) || "sms".equals(scheme)) {
            final String number = aUri.getSchemeSpecificPart();
            if (number.contains("#") || number.contains("*") || aUri.getFragment() != null) {
                return false;
            }
        }
        return true;
    }

    static boolean openUriExternal(String aUriSpec, String aMimeType, String aPackageName,
                                   String aClassName, String aAction, String aTitle) {
        Intent intent = getIntentForActionString(aAction);
        if (aAction.equalsIgnoreCase(Intent.ACTION_SEND)) {
            Intent shareIntent = getIntentForActionString(aAction);
            shareIntent.putExtra(Intent.EXTRA_TEXT, aUriSpec);
            shareIntent.putExtra(Intent.EXTRA_SUBJECT, aTitle);
            if (aMimeType != null && aMimeType.length() > 0)
                shareIntent.setType(aMimeType);
            intent = Intent.createChooser(shareIntent, GeckoApp.mAppContext.getResources().getString(R.string.share_title)); 
        } else if (aMimeType.length() > 0) {
            intent.setDataAndType(Uri.parse(aUriSpec), aMimeType);
        } else {
            Uri uri = Uri.parse(aUriSpec);
            if (isUriSafeForScheme(uri) == false) {
                return false;
            }
            
            final String scheme = uri.getScheme();
            if ("sms".equals(scheme)) {
                // Have a special handling for the SMS, as the message body
                // is not extracted from the URI automatically
                final String query = uri.getEncodedQuery();
                if (query != null && query.length() > 0) {
                    final String[] fields = query.split("&");
                    boolean foundBody = false;
                    String resultQuery = "";
                    for (int i = 0; i < fields.length; i++) {
                        final String field = fields[i];
                        if (field.length() > 5 && "body=".equals(field.substring(0, 5))) {
                            final String body = Uri.decode(field.substring(5));
                            intent.putExtra("sms_body", body);
                            foundBody = true;
                        } else {
                            resultQuery = resultQuery.concat(resultQuery.length() > 0 ? "&" + field : field);
                        }
                    }
                    if (foundBody) {
                        // Put the query without the body field back into the URI
                        final String prefix = aUriSpec.substring(0, aUriSpec.indexOf('?'));
                        uri = Uri.parse(resultQuery.length() > 0 ? prefix + "?" + resultQuery : prefix);
                    }
                }
            }
            intent.setData(uri);
        }
        if (aPackageName.length() > 0 && aClassName.length() > 0)
            intent.setClassName(aPackageName, aClassName);

        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        try {
            GeckoApp.mAppContext.startActivity(intent);
            return true;
        } catch(ActivityNotFoundException e) {
            return false;
        }
    }

    static SynchronousQueue<String> sClipboardQueue =
        new SynchronousQueue<String>();
    private static String EMPTY_STRING = new String();

    // On some devices, access to the clipboard service needs to happen
    // on a thread with a looper, so dispatch this to our looper thread
    // Note: the main looper won't work because it may be blocked on the
    // gecko thread, which is most likely this thread
    static String getClipboardText() {
        getHandler().post(new Runnable() { 
            @SuppressWarnings("deprecation")
            public void run() {
                Context context = GeckoApp.mAppContext;
                String text = null;
                if (android.os.Build.VERSION.SDK_INT >= 11) {
                    android.content.ClipboardManager cm = (android.content.ClipboardManager)
                        context.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (cm.hasPrimaryClip()) {
                        ClipData clip = cm.getPrimaryClip();
                        if (clip != null) {
                            ClipData.Item item = clip.getItemAt(0);
                            text = item.coerceToText(context).toString();
                        }
                    }
                } else {
                    android.text.ClipboardManager cm = (android.text.ClipboardManager)
                        context.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (cm.hasText())
                        text = cm.getText().toString();
                }
                try {
                    sClipboardQueue.put(text != null ? text : EMPTY_STRING);
                } catch (InterruptedException ie) {}
            }});
        try {
            String ret = sClipboardQueue.take();
            return (EMPTY_STRING.equals(ret) ? null : ret);
        } catch (InterruptedException ie) {}
        return null;
    }

    static void setClipboardText(final String text) {
        getHandler().post(new Runnable() { 
            @SuppressWarnings("deprecation")
            public void run() {
                Context context = GeckoApp.mAppContext;
                if (android.os.Build.VERSION.SDK_INT >= 11) {
                    android.content.ClipboardManager cm = (android.content.ClipboardManager)
                        context.getSystemService(Context.CLIPBOARD_SERVICE);
                    cm.setPrimaryClip(ClipData.newPlainText("Text", text));
                } else {
                    android.text.ClipboardManager cm = (android.text.ClipboardManager)
                        context.getSystemService(Context.CLIPBOARD_SERVICE);
                    cm.setText(text);
                }
            }});
    }

    public static void showAlertNotification(String aImageUrl, String aAlertTitle, String aAlertText,
                                             String aAlertCookie, String aAlertName) {
        Log.d(LOGTAG, "GeckoAppShell.showAlertNotification\n" +
            "- image = '" + aImageUrl + "'\n" +
            "- title = '" + aAlertTitle + "'\n" +
            "- text = '" + aAlertText +"'\n" +
            "- cookie = '" + aAlertCookie +"'\n" +
            "- name = '" + aAlertName + "'");

        int icon = R.drawable.ic_status_logo;

        Uri imageUri = Uri.parse(aImageUrl);
        String scheme = imageUri.getScheme();
        if ("drawable".equals(scheme)) {
            String resource = imageUri.getSchemeSpecificPart();
            resource = resource.substring(resource.lastIndexOf('/') + 1);
            try {
                Class<R.drawable> drawableClass = R.drawable.class;
                Field f = drawableClass.getField(resource);
                icon = f.getInt(null);
            } catch (Exception e) {} // just means the resource doesn't exist
            imageUri = null;
        }

        int notificationID = aAlertName.hashCode();

        // Remove the old notification with the same ID, if any
        removeNotification(notificationID);

        AlertNotification notification = 
            new AlertNotification(GeckoApp.mAppContext,notificationID, icon, 
                                  aAlertTitle, aAlertText, 
                                  System.currentTimeMillis());

        // The intent to launch when the user clicks the expanded notification
        Intent notificationIntent = new Intent(GeckoApp.ACTION_ALERT_CLICK);
        notificationIntent.setClassName(GeckoApp.mAppContext,
            GeckoApp.mAppContext.getPackageName() + ".NotificationHandler");

        // Put the strings into the intent as an URI "alert:?name=<alertName>&app=<appName>&cookie=<cookie>"
        Uri.Builder b = new Uri.Builder();
        String app = GeckoApp.mAppContext.getClass().getName();
        Uri dataUri = b.scheme("alert")
                                 .path(Integer.toString(notificationID))
                                 .appendQueryParameter("name", aAlertName)
                                 .appendQueryParameter("app", app)
                                 .appendQueryParameter("cookie", aAlertCookie)
                                 .build();
        notificationIntent.setData(dataUri);

        PendingIntent contentIntent = PendingIntent.getBroadcast(GeckoApp.mAppContext, 0, notificationIntent, 0);
        notification.setLatestEventInfo(GeckoApp.mAppContext, aAlertTitle, aAlertText, contentIntent);
        notification.setCustomIcon(imageUri);
        // The intent to execute when the status entry is deleted by the user with the "Clear All Notifications" button
        Intent clearNotificationIntent = new Intent(GeckoApp.ACTION_ALERT_CLEAR);
        clearNotificationIntent.setClassName(GeckoApp.mAppContext,
            GeckoApp.mAppContext.getPackageName() + ".NotificationHandler");
        clearNotificationIntent.setData(dataUri);
        notification.deleteIntent = PendingIntent.getBroadcast(GeckoApp.mAppContext, 0, clearNotificationIntent, 0);

        mAlertNotifications.put(notificationID, notification);

        notification.show();
    }

    public static void alertsProgressListener_OnProgress(String aAlertName, long aProgress, long aProgressMax, String aAlertText) {
        final int notificationID = aAlertName.hashCode();
        AlertNotification notification = mAlertNotifications.get(notificationID);
        if (notification != null)
            notification.updateProgress(aAlertText, aProgress, aProgressMax);

        if (aProgress == aProgressMax) {
            // Hide the notification at 100%
            removeObserver(aAlertName);
            removeNotification(notificationID);
        }
    }

    public static void alertsProgressListener_OnCancel(String aAlertName) {
        removeObserver(aAlertName);

        int notificationID = aAlertName.hashCode();
        removeNotification(notificationID);
    }

    public static void handleNotification(String aAction, String aAlertName, String aAlertCookie) {
        int notificationID = aAlertName.hashCode();

        if (GeckoApp.ACTION_ALERT_CALLBACK.equals(aAction)) {
            callObserver(aAlertName, "alertclickcallback", aAlertCookie);

            AlertNotification notification = mAlertNotifications.get(notificationID);
            if (notification != null && notification.isProgressStyle()) {
                // When clicked, keep the notification, if it displays a progress
                return;
            }
        }

        callObserver(aAlertName, "alertfinished", aAlertCookie);
        // Also send a notification to the observer service
        // New listeners should register for these notifications since they will be called even if
        // Gecko has been killed and restared between when your notification was shown and when the
        // user clicked on it.
        sendEventToGecko(GeckoEvent.createBroadcastEvent("Notification:Clicked", aAlertCookie));
        removeObserver(aAlertName);

        removeNotification(notificationID);
    }

    private static void removeNotification(int notificationID) {
        mAlertNotifications.remove(notificationID);

        NotificationManager notificationManager = (NotificationManager)
            GeckoApp.mAppContext.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(notificationID);
    }

    public static int getDpi() {
        if (sDensityDpi == 0) {
            sDensityDpi = GeckoApp.mAppContext.getResources().getDisplayMetrics().densityDpi;
        }

        return sDensityDpi;
    }

    public static void setFullScreen(boolean fullscreen) {
        GeckoApp.mAppContext.setFullScreen(fullscreen);
    }

    public static String showFilePickerForExtensions(String aExtensions) {
        return sActivityHelper.showFilePicker(GeckoApp.mAppContext, getMimeTypeFromExtensions(aExtensions));
    }

    public static String showFilePickerForMimeType(String aMimeType) {
        return sActivityHelper.showFilePicker(GeckoApp.mAppContext, aMimeType);
    }

    public static void performHapticFeedback(boolean aIsLongPress) {
        // Don't perform haptic feedback if a vibration is currently playing,
        // because the haptic feedback will nuke the vibration.
        if (!sVibrationMaybePlaying || System.nanoTime() >= sVibrationEndTime) {
            LayerView layerView = GeckoApp.mAppContext.getLayerView();
            layerView.performHapticFeedback(aIsLongPress ?
                                            HapticFeedbackConstants.LONG_PRESS :
                                            HapticFeedbackConstants.VIRTUAL_KEY);
        }
    }

    private static Vibrator vibrator() {
        LayerView layerView = GeckoApp.mAppContext.getLayerView();
        return (Vibrator) layerView.getContext().getSystemService(Context.VIBRATOR_SERVICE);
    }

    public static void vibrate(long milliseconds) {
        sVibrationEndTime = System.nanoTime() + milliseconds * 1000000;
        sVibrationMaybePlaying = true;
        vibrator().vibrate(milliseconds);
    }

    public static void vibrate(long[] pattern, int repeat) {
        // If pattern.length is even, the last element in the pattern is a
        // meaningless delay, so don't include it in vibrationDuration.
        long vibrationDuration = 0;
        int iterLen = pattern.length - (pattern.length % 2 == 0 ? 1 : 0);
        for (int i = 0; i < iterLen; i++) {
          vibrationDuration += pattern[i];
        }

        sVibrationEndTime = System.nanoTime() + vibrationDuration * 1000000;
        sVibrationMaybePlaying = true;
        vibrator().vibrate(pattern, repeat);
    }

    public static void cancelVibrate() {
        sVibrationMaybePlaying = false;
        sVibrationEndTime = 0;
        vibrator().cancel();
    }

    public static void showInputMethodPicker() {
        InputMethodManager imm = (InputMethodManager)
            GeckoApp.mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showInputMethodPicker();
    }

    public static void setKeepScreenOn(final boolean on) {
        GeckoApp.mAppContext.runOnUiThread(new Runnable() {
            public void run() {
                // TODO
            }
        });
    }

    public static void notifyDefaultPrevented(final boolean defaultPrevented) {
        getMainHandler().post(new Runnable() {
            public void run() {
                LayerView view = GeckoApp.mAppContext.getLayerView();
                TouchEventHandler handler = (view == null ? null : view.getTouchEventHandler());
                if (handler != null) {
                    handler.handleEventListenerAction(!defaultPrevented);
                }
            }
        });
    }

    public static boolean isNetworkLinkUp() {
        ConnectivityManager cm = (ConnectivityManager)
            GeckoApp.mAppContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null || !info.isConnected())
            return false;
        return true;
    }

    public static boolean isNetworkLinkKnown() {
        ConnectivityManager cm = (ConnectivityManager)
            GeckoApp.mAppContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm.getActiveNetworkInfo() == null)
            return false;
        return true;
    }

    public static void setSelectedLocale(String localeCode) {
        /* Bug 713464: This method is still called from Gecko side.
           Earlier we had an option to run Firefox in a language other than system's language.
           However, this is not supported as of now.
           Gecko resets the locale to en-US by calling this function with an empty string.
           This affects GeckoPreferences activity in multi-locale builds.

        //We're not using this, not need to save it (see bug 635342)
        SharedPreferences settings =
            GeckoApp.mAppContext.getPreferences(Activity.MODE_PRIVATE);
        settings.edit().putString(GeckoApp.mAppContext.getPackageName() + ".locale",
                                  localeCode).commit();
        Locale locale;
        int index;
        if ((index = localeCode.indexOf('-')) != -1 ||
            (index = localeCode.indexOf('_')) != -1) {
            String langCode = localeCode.substring(0, index);
            String countryCode = localeCode.substring(index + 1);
            locale = new Locale(langCode, countryCode);
        } else {
            locale = new Locale(localeCode);
        }
        Locale.setDefault(locale);

        Resources res = GeckoApp.mAppContext.getBaseContext().getResources();
        Configuration config = res.getConfiguration();
        config.locale = locale;
        res.updateConfiguration(config, res.getDisplayMetrics());
        */
    }

    public static int[] getSystemColors() {
        // attrsAppearance[] must correspond to AndroidSystemColors structure in android/AndroidBridge.h
        final int[] attrsAppearance = {
            android.R.attr.textColor,
            android.R.attr.textColorPrimary,
            android.R.attr.textColorPrimaryInverse,
            android.R.attr.textColorSecondary,
            android.R.attr.textColorSecondaryInverse,
            android.R.attr.textColorTertiary,
            android.R.attr.textColorTertiaryInverse,
            android.R.attr.textColorHighlight,
            android.R.attr.colorForeground,
            android.R.attr.colorBackground,
            android.R.attr.panelColorForeground,
            android.R.attr.panelColorBackground
        };

        int[] result = new int[attrsAppearance.length];

        final ContextThemeWrapper contextThemeWrapper =
            new ContextThemeWrapper(GeckoApp.mAppContext, android.R.style.TextAppearance);

        final TypedArray appearance = contextThemeWrapper.getTheme().obtainStyledAttributes(attrsAppearance);

        if (appearance != null) {
            for (int i = 0; i < appearance.getIndexCount(); i++) {
                int idx = appearance.getIndex(i);
                int color = appearance.getColor(idx, 0);
                result[idx] = color;
            }
            appearance.recycle();
        }

        return result;
    }

    public static void killAnyZombies() {
        GeckoProcessesVisitor visitor = new GeckoProcessesVisitor() {
            public boolean callback(int pid) {
                if (pid != android.os.Process.myPid())
                    android.os.Process.killProcess(pid);
                return true;
            }
        };
            
        EnumerateGeckoProcesses(visitor);
    }

    public static boolean checkForGeckoProcs() {

        class GeckoPidCallback implements GeckoProcessesVisitor {
            public boolean otherPidExist = false;
            public boolean callback(int pid) {
                if (pid != android.os.Process.myPid()) {
                    otherPidExist = true;
                    return false;
                }
                return true;
            }            
        }
        GeckoPidCallback visitor = new GeckoPidCallback();            
        EnumerateGeckoProcesses(visitor);
        return visitor.otherPidExist;
    }

    interface GeckoProcessesVisitor{
        boolean callback(int pid);
    }

    static int sPidColumn = -1;
    static int sUserColumn = -1;
    private static void EnumerateGeckoProcesses(GeckoProcessesVisitor visiter) {

        try {

            // run ps and parse its output
            java.lang.Process ps = Runtime.getRuntime().exec("ps");
            BufferedReader in = new BufferedReader(new InputStreamReader(ps.getInputStream()),
                                                   2048);

            String headerOutput = in.readLine();

            // figure out the column offsets.  We only care about the pid and user fields
            if (sPidColumn == -1 || sUserColumn == -1) {
                StringTokenizer st = new StringTokenizer(headerOutput);
                
                int tokenSoFar = 0;
                while(st.hasMoreTokens()) {
                    String next = st.nextToken();
                    if (next.equalsIgnoreCase("PID"))
                        sPidColumn = tokenSoFar;
                    else if (next.equalsIgnoreCase("USER"))
                        sUserColumn = tokenSoFar;
                    tokenSoFar++;
                }
            }

            // alright, the rest are process entries.
            String psOutput = null;
            while ((psOutput = in.readLine()) != null) {
                String[] split = psOutput.split("\\s+");
                if (split.length <= sPidColumn || split.length <= sUserColumn)
                    continue;
                int uid = android.os.Process.getUidForName(split[sUserColumn]);
                if (uid == android.os.Process.myUid() &&
                    !split[split.length - 1].equalsIgnoreCase("ps")) {
                    int pid = Integer.parseInt(split[sPidColumn]);
                    boolean keepGoing = visiter.callback(pid);
                    if (keepGoing == false)
                        break;
                }
            }
            in.close();
        }
        catch (Exception e) {
            Log.w(LOGTAG, "Failed to enumerate Gecko processes.",  e);
        }
    }

    public static void waitForAnotherGeckoProc(){
        int countdown = 40;
        while (!checkForGeckoProcs() &&  --countdown > 0) {
            try {
                Thread.sleep(100);
            } catch (InterruptedException ie) {}
        }
    }

    public static void scanMedia(String aFile, String aMimeType) {
        Context context = GeckoApp.mAppContext;
        GeckoMediaScannerClient client = new GeckoMediaScannerClient(context, aFile, aMimeType);
    }

    public static byte[] getIconForExtension(String aExt, int iconSize) {
        try {
            if (iconSize <= 0)
                iconSize = 16;

            if (aExt != null && aExt.length() > 1 && aExt.charAt(0) == '.')
                aExt = aExt.substring(1);

            PackageManager pm = GeckoApp.mAppContext.getPackageManager();
            Drawable icon = getDrawableForExtension(pm, aExt);
            if (icon == null) {
                // Use a generic icon
                icon = pm.getDefaultActivityIcon();
            }

            Bitmap bitmap = ((BitmapDrawable)icon).getBitmap();
            if (bitmap.getWidth() != iconSize || bitmap.getHeight() != iconSize)
                bitmap = Bitmap.createScaledBitmap(bitmap, iconSize, iconSize, true);

            ByteBuffer buf = ByteBuffer.allocate(iconSize * iconSize * 4);
            bitmap.copyPixelsToBuffer(buf);

            return buf.array();
        }
        catch (Exception e) {
            Log.w(LOGTAG, "getIconForExtension failed.",  e);
            return null;
        }
    }

    private static Drawable getDrawableForExtension(PackageManager pm, String aExt) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        MimeTypeMap mtm = MimeTypeMap.getSingleton();
        String mimeType = mtm.getMimeTypeFromExtension(aExt);
        if (mimeType != null && mimeType.length() > 0)
            intent.setType(mimeType);
        else
            return null;

        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        if (list.size() == 0)
            return null;

        ResolveInfo resolveInfo = list.get(0);

        if (resolveInfo == null)
            return null;

        ActivityInfo activityInfo = resolveInfo.activityInfo;

        return activityInfo.loadIcon(pm);
    }

    public static boolean getShowPasswordSetting() {
        try {
            int showPassword =
                Settings.System.getInt(GeckoApp.mAppContext.getContentResolver(),
                                       Settings.System.TEXT_SHOW_PASSWORD, 1);
            return (showPassword > 0);
        }
        catch (Exception e) {
            return true;
        }
    }

    public static void addPluginView(View view,
                                     int x, int y,
                                     int w, int h,
                                     boolean isFullScreen) {
        GeckoApp.mAppContext.addPluginView(view, new Rect(x, y, x + w, y + h), isFullScreen);
    }

    public static void removePluginView(View view, boolean isFullScreen) {
        GeckoApp.mAppContext.removePluginView(view, isFullScreen);
    }

    public static Class<?> loadPluginClass(String className, String libName) {
        try {
            final String packageName = GeckoApp.mAppContext.getPluginPackage(libName);
            final int contextFlags = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
            final Context pluginContext = GeckoApp.mAppContext.createPackageContext(packageName, contextFlags);
            return pluginContext.getClassLoader().loadClass(className);
        } catch (java.lang.ClassNotFoundException cnfe) {
            Log.w(LOGTAG, "Couldn't find plugin class " + className, cnfe);
            return null;
        } catch (android.content.pm.PackageManager.NameNotFoundException nnfe) {
            Log.w(LOGTAG, "Couldn't find package.", nnfe);
            return null;
        }
    }

    public static android.hardware.Camera sCamera = null;

    static native void cameraCallbackBridge(byte[] data);

    static int kPreferedFps = 25;
    static byte[] sCameraBuffer = null;

    static int[] initCamera(String aContentType, int aCamera, int aWidth, int aHeight) {
        getMainHandler().post(new Runnable() {
                @Override
                public void run() {
                    try {
                        GeckoApp.mAppContext.enableCameraView();
                    } catch (Exception e) {}
                }
            });

        // [0] = 0|1 (failure/success)
        // [1] = width
        // [2] = height
        // [3] = fps
        int[] result = new int[4];
        result[0] = 0;

        if (Build.VERSION.SDK_INT >= 9) {
            if (android.hardware.Camera.getNumberOfCameras() == 0)
                return result;
        }

        try {
            // no front/back camera before API level 9
            if (Build.VERSION.SDK_INT >= 9)
                sCamera = android.hardware.Camera.open(aCamera);
            else
                sCamera = android.hardware.Camera.open();

            android.hardware.Camera.Parameters params = sCamera.getParameters();
            params.setPreviewFormat(ImageFormat.NV21);

            // use the preview fps closest to 25 fps.
            int fpsDelta = 1000;
            try {
                Iterator<Integer> it = params.getSupportedPreviewFrameRates().iterator();
                while (it.hasNext()) {
                    int nFps = it.next();
                    if (Math.abs(nFps - kPreferedFps) < fpsDelta) {
                        fpsDelta = Math.abs(nFps - kPreferedFps);
                        params.setPreviewFrameRate(nFps);
                    }
                }
            } catch(Exception e) {
                params.setPreviewFrameRate(kPreferedFps);
            }

            // set up the closest preview size available
            Iterator<android.hardware.Camera.Size> sit = params.getSupportedPreviewSizes().iterator();
            int sizeDelta = 10000000;
            int bufferSize = 0;
            while (sit.hasNext()) {
                android.hardware.Camera.Size size = sit.next();
                if (Math.abs(size.width * size.height - aWidth * aHeight) < sizeDelta) {
                    sizeDelta = Math.abs(size.width * size.height - aWidth * aHeight);
                    params.setPreviewSize(size.width, size.height);
                    bufferSize = size.width * size.height;
                }
            }

            try {
                sCamera.setPreviewDisplay(GeckoApp.mAppContext.cameraView.getHolder());
            } catch(IOException e) {
                Log.w(LOGTAG, "Error setPreviewDisplay:", e);
            } catch(RuntimeException e) {
                Log.w(LOGTAG, "Error setPreviewDisplay:", e);
            }

            sCamera.setParameters(params);
            sCameraBuffer = new byte[(bufferSize * 12) / 8];
            sCamera.addCallbackBuffer(sCameraBuffer);
            sCamera.setPreviewCallbackWithBuffer(new android.hardware.Camera.PreviewCallback() {
                public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
                    cameraCallbackBridge(data);
                    if (sCamera != null)
                        sCamera.addCallbackBuffer(sCameraBuffer);
                }
            });
            sCamera.startPreview();
            params = sCamera.getParameters();
            result[0] = 1;
            result[1] = params.getPreviewSize().width;
            result[2] = params.getPreviewSize().height;
            result[3] = params.getPreviewFrameRate();
        } catch(RuntimeException e) {
            Log.w(LOGTAG, "initCamera RuntimeException.", e);
            result[0] = result[1] = result[2] = result[3] = 0;
        }
        return result;
    }

    static synchronized void closeCamera() {
        getMainHandler().post(new Runnable() {
                @Override
                public void run() {
                    try {
                        GeckoApp.mAppContext.disableCameraView();
                    } catch (Exception e) {}
                }
            });
        if (sCamera != null) {
            sCamera.stopPreview();
            sCamera.release();
            sCamera = null;
            sCameraBuffer = null;
        }
    }

    /**
     * Adds a listener for a gecko event.
     * This method is thread-safe and may be called at any time. In particular, calling it
     * with an event that is currently being processed has the properly-defined behaviour that
     * any added listeners will not be invoked on the event currently being processed, but
     * will be invoked on future events of that type.
     *
     * This method is referenced by Robocop via reflection.
     */
    public static void registerEventListener(String event, GeckoEventListener listener) {
        sEventDispatcher.registerEventListener(event, listener);
    }

    static EventDispatcher getEventDispatcher() {
        return sEventDispatcher;
    }

    /**
     * Remove a previously-registered listener for a gecko event.
     * This method is thread-safe and may be called at any time. In particular, calling it
     * with an event that is currently being processed has the properly-defined behaviour that
     * any removed listeners will still be invoked on the event currently being processed, but
     * will not be invoked on future events of that type.
     *
     * This method is referenced by Robocop via reflection.
     */
    public static void unregisterEventListener(String event, GeckoEventListener listener) {
        sEventDispatcher.unregisterEventListener(event, listener);
    }

    /*
     * Battery API related methods.
     */
    public static void enableBatteryNotifications() {
        GeckoBatteryManager.enableNotifications();
    }

    public static String handleGeckoMessage(String message) {
        return sEventDispatcher.dispatchEvent(message);
    }

    public static void disableBatteryNotifications() {
        GeckoBatteryManager.disableNotifications();
    }

    public static double[] getCurrentBatteryInformation() {
        return GeckoBatteryManager.getCurrentInformation();
    }

    static void checkUriVisited(String uri) {   // invoked from native JNI code
        GlobalHistory.getInstance().checkUriVisited(uri);
    }

    static void markUriVisited(final String uri) {    // invoked from native JNI code
        getHandler().post(new Runnable() { 
            public void run() {
                GlobalHistory.getInstance().add(uri);
            }
        });
    }

    static void hideProgressDialog() {
        // unused stub
    }

    /*
     * WebSMS related methods.
     */
    public static int getNumberOfMessagesForText(String aText) {
        if (SmsManager.getInstance() == null) {
            return 0;
        }

        return SmsManager.getInstance().getNumberOfMessagesForText(aText);
    }

    public static void sendMessage(String aNumber, String aMessage, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().send(aNumber, aMessage, aRequestId);
    }

    public static void getMessage(int aMessageId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().getMessage(aMessageId, aRequestId);
    }

    public static void deleteMessage(int aMessageId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().deleteMessage(aMessageId, aRequestId);
    }

    public static void createMessageList(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, int aDeliveryState, boolean aReverse, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().createMessageList(aStartDate, aEndDate, aNumbers, aNumbersCount, aDeliveryState, aReverse, aRequestId);
    }

    public static void getNextMessageInList(int aListId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().getNextMessageInList(aListId, aRequestId);
    }

    public static void clearMessageList(int aListId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().clearMessageList(aListId);
    }

    public static boolean isTablet() {
        return GeckoApp.mAppContext.isTablet();
    }

    public static void viewSizeChanged() {
        LayerView v = GeckoApp.mAppContext.getLayerView();
        if (v != null && v.isIMEEnabled()) {
            sendEventToGecko(GeckoEvent.createBroadcastEvent(
                    "ScrollTo:FocusedInput", ""));
        }
    }

    public static double[] getCurrentNetworkInformation() {
        return GeckoNetworkManager.getInstance().getCurrentInformation();
    }

    public static void enableNetworkNotifications() {
        GeckoNetworkManager.getInstance().enableNotifications();
    }

    public static void disableNetworkNotifications() {
        GeckoNetworkManager.getInstance().disableNotifications();
    }

    // values taken from android's Base64
    public static final int BASE64_DEFAULT = 0;
    public static final int BASE64_URL_SAFE = 8;

    /**
     * taken from http://www.source-code.biz/base64coder/java/Base64Coder.java.txt and modified (MIT License)
     */
    // Mapping table from 6-bit nibbles to Base64 characters.
    private static final byte[] map1 = new byte[64];
    private static final byte[] map1_urlsafe;
    static {
      int i=0;
      for (byte c='A'; c<='Z'; c++) map1[i++] = c;
      for (byte c='a'; c<='z'; c++) map1[i++] = c;
      for (byte c='0'; c<='9'; c++) map1[i++] = c;
      map1[i++] = '+'; map1[i++] = '/';
      map1_urlsafe = map1.clone();
      map1_urlsafe[62] = '-'; map1_urlsafe[63] = '_'; 
    }

    // Mapping table from Base64 characters to 6-bit nibbles.
    private static final byte[] map2 = new byte[128];
    static {
        for (int i=0; i<map2.length; i++) map2[i] = -1;
        for (int i=0; i<64; i++) map2[map1[i]] = (byte)i;
        map2['-'] = (byte)62; map2['_'] = (byte)63;
    }

    final static byte EQUALS_ASCII = (byte) '=';

    /**
     * Encodes a byte array into Base64 format.
     * No blanks or line breaks are inserted in the output.
     * @param in    An array containing the data bytes to be encoded.
     * @return      A character array containing the Base64 encoded data.
     */
    public static byte[] encodeBase64(byte[] in, int flags) {
        if (Build.VERSION.SDK_INT >=Build.VERSION_CODES.FROYO)
            return Base64.encode(in, flags | Base64.NO_WRAP);
        int oDataLen = (in.length*4+2)/3;       // output length without padding
        int oLen = ((in.length+2)/3)*4;         // output length including padding
        byte[] out = new byte[oLen];
        int ip = 0;
        int iEnd = in.length;
        int op = 0;
        byte[] toMap = ((flags & BASE64_URL_SAFE) == 0 ? map1 : map1_urlsafe);
        while (ip < iEnd) {
            int i0 = in[ip++] & 0xff;
            int i1 = ip < iEnd ? in[ip++] & 0xff : 0;
            int i2 = ip < iEnd ? in[ip++] & 0xff : 0;
            int o0 = i0 >>> 2;
            int o1 = ((i0 &   3) << 4) | (i1 >>> 4);
            int o2 = ((i1 & 0xf) << 2) | (i2 >>> 6);
            int o3 = i2 & 0x3F;
            out[op++] = toMap[o0];
            out[op++] = toMap[o1];
            out[op] = op < oDataLen ? toMap[o2] : EQUALS_ASCII; op++;
            out[op] = op < oDataLen ? toMap[o3] : EQUALS_ASCII; op++;
        }
        return out; 
    }

    /**
     * Decodes a byte array from Base64 format.
     * No blanks or line breaks are allowed within the Base64 encoded input data.
     * @param in    A character array containing the Base64 encoded data.
     * @param iOff  Offset of the first character in <code>in</code> to be processed.
     * @param iLen  Number of characters to process in <code>in</code>, starting at <code>iOff</code>.
     * @return      An array containing the decoded data bytes.
     * @throws      IllegalArgumentException If the input is not valid Base64 encoded data.
     */
    public static byte[] decodeBase64(byte[] in, int flags) {
        if (Build.VERSION.SDK_INT >=Build.VERSION_CODES.FROYO)
            return Base64.decode(in, flags);
        int iOff = 0;
        int iLen = in.length;
        if (iLen%4 != 0) throw new IllegalArgumentException ("Length of Base64 encoded input string is not a multiple of 4.");
        while (iLen > 0 && in[iOff+iLen-1] == '=') iLen--;
        int oLen = (iLen*3) / 4;
        byte[] out = new byte[oLen];
        int ip = iOff;
        int iEnd = iOff + iLen;
        int op = 0;
        while (ip < iEnd) {
            int i0 = in[ip++];
            int i1 = in[ip++];
            int i2 = ip < iEnd ? in[ip++] : 'A';
            int i3 = ip < iEnd ? in[ip++] : 'A';
            if (i0 > 127 || i1 > 127 || i2 > 127 || i3 > 127)
                throw new IllegalArgumentException ("Illegal character in Base64 encoded data.");
            int b0 = map2[i0];
            int b1 = map2[i1];
            int b2 = map2[i2];
            int b3 = map2[i3];
            if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0)
                throw new IllegalArgumentException ("Illegal character in Base64 encoded data.");
            int o0 = ( b0       <<2) | (b1>>>4);
            int o1 = ((b1 & 0xf)<<4) | (b2>>>2);
            int o2 = ((b2 &   3)<<6) |  b3;
            out[op++] = (byte)o0;
            if (op<oLen) out[op++] = (byte)o1;
            if (op<oLen) out[op++] = (byte)o2; }
        return out; 
    }

    public static byte[] decodeBase64(String s, int flags) {
        return decodeBase64(s.getBytes(), flags);
    }

    public static short getScreenOrientation() {
        return GeckoScreenOrientationListener.getInstance().getScreenOrientation();
    }

    public static void enableScreenOrientationNotifications() {
        GeckoScreenOrientationListener.getInstance().enableNotifications();
    }

    public static void disableScreenOrientationNotifications() {
        GeckoScreenOrientationListener.getInstance().disableNotifications();
    }

    public static void lockScreenOrientation(int aOrientation) {
        GeckoScreenOrientationListener.getInstance().lockScreenOrientation(aOrientation);
    }

    public static void unlockScreenOrientation() {
        GeckoScreenOrientationListener.getInstance().unlockScreenOrientation();
    }

    public static boolean pumpMessageLoop() {
        MessageQueue mq = Looper.myQueue();
        Message msg = getNextMessageFromQueue(mq); 
        if (msg == null)
            return false;
        if (msg.getTarget() == null) 
            Looper.myLooper().quit();
        else
            msg.getTarget().dispatchMessage(msg);
        msg.recycle();
        return true;
    }

    static class AsyncResultHandler extends FilePickerResultHandler {
        private long mId;
        AsyncResultHandler(long id) {
            super(null);
            mId = id;
        }

        public void onActivityResult(int resultCode, Intent data) {
            GeckoAppShell.notifyFilePickerResult(handleActivityResult(resultCode, data), mId);
        }
        
    }

    static native void notifyFilePickerResult(String filePath, long id);

    /* Called by JNI from AndroidBridge */
    public static void showFilePickerAsync(String aMimeType, long id) {
        if (!sActivityHelper.showFilePicker(GeckoApp.mAppContext, aMimeType, new AsyncResultHandler(id))) {
            GeckoAppShell.notifyFilePickerResult("", id);
        }
    }

    public static void notifyWakeLockChanged(String topic, String state) {
        GeckoApp.mAppContext.notifyWakeLockChanged(topic, state);
    }

    public static String getGfxInfoData() {
        String data = sGfxInfoThread.getData();
        sGfxInfoThread = null;
        return data;
    }

    public static void registerSurfaceTextureFrameListener(Object surfaceTexture, final int id) {
        ((SurfaceTexture)surfaceTexture).setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
            public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                GeckoAppShell.onSurfaceTextureFrameAvailable(surfaceTexture, id);
            }
        });
    }

    public static void unregisterSurfaceTextureFrameListener(Object surfaceTexture) {
        ((SurfaceTexture)surfaceTexture).setOnFrameAvailableListener(null);
    }

    public static void notifyCheckUpdateResult(boolean result) {
        if (GeckoApp.mAppContext != null)
            GeckoApp.mAppContext.notifyCheckUpdateResult(result);
    }
    
    public static boolean unlockProfile() {
        // Try to kill any zombie Fennec's that might be running
        GeckoAppShell.killAnyZombies();

        // Then force unlock this profile
        GeckoProfile profile = GeckoApp.mAppContext.getProfile();
        File lock = profile.getFile(".parentlock");
        if (lock.exists()) {
            lock.delete();
            return true;
        }
        return false;
    }

    public static String getProxyForURI(String spec, String scheme, String host, int port) {
        URI uri = null;
        try {
            uri = new URI(spec);
        } catch(java.net.URISyntaxException uriEx) {
            try {
                uri = new URI(scheme, null, host, port, null, null, null);
            } catch(java.net.URISyntaxException uriEx2) {
                Log.d("GeckoProxy", "Failed to create uri from spec", uriEx);
                Log.d("GeckoProxy", "Failed to create uri from parts", uriEx2);
            }
        }
        if (uri != null) {
            ProxySelector ps = ProxySelector.getDefault();
            if (ps != null) {
                List<Proxy> proxies = ps.select(uri);
                if (proxies != null && !proxies.isEmpty()) {
                    Proxy proxy = proxies.get(0);
                    if (!Proxy.NO_PROXY.equals(proxy)) {
                        final String proxyStr;
                        switch (proxy.type()) {
                        case HTTP:
                            proxyStr = "PROXY " + proxy.address().toString();
                            break;
                        case SOCKS:
                            proxyStr = "SOCKS " + proxy.address().toString();
                            break;
                        case DIRECT:
                        default:
                            proxyStr = "DIRECT";
                            break;
                        }
                        return proxyStr;
                    }
                }
            }
        }
        return "DIRECT";
    }
}
