/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.net.MalformedURLException;
import java.net.Proxy;
import java.net.URLConnection;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.TreeMap;

import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.BitmapUtils;
import org.mozilla.gecko.util.HardwareCodecCapabilityUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.StrictModeContext;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.BuildConfig;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.hardware.Camera;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Looper;
import android.os.PowerManager;
import android.os.Vibrator;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.v4.util.SimpleArrayMap;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.WindowManager;
import android.webkit.MimeTypeMap;

public class GeckoAppShell {
    private static final String LOGTAG = "GeckoAppShell";

    // We have static members only.
    private GeckoAppShell() { }

    private static class GeckoCrashHandler extends CrashHandler {

        public GeckoCrashHandler(final Class<? extends Service> handlerService) {
            super(handlerService);
        }

        @Override
        protected String getAppPackageName() {
            final Context appContext = getAppContext();
            if (appContext == null) {
                return "<unknown>";
            }
            return appContext.getPackageName();
        }

        @Override
        protected Context getAppContext() {
            return getApplicationContext();
        }

        @Override
        protected Bundle getCrashExtras(final Thread thread, final Throwable exc) {
            final Bundle extras = super.getCrashExtras(thread, exc);

            extras.putString("ProductName", BuildConfig.MOZ_APP_BASENAME);
            extras.putString("ProductID", BuildConfig.MOZ_APP_ID);
            extras.putString("Version", BuildConfig.MOZ_APP_VERSION);
            extras.putString("BuildID", BuildConfig.MOZ_APP_BUILDID);
            extras.putString("Vendor", BuildConfig.MOZ_APP_VENDOR);
            extras.putString("ReleaseChannel", BuildConfig.MOZ_UPDATE_CHANNEL);

            final String appNotes = getAppNotes();
            if (appNotes != null) {
                extras.putString("Notes", appNotes);
            }
            return extras;
        }

        @Override
        public boolean reportException(final Thread thread, final Throwable exc) {
            try {
                if (exc instanceof OutOfMemoryError) {
                    final SharedPreferences prefs =
                            GeckoSharedPrefs.forApp(getApplicationContext());
                    final SharedPreferences.Editor editor = prefs.edit();
                    editor.putBoolean(PREFS_OOM_EXCEPTION, true);

                    // Synchronously write to disk so we know it's done before we
                    // shutdown
                    editor.commit();
                }

                reportJavaCrash(exc, getExceptionStackTrace(exc));

            } catch (final Throwable e) {
            }

            // reportJavaCrash should have caused us to hard crash. If we're still here,
            // it probably means Gecko is not loaded, and we should do something else.
            if (BuildConfig.MOZ_CRASHREPORTER && BuildConfig.MOZILLA_OFFICIAL) {
                // Only use Java crash reporter if enabled on official build.
                return super.reportException(thread, exc);
            }
            return false;
        }
    };

    private static String sAppNotes;
    private static CrashHandler sCrashHandler;

    public static synchronized CrashHandler ensureCrashHandling(final Class<? extends Service> handler) {
        if (sCrashHandler == null) {
            sCrashHandler = new GeckoCrashHandler(handler);
        }

        return sCrashHandler;
    }

    private static Class<? extends Service> sCrashHandlerService;
    public static synchronized void setCrashHandlerService(final Class<? extends Service> handlerService) {
        sCrashHandlerService = handlerService;
    }

    public static synchronized Class<? extends Service> getCrashHandlerService() {
        return sCrashHandlerService;
    }

    public static synchronized boolean isCrashHandlingEnabled() {
        return sCrashHandler != null;
    }

    @WrapForJNI(exceptionMode = "ignore")
    /* package */ static synchronized String getAppNotes() {
        return sAppNotes;
    }

    public static synchronized void appendAppNotesToCrashReport(final String notes) {
        if (sAppNotes == null) {
            sAppNotes = notes;
        } else {
            sAppNotes += '\n' + notes;
        }
    }

    private static volatile boolean locationHighAccuracyEnabled;

    // See also HardwareUtils.LOW_MEMORY_THRESHOLD_MB.
    private static final int HIGH_MEMORY_DEVICE_THRESHOLD_MB = 768;

    static private int sDensityDpi;
    static private Float sDensity;
    static private int sScreenDepth;
    static private boolean sUseMaxScreenDepth;

    /* Is the value in sVibrationEndTime valid? */
    private static boolean sVibrationMaybePlaying;

    /* Time (in System.nanoTime() units) when the currently-playing vibration
     * is scheduled to end.  This value is valid only when
     * sVibrationMaybePlaying is true. */
    private static long sVibrationEndTime;

    private static Sensor gAccelerometerSensor;
    private static Sensor gLinearAccelerometerSensor;
    private static Sensor gGyroscopeSensor;
    private static Sensor gOrientationSensor;
    private static Sensor gProximitySensor;
    private static Sensor gLightSensor;
    private static Sensor gRotationVectorSensor;
    private static Sensor gGameRotationVectorSensor;

    /*
     * Keep in sync with constants found here:
     * http://dxr.mozilla.org/mozilla-central/source/uriloader/base/nsIWebProgressListener.idl
    */
    static public final int WPL_STATE_START = 0x00000001;
    static public final int WPL_STATE_STOP = 0x00000010;
    static public final int WPL_STATE_IS_DOCUMENT = 0x00020000;
    static public final int WPL_STATE_IS_NETWORK = 0x00040000;

    /* Keep in sync with constants found here:
      http://dxr.mozilla.org/mozilla-central/source/netwerk/base/nsINetworkLinkService.idl
    */
    static public final int LINK_TYPE_UNKNOWN = 0;
    static public final int LINK_TYPE_ETHERNET = 1;
    static public final int LINK_TYPE_USB = 2;
    static public final int LINK_TYPE_WIFI = 3;
    static public final int LINK_TYPE_WIMAX = 4;
    static public final int LINK_TYPE_2G = 5;
    static public final int LINK_TYPE_3G = 6;
    static public final int LINK_TYPE_4G = 7;

    public static final String PREFS_OOM_EXCEPTION = "OOMException";

    /* The Android-side API: API methods that Android calls */

    // helper methods
    @WrapForJNI
    /* package */ static native void reportJavaCrash(Throwable exc, String stackTrace);

    @WrapForJNI(dispatchTo = "gecko")
    public static native void notifyUriVisited(String uri);

    private static Rect sScreenSizeOverride;

    @WrapForJNI(stubName = "NotifyObservers", dispatchTo = "gecko")
    private static native void nativeNotifyObservers(String topic, String data);

    @RobocopTarget
    public static void notifyObservers(final String topic, final String data) {
        notifyObservers(topic, data, GeckoThread.State.RUNNING);
    }

    public static void notifyObservers(final String topic, final String data, final GeckoThread.State state) {
        if (GeckoThread.isStateAtLeast(state)) {
            nativeNotifyObservers(topic, data);
        } else {
            GeckoThread.queueNativeCallUntil(
                    state, GeckoAppShell.class, "nativeNotifyObservers",
                    String.class, topic, String.class, data);
        }
    }

    /*
     *  The Gecko-side API: API methods that Gecko calls
     */

    @WrapForJNI(exceptionMode = "ignore")
    private static String getExceptionStackTrace(final Throwable e) {
        return CrashHandler.getExceptionStackTrace(CrashHandler.getRootException(e));
    }

    @WrapForJNI(exceptionMode = "ignore")
    private static synchronized void handleUncaughtException(final Throwable e) {
        if (sCrashHandler != null) {
            sCrashHandler.uncaughtException(null, e);
        }
    }

