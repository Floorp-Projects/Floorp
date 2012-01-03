/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.GeckoSoftwareLayerClient;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerView;

import java.io.*;
import java.lang.reflect.*;
import java.nio.*;
import java.nio.channels.*;
import java.text.*;
import java.util.*;
import java.util.zip.*;
import java.util.concurrent.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.content.res.*;
import android.content.pm.*;
import android.graphics.*;
import android.widget.*;
import android.hardware.*;
import android.location.*;
import android.webkit.MimeTypeMap;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.provider.Settings;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityEvent;

import android.util.*;
import android.net.Uri;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import android.graphics.Bitmap;
import android.graphics.drawable.*;

import org.json.JSONArray;
import org.json.JSONObject;

public class GeckoAppShell
{
    private static final String LOGTAG = "GeckoAppShell";

    // static members only
    private GeckoAppShell() { }

    static private LinkedList<GeckoEvent> gPendingEvents =
        new LinkedList<GeckoEvent>();

    static private boolean gRestartScheduled = false;
    static private PromptService gPromptService = null;

    static private GeckoInputConnection mInputConnection = null;

    static private final HashMap<Integer, AlertNotification>
        mAlertNotifications = new HashMap<Integer, AlertNotification>();

    /* Keep in sync with constants found here:
      http://mxr.mozilla.org/mozilla-central/source/uriloader/base/nsIWebProgressListener.idl
    */
    static public final int WPL_STATE_START = 0x00000001;
    static public final int WPL_STATE_STOP = 0x00000010;
    static public final int WPL_STATE_IS_DOCUMENT = 0x00020000;
    static public final int WPL_STATE_IS_NETWORK = 0x00040000;

    static private File sCacheFile = null;
    static private int sFreeSpace = -1;
    static File sHomeDir = null;
    static private int sDensityDpi = 0;

    private static HashMap<String, ArrayList<GeckoEventListener>> mEventListeners;

    /* The Android-side API: API methods that Android calls */

    // Initialization methods
    public static native void nativeInit();
    public static native void nativeRun(String args);

    // helper methods
    //    public static native void setSurfaceView(GeckoSurfaceView sv);
    public static native void setSoftwareLayerClient(GeckoSoftwareLayerClient client);
    public static native void putenv(String map);
    public static native void onResume();
    public static native void onLowMemory();
    public static native void callObserver(String observerKey, String topic, String data);
    public static native void removeObserver(String observerKey);
    public static native void loadLibs(String apkName, boolean shouldExtract);
    public static native void onChangeNetworkLinkStatus(String status);
    public static native void reportJavaCrash(String stack);
    public static native void notifyUriVisited(String uri);

    public static native void processNextNativeEvent();

    public static native void notifyBatteryChange(double aLevel, boolean aCharging, double aRemainingTime);

    public static native void notifySmsReceived(String aSender, String aBody, long aTimestamp);
    public static native ByteBuffer allocateDirectBuffer(long size);
    public static native void freeDirectBuffer(ByteBuffer buf);
    public static native void bindWidgetTexture();
    public static native boolean testDirectTexture();

    // A looper thread, accessed by GeckoAppShell.getHandler
    private static class LooperThread extends Thread {
        public SynchronousQueue<Handler> mHandlerQueue =
            new SynchronousQueue<Handler>();
        
        public void run() {
            setName("GeckoLooper Thread");
            Looper.prepare();
            try {
                mHandlerQueue.put(new Handler());
            } catch (InterruptedException ie) {}
            Looper.loop();
        }
    }

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

    private static Handler sHandler = null;

    // Get a Handler for a looper thread, or create one if it doesn't exist yet
    public static Handler getHandler() {
        if (sHandler == null) {
            LooperThread lt = new LooperThread();
            lt.start();
            try {
                sHandler = lt.mHandlerQueue.take();
            } catch (InterruptedException ie) {}
        }
        return sHandler;
    }

    public static File getCacheDir() {
        if (sCacheFile == null)
            sCacheFile = GeckoApp.mAppContext.getCacheDir();
        return sCacheFile;
    }

