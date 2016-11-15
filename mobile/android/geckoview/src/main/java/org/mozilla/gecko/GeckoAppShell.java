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
import java.util.Map;
import java.util.StringTokenizer;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;

import android.annotation.SuppressLint;
import org.mozilla.gecko.annotation.JNITarget;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PanZoomController;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.process.GeckoProcessManager;
import org.mozilla.gecko.process.GeckoServiceChildProcess;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.HardwareCodecCapabilityUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSContainer;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.ThreadUtils;

import android.Manifest;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
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
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.os.Vibrator;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.HapticFeedbackConstants;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.MimeTypeMap;
import android.widget.AbsoluteLayout;

public class GeckoAppShell
{
    private static final String LOGTAG = "GeckoAppShell";

    // We have static members only.
    private GeckoAppShell() { }

    private static final CrashHandler CRASH_HANDLER = new CrashHandler() {
        @Override
        protected String getAppPackageName() {
            return AppConstants.ANDROID_PACKAGE_NAME;
        }

        @Override
        protected Context getAppContext() {
            return sContextGetter != null ? getApplicationContext() : null;
        }

        @Override
        protected Bundle getCrashExtras(final Thread thread, final Throwable exc) {
            final Bundle extras = super.getCrashExtras(thread, exc);

            extras.putString("ProductName", AppConstants.MOZ_APP_BASENAME);
            extras.putString("ProductID", AppConstants.MOZ_APP_ID);
            extras.putString("Version", AppConstants.MOZ_APP_VERSION);
            extras.putString("BuildID", AppConstants.MOZ_APP_BUILDID);
            extras.putString("Vendor", AppConstants.MOZ_APP_VENDOR);
            extras.putString("ReleaseChannel", AppConstants.MOZ_UPDATE_CHANNEL);
            return extras;
        }

        @Override
        public void uncaughtException(final Thread thread, final Throwable exc) {
            if (GeckoThread.isState(GeckoThread.State.EXITING) ||
                    GeckoThread.isState(GeckoThread.State.EXITED)) {
                // We've called System.exit. All exceptions after this point are Android
                // berating us for being nasty to it.
                return;
            }

            super.uncaughtException(thread, exc);
        }

        @Override
        public boolean reportException(final Thread thread, final Throwable exc) {
            try {
                if (exc instanceof OutOfMemoryError) {
                    SharedPreferences prefs = getSharedPreferences();
                    SharedPreferences.Editor editor = prefs.edit();
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
            if (AppConstants.MOZ_CRASHREPORTER && AppConstants.MOZILLA_OFFICIAL) {
                // Only use Java crash reporter if enabled on official build.
                return super.reportException(thread, exc);
            }
            return false;
        }
    };

    public static CrashHandler ensureCrashHandling() {
        // Crash handling is automatically enabled when GeckoAppShell is loaded.
        return CRASH_HANDLER;
    }

    private static volatile boolean locationHighAccuracyEnabled;

    // See also HardwareUtils.LOW_MEMORY_THRESHOLD_MB.
    private static final int HIGH_MEMORY_DEVICE_THRESHOLD_MB = 768;

    static private int sDensityDpi;
    static private int sScreenDepth;

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

    private static final String GECKOREQUEST_RESPONSE_KEY = "response";
    private static final String GECKOREQUEST_ERROR_KEY = "error";

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

    private static LayerView sLayerView;
    private static Rect sScreenSize;

    public static void setLayerView(LayerView lv) {
        if (sLayerView == lv) {
            return;
        }
        sLayerView = lv;
    }

    @RobocopTarget
    public static LayerView getLayerView() {
        return sLayerView;
    }

    /**
     * Sends an asynchronous request to Gecko.
     *
     * The response data will be passed to {@link GeckoRequest#onResponse(NativeJSObject)} if the
     * request succeeds; otherwise, {@link GeckoRequest#onError()} will fire.
     *
     * It can be called from any thread. The GeckoRequest callbacks will be executed on the Gecko thread.
     *
     * @param request The request to dispatch. Cannot be null.
     */
    @RobocopTarget
    public static void sendRequestToGecko(final GeckoRequest request) {
        final String responseMessage = "Gecko:Request" + request.getId();

        EventDispatcher.getInstance().registerGeckoThreadListener(new NativeEventListener() {
            @Override
            public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
                EventDispatcher.getInstance().unregisterGeckoThreadListener(this, event);
                if (!message.has(GECKOREQUEST_RESPONSE_KEY)) {
                    request.onError(message.getObject(GECKOREQUEST_ERROR_KEY));
                    return;
                }
                request.onResponse(message.getObject(GECKOREQUEST_RESPONSE_KEY));
            }
        }, responseMessage);

        notifyObservers(request.getName(), request.getData());
    }

    // Synchronously notify a Gecko observer; must be called from Gecko thread.
    @WrapForJNI(calledFrom = "gecko")
    public static native void syncNotifyObservers(String topic, String data);

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
    private static String getExceptionStackTrace(Throwable e) {
        return CrashHandler.getExceptionStackTrace(CrashHandler.getRootException(e));
    }

    @WrapForJNI(exceptionMode = "ignore")
    private static void handleUncaughtException(Throwable e) {
        CRASH_HANDLER.uncaughtException(null, e);
    }

    @WrapForJNI
    public static void openWindowForNotification() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);