    private static float getLocationAccuracy(final Location location) {
        float radius = location.getAccuracy();
        return (location.hasAccuracy() && radius > 0) ? radius : 1001;
    }

    // Permissions are explicitly checked when requesting content permission.
    @SuppressLint("MissingPermission")
    private static Location getLastKnownLocation(final LocationManager lm) {
        Location lastKnownLocation = null;
        List<String> providers = lm.getAllProviders();

        for (String provider : providers) {
            Location location = lm.getLastKnownLocation(provider);
            if (location == null) {
                continue;
            }

            if (lastKnownLocation == null) {
                lastKnownLocation = location;
                continue;
            }

            long timeDiff = location.getTime() - lastKnownLocation.getTime();
            if (timeDiff > 0 ||
                (timeDiff == 0 &&
                 getLocationAccuracy(location) < getLocationAccuracy(lastKnownLocation))) {
                lastKnownLocation = location;
            }
        }

        return lastKnownLocation;
    }

    @WrapForJNI(calledFrom = "gecko")
    // Permissions are explicitly checked when requesting content permission.
    @SuppressLint("MissingPermission")
    private static synchronized boolean enableLocation(final boolean enable) {
        final LocationManager lm = getLocationManager(getApplicationContext());
        if (lm == null) {
            return false;
        }

        if (!enable) {
            lm.removeUpdates(getLocationListener());
            return true;
        }

        if (!lm.isProviderEnabled(LocationManager.GPS_PROVIDER) &&
            !lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            return false;
        }

        final Location lastKnownLocation = getLastKnownLocation(lm);
        if (lastKnownLocation != null) {
            getLocationListener().onLocationChanged(lastKnownLocation);
        }

        final Criteria criteria = new Criteria();
        criteria.setSpeedRequired(false);
        criteria.setBearingRequired(false);
        criteria.setAltitudeRequired(false);
        if (locationHighAccuracyEnabled) {
            criteria.setAccuracy(Criteria.ACCURACY_FINE);
            criteria.setCostAllowed(true);
            criteria.setPowerRequirement(Criteria.POWER_HIGH);
        } else {
            criteria.setAccuracy(Criteria.ACCURACY_COARSE);
            criteria.setCostAllowed(false);
            criteria.setPowerRequirement(Criteria.POWER_LOW);
        }

        final String provider = lm.getBestProvider(criteria, true);
        if (provider == null) {
            return false;
        }

        final Looper l = Looper.getMainLooper();
        lm.requestLocationUpdates(provider, 100, 0.5f, getLocationListener(), l);
        return true;
    }