    public static long getFreeSpace() {
        try {
            if (sFreeSpace == -1) {
                File cacheDir = getCacheDir();
                if (cacheDir != null) {
                    StatFs cacheStats = new StatFs(cacheDir.getPath());
                    sFreeSpace = cacheStats.getFreeBlocks() *
                        cacheStats.getBlockSize();
                } else {
                    Log.i(LOGTAG, "Unable to get cache dir");
                }
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while stating cache dir: ", e);
        }
        return sFreeSpace;
    }

    static boolean moveFile(File inFile, File outFile)
    {
        Log.i(LOGTAG, "moving " + inFile + " to " + outFile);
        if (outFile.isDirectory())
            outFile = new File(outFile, inFile.getName());
        try {
            if (inFile.renameTo(outFile))
                return true;
        } catch (SecurityException se) {
            Log.w(LOGTAG, "error trying to rename file", se);
        }
        try {
            long lastModified = inFile.lastModified();
            outFile.createNewFile();
            // so copy it instead
            FileChannel inChannel = new FileInputStream(inFile).getChannel();
            FileChannel outChannel = new FileOutputStream(outFile).getChannel();
            long size = inChannel.size();
            long transferred = inChannel.transferTo(0, size, outChannel);
            inChannel.close();
            outChannel.close();
            outFile.setLastModified(lastModified);

            if (transferred == size)
                inFile.delete();
            else
                return false;
        } catch (Exception e) {
            Log.e(LOGTAG, "exception while moving file: ", e);
            try {
                outFile.delete();
            } catch (SecurityException se) {
                Log.w(LOGTAG, "error trying to delete file", se);
            }
            return false;
        }
        return true;
    }

    static boolean moveDir(File from, File to) {
        try {
            to.mkdirs();
            if (from.renameTo(to))
                return true;
        } catch (SecurityException se) {
            Log.w(LOGTAG, "error trying to rename file", se);
        }
        File[] files = from.listFiles();
        boolean retVal = true;
        if (files == null)
            return false;
        try {
            Iterator fileIterator = Arrays.asList(files).iterator();
            while (fileIterator.hasNext()) {
                File file = (File)fileIterator.next();
                File dest = new File(to, file.getName());
                if (file.isDirectory())
                    retVal = moveDir(file, dest) ? retVal : false;
                else
                    retVal = moveFile(file, dest) ? retVal : false;
            }
            from.delete();
        } catch(Exception e) {
            Log.e(LOGTAG, "error trying to move file", e);
        }
        return retVal;
    }

    // java-side stuff
    public static void loadGeckoLibs(String apkName) {
        // The package data lib directory isn't placed in ld.so's
        // search path, so we have to manually load libraries that
        // libxul will depend on.  Not ideal.
        GeckoApp geckoApp = GeckoApp.mAppContext;
        String homeDir;
        if (Build.VERSION.SDK_INT < 8 ||
            geckoApp.getApplication().getPackageResourcePath().startsWith("/data") ||
            geckoApp.getApplication().getPackageResourcePath().startsWith("/system")) {
            sHomeDir = geckoApp.getFilesDir();
            homeDir = sHomeDir.getPath();
            // handle the application being moved to phone from sdcard
            File profileDir = new File(homeDir, "mozilla");
            File oldHome = new File("/data/data/" + 
                        GeckoApp.mAppContext.getPackageName() + "/mozilla");
            if (oldHome.exists())
                moveDir(oldHome, profileDir);
            if (Build.VERSION.SDK_INT >= 8) {
                File extHome =  geckoApp.getExternalFilesDir(null);
                File extProf = new File (extHome, "mozilla");
                if (extHome != null && extProf != null && extProf.exists())
                    moveDir(extProf, profileDir);
            }
        } else {
            sHomeDir = geckoApp.getExternalFilesDir(null);
            homeDir = sHomeDir.getPath();
            // handle the application being moved to phone from sdcard
            File profileDir = new File(homeDir, "mozilla");
            File oldHome = new File("/data/data/" + 
                        GeckoApp.mAppContext.getPackageName() + "/mozilla");
            if (oldHome.exists())
                moveDir(oldHome, profileDir);

            File intHome =  geckoApp.getFilesDir();
            File intProf = new File(intHome, "mozilla");
            if (intHome != null && intProf != null && intProf.exists())
                moveDir(intProf, profileDir);
        }
        try {
            String[] dirs = GeckoApp.mAppContext.getPluginDirectories();
            StringBuffer pluginSearchPath = new StringBuffer();
            for (int i = 0; i < dirs.length; i++) {
                Log.i(LOGTAG, "dir: " + dirs[i]);
                pluginSearchPath.append(dirs[i]);
                pluginSearchPath.append(":");
            }
            GeckoAppShell.putenv("MOZ_PLUGIN_PATH="+pluginSearchPath);
        } catch (Exception ex) {
            Log.i(LOGTAG, "exception getting plugin dirs", ex);
        }

        GeckoAppShell.putenv("HOME=" + homeDir);
        GeckoAppShell.putenv("GRE_HOME=" + GeckoApp.sGREDir.getPath());
        Intent i = geckoApp.getIntent();
        String env = i.getStringExtra("env0");
        Log.i(LOGTAG, "env0: "+ env);
        for (int c = 1; env != null; c++) {
            GeckoAppShell.putenv(env);
            env = i.getStringExtra("env" + c);
            Log.i(LOGTAG, "env"+ c +": "+ env);
        }

        File f = geckoApp.getDir("tmp", Context.MODE_WORLD_READABLE |
                                 Context.MODE_WORLD_WRITEABLE );

        if (!f.exists())
            f.mkdirs();

        GeckoAppShell.putenv("TMPDIR=" + f.getPath());

        f = Environment.getDownloadCacheDirectory();
        GeckoAppShell.putenv("EXTERNAL_STORAGE=" + f.getPath());

        File cacheFile = getCacheDir();
        GeckoAppShell.putenv("CACHE_PATH=" + cacheFile.getPath());

        File pluginDataDir = GeckoApp.mAppContext.getDir("plugins", 0);
        GeckoAppShell.putenv("ANDROID_PLUGIN_DATADIR=" + pluginDataDir.getPath());

        // gingerbread introduces File.getUsableSpace(). We should use that.
        long freeSpace = getFreeSpace();
        try {
            File downloadDir = null;
            File updatesDir  = null;
            if (Build.VERSION.SDK_INT >= 8) {
                downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
                updatesDir  = GeckoApp.mAppContext.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
            } else {
                updatesDir = downloadDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");
            }
            GeckoAppShell.putenv("DOWNLOADS_DIRECTORY=" + downloadDir.getPath());
            GeckoAppShell.putenv("UPDATES_DIRECTORY="   + updatesDir.getPath());
        }
        catch (Exception e) {
            Log.i(LOGTAG, "No download directory has been found: " + e);
        }

        putLocaleEnv();

        boolean extractLibs = GeckoApp.ACTION_DEBUG.equals(i.getAction());
        if (!extractLibs) {
            // remove any previously extracted libs
            File[] files = cacheFile.listFiles();
            if (files != null) {
                Iterator cacheFiles = Arrays.asList(files).iterator();
                while (cacheFiles.hasNext()) {
                    File libFile = (File)cacheFiles.next();
                    if (libFile.getName().endsWith(".so"))
                        libFile.delete();
                }
            }
        }
        loadLibs(apkName, extractLibs);
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

    public static void runGecko(String apkPath, String args, String url) {
        // run gecko -- it will spawn its own thread
        GeckoAppShell.nativeInit();

        Log.i(LOGTAG, "post native init");

        // If we have direct texture available, use it
        if (GeckoAppShell.testDirectTexture()) {
            Log.i(LOGTAG, "Using direct texture for widget layer");
            GeckoApp.mAppContext.getSoftwareLayerClient().installWidgetLayer();
        } else {
            Log.i(LOGTAG, "Falling back to traditional texture upload");
        }

        // Tell Gecko where the target byte buffer is for rendering
        GeckoAppShell.setSoftwareLayerClient(GeckoApp.mAppContext.getSoftwareLayerClient());

        Log.i(LOGTAG, "setSoftwareLayerClient called");

        // First argument is the .apk path
        String combinedArgs = apkPath + " -greomni " + apkPath;
        if (args != null)
            combinedArgs += " " + args;
        if (url != null)
            combinedArgs += " -remote " + url;

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
        final LayerController layerController = GeckoApp.mAppContext.getLayerController();
        LayerView v = layerController.getView();
        mInputConnection = GeckoInputConnection.create(v);
        v.setInputConnectionHandler(mInputConnection);

        layerController.setOnTouchListener(new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                return true;
            }
        });

        layerController.notifyLayerClientOfGeometryChange();
    }