        getApplicationContext().startActivity(intent);
    }

    private static float getLocationAccuracy(Location location) {
        float radius = location.getAccuracy();
        return (location.hasAccuracy() && radius > 0) ? radius : 1001;
    }

    @SuppressLint("MissingPermission") // Permissions are explicitly checked for in enableLocation()
    private static Location getLastKnownLocation(LocationManager lm) {
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
    @SuppressLint("MissingPermission") // Permissions are explicitly checked for within this method
    private static void enableLocation(final boolean enable) {
        final Runnable requestLocation = new Runnable() {
            @Override
            public void run() {
                LocationManager lm = getLocationManager(getApplicationContext());
                if (lm == null) {
                    return;
                }

                if (!enable) {
                    lm.removeUpdates(getLocationListener());
                    return;
                }

                Location lastKnownLocation = getLastKnownLocation(lm);
                if (lastKnownLocation != null) {
                    getLocationListener().onLocationChanged(lastKnownLocation);
                }

                Criteria criteria = new Criteria();
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

                String provider = lm.getBestProvider(criteria, true);
                if (provider == null)
                    return;

                Looper l = Looper.getMainLooper();
                lm.requestLocationUpdates(provider, 100, 0.5f, getLocationListener(), l);
            }
        };

        Permissions
                .from((Activity) getContext())
                .withPermissions(Manifest.permission.ACCESS_FINE_LOCATION)
                .onUIThread()
                .doNotPromptIf(!enable)
                .run(requestLocation);
    }

    private static LocationManager getLocationManager(Context context) {
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

    @WrapForJNI(calledFrom = "gecko")
    private static boolean setAlarm(int aSeconds, int aNanoSeconds) {
        AlarmManager am = (AlarmManager)
            getApplicationContext().getSystemService(Context.ALARM_SERVICE);

        Intent intent = new Intent(getApplicationContext(), AlarmReceiver.class);
        PendingIntent pi = PendingIntent.getBroadcast(
                getApplicationContext(), 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        // AlarmManager only supports millisecond precision
        long time = ((long) aSeconds * 1000) + ((long) aNanoSeconds / 1_000_000L);
        am.setExact(AlarmManager.RTC_WAKEUP, time, pi);

        return true;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void disableAlarm() {
        AlarmManager am = (AlarmManager)
            getApplicationContext().getSystemService(Context.ALARM_SERVICE);

        Intent intent = new Intent(getApplicationContext(), AlarmReceiver.class);
        PendingIntent pi = PendingIntent.getBroadcast(
                getApplicationContext(), 0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
        am.cancel(pi);
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    /* package */ static native void onSensorChanged(int hal_type, float x, float y, float z,
                                                     float w, int accuracy, long time);

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    /* package */ static native void onLocationChanged(double latitude, double longitude,
                                                       double altitude, float accuracy,
                                                       float bearing, float speed, long time);

    private static class DefaultListeners
            implements SensorEventListener, LocationListener, NotificationListener {
        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
        }

        private static int HalSensorAccuracyFor(int androidAccuracy) {
            switch (androidAccuracy) {
            case SensorManager.SENSOR_STATUS_UNRELIABLE:
                return GeckoHalDefines.SENSOR_ACCURACY_UNRELIABLE;
            case SensorManager.SENSOR_STATUS_ACCURACY_LOW:
                return GeckoHalDefines.SENSOR_ACCURACY_LOW;
            case SensorManager.SENSOR_STATUS_ACCURACY_MEDIUM:
                return GeckoHalDefines.SENSOR_ACCURACY_MED;
            case SensorManager.SENSOR_STATUS_ACCURACY_HIGH:
                return GeckoHalDefines.SENSOR_ACCURACY_HIGH;
            }
            return GeckoHalDefines.SENSOR_ACCURACY_UNKNOWN;
        }

        @Override
        public void onSensorChanged(SensorEvent s) {
            int sensor_type = s.sensor.getType();
            int hal_type = 0;
            float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
            final int accuracy = HalSensorAccuracyFor(s.accuracy);
            // SensorEvent timestamp is in nanoseconds, Gecko expects microseconds.
            final long time = s.timestamp / 1000;

            switch (sensor_type) {
            case Sensor.TYPE_ACCELEROMETER:
            case Sensor.TYPE_LINEAR_ACCELERATION:
            case Sensor.TYPE_ORIENTATION:
                if (sensor_type == Sensor.TYPE_ACCELEROMETER) {
                    hal_type = GeckoHalDefines.SENSOR_ACCELERATION;
                } else if (sensor_type == Sensor.TYPE_LINEAR_ACCELERATION) {
                    hal_type = GeckoHalDefines.SENSOR_LINEAR_ACCELERATION;
                } else {
                    hal_type = GeckoHalDefines.SENSOR_ORIENTATION;
                }
                x = s.values[0];
                y = s.values[1];
                z = s.values[2];
                break;

            case Sensor.TYPE_GYROSCOPE:
                hal_type = GeckoHalDefines.SENSOR_GYROSCOPE;
                x = (float) Math.toDegrees(s.values[0]);
                y = (float) Math.toDegrees(s.values[1]);
                z = (float) Math.toDegrees(s.values[2]);
                break;

            case Sensor.TYPE_PROXIMITY:
                hal_type = GeckoHalDefines.SENSOR_PROXIMITY;
                x = s.values[0];
                z = s.sensor.getMaximumRange();
                break;

            case Sensor.TYPE_LIGHT:
                hal_type = GeckoHalDefines.SENSOR_LIGHT;
                x = s.values[0];
                break;

            case Sensor.TYPE_ROTATION_VECTOR:
            case Sensor.TYPE_GAME_ROTATION_VECTOR: // API >= 18
                hal_type = (sensor_type == Sensor.TYPE_ROTATION_VECTOR ?
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

            GeckoAppShell.onSensorChanged(hal_type, x, y, z, w, accuracy, time);
        }

        // Geolocation.
        @Override
        public void onLocationChanged(Location location) {
            // No logging here: user-identifying information.
            GeckoAppShell.onLocationChanged(location.getLatitude(), location.getLongitude(),
                                            location.getAltitude(), location.getAccuracy(),
                                            location.getBearing(), location.getSpeed(),
                                            location.getTime());
        }

        @Override
        public void onProviderDisabled(String provider)
        {
        }

        @Override
        public void onProviderEnabled(String provider)
        {
        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras)
        {
        }

        @Override // NotificationListener
        public void showNotification(String name, String cookie, String host,
                                     String title, String text, String imageUrl) {
            // Default is to not show the notification, and immediate send close message.
            GeckoAppShell.onNotificationClose(name, cookie);
        }

        @Override // NotificationListener
        public void showPersistentNotification(String name, String cookie, String host,
                                               String title, String text, String imageUrl,
                                               String data) {
            // Default is to not show the notification, and immediate send close message.
            GeckoAppShell.onNotificationClose(name, cookie);
        }

        @Override // NotificationListener
        public void closeNotification(String name) {
            // Do nothing.
        }
    }

    private static final DefaultListeners DEFAULT_LISTENERS = new DefaultListeners();
    private static SensorEventListener sSensorListener = DEFAULT_LISTENERS;
    private static LocationListener sLocationListener = DEFAULT_LISTENERS;
    private static NotificationListener sNotificationListener = DEFAULT_LISTENERS;

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

    @WrapForJNI(calledFrom = "gecko")
    private static void enableSensor(int aSensortype) {
        GeckoInterface gi = getGeckoInterface();
        if (gi == null) {
            return;
        }
        SensorManager sm = (SensorManager)
            getApplicationContext().getSystemService(Context.SENSOR_SERVICE);

        switch (aSensortype) {
        case GeckoHalDefines.SENSOR_GAME_ROTATION_VECTOR:
            if (gGameRotationVectorSensor == null) {
                gGameRotationVectorSensor = sm.getDefaultSensor(15);
                    // sm.getDefaultSensor(
                    //     Sensor.TYPE_GAME_ROTATION_VECTOR); // API >= 18
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

    @WrapForJNI(calledFrom = "gecko")
    private static void disableSensor(int aSensortype) {
        GeckoInterface gi = getGeckoInterface();
        if (gi == null)
            return;

        SensorManager sm = (SensorManager)
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
        if (getGeckoInterface() != null)
            getGeckoInterface().getActivity().moveTaskToBack(true);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static void scheduleRestart() {
        getGeckoInterface().doRestart();
    }

    // Creates a homescreen shortcut for a web page.
    // This is the entry point from nsIShellService.
    @WrapForJNI(calledFrom = "gecko")
    public static void createShortcut(final String aTitle, final String aURI) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return;
        }
        geckoInterface.createShortcut(aTitle, aURI);
    }

    @JNITarget
    static public int getPreferredIconSize() {
        ActivityManager am = (ActivityManager)
            getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE);
        return am.getLauncherLargeIconSize();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String[] getHandlersForMimeType(String aMimeType, String aAction) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return new String[] {};
        }
        return geckoInterface.getHandlersForMimeType(aMimeType, aAction);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String[] getHandlersForURL(String aURL, String aAction) {
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

    static List<ResolveInfo> queryIntentActivities(Intent intent) {
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
    public static String getExtensionFromMimeType(String aMimeType) {
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(aMimeType);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static String getMimeTypeFromExtensions(String aFileExt) {
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

    @WrapForJNI(calledFrom = "gecko")
    private static boolean openUriExternal(String targetURI,
                                           String mimeType,
                                           String packageName,
                                           String className,
                                           String action,
                                           String title) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return false;
        }
        return geckoInterface.openUriExternal(targetURI, mimeType, packageName, className, action, title);
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
    private static void showNotification(String name, String cookie, String title,
                                         String text, String host, String imageUrl,
                                         String persistentData) {
        if (persistentData == null) {
            getNotificationListener().showNotification(name, cookie, title, text, host, imageUrl);
            return;
        }

        getNotificationListener().showPersistentNotification(
                name, cookie, title, text, host, imageUrl, persistentData);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void closeNotification(String name) {
        getNotificationListener().closeNotification(name);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static int getDpi() {
        if (sDensityDpi == 0) {
            sDensityDpi = getApplicationContext().getResources().getDisplayMetrics().densityDpi;
        }

        return sDensityDpi;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static float getDensity() {
        return getApplicationContext().getResources().getDisplayMetrics().density;
    }

    private static boolean isHighMemoryDevice() {
        return HardwareUtils.getMemSize() > HIGH_MEMORY_DEVICE_THRESHOLD_MB;
    }

    /**
     * Returns the colour depth of the default screen. This will either be
     * 24 or 16.
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
                sScreenDepth = 24;
            }
        }

        return sScreenDepth;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static synchronized void setScreenDepthOverride(int aScreenDepth) {
        if (sScreenDepth != 0) {
            Log.e(LOGTAG, "Tried to override screen depth after it's already been set");
            return;
        }

        sScreenDepth = aScreenDepth;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void setFullScreen(boolean fullscreen) {
        if (getGeckoInterface() != null)
            getGeckoInterface().setFullScreen(fullscreen);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void performHapticFeedback(boolean aIsLongPress) {
        // Don't perform haptic feedback if a vibration is currently playing,
        // because the haptic feedback will nuke the vibration.
        if (!sVibrationMaybePlaying || System.nanoTime() >= sVibrationEndTime) {
            LayerView layerView = getLayerView();
            layerView.performHapticFeedback(aIsLongPress ?
                                            HapticFeedbackConstants.LONG_PRESS :
                                            HapticFeedbackConstants.VIRTUAL_KEY);
        }
    }

    private static Vibrator vibrator() {
        return (Vibrator) getApplicationContext().getSystemService(Context.VIBRATOR_SERVICE);
    }

    // Helper method to convert integer array to long array.
    private static long[] convertIntToLongArray(int[] input) {
        long[] output = new long[input.length];
        for (int i = 0; i < input.length; i++) {
            output[i] = input[i];
        }
        return output;
    }

    // Vibrate only if haptic feedback is enabled.
    public static void vibrateOnHapticFeedbackEnabled(int[] milliseconds) {
        if (Settings.System.getInt(getApplicationContext().getContentResolver(),
                                   Settings.System.HAPTIC_FEEDBACK_ENABLED, 0) > 0) {
            vibrate(convertIntToLongArray(milliseconds), -1);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void vibrate(long milliseconds) {
        sVibrationEndTime = System.nanoTime() + milliseconds * 1000000;
        sVibrationMaybePlaying = true;
        vibrator().vibrate(milliseconds);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void vibrate(long[] pattern, int repeat) {
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

    @WrapForJNI(calledFrom = "gecko")
    private static void cancelVibrate() {
        sVibrationMaybePlaying = false;
        sVibrationEndTime = 0;
        vibrator().cancel();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void setKeepScreenOn(final boolean on) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // TODO
            }
        });
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
            public boolean callback(int pid) {
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

    private static void EnumerateGeckoProcesses(GeckoProcessesVisitor visiter) {
        int pidColumn = -1;
        int userColumn = -1;

        try {
            // run ps and parse its output
            java.lang.Process ps = Runtime.getRuntime().exec("ps");
            BufferedReader in = new BufferedReader(new InputStreamReader(ps.getInputStream()),
                                                   2048);

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
            in.close();
        }
        catch (Exception e) {
            Log.w(LOGTAG, "Failed to enumerate Gecko processes.",  e);
        }
    }

    public static String getAppNameByPID(int pid) {
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
            if (null != cmdlineReader) {
                try {
                    cmdlineReader.close();
                } catch (Exception e) { }
            }
        }
    }

    public static void listOfOpenFiles() {
        int pidColumn = -1;
        int nameColumn = -1;

        try {
            String filter = GeckoProfile.get(getApplicationContext()).getDir().toString();
            Log.d(LOGTAG, "[OPENFILE] Filter: " + filter);

            // run lsof and parse its output
            java.lang.Process lsof = Runtime.getRuntime().exec("lsof");
            BufferedReader in = new BufferedReader(new InputStreamReader(lsof.getInputStream()), 2048);

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
            in.close();
        } catch (Exception e) { }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static byte[] getIconForExtension(String aExt, int iconSize) {
        try {
            if (iconSize <= 0)
                iconSize = 16;

            if (aExt != null && aExt.length() > 1 && aExt.charAt(0) == '.')
                aExt = aExt.substring(1);

            PackageManager pm = getApplicationContext().getPackageManager();
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

    public static String getMimeTypeFromExtension(String ext) {
        final MimeTypeMap mtm = MimeTypeMap.getSingleton();
        return mtm.getMimeTypeFromExtension(ext);
    }

    private static Drawable getDrawableForExtension(PackageManager pm, String aExt) {
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
        }
        catch (Exception e) {
            return true;
        }
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
    public static native void onFullScreenPluginHidden(View view);

    @WrapForJNI(calledFrom = "gecko")
    private static void addFullScreenPluginView(View view) {
        if (getGeckoInterface() != null)
             getGeckoInterface().addPluginView(view);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void removeFullScreenPluginView(View view) {
        if (getGeckoInterface() != null)
            getGeckoInterface().removePluginView(view);
    }

    /**
     * A plugin that wish to be loaded in the WebView must provide this permission
     * in their AndroidManifest.xml.
     */
    public static final String PLUGIN_ACTION = "android.webkit.PLUGIN";
    public static final String PLUGIN_PERMISSION = "android.webkit.permission.PLUGIN";

    private static final String PLUGIN_SYSTEM_LIB = "/system/lib/plugins/";

    private static final String PLUGIN_TYPE = "type";
    private static final String TYPE_NATIVE = "native";
    public static final ArrayList<PackageInfo> mPackageInfoCache = new ArrayList<>();

    // Returns null if plugins are blocked on the device.
    static String[] getPluginDirectories() {

        // Block on Pixel C.
        if ((new File("/system/lib/hw/power.dragon.so")).exists()) {
            Log.w(LOGTAG, "Blocking plugins because of Pixel C device (bug 1255122)");
            return null;
        }
        // An awful hack to detect Tegra devices. Easiest way to do it without spinning up a EGL context.
        boolean isTegra = (new File("/system/lib/hw/gralloc.tegra.so")).exists() ||
                          (new File("/system/lib/hw/gralloc.tegra3.so")).exists() ||
                          (new File("/sys/class/nvidia-gpu")).exists();
        if (isTegra) {
            // disable on KitKat (bug 957694)
            if (Versions.feature19Plus) {
                Log.w(LOGTAG, "Blocking plugins because of Tegra (bug 957694)");
                return null;
            }

            // disable Flash on Tegra ICS with CM9 and other custom firmware (bug 736421)
            final File vfile = new File("/proc/version");
            try {
                if (vfile.canRead()) {
                    final BufferedReader reader = new BufferedReader(new FileReader(vfile));
                    try {
                        final String version = reader.readLine();
                        if (version.indexOf("CM9") != -1 ||
                            version.indexOf("cyanogen") != -1 ||
                            version.indexOf("Nova") != -1) {
                            Log.w(LOGTAG, "Blocking plugins because of Tegra 2 + unofficial ICS bug (bug 736421)");
                            return null;
                        }
                    } finally {
                      reader.close();
                    }
                }
            } catch (IOException ex) {
                // Do nothing.
            }
        }

        ArrayList<String> directories = new ArrayList<String>();
        PackageManager pm = getApplicationContext().getPackageManager();
        List<ResolveInfo> plugins = pm.queryIntentServices(new Intent(PLUGIN_ACTION),
                PackageManager.GET_META_DATA);

        synchronized (mPackageInfoCache) {

            // clear the list of existing packageInfo objects
            mPackageInfoCache.clear();


            for (ResolveInfo info : plugins) {

                // retrieve the plugin's service information
                ServiceInfo serviceInfo = info.serviceInfo;
                if (serviceInfo == null) {
                    Log.w(LOGTAG, "Ignoring bad plugin.");
                    continue;
                }

                // Blacklist HTC's flash lite.
                // See bug #704516 - We're not quite sure what Flash Lite does,
                // but loading it causes Flash to give errors and fail to draw.
                if (serviceInfo.packageName.equals("com.htc.flashliteplugin")) {
                    Log.w(LOGTAG, "Skipping HTC's flash lite plugin");
                    continue;
                }


                // Retrieve information from the plugin's manifest.
                PackageInfo pkgInfo;
                try {
                    pkgInfo = pm.getPackageInfo(serviceInfo.packageName,
                                    PackageManager.GET_PERMISSIONS
                                    | PackageManager.GET_SIGNATURES);
                } catch (Exception e) {
                    Log.w(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                }

                if (pkgInfo == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Could not load package information.");
                    continue;
                }

                /*
                 * find the location of the plugin's shared library. The default
                 * is to assume the app is either a user installed app or an
                 * updated system app. In both of these cases the library is
                 * stored in the app's data directory.
                 */
                String directory = pkgInfo.applicationInfo.dataDir + "/lib";
                final int appFlags = pkgInfo.applicationInfo.flags;
                final int updatedSystemFlags = ApplicationInfo.FLAG_SYSTEM |
                                               ApplicationInfo.FLAG_UPDATED_SYSTEM_APP;

                // preloaded system app with no user updates
                if ((appFlags & updatedSystemFlags) == ApplicationInfo.FLAG_SYSTEM) {
                    directory = PLUGIN_SYSTEM_LIB + pkgInfo.packageName;
                }

                // check if the plugin has the required permissions
                String permissions[] = pkgInfo.requestedPermissions;
                if (permissions == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission.");
                    continue;
                }
                boolean permissionOk = false;
                for (String permit : permissions) {
                    if (PLUGIN_PERMISSION.equals(permit)) {
                        permissionOk = true;
                        break;
                    }
                }
                if (!permissionOk) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission (2).");
                    continue;
                }

                // check to ensure the plugin is properly signed
                Signature signatures[] = pkgInfo.signatures;
                if (signatures == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Not signed.");
                    continue;
                }

                // determine the type of plugin from the manifest
                if (serviceInfo.metaData == null) {
                    Log.e(LOGTAG, "The plugin '" + serviceInfo.name + "' has no defined type.");
                    continue;
                }

                String pluginType = serviceInfo.metaData.getString(PLUGIN_TYPE);
                if (!TYPE_NATIVE.equals(pluginType)) {
                    Log.e(LOGTAG, "Unrecognized plugin type: " + pluginType);
                    continue;
                }

                try {
                    Class<?> cls = getPluginClass(serviceInfo.packageName, serviceInfo.name);

                    //TODO implement any requirements of the plugin class here!
                    boolean classFound = true;

                    if (!classFound) {
                        Log.e(LOGTAG, "The plugin's class' " + serviceInfo.name + "' does not extend the appropriate class.");
                        continue;
                    }

                } catch (NameNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                } catch (ClassNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin's class: " + serviceInfo.name);
                    continue;
                }

                // if all checks have passed then make the plugin available
                mPackageInfoCache.add(pkgInfo);
                directories.add(directory);
            }
        }

        return directories.toArray(new String[directories.size()]);
    }

    static String getPluginPackage(String pluginLib) {

        if (pluginLib == null || pluginLib.length() == 0) {
            return null;
        }

        synchronized (mPackageInfoCache) {
            for (PackageInfo pkgInfo : mPackageInfoCache) {
                if (pluginLib.contains(pkgInfo.packageName)) {
                    return pkgInfo.packageName;
                }
            }
        }

        return null;
    }

    static Class<?> getPluginClass(String packageName, String className)
            throws NameNotFoundException, ClassNotFoundException {
        Context pluginContext = getApplicationContext().createPackageContext(packageName,
                Context.CONTEXT_INCLUDE_CODE |
                Context.CONTEXT_IGNORE_SECURITY);
        ClassLoader pluginCL = pluginContext.getClassLoader();
        return pluginCL.loadClass(className);
    }

    @WrapForJNI
    private static Class<?> loadPluginClass(String className, String libName) {
        if (getGeckoInterface() == null)
            return null;
        try {
            final String packageName = getPluginPackage(libName);
            final int contextFlags = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
            final Context pluginContext = getApplicationContext().createPackageContext(
                    packageName, contextFlags);
            return pluginContext.getClassLoader().loadClass(className);
        } catch (java.lang.ClassNotFoundException cnfe) {
            Log.w(LOGTAG, "Couldn't find plugin class " + className, cnfe);
            return null;
        } catch (android.content.pm.PackageManager.NameNotFoundException nnfe) {
            Log.w(LOGTAG, "Couldn't find package.", nnfe);
            return null;
        }
    }

    private static Context sApplicationContext;
    private static ContextGetter sContextGetter;

    @Deprecated
    @WrapForJNI
    public static Context getContext() {
        return sContextGetter.getContext();
    }

    public static void setContextGetter(ContextGetter cg) {
        sContextGetter = cg;
    }

    @WrapForJNI
    public static Context getApplicationContext() {
        return sApplicationContext;
    }

    public static void setApplicationContext(final Context context) {
        sApplicationContext = context;
    }

    public static SharedPreferences getSharedPreferences() {
        if (sContextGetter == null) {
            throw new IllegalStateException("No ContextGetter; cannot fetch prefs.");
        }
        return sContextGetter.getSharedPreferences();
    }

    public interface AppStateListener {
        public void onPause();
        public void onResume();
        public void onOrientationChanged();
    }

    public interface GeckoInterface {
        public EventDispatcher getAppEventDispatcher();
        public GeckoProfile getProfile();
        public Activity getActivity();
        public String getDefaultUAString();
        public void doRestart();
        public void setFullScreen(boolean fullscreen);
        public void addPluginView(View view);
        public void removePluginView(final View view);
        public void enableOrientationListener();
        public void disableOrientationListener();
        public void addAppStateListener(AppStateListener listener);
        public void removeAppStateListener(AppStateListener listener);
        public void notifyWakeLockChanged(String topic, String state);
        public boolean areTabsShown();
        public AbsoluteLayout getPluginContainer();
        public void notifyCheckUpdateResult(String result);
        public void invalidateOptionsMenu();

        /**
         * Create a shortcut -- generally a home-screen icon -- linking the given title to the given URI.
         * <p>
         * This method is always invoked on the Gecko thread.
         *
         * @param title of URI to link to.
         * @param URI to link to.
         */
        public void createShortcut(String title, String URI);

        /**
         * Check if the given URI is visited.
         * <p/>
         * If it has been visited, call {@link GeckoAppShell#notifyUriVisited(String)}.  (If it
         * has not been visited, do nothing.)
         * <p/>
         * This method is always invoked on the Gecko thread.
         *
         * @param uri to check.
         */
        public void checkUriVisited(String uri);

        /**
         * Mark the given URI as visited in Gecko.
         * <p/>
         * Implementors may maintain some local store of visited URIs in order to be able to
         * answer {@link #checkUriVisited(String)} requests affirmatively.
         * <p/>
         * This method is always invoked on the Gecko thread.
         *
         * @param uri to mark.
         */
        public void markUriVisited(final String uri);

        /**
         * Set the title of the given URI, as determined by Gecko.
         * <p/>
         * This method is always invoked on the Gecko thread.
         *
         * @param uri given.
         * @param title to associate with the given URI.
         */
        public void setUriTitle(final String uri, final String title);

        public void setAccessibilityEnabled(boolean enabled);

        public boolean openUriExternal(String targetURI, String mimeType, String packageName, String className, String action, String title);

        public String[] getHandlersForMimeType(String mimeType, String action);
        public String[] getHandlersForURL(String url, String action);

        /**
         * URI of the underlying chrome window to be opened, or null to use the default GeckoView
         * XUL container <tt>chrome://browser/content/geckoview.xul</tt>.  See
         * <a href="https://developer.mozilla.org/en/docs/toolkit.defaultChromeURI">https://developer.mozilla.org/en/docs/toolkit.defaultChromeURI</a>
         *
         * @return URI or null.
         */
        String getDefaultChromeURI();
    };

    private static GeckoInterface sGeckoInterface;

    public static GeckoInterface getGeckoInterface() {
        return sGeckoInterface;
    }

    public static void setGeckoInterface(GeckoInterface aGeckoInterface) {
        sGeckoInterface = aGeckoInterface;
    }

    /* package */ static Camera sCamera;

    private static final int kPreferredFPS = 25;
    private static byte[] sCameraBuffer;

    private static class CameraCallback implements Camera.PreviewCallback {
        @WrapForJNI(calledFrom = "gecko")
        private static native void onFrameData(int camera, byte[] data);

        private final int mCamera;

        public CameraCallback(int camera) {
            mCamera = camera;
        }

        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
            onFrameData(mCamera, data);

            if (sCamera != null) {
                sCamera.addCallbackBuffer(sCameraBuffer);
            }
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static int[] initCamera(String aContentType, int aCamera, int aWidth, int aHeight) {
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (getGeckoInterface() != null)
                            getGeckoInterface().enableOrientationListener();
                    } catch (Exception e) { }
                }
            });

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
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (getGeckoInterface() != null)
                            getGeckoInterface().disableOrientationListener();
                    } catch (Exception e) { }
                }
            });
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
    private static void handleGeckoMessage(final NativeJSContainer message) {
        boolean success = EventDispatcher.getInstance().dispatchEvent(message);
        if (getGeckoInterface() != null && getGeckoInterface().getAppEventDispatcher() != null) {
            success |= getGeckoInterface().getAppEventDispatcher().dispatchEvent(message);
        }

        if (!success) {
            final String type = message.optString("type", null);
            final String guid = message.optString(EventDispatcher.GUID, null);
            if (type != null && guid != null) {
                (new EventDispatcher.GeckoEventCallback(guid, type)).sendError("No listeners for request");
            }
        }
        message.disposeNative();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void disableBatteryNotifications() {
        GeckoBatteryManager.disableNotifications();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static double[] getCurrentBatteryInformation() {
        return GeckoBatteryManager.getCurrentInformation();
    }

    @WrapForJNI(stubName = "CheckURIVisited", calledFrom = "gecko")
    private static void checkUriVisited(String uri) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return;
        }
        geckoInterface.checkUriVisited(uri);
    }

    @WrapForJNI(stubName = "MarkURIVisited", calledFrom = "gecko")
    private static void markUriVisited(final String uri) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return;
        }
        geckoInterface.markUriVisited(uri);
    }

    @WrapForJNI(stubName = "SetURITitle", calledFrom = "gecko")
    private static void setUriTitle(final String uri, final String title) {
        final GeckoInterface geckoInterface = getGeckoInterface();
        if (geckoInterface == null) {
            return;
        }
        geckoInterface.setUriTitle(uri, title);
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

    private static boolean sImeWasEnabledOnLastResize = false;
    public static void viewSizeChanged() {
        GeckoView v = (GeckoView) getLayerView();
        if (v == null) {
            return;
        }
        boolean imeIsEnabled = v.isIMEEnabled();
        if (imeIsEnabled && !sImeWasEnabledOnLastResize) {
            // The IME just came up after not being up, so let's scroll
            // to the focused input.
            notifyObservers("ScrollTo:FocusedInput", "");
        }
        sImeWasEnabledOnLastResize = imeIsEnabled;
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
    private static void lockScreenOrientation(int aOrientation) {
        GeckoScreenOrientation.getInstance().lock(aOrientation);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void unlockScreenOrientation() {
        GeckoScreenOrientation.getInstance().unlock();
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void notifyWakeLockChanged(String topic, String state) {
        if (getGeckoInterface() != null)
            getGeckoInterface().notifyWakeLockChanged(topic, state);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean unlockProfile() {
        // Try to kill any zombie Fennec's that might be running
        GeckoAppShell.killAnyZombies();

        // Then force unlock this profile
        if (getGeckoInterface() != null) {
            GeckoProfile profile = getGeckoInterface().getProfile();
            File lock = profile.getFile(".parentlock");
            return lock.exists() && lock.delete();
        }
        return false;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static String getProxyForURI(String spec, String scheme, String host, int port) {
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
    private static InputStream createInputStream(URLConnection connection) throws IOException {
        return connection.getInputStream();
    }

    private static class BitmapConnection extends URLConnection {
        private Bitmap bitmap;

        BitmapConnection(Bitmap b) throws MalformedURLException, IOException {
            super(null);
            bitmap = b;
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
            public synchronized int read(byte[] buffer, int byteOffset, int byteCount)
                                    throws IOException {
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
                                bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
                                output.close();
                            } catch (IOException ioe) { }
                        }
                    });
                mHaveConnected = true;
                return super.read(buffer, byteOffset, byteCount);
            }
        }
    }

    @WrapForJNI
    private static URLConnection getConnection(String url) {
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
    private static String connectionGetMimeType(URLConnection connection) {
        return connection.getContentType();
    }

    /**
     * Retrieve the absolute path of an external storage directory.
     *
     * @param type The type of directory to return
     * @return Absolute path of the specified directory or null on failure
     */
    @WrapForJNI(calledFrom = "gecko")
    private static String getExternalPublicDirectory(final String type) {
        final String state = Environment.getExternalStorageState();
        if (!Environment.MEDIA_MOUNTED.equals(state) &&
            !Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
            // External storage is not available.
            return null;
        }

        if ("sdcard".equals(type)) {
            // SD card has a separate path.
            return Environment.getExternalStorageDirectory().getAbsolutePath();
        }

        final String systemType;
        if ("downloads".equals(type)) {
            systemType = Environment.DIRECTORY_DOWNLOADS;
        } else if ("pictures".equals(type)) {
            systemType = Environment.DIRECTORY_PICTURES;
        } else if ("videos".equals(type)) {
            systemType = Environment.DIRECTORY_MOVIES;
        } else if ("music".equals(type)) {
            systemType = Environment.DIRECTORY_MUSIC;
        } else if ("apps".equals(type)) {
            File appInternalStorageDirectory = getApplicationContext().getFilesDir();
            return new File(appInternalStorageDirectory, "mozilla").getAbsolutePath();
        } else {
            return null;
        }
        return Environment.getExternalStoragePublicDirectory(systemType).getAbsolutePath();
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

    public static synchronized void resetScreenSize() {
        sScreenSize = null;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static synchronized Rect getScreenSize() {
        if (sScreenSize == null) {
            final WindowManager wm = (WindowManager)
                    getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
            final Display disp = wm.getDefaultDisplay();
            sScreenSize = new Rect(0, 0, disp.getWidth(), disp.getHeight());
        }
        return sScreenSize;
    }

    @WrapForJNI
    private static int startGeckoServiceChildProcess(String type, String[] args, int crashFd, int ipcFd) {
        return GeckoProcessManager.getInstance().start(type, args, crashFd, ipcFd);
    }
}