    private static LocationManager getLocationManager(final Context context) {
        try {
            return (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        } catch (NoSuchFieldError e) {
            // Some Tegras throw exceptions about missing the CONTROL_LOCATION_UPDATES permission,
            // which allows enabling/disabling location update notifications from the cell radio.
            // CONTROL_LOCATION_UPDATES is not for use by normal applications, but we might be
            // hitting this problem if the Tegras are confused about missing cell radios.
            Log.e(LOGTAG, "LOCATION_SERVICE not found?!", e);
            return null;
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void enableLocationHighAccuracy(final boolean enable) {
        locationHighAccuracyEnabled = enable;
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    /* package */ static native void onSensorChanged(int halType, float x, float y, float z,
                                                     float w, long time);

    @WrapForJNI(calledFrom = "any", dispatchTo = "gecko")
    /* package */ static native void onLocationChanged(double latitude, double longitude,
                                                       double altitude, float accuracy,
                                                       float altitudeAccuracy,
                                                       float heading, float speed, long time);

    private static class DefaultListeners implements SensorEventListener,
                                                     LocationListener,
                                                     NotificationListener,
                                                     ScreenOrientationDelegate,
                                                     WakeLockDelegate,
                                                     HapticFeedbackDelegate {
        @Override
        public void onAccuracyChanged(final Sensor sensor, final int accuracy) {
        }

        @Override
        public void onSensorChanged(final SensorEvent s) {
            int sensorType = s.sensor.getType();
            int halType = 0;
            float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
            // SensorEvent timestamp is in nanoseconds, Gecko expects microseconds.
            final long time = s.timestamp / 1000;

            switch (sensorType) {
                case Sensor.TYPE_ACCELEROMETER:
                case Sensor.TYPE_LINEAR_ACCELERATION:
                case Sensor.TYPE_ORIENTATION:
                    if (sensorType == Sensor.TYPE_ACCELEROMETER) {
                        halType = GeckoHalDefines.SENSOR_ACCELERATION;
                    } else if (sensorType == Sensor.TYPE_LINEAR_ACCELERATION) {
                        halType = GeckoHalDefines.SENSOR_LINEAR_ACCELERATION;
                    } else {
                        halType = GeckoHalDefines.SENSOR_ORIENTATION;
                    }
                    x = s.values[0];
                    y = s.values[1];
                    z = s.values[2];
                    break;

                case Sensor.TYPE_GYROSCOPE:
                    halType = GeckoHalDefines.SENSOR_GYROSCOPE;
                    x = (float) Math.toDegrees(s.values[0]);
                    y = (float) Math.toDegrees(s.values[1]);
                    z = (float) Math.toDegrees(s.values[2]);
                    break;

                case Sensor.TYPE_PROXIMITY:
                    halType = GeckoHalDefines.SENSOR_PROXIMITY;
                    x = s.values[0];
                    z = s.sensor.getMaximumRange();
                    break;

                case Sensor.TYPE_LIGHT:
                    halType = GeckoHalDefines.SENSOR_LIGHT;
                    x = s.values[0];
                    break;

                case Sensor.TYPE_ROTATION_VECTOR:
                case Sensor.TYPE_GAME_ROTATION_VECTOR: // API >= 18
                    halType = (sensorType == Sensor.TYPE_ROTATION_VECTOR ?
                            GeckoHalDefines.SENSOR_ROTATION_VECTOR :
                            GeckoHalDefines.SENSOR_GAME_ROTATION_VECTOR);
                    x = s.values[0];
                    y = s.values[1];
                    z = s.values[2];
                    if (s.values.length >= 4) {
                        w = s.values[3];
                    } else {
                        // s.values[3] was optional in API <= 18, so we need to compute it
                        // The values form a unit quaternion, so we can compute the angle of
                        // rotation purely based on the given 3 values.
                        w = 1.0f - s.values[0] * s.values[0] -
                                s.values[1] * s.values[1] - s.values[2] * s.values[2];
                        w = (w > 0.0f) ? (float) Math.sqrt(w) : 0.0f;
                    }
                    break;
            }

            GeckoAppShell.onSensorChanged(halType, x, y, z, w, time);
        }

        // Geolocation.
        @Override
        public void onLocationChanged(final Location location) {
            // No logging here: user-identifying information.

            double altitude = location.hasAltitude()
                            ? location.getAltitude()
                            : Double.NaN;

            float accuracy = location.hasAccuracy()
                           ? location.getAccuracy()
                           : Float.NaN;

            float altitudeAccuracy = Build.VERSION.SDK_INT >= 26 &&
                                     location.hasVerticalAccuracy()
                                   ? location.getVerticalAccuracyMeters()
                                   : Float.NaN;

            float speed = location.hasSpeed()
                        ? location.getSpeed()
                        : Float.NaN;

            float heading = location.hasBearing()
                          ? location.getBearing()
                          : Float.NaN;

            // nsGeoPositionCoords will convert NaNs to null for optional
            // properties of the JavaScript Coordinates object.
            GeckoAppShell.onLocationChanged(
                location.getLatitude(), location.getLongitude(),
                altitude, accuracy, altitudeAccuracy,
                heading, speed, location.getTime());
        }

        @Override
        public void onProviderDisabled(final String provider) {}

        @Override
        public void onProviderEnabled(final String provider) {}

        @Override
        public void onStatusChanged(final String provider, final int status, final Bundle extras) {}

        @Override // NotificationListener
        public void showNotification(final String name, final String cookie, final String host,
                                     final String title, final String text, final String imageUrl) {
            // Default is to not show the notification, and immediate send close message.
            GeckoAppShell.onNotificationClose(name, cookie);
        }

        @Override // NotificationListener
        public void showPersistentNotification(final String name, final String cookie,
                                               final String host, final String title,
                                               final String text, final String imageUrl,
                                               final String data) {
            // Default is to not show the notification, and immediate send close message.
            GeckoAppShell.onNotificationClose(name, cookie);
        }

        @Override // NotificationListener
        public void closeNotification(final String name) {
            // Do nothing.
        }

        @Override // ScreenOrientationDelegate
        public boolean setRequestedOrientationForCurrentActivity(
                final int requestedActivityInfoOrientation) {
            // Do nothing, and report that the orientation was not set.
            return false;
        }

        private SimpleArrayMap<String, PowerManager.WakeLock> mWakeLocks;

        @Override // WakeLockDelegate
        @SuppressLint("Wakelock") // We keep the wake lock independent from the function
                                  // scope, so we need to suppress the linter warning.
        public void setWakeLockState(final String lock, final int state) {
            if (mWakeLocks == null) {
                mWakeLocks = new SimpleArrayMap<>(WakeLockDelegate.LOCKS_COUNT);
            }

            PowerManager.WakeLock wl = mWakeLocks.get(lock);

            // we should still hold the lock for background audio.
            if (WakeLockDelegate.LOCK_AUDIO_PLAYING.equals(lock) &&
                state == WakeLockDelegate.STATE_LOCKED_BACKGROUND) {
                return;
            }

            if (state == WakeLockDelegate.STATE_LOCKED_FOREGROUND && wl == null) {
                final PowerManager pm = (PowerManager)
                        getApplicationContext().getSystemService(Context.POWER_SERVICE);

                if (WakeLockDelegate.LOCK_CPU.equals(lock) ||
                    WakeLockDelegate.LOCK_AUDIO_PLAYING.equals(lock)) {
                    wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lock);
                } else if (WakeLockDelegate.LOCK_SCREEN.equals(lock) ||
                           WakeLockDelegate.LOCK_VIDEO_PLAYING.equals(lock)) {
                  // ON_AFTER_RELEASE is set, the user activity timer will be reset when the
                  // WakeLock is released, causing the illumination to remain on a bit longer.
                    wl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK |
                                        PowerManager.ON_AFTER_RELEASE, lock);
                } else {
                    Log.w(LOGTAG, "Unsupported wake-lock: " + lock);
                    return;
                }

                wl.acquire();
                mWakeLocks.put(lock, wl);
            } else if (state != WakeLockDelegate.STATE_LOCKED_FOREGROUND && wl != null) {
                wl.release();
                mWakeLocks.remove(lock);
            }
        }

        @Override
        public void performHapticFeedback(final int effect) {
            final int[] pattern;
            // Use default platform values.
            if (effect == HapticFeedbackConstants.KEYBOARD_TAP) {
                pattern = new int[] { 40 };
            } else if (effect == HapticFeedbackConstants.LONG_PRESS) {
                pattern = new int[] { 0, 1, 20, 21 };
            } else if (effect == HapticFeedbackConstants.VIRTUAL_KEY) {
                pattern = new int[] { 0, 10, 20, 30 };
            } else {
                return;
            }
            vibrateOnHapticFeedbackEnabled(pattern);
        }
    }

    private static final DefaultListeners DEFAULT_LISTENERS = new DefaultListeners();
    private static SensorEventListener sSensorListener = DEFAULT_LISTENERS;
    private static LocationListener sLocationListener = DEFAULT_LISTENERS;
    private static NotificationListener sNotificationListener = DEFAULT_LISTENERS;
    private static WakeLockDelegate sWakeLockDelegate = DEFAULT_LISTENERS;
    private static HapticFeedbackDelegate sHapticFeedbackDelegate = DEFAULT_LISTENERS;

    /**
     * A delegate for supporting the Screen Orientation API.
     */
    private static ScreenOrientationDelegate sScreenOrientationDelegate = DEFAULT_LISTENERS;

    public static SensorEventListener getSensorListener() {
        return sSensorListener;
    }

    public static void setSensorListener(final SensorEventListener listener) {
        sSensorListener = (listener != null) ? listener : DEFAULT_LISTENERS;
    }

    public static LocationListener getLocationListener() {
        return sLocationListener;
    }

    public static void setLocationListener(final LocationListener listener) {
        sLocationListener = (listener != null) ? listener : DEFAULT_LISTENERS;
    }

    public static NotificationListener getNotificationListener() {
        return sNotificationListener;
    }

    public static void setNotificationListener(final NotificationListener listener) {
        sNotificationListener = (listener != null) ? listener : DEFAULT_LISTENERS;
    }

    public static ScreenOrientationDelegate getScreenOrientationDelegate() {
        return sScreenOrientationDelegate;
    }

    public static void setScreenOrientationDelegate(
            final @Nullable ScreenOrientationDelegate screenOrientationDelegate) {
        sScreenOrientationDelegate = (screenOrientationDelegate != null) ? screenOrientationDelegate : DEFAULT_LISTENERS;
    }

    public static WakeLockDelegate getWakeLockDelegate() {
        return sWakeLockDelegate;
    }

    public void setWakeLockDelegate(final WakeLockDelegate delegate) {
        sWakeLockDelegate = (delegate != null) ? delegate : DEFAULT_LISTENERS;
    }

    public static HapticFeedbackDelegate getHapticFeedbackDelegate() {
        return sHapticFeedbackDelegate;
    }

    public static void setHapticFeedbackDelegate(final HapticFeedbackDelegate delegate) {
        sHapticFeedbackDelegate = (delegate != null) ? delegate : DEFAULT_LISTENERS;
    }

    @SuppressWarnings("fallthrough")
    @WrapForJNI(calledFrom = "gecko")
    private static void enableSensor(final int aSensortype) {
        final SensorManager sm = (SensorManager)
            getApplicationContext().getSystemService(Context.SENSOR_SERVICE);

        switch (aSensortype) {
            case GeckoHalDefines.SENSOR_GAME_ROTATION_VECTOR:
                if (gGameRotationVectorSensor == null) {
                    gGameRotationVectorSensor = sm.getDefaultSensor(
                            Sensor.TYPE_GAME_ROTATION_VECTOR);
                }
                if (gGameRotationVectorSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gGameRotationVectorSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                if (gGameRotationVectorSensor != null) {
                    break;
                }
                // Fallthrough

            case GeckoHalDefines.SENSOR_ROTATION_VECTOR:
                if (gRotationVectorSensor == null) {
                    gRotationVectorSensor = sm.getDefaultSensor(
                        Sensor.TYPE_ROTATION_VECTOR);
                }
                if (gRotationVectorSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gRotationVectorSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                if (gRotationVectorSensor != null) {
                    break;
                }
                // Fallthrough

            case GeckoHalDefines.SENSOR_ORIENTATION:
                if (gOrientationSensor == null) {
                    gOrientationSensor = sm.getDefaultSensor(
                        Sensor.TYPE_ORIENTATION);
                }
                if (gOrientationSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gOrientationSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                break;

            case GeckoHalDefines.SENSOR_ACCELERATION:
                if (gAccelerometerSensor == null) {
                    gAccelerometerSensor = sm.getDefaultSensor(
                        Sensor.TYPE_ACCELEROMETER);
                }
                if (gAccelerometerSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gAccelerometerSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                break;

            case GeckoHalDefines.SENSOR_PROXIMITY:
                if (gProximitySensor == null) {
                    gProximitySensor = sm.getDefaultSensor(Sensor.TYPE_PROXIMITY);
                }
                if (gProximitySensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gProximitySensor,
                                        SensorManager.SENSOR_DELAY_NORMAL);
                }
                break;

            case GeckoHalDefines.SENSOR_LIGHT:
                if (gLightSensor == null) {
                    gLightSensor = sm.getDefaultSensor(Sensor.TYPE_LIGHT);
                }
                if (gLightSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gLightSensor,
                                        SensorManager.SENSOR_DELAY_NORMAL);
                }
                break;

            case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
                if (gLinearAccelerometerSensor == null) {
                    gLinearAccelerometerSensor = sm.getDefaultSensor(
                        Sensor.TYPE_LINEAR_ACCELERATION);
                }
                if (gLinearAccelerometerSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gLinearAccelerometerSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                break;

            case GeckoHalDefines.SENSOR_GYROSCOPE:
                if (gGyroscopeSensor == null) {
                    gGyroscopeSensor = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
                }
                if (gGyroscopeSensor != null) {
                    sm.registerListener(getSensorListener(),
                                        gGyroscopeSensor,
                                        SensorManager.SENSOR_DELAY_FASTEST);
                }
                break;

            default:
                Log.w(LOGTAG, "Error! Can't enable unknown SENSOR type " +
                      aSensortype);
        }
    }

    @SuppressWarnings("fallthrough")
    @WrapForJNI(calledFrom = "gecko")
    private static void disableSensor(final int aSensortype) {
        final SensorManager sm = (SensorManager)
            getApplicationContext().getSystemService(Context.SENSOR_SERVICE);

        switch (aSensortype) {
            case GeckoHalDefines.SENSOR_GAME_ROTATION_VECTOR:
                if (gGameRotationVectorSensor != null) {
                    sm.unregisterListener(getSensorListener(), gGameRotationVectorSensor);
                    break;
                }
                // Fallthrough

            case GeckoHalDefines.SENSOR_ROTATION_VECTOR:
                if (gRotationVectorSensor != null) {
                    sm.unregisterListener(getSensorListener(), gRotationVectorSensor);
                    break;
                }
                // Fallthrough

            case GeckoHalDefines.SENSOR_ORIENTATION:
                if (gOrientationSensor != null) {
                    sm.unregisterListener(getSensorListener(), gOrientationSensor);
                }
                break;

            case GeckoHalDefines.SENSOR_ACCELERATION:
                if (gAccelerometerSensor != null) {
                    sm.unregisterListener(getSensorListener(), gAccelerometerSensor);
                }
                break;

            case GeckoHalDefines.SENSOR_PROXIMITY:
                if (gProximitySensor != null) {
                    sm.unregisterListener(getSensorListener(), gProximitySensor);
                }
                break;

            case GeckoHalDefines.SENSOR_LIGHT:
                if (gLightSensor != null) {
                    sm.unregisterListener(getSensorListener(), gLightSensor);
                }
                break;

            case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
                if (gLinearAccelerometerSensor != null) {
                    sm.unregisterListener(getSensorListener(), gLinearAccelerometerSensor);
                }
                break;

            case GeckoHalDefines.SENSOR_GYROSCOPE:
                if (gGyroscopeSensor != null) {
                    sm.unregisterListener(getSensorListener(), gGyroscopeSensor);
                }
                break;
            default:
                Log.w(LOGTAG, "Error! Can't disable unknown SENSOR type " + aSensortype);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void moveTaskToBack() {
        // This is a vestige, to be removed as full-screen support for GeckoView is implemented.
    }

    @JNITarget
    static public int getPreferredIconSize() {
        ActivityManager am = (ActivityManager)
            getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
        return am.getLauncherLargeIconSize();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String[] getHandlersForMimeType(final String aMimeType, final String aAction) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return new String[] {};
        }
        return geckoInterface.getHandlersForMimeType(aMimeType, aAction);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String[] getHandlersForURL(final String aURL, final String aAction) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return new String[] {};
        }
        return geckoInterface.getHandlersForURL(aURL, aAction);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean getHWEncoderCapability() {
        return HardwareCodecCapabilityUtils.getHWEncoderCapability();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean getHWDecoderCapability() {
        return HardwareCodecCapabilityUtils.getHWDecoderCapability();
    }

    static List<ResolveInfo> queryIntentActivities(final Intent intent) {
        final PackageManager pm = getApplicationContext().getPackageManager();

        // Exclude any non-exported activities: we can't open them even if we want to!
        // Bug 1031569 has some details.
        final ArrayList<ResolveInfo> list = new ArrayList<>();
        for (ResolveInfo ri: pm.queryIntentActivities(intent, 0)) {
            if (ri.activityInfo.exported) {
                list.add(ri);
            }
        }

        return list;
    }

    @WrapForJNI(calledFrom = "gecko")
    public static String getExtensionFromMimeType(final String aMimeType) {
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(aMimeType);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static String getMimeTypeFromExtensions(final String aFileExt) {
        StringTokenizer st = new StringTokenizer(aFileExt, ".,; ");
        String type = null;
        String subType = null;
        while (st.hasMoreElements()) {
            String ext = st.nextToken();
            String mt = getMimeTypeFromExtension(ext);
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

    @SuppressWarnings("try")
    @WrapForJNI(calledFrom = "gecko")
    private static boolean openUriExternal(final String targetURI,
                                           final String mimeType,
                                           final String packageName,
                                           final String className,
                                           final String action,
                                           final String title) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return false;
        }
        // Bug 1450449 - Downloaded files already are already in a public directory and aren't
        // really owned exclusively by Firefox, so there's no real benefit to using
        // content:// URIs here.
        try (StrictModeContext unused = StrictModeContext.allowAllVmPolicies()) {
            return geckoInterface.openUriExternal(targetURI, mimeType, packageName, className, action, title);
        }
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void notifyAlertListener(String name, String topic, String cookie);

    /**
     * Called by the NotificationListener to notify Gecko that a notification has been
     * shown.
     */
    public static void onNotificationShow(final String name, final String cookie) {
        if (GeckoThread.isRunning()) {
            notifyAlertListener(name, "alertshow", cookie);
        }
    }

    /**
     * Called by the NotificationListener to notify Gecko that a previously shown
     * notification has been closed.
     */
    public static void onNotificationClose(final String name, final String cookie) {
        if (GeckoThread.isRunning()) {
            notifyAlertListener(name, "alertfinished", cookie);
        }
    }

    /**
     * Called by the NotificationListener to notify Gecko that a previously shown
     * notification has been clicked on.
     */
    public static void onNotificationClick(final String name, final String cookie) {
        if (GeckoThread.isRunning()) {
            notifyAlertListener(name, "alertclickcallback", cookie);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void showNotification(final String name, final String cookie, final String title,
                                         final String text, final String host,
                                         final String imageUrl, final String persistentData) {
        if (persistentData == null) {
            getNotificationListener().showNotification(name, cookie, title, text, host, imageUrl);
            return;
        }

        getNotificationListener().showPersistentNotification(
                name, cookie, title, text, host, imageUrl, persistentData);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void closeNotification(final String name) {
        getNotificationListener().closeNotification(name);
    }

    public static synchronized void setDisplayDpiOverride(@Nullable final Integer dpi) {
        if (dpi == null) {
            return;
        }
        if (sDensityDpi != 0) {
            Log.e(LOGTAG, "Tried to override screen DPI after it's already been set");
            return;
        }
        sDensityDpi = dpi;
    }

    @WrapForJNI(calledFrom = "gecko")
    public static synchronized int getDpi() {
        if (sDensityDpi == 0) {
            sDensityDpi = getApplicationContext().getResources().getDisplayMetrics().densityDpi;
        }
        return sDensityDpi;
    }

    public static synchronized void setDisplayDensityOverride(@Nullable final Float density) {
        if (density == null) {
            return;
        }
        if (sDensity != null) {
            Log.e(LOGTAG, "Tried to override screen density after it's already been set");
            return;
        }
        sDensity = density;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static synchronized float getDensity() {
        if (sDensity == null) {
            sDensity =  new Float(getApplicationContext().getResources().getDisplayMetrics().density);
        }

        return sDensity;
    }

    private static boolean isHighMemoryDevice() {
        return HardwareUtils.getMemSize() > HIGH_MEMORY_DEVICE_THRESHOLD_MB;
    }

    public static synchronized void useMaxScreenDepth(final boolean enable) {
        sUseMaxScreenDepth = enable;
    }

    /**
     * Returns the colour depth of the default screen. This will either be
     * 32, 24 or 16.
     */
    @WrapForJNI(calledFrom = "gecko")
    public static synchronized int getScreenDepth() {
        if (sScreenDepth == 0) {
            sScreenDepth = 16;
            PixelFormat info = new PixelFormat();
            final WindowManager wm = (WindowManager)
                    getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            PixelFormat.getPixelFormatInfo(wm.getDefaultDisplay().getPixelFormat(), info);
            if (info.bitsPerPixel >= 24 && isHighMemoryDevice()) {
                sScreenDepth = sUseMaxScreenDepth ? info.bitsPerPixel : 24;
            }
        }

        return sScreenDepth;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void performHapticFeedback(final boolean aIsLongPress) {
        // Don't perform haptic feedback if a vibration is currently playing,
        // because the haptic feedback will nuke the vibration.
        if (!sVibrationMaybePlaying || System.nanoTime() >= sVibrationEndTime) {
            getHapticFeedbackDelegate().performHapticFeedback(
                    aIsLongPress ? HapticFeedbackConstants.LONG_PRESS
                                 : HapticFeedbackConstants.VIRTUAL_KEY);
            sVibrationMaybePlaying = false;
            sVibrationEndTime = 0;
        }
    }

    private static Vibrator vibrator() {
        return (Vibrator) getApplicationContext().getSystemService(Context.VIBRATOR_SERVICE);
    }

    // Helper method to convert integer array to long array.
    private static long[] convertIntToLongArray(final int[] input) {
        long[] output = new long[input.length];
        for (int i = 0; i < input.length; i++) {
            output[i] = input[i];
        }
        return output;
    }

    // Vibrate only if haptic feedback is enabled.
    private static void vibrateOnHapticFeedbackEnabled(final int[] milliseconds) {
        if (Settings.System.getInt(getApplicationContext().getContentResolver(),
                                   Settings.System.HAPTIC_FEEDBACK_ENABLED, 0) > 0) {
            if (milliseconds.length == 1) {
                vibrate(milliseconds[0]);
            } else {
                vibrate(convertIntToLongArray(milliseconds), -1);
            }
        }
    }

    @SuppressLint("MissingPermission")
    @WrapForJNI(calledFrom = "gecko")
    private static void vibrate(final long milliseconds) {
        sVibrationEndTime = System.nanoTime() + milliseconds * 1000000;
        sVibrationMaybePlaying = true;
        try {
            vibrator().vibrate(milliseconds);
        } catch (final SecurityException ignore) {
            Log.w(LOGTAG, "No VIBRATE permission");
        }
    }

    @SuppressLint("MissingPermission")
    @WrapForJNI(calledFrom = "gecko")
    private static void vibrate(final long[] pattern, final int repeat) {
        // If pattern.length is odd, the last element in the pattern is a
        // meaningless delay, so don't include it in vibrationDuration.
        long vibrationDuration = 0;
        int iterLen = pattern.length & ~1;
        for (int i = 0; i < iterLen; i++) {
            vibrationDuration += pattern[i];
        }

        sVibrationEndTime = System.nanoTime() + vibrationDuration * 1000000;
        sVibrationMaybePlaying = true;
        try {
            vibrator().vibrate(pattern, repeat);
        } catch (final SecurityException ignore) {
            Log.w(LOGTAG, "No VIBRATE permission");
        }
    }

    @SuppressLint("MissingPermission")
    @WrapForJNI(calledFrom = "gecko")
    private static void cancelVibrate() {
        sVibrationMaybePlaying = false;
        sVibrationEndTime = 0;
        try {
            vibrator().cancel();
        } catch (final SecurityException ignore) {
            Log.w(LOGTAG, "No VIBRATE permission");
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean isNetworkLinkUp() {
        ConnectivityManager cm = (ConnectivityManager)
                getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        try {
            NetworkInfo info = cm.getActiveNetworkInfo();
            if (info == null || !info.isConnected())
                return false;
        } catch (SecurityException se) {
            return false;
        }
        return true;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean isNetworkLinkKnown() {
        ConnectivityManager cm = (ConnectivityManager)
            getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        try {
            if (cm.getActiveNetworkInfo() == null)
                return false;
        } catch (SecurityException se) {
            return false;
        }
        return true;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int getNetworkLinkType() {
        ConnectivityManager cm = (ConnectivityManager)
            getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null) {
            return LINK_TYPE_UNKNOWN;
        }

        switch (info.getType()) {
            case ConnectivityManager.TYPE_ETHERNET:
                return LINK_TYPE_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return LINK_TYPE_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return LINK_TYPE_WIMAX;
            case ConnectivityManager.TYPE_MOBILE:
                break; // We will handle sub-types after the switch.
            default:
                Log.w(LOGTAG, "Ignoring the current network type.");
                return LINK_TYPE_UNKNOWN;
        }

        TelephonyManager tm = (TelephonyManager)
            getApplicationContext().getSystemService(Context.TELEPHONY_SERVICE);
        if (tm == null) {
            Log.e(LOGTAG, "Telephony service does not exist");
            return LINK_TYPE_UNKNOWN;
        }

        switch (tm.getNetworkType()) {
            case TelephonyManager.NETWORK_TYPE_IDEN:
            case TelephonyManager.NETWORK_TYPE_CDMA:
            case TelephonyManager.NETWORK_TYPE_GPRS:
                return LINK_TYPE_2G;
            case TelephonyManager.NETWORK_TYPE_1xRTT:
            case TelephonyManager.NETWORK_TYPE_EDGE:
                return LINK_TYPE_2G; // 2.5G
            case TelephonyManager.NETWORK_TYPE_UMTS:
            case TelephonyManager.NETWORK_TYPE_EVDO_0:
                return LINK_TYPE_3G;
            case TelephonyManager.NETWORK_TYPE_HSPA:
            case TelephonyManager.NETWORK_TYPE_HSDPA:
            case TelephonyManager.NETWORK_TYPE_HSUPA:
            case TelephonyManager.NETWORK_TYPE_EVDO_A:
            case TelephonyManager.NETWORK_TYPE_EVDO_B:
            case TelephonyManager.NETWORK_TYPE_EHRPD:
                return LINK_TYPE_3G; // 3.5G
            case TelephonyManager.NETWORK_TYPE_HSPAP:
                return LINK_TYPE_3G; // 3.75G
            case TelephonyManager.NETWORK_TYPE_LTE:
                return LINK_TYPE_4G; // 3.9G
            case TelephonyManager.NETWORK_TYPE_UNKNOWN:
            default:
                Log.w(LOGTAG, "Connected to an unknown mobile network!");
                return LINK_TYPE_UNKNOWN;
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int[] getSystemColors() {
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
            new ContextThemeWrapper(getApplicationContext(), android.R.style.TextAppearance);

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

    @WrapForJNI(calledFrom = "gecko")
    public static void killAnyZombies() {
        GeckoProcessesVisitor visitor = new GeckoProcessesVisitor() {
            @Override
            public boolean callback(final int pid) {
                if (pid != android.os.Process.myPid())
                    android.os.Process.killProcess(pid);
                return true;
            }
        };

        EnumerateGeckoProcesses(visitor);
    }

    interface GeckoProcessesVisitor {
        boolean callback(int pid);
    }

    private static void EnumerateGeckoProcesses(final GeckoProcessesVisitor visiter) {
        int pidColumn = -1;
        int userColumn = -1;

        Process ps = null;
        InputStreamReader inputStreamReader = null;
        BufferedReader in = null;
        try {
            // run ps and parse its output
            ps = Runtime.getRuntime().exec("ps");
            inputStreamReader = new InputStreamReader(ps.getInputStream());
            in = new BufferedReader(inputStreamReader, 2048);

            String headerOutput = in.readLine();

            // figure out the column offsets.  We only care about the pid and user fields
            StringTokenizer st = new StringTokenizer(headerOutput);

            int tokenSoFar = 0;
            while (st.hasMoreTokens()) {
                String next = st.nextToken();
                if (next.equalsIgnoreCase("PID"))
                    pidColumn = tokenSoFar;
                else if (next.equalsIgnoreCase("USER"))
                    userColumn = tokenSoFar;
                tokenSoFar++;
            }

            // alright, the rest are process entries.
            String psOutput = null;
            while ((psOutput = in.readLine()) != null) {
                String[] split = psOutput.split("\\s+");
                if (split.length <= pidColumn || split.length <= userColumn)
                    continue;
                int uid = android.os.Process.getUidForName(split[userColumn]);
                if (uid == android.os.Process.myUid() &&
                    !split[split.length - 1].equalsIgnoreCase("ps")) {
                    int pid = Integer.parseInt(split[pidColumn]);
                    boolean keepGoing = visiter.callback(pid);
                    if (keepGoing == false)
                        break;
                }
            }
        } catch (Exception e) {
            Log.w(LOGTAG, "Failed to enumerate Gecko processes.",  e);
        } finally {
            IOUtils.safeStreamClose(in);
            IOUtils.safeStreamClose(inputStreamReader);
            if (ps != null) {
                ps.destroy();
            }
        }
    }

    public static String getAppNameByPID(final int pid) {
        BufferedReader cmdlineReader = null;
        String path = "/proc/" + pid + "/cmdline";
        try {
            File cmdlineFile = new File(path);
            if (!cmdlineFile.exists())
                return "";
            cmdlineReader = new BufferedReader(new FileReader(cmdlineFile));
            return cmdlineReader.readLine().trim();
        } catch (Exception ex) {
            return "";
        } finally {
            IOUtils.safeStreamClose(cmdlineReader);
        }
    }

    public static void listOfOpenFiles() {
        int pidColumn = -1;
        int nameColumn = -1;

        // run lsof and parse its output
        Process process = null;
        InputStreamReader inputStreamReader = null;
        BufferedReader in = null;
        try {
            String filter = GeckoProfile.get(getApplicationContext()).getDir().toString();
            Log.d(LOGTAG, "[OPENFILE] Filter: " + filter);

            process = Runtime.getRuntime().exec("lsof");
            inputStreamReader = new InputStreamReader(process.getInputStream());
            in = new BufferedReader(inputStreamReader, 2048);

            String headerOutput = in.readLine();
            StringTokenizer st = new StringTokenizer(headerOutput);
            int token = 0;
            while (st.hasMoreTokens()) {
                String next = st.nextToken();
                if (next.equalsIgnoreCase("PID"))
                    pidColumn = token;
                else if (next.equalsIgnoreCase("NAME"))
                    nameColumn = token;
                token++;
            }

            // alright, the rest are open file entries.
            Map<Integer, String> pidNameMap = new TreeMap<Integer, String>();
            String output = null;
            while ((output = in.readLine()) != null) {
                String[] split = output.split("\\s+");
                if (split.length <= pidColumn || split.length <= nameColumn)
                    continue;
                final Integer pid = Integer.valueOf(split[pidColumn]);
                String name = pidNameMap.get(pid);
                if (name == null) {
                    name = getAppNameByPID(pid.intValue());
                    pidNameMap.put(pid, name);
                }
                String file = split[nameColumn];
                if (!TextUtils.isEmpty(name) && !TextUtils.isEmpty(file) && file.startsWith(filter))
                    Log.d(LOGTAG, "[OPENFILE] " + name + "(" + split[pidColumn] + ") : " + file);
            }
        } catch (Exception e) {
        } finally {
            IOUtils.safeStreamClose(in);
            IOUtils.safeStreamClose(inputStreamReader);
            if (process != null) {
                process.destroy();
            }
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static byte[] getIconForExtension(final String aExt, final int iconSize) {
        try {
            int resolvedIconSize = iconSize;
            if (iconSize <= 0) {
                resolvedIconSize = 16;
            }

            String resolvedExt = aExt;
            if (aExt != null && aExt.length() > 1 && aExt.charAt(0) == '.') {
                resolvedExt = aExt.substring(1);
            }

            PackageManager pm = getApplicationContext().getPackageManager();
            Drawable icon = getDrawableForExtension(pm, resolvedExt);
            if (icon == null) {
                // Use a generic icon
                icon = pm.getDefaultActivityIcon();
            }

            Bitmap bitmap = ((BitmapDrawable)icon).getBitmap();
            if (bitmap.getWidth() != resolvedIconSize || bitmap.getHeight() != resolvedIconSize)
                bitmap = Bitmap.createScaledBitmap(bitmap, resolvedIconSize, resolvedIconSize, true);

            ByteBuffer buf = ByteBuffer.allocate(resolvedIconSize * resolvedIconSize * 4);
            bitmap.copyPixelsToBuffer(buf);

            return buf.array();
        } catch (Exception e) {
            Log.w(LOGTAG, "getIconForExtension failed.",  e);
            return null;
        }
    }

    public static String getMimeTypeFromExtension(final String ext) {
        final MimeTypeMap mtm = MimeTypeMap.getSingleton();
        return mtm.getMimeTypeFromExtension(ext);
    }

    private static Drawable getDrawableForExtension(final PackageManager pm, final String aExt) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        final String mimeType = getMimeTypeFromExtension(aExt);
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

    @WrapForJNI(calledFrom = "gecko")
    private static boolean getShowPasswordSetting() {
        try {
            int showPassword =
                Settings.System.getInt(getApplicationContext().getContentResolver(),
                                       Settings.System.TEXT_SHOW_PASSWORD, 1);
            return (showPassword > 0);
        } catch (Exception e) {
            return true;
        }
    }

    private static Context sApplicationContext;

    @WrapForJNI
    public static Context getApplicationContext() {
        return sApplicationContext;
    }

    public static void setApplicationContext(final Context context) {
        sApplicationContext = context;
    }

    public interface GeckoInterface {
        public boolean openUriExternal(String targetURI, String mimeType, String packageName, String className, String action, String title);

        public String[] getHandlersForMimeType(String mimeType, String action);
        public String[] getHandlersForURL(String url, String action);
    };

    private static GeckoInterface sGeckoInterface;

    public static GeckoInterface getGeckoInterface() {
        return sGeckoInterface;
    }

    public static void setGeckoInterface(final GeckoInterface aGeckoInterface) {
        sGeckoInterface = aGeckoInterface;
    }

    /* package */ static Camera sCamera;

    private static final int kPreferredFPS = 25;
    private static byte[] sCameraBuffer;

    private static class CameraCallback implements Camera.PreviewCallback {
        @WrapForJNI(calledFrom = "gecko")
        private static native void onFrameData(int camera, byte[] data);

        private final int mCamera;

        public CameraCallback(final int camera) {
            mCamera = camera;
        }

        @Override
        public void onPreviewFrame(final byte[] data, final Camera camera) {
            onFrameData(mCamera, data);

            if (sCamera != null) {
                sCamera.addCallbackBuffer(sCameraBuffer);
            }
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int[] initCamera(final String aContentType, final int aCamera, final int aWidth,
                                    final int aHeight) {
        // [0] = 0|1 (failure/success)
        // [1] = width
        // [2] = height
        // [3] = fps
        int[] result = new int[4];
        result[0] = 0;

        if (Camera.getNumberOfCameras() == 0) {
            return result;
        }

        try {
            sCamera = Camera.open(aCamera);

            Camera.Parameters params = sCamera.getParameters();
            params.setPreviewFormat(ImageFormat.NV21);

            // use the preview fps closest to 25 fps.
            int fpsDelta = 1000;
            try {
                Iterator<Integer> it = params.getSupportedPreviewFrameRates().iterator();
                while (it.hasNext()) {
                    int nFps = it.next();
                    if (Math.abs(nFps - kPreferredFPS) < fpsDelta) {
                        fpsDelta = Math.abs(nFps - kPreferredFPS);
                        params.setPreviewFrameRate(nFps);
                    }
                }
            } catch (Exception e) {
                params.setPreviewFrameRate(kPreferredFPS);
            }

            // set up the closest preview size available
            Iterator<Camera.Size> sit = params.getSupportedPreviewSizes().iterator();
            int sizeDelta = 10000000;
            int bufferSize = 0;
            while (sit.hasNext()) {
                Camera.Size size = sit.next();
                if (Math.abs(size.width * size.height - aWidth * aHeight) < sizeDelta) {
                    sizeDelta = Math.abs(size.width * size.height - aWidth * aHeight);
                    params.setPreviewSize(size.width, size.height);
                    bufferSize = size.width * size.height;
                }
            }

            sCamera.setParameters(params);
            sCameraBuffer = new byte[(bufferSize * 12) / 8];
            sCamera.addCallbackBuffer(sCameraBuffer);
            sCamera.setPreviewCallbackWithBuffer(new CameraCallback(aCamera));
            sCamera.startPreview();
            params = sCamera.getParameters();
            result[0] = 1;
            result[1] = params.getPreviewSize().width;
            result[2] = params.getPreviewSize().height;
            result[3] = params.getPreviewFrameRate();
        } catch (RuntimeException e) {
            Log.w(LOGTAG, "initCamera RuntimeException.", e);
            result[0] = result[1] = result[2] = result[3] = 0;
        }
        return result;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static synchronized void closeCamera() {
        if (sCamera != null) {
            sCamera.stopPreview();
            sCamera.release();
            sCamera = null;
            sCameraBuffer = null;
        }
    }

    /*
     * Battery API related methods.
     */
    @WrapForJNI(calledFrom = "gecko")
    private static void enableBatteryNotifications() {
        GeckoBatteryManager.enableNotifications();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void disableBatteryNotifications() {
        GeckoBatteryManager.disableNotifications();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static double[] getCurrentBatteryInformation() {
        return GeckoBatteryManager.getCurrentInformation();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void hideProgressDialog() {
        // unused stub
    }

    /* Called by JNI from AndroidBridge, and by reflection from tests/BaseTest.java.in */
    @WrapForJNI(calledFrom = "gecko")
    @RobocopTarget
    public static boolean isTablet() {
        return HardwareUtils.isTablet();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static double[] getCurrentNetworkInformation() {
        return GeckoNetworkManager.getInstance().getCurrentInformation();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void enableNetworkNotifications() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                GeckoNetworkManager.getInstance().enableNotifications();
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void disableNetworkNotifications() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                GeckoNetworkManager.getInstance().disableNotifications();
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko")
    private static short getScreenOrientation() {
        return GeckoScreenOrientation.getInstance().getScreenOrientation().value;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int getScreenAngle() {
        return GeckoScreenOrientation.getInstance().getAngle();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void enableScreenOrientationNotifications() {
        GeckoScreenOrientation.getInstance().enableNotifications();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void disableScreenOrientationNotifications() {
        GeckoScreenOrientation.getInstance().disableNotifications();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void lockScreenOrientation(final int aOrientation) {
        // TODO: don't vector through GeckoAppShell.
        GeckoScreenOrientation.getInstance().lock(aOrientation);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void unlockScreenOrientation() {
        // TODO: don't vector through GeckoAppShell.
        GeckoScreenOrientation.getInstance().unlock();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void notifyWakeLockChanged(final String topic, final String state) {
        final int intState;
        if ("unlocked".equals(state)) {
            intState = WakeLockDelegate.STATE_UNLOCKED;
        } else if ("locked-foreground".equals(state)) {
            intState = WakeLockDelegate.STATE_LOCKED_FOREGROUND;
        } else if ("locked-background".equals(state)) {
            intState = WakeLockDelegate.STATE_LOCKED_BACKGROUND;
        } else {
            throw new IllegalArgumentException();
        }
        getWakeLockDelegate().setWakeLockState(topic, intState);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean unlockProfile() {
        // Try to kill any zombie Fennec's that might be running
        GeckoAppShell.killAnyZombies();

        // Then force unlock this profile
        final GeckoProfile profile = GeckoThread.getActiveProfile();
        if (profile != null) {
            File lock = profile.getFile(".parentlock");
            return lock != null && lock.exists() && lock.delete();
        }
        return false;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String getProxyForURI(final String spec, final String scheme, final String host,
                                         final int port) {
        final ProxySelector ps = new ProxySelector();

        Proxy proxy = ps.select(scheme, host);
        if (Proxy.NO_PROXY.equals(proxy)) {
            return "DIRECT";
        }

        switch (proxy.type()) {
            case HTTP:
                return "PROXY " + proxy.address().toString();
            case SOCKS:
                return "SOCKS " + proxy.address().toString();
        }

        return "DIRECT";
    }

    @WrapForJNI
    private static InputStream createInputStream(final URLConnection connection)
            throws IOException {
        return connection.getInputStream();
    }

    private static class BitmapConnection extends URLConnection {
        private Bitmap mBitmap;

        BitmapConnection(final Bitmap b) throws MalformedURLException, IOException {
            super(null);
            mBitmap = b;
        }

        @Override
        public void connect() {}

        @Override
        public InputStream getInputStream() throws IOException {
            return new BitmapInputStream();
        }

        @Override
        public String getContentType() {
            return "image/png";
        }

        private final class BitmapInputStream extends PipedInputStream {
            private boolean mHaveConnected = false;

            @Override
            public synchronized int read(final byte[] buffer, final int byteOffset,
                                         final int byteCount) throws IOException {
                if (mHaveConnected) {
                    return super.read(buffer, byteOffset, byteCount);
                }

                final PipedOutputStream output = new PipedOutputStream();
                connect(output);

                ThreadUtils.postToBackgroundThread(
                    new Runnable() {
                        @Override
                        public void run() {
                            try {
                                mBitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
                            } finally {
                                IOUtils.safeStreamClose(output);
                            }
                        }
                    });
                mHaveConnected = true;
                return super.read(buffer, byteOffset, byteCount);
            }
        }
    }

    @WrapForJNI
    private static URLConnection getConnection(final String url) {
        try {
            String spec;
            if (url.startsWith("android://")) {
                spec = url.substring(10);
            } else {
                spec = url.substring(8);
            }

            // Check if we are loading a package icon.
            try {
                if (spec.startsWith("icon/")) {
                    String[] splits = spec.split("/");
                    if (splits.length != 2) {
                        return null;
                    }
                    final String pkg = splits[1];
                    final PackageManager pm = getApplicationContext().getPackageManager();
                    final Drawable d = pm.getApplicationIcon(pkg);
                    final Bitmap bitmap = BitmapUtils.getBitmapFromDrawable(d);
                    return new BitmapConnection(bitmap);
                }
            } catch (Exception ex) {
                Log.e(LOGTAG, "error", ex);
            }

            // if the colon got stripped, put it back
            int colon = spec.indexOf(':');
            if (colon == -1 || colon > spec.indexOf('/')) {
                spec = spec.replaceFirst("/", ":/");
            }
        } catch (Exception ex) {
            return null;
        }
        return null;
    }

    @WrapForJNI
    private static String connectionGetMimeType(final URLConnection connection) {
        return connection.getContentType();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int getMaxTouchPoints() {
        PackageManager pm = getApplicationContext().getPackageManager();
        if (pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_JAZZHAND)) {
            // at least, 5+ fingers.
            return 5;
        } else if (pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT)) {
            // at least, 2+ fingers.
            return 2;
        } else if (pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH)) {
            // 2 fingers
            return 2;
        } else if (pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)) {
            // 1 finger
            return 1;
        }
        return 0;
    }

    /*
     * Keep in sync with PointerCapabilities in ServoTypes.h
    */
    static private final int NO_POINTER            = 0x00000000;
    static private final int COARSE_POINTER        = 0x00000001;
    static private final int FINE_POINTER          = 0x00000002;
    static private final int HOVER_CAPABLE_POINTER = 0x00000004;
    private static int getPointerCapabilities(final InputDevice inputDevice) {
        int result = NO_POINTER;
        int sources = inputDevice.getSources();

        if (hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHSCREEN) ||
            hasInputDeviceSource(sources, InputDevice.SOURCE_JOYSTICK)) {
            result |= COARSE_POINTER;
        } else if (hasInputDeviceSource(sources, InputDevice.SOURCE_MOUSE) ||
                   hasInputDeviceSource(sources, InputDevice.SOURCE_STYLUS) ||
                   hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHPAD) ||
                   hasInputDeviceSource(sources, InputDevice.SOURCE_TRACKBALL)) {
            result |= FINE_POINTER;
        }

        if (hasInputDeviceSource(sources, InputDevice.SOURCE_MOUSE) ||
            hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHPAD) ||
            hasInputDeviceSource(sources, InputDevice.SOURCE_TRACKBALL) ||
            hasInputDeviceSource(sources, InputDevice.SOURCE_JOYSTICK)) {
            result |= HOVER_CAPABLE_POINTER;
        }

        return result;
    }

    @WrapForJNI(calledFrom = "gecko")
    // For any-pointer and any-hover media queries features.
    private static int getAllPointerCapabilities() {
        int result = NO_POINTER;

        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice inputDevice = InputDevice.getDevice(deviceId);
            if (inputDevice == null ||
                !InputDeviceUtils.isPointerTypeDevice(inputDevice)) {
                continue;
            }

            result |= getPointerCapabilities(inputDevice);
        }

        return result;
    }

    @WrapForJNI(calledFrom = "gecko")
    // For pointer and hover media queries features.
    private static int getPrimaryPointerCapabilities() {
        int result = NO_POINTER;

        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice inputDevice = InputDevice.getDevice(deviceId);
            if (inputDevice == null ||
                !InputDeviceUtils.isPointerTypeDevice(inputDevice)) {
                continue;
            }

            result = getPointerCapabilities(inputDevice);

            // We need information only for the primary pointer.
            // (Assumes that the primary pointer appears first in the list)
            break;
        }

        return result;
    }

    private static boolean hasInputDeviceSource(final int sources, final int inputDeviceSource) {
        return (sources & inputDeviceSource) == inputDeviceSource;
    }

    public static synchronized void setScreenSizeOverride(final Rect size) {
        sScreenSizeOverride = size;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static synchronized Rect getScreenSize() {
        if (sScreenSizeOverride != null) {
            return sScreenSizeOverride;
        }
        final WindowManager wm = (WindowManager)
                getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
        final Display disp = wm.getDefaultDisplay();
        return new Rect(0, 0, disp.getWidth(), disp.getHeight());
    }

    @WrapForJNI(calledFrom = "any")
    public static int getAudioOutputFramesPerBuffer() {
        final int DEFAULT = 512;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return DEFAULT;
        }
        final AudioManager am = (AudioManager)getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        if (am == null) {
            return DEFAULT;
        }
        final String prop = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        if (prop == null) {
            return DEFAULT;
        }
        return Integer.parseInt(prop);
    }

    @WrapForJNI(calledFrom = "any")
    public static int getAudioOutputSampleRate() {
        final int DEFAULT = 44100;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return DEFAULT;
        }
        final AudioManager am = (AudioManager)getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        if (am == null) {
            return DEFAULT;
        }
        final String prop = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        if (prop == null) {
            return DEFAULT;
        }
        return Integer.parseInt(prop);
    }

    static private int sPreviousAudioMode = -2;

    @WrapForJNI(calledFrom = "any")
    public static void setCommunicationAudioModeOn(final boolean on) {
        final AudioManager am = (AudioManager)getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        if (am == null) {
            return;
        }

        try {
            if (on)  {
                Log.e(LOGTAG, "Setting communication mode ON");
                am.startBluetoothSco();
                am.setBluetoothScoOn(true);
            } else {
                Log.e(LOGTAG, "Setting communication mode OFF");
                am.stopBluetoothSco();
                am.setBluetoothScoOn(false);
            }
        } catch (SecurityException e) {
            Log.e(LOGTAG, "could not set communication mode", e);
        }
    }

    private static String getLanguageTag(final Locale locale) {
        final StringBuilder out = new StringBuilder(locale.getLanguage());
        final String country = locale.getCountry();
        final String variant = locale.getVariant();
        if (!TextUtils.isEmpty(country)) {
            out.append('-').append(country);
        }
        if (!TextUtils.isEmpty(variant)) {
            out.append('-').append(variant);
        }
        // e.g. "en", "en-US", or "en-US-POSIX".
        return out.toString();
    }

    @WrapForJNI
    public static String[] getDefaultLocales() {
        // XXX We may have to convert some language codes such as "id" vs "in".
        if (Build.VERSION.SDK_INT >= 24) {
            final LocaleList localeList = LocaleList.getDefault();
            String[] locales = new String[localeList.size()];
            for (int i = 0; i < localeList.size(); i++) {
                locales[i] = localeList.get(i).toLanguageTag();
            }
            return locales;
        }
        String[] locales = new String[1];
        final Locale locale = Locale.getDefault();
        if (Build.VERSION.SDK_INT >= 21) {
            locales[0] = locale.toLanguageTag();
            return locales;
        }

        locales[0] = getLanguageTag(locale);
        return locales;
    }

    private static Boolean sIsFennec;

    public static boolean isFennec() {
        if (sIsFennec == null) {
            try {
                sIsFennec = Class.forName("org.mozilla.gecko.GeckoApp") != null;
            } catch (ClassNotFoundException e) {
                sIsFennec = false;
            }
        }
        return sIsFennec;
    }
}