    static void sendPendingEventsToGecko() {
        try {
            while (!gPendingEvents.isEmpty()) {
                GeckoEvent e = gPendingEvents.removeFirst();
                notifyGeckoOfEvent(e);
            }
        } catch (NoSuchElementException e) {}
    }

    public static void sendEventToGecko(GeckoEvent e) {
        if (GeckoApp.mAppContext.checkLaunchState(GeckoApp.LaunchState.GeckoRunning)) {
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
        if (mInputConnection != null)
            mInputConnection.notifyIME(type, state);
    }

    public static void notifyIMEEnabled(int state, String typeHint,
                                        String actionHint, boolean landscapeFS) {
        if (mInputConnection != null)
            mInputConnection.notifyIMEEnabled(state, typeHint, actionHint, landscapeFS);
    }

    public static void notifyIMEChange(String text, int start, int end, int newEnd) {
        if (mInputConnection != null)
            mInputConnection.notifyIMEChange(text, start, end, newEnd);
    }

    private static CountDownLatch sGeckoPendingAcks = null;

    // Block the current thread until the Gecko event loop is caught up
    synchronized public static void geckoEventSync() {
        sGeckoPendingAcks = new CountDownLatch(1);
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.GECKO_EVENT_SYNC));
        while (sGeckoPendingAcks.getCount() != 0) {
            try {
                sGeckoPendingAcks.await();
            } catch (InterruptedException e) {}
        }
        sGeckoPendingAcks = null;
    }

    // Signal the Java thread that it's time to wake up
    public static void acknowledgeEventSync() {
        CountDownLatch tmp = sGeckoPendingAcks;
        if (tmp != null)
            tmp.countDown();
    }

    static Sensor gAccelerometerSensor = null;
    static Sensor gOrientationSensor = null;

    public static void enableDeviceMotion(boolean enable) {
        LayerView v = GeckoApp.mAppContext.getLayerController().getView();
        SensorManager sm = (SensorManager) v.getContext().getSystemService(Context.SENSOR_SERVICE);

        if (gAccelerometerSensor == null || gOrientationSensor == null) {
            gAccelerometerSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            gOrientationSensor   = sm.getDefaultSensor(Sensor.TYPE_ORIENTATION);
        }

        if (enable) {
            if (gAccelerometerSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gAccelerometerSensor, SensorManager.SENSOR_DELAY_GAME);
            if (gOrientationSensor != null)
                sm.registerListener(GeckoApp.mAppContext, gOrientationSensor,   SensorManager.SENSOR_DELAY_GAME);
        } else {
            if (gAccelerometerSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gAccelerometerSensor);
            if (gOrientationSensor != null)
                sm.unregisterListener(GeckoApp.mAppContext, gOrientationSensor);
        }
    }

    public static void enableLocation(final boolean enable) {
        getMainHandler().post(new Runnable() { 
                public void run() {
                    LayerView v = GeckoApp.mAppContext.getLayerController().getView();

                    LocationManager lm = (LocationManager)
                        GeckoApp.mAppContext.getSystemService(Context.LOCATION_SERVICE);

                    if (enable) {
                        Criteria crit = new Criteria();
                        crit.setAccuracy(Criteria.ACCURACY_FINE);
                        String provider = lm.getBestProvider(crit, true);
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

    public static void moveTaskToBack() {
        GeckoApp.mAppContext.moveTaskToBack(true);
    }

    public static void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        mInputConnection.returnIMEQueryResult(result, selectionStart, selectionLength);
    }

    static void onXreExit() {
        // mLaunchState can only be Launched or GeckoRunning at this point
        GeckoApp.mAppContext.setLaunchState(GeckoApp.LaunchState.GeckoExiting);
        Log.i(LOGTAG, "XRE exited");
        if (gRestartScheduled) {
            GeckoApp.mAppContext.doRestart();
        } else {
            Log.i(LOGTAG, "we're done, good bye");
            GeckoApp.mAppContext.finish();
        }

        Log.w(LOGTAG, "Killing via System.exit()");
        System.exit(0);
    }
    static void scheduleRestart() {
        Log.i(LOGTAG, "scheduling restart");
        gRestartScheduled = true;
    }

    // "Installs" an application by creating a shortcut
    static void createShortcut(String aTitle, String aURI, String aIconData, String aType) {
        byte[] raw = Base64.decode(aIconData.substring(22), Base64.DEFAULT);
        Bitmap bitmap = BitmapFactory.decodeByteArray(raw, 0, raw.length);
        createShortcut(aTitle, aURI, bitmap, aType);
    }

    public static void createShortcut(final String aTitle, final String aURI, final Bitmap aIcon, final String aType) {
        getHandler().post(new Runnable() {
            public void run() {
                Log.w(LOGTAG, "createShortcut for " + aURI + " [" + aTitle + "] > " + aType);
        
                // the intent to be launched by the shortcut
                Intent shortcutIntent = new Intent();
                if (aType.equalsIgnoreCase("webapp")) {
                    shortcutIntent.setAction("org.mozilla.gecko.WEBAPP");
                    shortcutIntent.putExtra("args", aURI);
                } else {
                    shortcutIntent.setAction("org.mozilla.gecko.BOOKMARK");
                    shortcutIntent.putExtra("args", aURI);
                }
                shortcutIntent.setClassName(GeckoApp.mAppContext,
                                            GeckoApp.mAppContext.getPackageName() + ".App");
        
                Intent intent = new Intent();
                intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
                if (aTitle != null)
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
                else
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);
                intent.putExtra(Intent.EXTRA_SHORTCUT_ICON, getLauncherIcon(aIcon));
                intent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
                GeckoApp.mAppContext.sendBroadcast(intent);
            }
        });
    }

    static private Bitmap getLauncherIcon(Bitmap aSource) {
        // The background images are 72px, but Android will resize as needed.
        // Bigger is better than too small.
        final int kIconSize = 72;
        final int kOverlaySize = 32;
        final int kOffset = 6;
        final int kRadius = 5;

        Bitmap bitmap = Bitmap.createBitmap(kIconSize, kIconSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        // draw a base color
        Paint paint = new Paint();
        
        if (aSource == null) {
            float[] hsv = new float[3];
            hsv[0] = 32.0f;
            hsv[1] = 1.0f;
            hsv[2] = 1.0f;
            paint.setColor(Color.HSVToColor(hsv));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, kIconSize - kOffset, kIconSize - kOffset), kRadius, kRadius, paint);
        } else {
            int color = BitmapUtils.getDominantColor(aSource);
            paint.setColor(color);
            canvas.drawRoundRect(new RectF(kOffset, kOffset, kIconSize - kOffset, kIconSize - kOffset), kRadius, kRadius, paint);
            paint.setColor(Color.argb(100, 255, 255, 255));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, kIconSize - kOffset, kIconSize - kOffset), kRadius, kRadius, paint);
        }

        // draw the overlay
        Bitmap overlay = BitmapFactory.decodeResource(GeckoApp.mAppContext.getResources(), R.drawable.home_bg);
        canvas.drawBitmap(overlay, null, new Rect(0,0,kIconSize, kIconSize), null);

        // draw the bitmap
        if (aSource == null)
            aSource = BitmapFactory.decodeResource(GeckoApp.mAppContext.getResources(), R.drawable.home_star);

        if (aSource.getWidth() < kOverlaySize || aSource.getHeight() < kOverlaySize) {
            canvas.drawBitmap(aSource,
                              null,
                              new Rect(kIconSize/2 - kOverlaySize/2,
                                       kIconSize/2 - kOverlaySize/2,
                                       kIconSize/2 + kOverlaySize/2,
                                       kIconSize/2 + kOverlaySize/2),
                              null);
        } else {
            canvas.drawBitmap(aSource,
                              null,
                              new Rect(kIconSize/2 - aSource.getWidth()/2,
                                       kIconSize/2 - aSource.getHeight()/2,
                                       kIconSize/2 + aSource.getWidth()/2,
                                       kIconSize/2 + aSource.getHeight()/2),
                              null);
        }

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
        StringTokenizer st = new StringTokenizer(aFileExt, "., ");
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

    static boolean openUriExternal(String aUriSpec, String aMimeType, String aPackageName,
                                   String aClassName, String aAction, String aTitle) {
        Intent intent = getIntentForActionString(aAction);
        if (aAction.equalsIgnoreCase(Intent.ACTION_SEND)) {
            intent.putExtra(Intent.EXTRA_TEXT, aUriSpec);
            intent.putExtra(Intent.EXTRA_SUBJECT, aTitle);
            if (aMimeType != null && aMimeType.length() > 0)
                intent.setType(aMimeType);
        } else if (aMimeType.length() > 0) {
            intent.setDataAndType(Uri.parse(aUriSpec), aMimeType);
        } else {
            Uri uri = Uri.parse(aUriSpec);
            if ("sms".equals(uri.getScheme())) {
                // Have a apecial handling for the SMS, as the message body
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
                        }
                        else {
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
            return (ret.equals(EMPTY_STRING) ? null : ret);
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
        Log.i(LOGTAG, "GeckoAppShell.showAlertNotification\n" +
            "- image = '" + aImageUrl + "'\n" +
            "- title = '" + aAlertTitle + "'\n" +
            "- text = '" + aAlertText +"'\n" +
            "- cookie = '" + aAlertCookie +"'\n" +
            "- name = '" + aAlertName + "'");

        int icon = R.drawable.icon; // Just use the app icon by default

        Uri imageUri = Uri.parse(aImageUrl);
        String scheme = imageUri.getScheme();
        if ("drawable".equals(scheme)) {
            String resource = imageUri.getSchemeSpecificPart();
            resource = resource.substring(resource.lastIndexOf('/') + 1);
            try {
                Class drawableClass = R.drawable.class;
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

        // Put the strings into the intent as an URI "alert:<name>#<cookie>"
        Uri dataUri = Uri.fromParts("alert", aAlertName, aAlertCookie);
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

        Log.i(LOGTAG, "Created notification ID " + notificationID);
    }

    public static void alertsProgressListener_OnProgress(String aAlertName, long aProgress, long aProgressMax, String aAlertText) {
        Log.i(LOGTAG, "GeckoAppShell.alertsProgressListener_OnProgress\n" +
            "- name = '" + aAlertName +"', " +
            "progress = " + aProgress +" / " + aProgressMax + ", text = '" + aAlertText + "'");

        int notificationID = aAlertName.hashCode();
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
        Log.i(LOGTAG, "GeckoAppShell.alertsProgressListener_OnCancel('" + aAlertName + "')");

        removeObserver(aAlertName);

        int notificationID = aAlertName.hashCode();
        removeNotification(notificationID);
    }

    public static void handleNotification(String aAction, String aAlertName, String aAlertCookie) {
        int notificationID = aAlertName.hashCode();

        if (GeckoApp.ACTION_ALERT_CLICK.equals(aAction)) {
            Log.i(LOGTAG, "GeckoAppShell.handleNotification: callObserver(alertclickcallback)");
            callObserver(aAlertName, "alertclickcallback", aAlertCookie);

            AlertNotification notification = mAlertNotifications.get(notificationID);
            if (notification != null && notification.isProgressStyle()) {
                // When clicked, keep the notification, if it displays a progress
                return;
            }
        }

        callObserver(aAlertName, "alertfinished", aAlertCookie);

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
            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            sDensityDpi = metrics.densityDpi;
        }

        return sDensityDpi;
    }

    public static void setFullScreen(boolean fullscreen) {
        GeckoApp.mAppContext.setFullScreen(fullscreen);
    }

    public static String showFilePicker(String aFilters) {
        return GeckoApp.mAppContext.
            showFilePicker(getMimeTypeFromExtensions(aFilters));
    }

    public static void performHapticFeedback(boolean aIsLongPress) {
        LayerController layerController = GeckoApp.mAppContext.getLayerController();
        LayerView layerView = layerController.getView();
        layerView.performHapticFeedback(aIsLongPress ?
                                        HapticFeedbackConstants.LONG_PRESS :
                                        HapticFeedbackConstants.VIRTUAL_KEY);
    }

    private static Vibrator vibrator() {
        LayerController layerController = GeckoApp.mAppContext.getLayerController();
        LayerView layerView = layerController.getView();

        return (Vibrator) layerView.getContext().getSystemService(Context.VIBRATOR_SERVICE);
    }

    public static void vibrate(long milliseconds) {
        vibrator().vibrate(milliseconds);
    }

    public static void vibrate(long[] pattern, int repeat) {
        vibrator().vibrate(pattern, repeat);
    }

    public static void cancelVibrate() {
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
        /* We're not using this, not need to save it (see bug 635342)
          SharedPreferences settings =
            GeckoApp.mAppContext.getPreferences(Activity.MODE_PRIVATE);
        settings.edit().putString(GeckoApp.mAppContext.getPackageName() + ".locale",
                                  localeCode).commit();
        */
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
            Log.i(LOGTAG, "finding procs throws ",  e);
        }
    }

    public static void waitForAnotherGeckoProc(){
        int countdown = 40;
        while (!checkForGeckoProcs() &&  --countdown > 0) {
            try {
                Thread.currentThread().sleep(100);
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
            Log.i(LOGTAG, "getIconForExtension error: ",  e);
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

    public static boolean getAccessibilityEnabled() {
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) GeckoApp.mAppContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        return accessibilityManager.isEnabled();
    }

    public static void addPluginView(View view,
                                     double x, double y,
                                     double w, double h)
    {
        Log.i(LOGTAG, "addPluginView:" + view + " @ x:" + x + " y:" + y + " w:" + w + " h:" + h ) ;
        GeckoApp.mAppContext.addPluginView(view, x, y, w, h);
    }

    public static void removePluginView(View view) {
        Log.i(LOGTAG, "remove view:" + view);
        GeckoApp.mAppContext.removePluginView(view);
    }

    public static Class<?> loadPluginClass(String className, String libName) {
        Log.i(LOGTAG, "in loadPluginClass... attempting to access className, then libName.....");
        Log.i(LOGTAG, "className: " + className);
        Log.i(LOGTAG, "libName: " + libName);

        try {
            String packageName = GeckoApp.mAppContext.getPluginPackage(libName);
            Log.i(LOGTAG, "load \"" + className + "\" from \"" + packageName + 
                  "\" for \"" + libName + "\"");
            Context pluginContext = 
                GeckoApp.mAppContext.createPackageContext(packageName,
                                                          Context.CONTEXT_INCLUDE_CODE |
                                                      Context.CONTEXT_IGNORE_SECURITY);
            ClassLoader pluginCL = pluginContext.getClassLoader();
            return pluginCL.loadClass(className);
        } catch (java.lang.ClassNotFoundException cnfe) {
            Log.i(LOGTAG, "class not found", cnfe);
        } catch (android.content.pm.PackageManager.NameNotFoundException nnfe) {
            Log.i(LOGTAG, "package not found", nnfe);
        }
        Log.e(LOGTAG, "couldn't find class");
        return null;
    }

    static native void executeNextRunnable();

    static class GeckoRunnableCallback implements Runnable {
        public void run() {
            Log.i(LOGTAG, "run GeckoRunnableCallback");
            GeckoAppShell.executeNextRunnable();
        }
    }

    public static void postToJavaThread(boolean mainThread) {
        Log.i(LOGTAG, "post to " + (mainThread ? "main " : "") + "java thread");
        getMainHandler().post(new GeckoRunnableCallback());
    }
    
    public static android.hardware.Camera sCamera = null;
    
    static native void cameraCallbackBridge(byte[] data);

    static int kPreferedFps = 25;
    static byte[] sCameraBuffer = null;

    static int[] initCamera(String aContentType, int aCamera, int aWidth, int aHeight) {
        Log.i(LOGTAG, "initCamera(" + aContentType + ", " + aWidth + "x" + aHeight + ") on thread " + Thread.currentThread().getId());

        getMainHandler().post(new Runnable() {
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
                sCamera.setPreviewDisplay(GeckoApp.cameraView.getHolder());
            } catch(IOException e) {
                Log.e(LOGTAG, "Error setPreviewDisplay:", e);
            } catch(RuntimeException e) {
                Log.e(LOGTAG, "Error setPreviewDisplay:", e);
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
            Log.i(LOGTAG, "Camera: " + params.getPreviewSize().width + "x" + params.getPreviewSize().height +
                  " @ " + params.getPreviewFrameRate() + "fps. format is " + params.getPreviewFormat());
            result[0] = 1;
            result[1] = params.getPreviewSize().width;
            result[2] = params.getPreviewSize().height;
            result[3] = params.getPreviewFrameRate();

            Log.i(LOGTAG, "Camera preview started");
        } catch(RuntimeException e) {
            Log.e(LOGTAG, "initCamera RuntimeException : ", e);
            result[0] = result[1] = result[2] = result[3] = 0;
        }
        return result;
    }

    static synchronized void closeCamera() {
        Log.i(LOGTAG, "closeCamera() on thread " + Thread.currentThread().getId());
        getMainHandler().post(new Runnable() {
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

    static SynchronousQueue<String> sPromptQueue = null;

    public static void registerGeckoEventListener(String event, GeckoEventListener listener) {
        if (mEventListeners == null)
            mEventListeners = new HashMap<String, ArrayList<GeckoEventListener>>();

        if (!mEventListeners.containsKey(event))
            mEventListeners.put(event, new ArrayList<GeckoEventListener>());

        ArrayList<GeckoEventListener> listeners = mEventListeners.get(event);
        listeners.add(listener);
    }

    static SynchronousQueue<Date> sTracerQueue = new SynchronousQueue<Date>();
    public static void fireAndWaitForTracerEvent() {
        getMainHandler().post(new Runnable() { 
                public void run() {
                    try {
                        sTracerQueue.put(new Date());
                    } catch(InterruptedException ie) {
                        Log.w(LOGTAG, "exception firing tracer", ie);
                    }
                }
        });
        try {
            sTracerQueue.take();
        } catch(InterruptedException ie) {
            Log.w(LOGTAG, "exception firing tracer", ie);
        }
    }

    public static void unregisterGeckoEventListener(String event, GeckoEventListener listener) {
        if (mEventListeners == null)
            return;

        if (!mEventListeners.containsKey(event))
            return;

        ArrayList<GeckoEventListener> listeners = mEventListeners.get(event);
        listeners.remove(listener);
    }

    /*
     * Battery API related methods.
     */
    public static void enableBatteryNotifications() {
        GeckoBatteryManager.enableNotifications();
    }

    public static String handleGeckoMessage(String message) {
        //        
        //        {"gecko": {
        //                "type": "value",
        //                "event_specific": "value",
        //                ....
        try {
            JSONObject json = new JSONObject(message);
            final JSONObject geckoObject = json.getJSONObject("gecko");
            String type = geckoObject.getString("type");
            
            if (type.equals("Prompt:Show")) {
                if (sPromptQueue == null)
                    sPromptQueue = new SynchronousQueue<String>();
                getHandler().post(new Runnable() {
                    public void run() {
                        getPromptService().processMessage(geckoObject);
                    }
                });

                String promptServiceResult = "";
                try {
                    while (null == (promptServiceResult = sPromptQueue.poll(1, TimeUnit.MILLISECONDS))) {
                        processNextNativeEvent();
                    }
                } catch (InterruptedException e) {
                    Log.i(LOGTAG, "showing prompt ",  e);
                }
                return promptServiceResult;
            }
            
            if (mEventListeners == null)
                return "";

            if (!mEventListeners.containsKey(type))
                return "";
            
            ArrayList<GeckoEventListener> listeners = mEventListeners.get(type);
            Iterator items = listeners.iterator();
            while (items.hasNext()) {
                ((GeckoEventListener) items.next()).handleMessage(type, geckoObject);
            }

        } catch (Exception e) {
            Log.i(LOGTAG, "handleGeckoMessage throws " + e);
        }

        return "";
    }

    public static void disableBatteryNotifications() {
        GeckoBatteryManager.disableNotifications();
    }

    public static PromptService getPromptService() {
        if (gPromptService == null) {
            gPromptService = new PromptService();
        }
        return gPromptService;
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
        return GeckoSmsManager.getNumberOfMessagesForText(aText);
    }

    public static void sendMessage(String aNumber, String aMessage) {
        GeckoSmsManager.send(aNumber, aMessage);
    }

    public static boolean isTablet() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
            Configuration config = GeckoApp.mAppContext.getResources().getConfiguration();
            // xlarge is defined by android as screens larger than 960dp x 720dp
            // and should include most devices ~7in and up.
            // http://developer.android.com/guide/practices/screens_support.html
            if ((config.screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_XLARGE) {
                return true;
            }
        }
        return false;
    }

    public static void emitGeckoAccessibilityEvent (int eventType, String role, String text, String description, boolean enabled, boolean checked, boolean password) {
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) GeckoApp.mAppContext.getSystemService(Context.ACCESSIBILITY_SERVICE);

        if (!accessibilityManager.isEnabled())
            return;

        LayerController layerController = GeckoApp.mAppContext.getLayerController();
        LayerView layerView = layerController.getView();

        AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setClassName(layerView.getClass().getName() + "$" + role);
        event.setPackageName(GeckoApp.mAppContext.getPackageName());
        event.setEnabled(enabled);
        event.setChecked(checked);
        event.setPassword(password);
        event.setContentDescription(description);
        event.getText().add(text);

        accessibilityManager.sendAccessibilityEvent(event);
    }

    public static void viewSizeChanged() {
        if (mInputConnection != null && mInputConnection.isIMEEnabled()) {
            sendEventToGecko(new GeckoEvent("ScrollTo:FocusedInput", ""));
        }
    }
}
