/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.display.DisplayManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Looper;
import android.os.PowerManager;
import android.os.Vibrator;
import android.provider.Settings;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.InputDevice;
import android.view.WindowManager;
import android.webkit.MimeTypeMap;
import androidx.annotation.Nullable;
import androidx.collection.SimpleArrayMap;
import androidx.core.content.res.ResourcesCompat;
import java.net.Proxy;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Locale;
import java.util.StringTokenizer;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.HardwareCodecCapabilityUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.R;

public class GeckoAppShell {
  private static final String LOGTAG = "GeckoAppShell";

  /*
   * Keep these values consistent with |SensorType| in HalSensor.h
   */
  public static final int SENSOR_ORIENTATION = 0;
  public static final int SENSOR_ACCELERATION = 1;
  public static final int SENSOR_PROXIMITY = 2;
  public static final int SENSOR_LINEAR_ACCELERATION = 3;
  public static final int SENSOR_GYROSCOPE = 4;
  public static final int SENSOR_LIGHT = 5;
  public static final int SENSOR_ROTATION_VECTOR = 6;
  public static final int SENSOR_GAME_ROTATION_VECTOR = 7;

  // We have static members only.
  private GeckoAppShell() {}

  // Name for app-scoped prefs
  public static final String APP_PREFS_NAME = "GeckoApp";

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
    public boolean reportException(final Thread thread, final Throwable exc) {
      try {
        if (exc instanceof OutOfMemoryError) {
          final SharedPreferences prefs =
              getApplicationContext().getSharedPreferences(APP_PREFS_NAME, 0);
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

  public static synchronized CrashHandler ensureCrashHandling(
      final Class<? extends Service> handler) {
    if (sCrashHandler == null) {
      sCrashHandler = new GeckoCrashHandler(handler);
    }

    return sCrashHandler;
  }

  private static Class<? extends Service> sCrashHandlerService;

  public static synchronized void setCrashHandlerService(
      final Class<? extends Service> handlerService) {
    sCrashHandlerService = handlerService;
  }

  public static synchronized Class<? extends Service> getCrashHandlerService() {
    return sCrashHandlerService;
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
  private static volatile boolean locationListeningRequested = false;
  private static volatile boolean locationPaused = false;

  // See also HardwareUtils.LOW_MEMORY_THRESHOLD_MB.
  private static final int HIGH_MEMORY_DEVICE_THRESHOLD_MB = 768;

  private static int sDensityDpi;
  private static Float sDensity;
  private static int sScreenDepth;
  private static boolean sUseMaxScreenDepth;

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
  private static Sensor gLightSensor;
  private static Sensor gRotationVectorSensor;
  private static Sensor gGameRotationVectorSensor;

  /*
   * Keep in sync with constants found here:
   * http://searchfox.org/mozilla-central/source/uriloader/base/nsIWebProgressListener.idl
   */
  public static final int WPL_STATE_START = 0x00000001;
  public static final int WPL_STATE_STOP = 0x00000010;
  public static final int WPL_STATE_IS_DOCUMENT = 0x00020000;
  public static final int WPL_STATE_IS_NETWORK = 0x00040000;

  /* Keep in sync with constants found here:
    http://searchfox.org/mozilla-central/source/netwerk/base/nsINetworkLinkService.idl
  */
  public static final int LINK_TYPE_UNKNOWN = 0;
  public static final int LINK_TYPE_ETHERNET = 1;
  public static final int LINK_TYPE_USB = 2;
  public static final int LINK_TYPE_WIFI = 3;
  public static final int LINK_TYPE_WIMAX = 4;
  public static final int LINK_TYPE_MOBILE = 9;

  public static final String PREFS_OOM_EXCEPTION = "OOMException";

  /* The Android-side API: API methods that Android calls */

  // helper methods
  @WrapForJNI
  /* package */ static native void reportJavaCrash(Throwable exc, String stackTrace);

  private static Rect sScreenSizeOverride;

  @WrapForJNI(stubName = "NotifyObservers", dispatchTo = "gecko")
  private static native void nativeNotifyObservers(String topic, String data);

  @WrapForJNI(stubName = "AppendAppNotesToCrashReport", dispatchTo = "gecko")
  public static native void nativeAppendAppNotesToCrashReport(final String notes);

  @RobocopTarget
  public static void notifyObservers(final String topic, final String data) {
    notifyObservers(topic, data, GeckoThread.State.RUNNING);
  }

  public static void notifyObservers(
      final String topic, final String data, final GeckoThread.State state) {
    if (GeckoThread.isStateAtLeast(state)) {
      nativeNotifyObservers(topic, data);
    } else {
      GeckoThread.queueNativeCallUntil(
          state,
          GeckoAppShell.class,
          "nativeNotifyObservers",
          String.class,
          topic,
          String.class,
          data);
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
    final float radius = location.getAccuracy();
    return (location.hasAccuracy() && radius > 0) ? radius : 1001;
  }

  // Permissions are explicitly checked when requesting content permission.
  @SuppressLint("MissingPermission")
  private static Location getLastKnownLocation(final LocationManager lm) {
    Location lastKnownLocation = null;
    final List<String> providers = lm.getAllProviders();

    for (final String provider : providers) {
      final Location location = lm.getLastKnownLocation(provider);
      if (location == null) {
        continue;
      }

      if (lastKnownLocation == null) {
        lastKnownLocation = location;
        continue;
      }

      final long timeDiff = location.getTime() - lastKnownLocation.getTime();
      if (timeDiff > 0
          || (timeDiff == 0
              && getLocationAccuracy(location) < getLocationAccuracy(lastKnownLocation))) {
        lastKnownLocation = location;
      }
    }

    return lastKnownLocation;
  }

  // Toggles the location listeners on/off, which will then provide/stop location information
  @WrapForJNI(calledFrom = "gecko")
  private static synchronized boolean enableLocationUpdates(final boolean enable) {
    locationListeningRequested = enable;
    final boolean canListen = updateLocationListeners();
    if (!canListen && locationListeningRequested) {
      // Didn't successfully start listener when requested
      locationListeningRequested = false;
    }
    return canListen;
  }

  // Permissions are explicitly checked when requesting content permission.
  @SuppressLint("MissingPermission")
  private static synchronized boolean updateLocationListeners() {
    final boolean shouldListen = locationListeningRequested && !locationPaused;
    final LocationManager lm = getLocationManager(getApplicationContext());
    if (lm == null) {
      return false;
    }

    if (!shouldListen) {
      // Could not complete request, because paused
      if (locationListeningRequested) {
        return false;
      }
      lm.removeUpdates(sAndroidListeners);
      return true;
    }

    if (!lm.isProviderEnabled(LocationManager.GPS_PROVIDER)
        && !lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
      return false;
    }

    final Location lastKnownLocation = getLastKnownLocation(lm);
    if (lastKnownLocation != null) {
      sAndroidListeners.onLocationChanged(lastKnownLocation);
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
    lm.requestLocationUpdates(provider, 100, 0.5f, sAndroidListeners, l);
    return true;
  }

  public static void pauseLocation() {
    locationPaused = true;
    updateLocationListeners();
  }

  public static void resumeLocation() {
    locationPaused = false;
    updateLocationListeners();
  }

  private static LocationManager getLocationManager(final Context context) {
    try {
      return (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
    } catch (final NoSuchFieldError e) {
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
  /* package */ static native void onSensorChanged(
      int halType, float x, float y, float z, float w, long time);

  @WrapForJNI(calledFrom = "any", dispatchTo = "gecko")
  /* package */ static native void onLocationChanged(
      double latitude,
      double longitude,
      double altitude,
      float accuracy,
      float altitudeAccuracy,
      float heading,
      float speed);

  private static class AndroidListeners implements SensorEventListener, LocationListener {
    @Override
    public void onAccuracyChanged(final Sensor sensor, final int accuracy) {}

    @Override
    public void onSensorChanged(final SensorEvent s) {
      final int sensorType = s.sensor.getType();
      int halType = 0;
      float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
      // SensorEvent timestamp is in nanoseconds, Gecko expects microseconds.
      final long time = s.timestamp / 1000;

      switch (sensorType) {
        case Sensor.TYPE_ACCELEROMETER:
        case Sensor.TYPE_LINEAR_ACCELERATION:
        case Sensor.TYPE_ORIENTATION:
          if (sensorType == Sensor.TYPE_ACCELEROMETER) {
            halType = SENSOR_ACCELERATION;
          } else if (sensorType == Sensor.TYPE_LINEAR_ACCELERATION) {
            halType = SENSOR_LINEAR_ACCELERATION;
          } else {
            halType = SENSOR_ORIENTATION;
          }
          x = s.values[0];
          y = s.values[1];
          z = s.values[2];
          break;

        case Sensor.TYPE_GYROSCOPE:
          halType = SENSOR_GYROSCOPE;
          x = (float) Math.toDegrees(s.values[0]);
          y = (float) Math.toDegrees(s.values[1]);
          z = (float) Math.toDegrees(s.values[2]);
          break;

        case Sensor.TYPE_LIGHT:
          halType = SENSOR_LIGHT;
          x = s.values[0];
          break;

        case Sensor.TYPE_ROTATION_VECTOR:
        case Sensor.TYPE_GAME_ROTATION_VECTOR: // API >= 18
          halType =
              (sensorType == Sensor.TYPE_ROTATION_VECTOR
                  ? SENSOR_ROTATION_VECTOR
                  : SENSOR_GAME_ROTATION_VECTOR);
          x = s.values[0];
          y = s.values[1];
          z = s.values[2];
          if (s.values.length >= 4) {
            w = s.values[3];
          } else {
            // s.values[3] was optional in API <= 18, so we need to compute it
            // The values form a unit quaternion, so we can compute the angle of
            // rotation purely based on the given 3 values.
            w =
                1.0f
                    - s.values[0] * s.values[0]
                    - s.values[1] * s.values[1]
                    - s.values[2] * s.values[2];
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

      final double altitude = location.hasAltitude() ? location.getAltitude() : Double.NaN;

      final float accuracy = location.hasAccuracy() ? location.getAccuracy() : Float.NaN;

      final float altitudeAccuracy =
          Build.VERSION.SDK_INT >= 26 && location.hasVerticalAccuracy()
              ? location.getVerticalAccuracyMeters()
              : Float.NaN;

      final float speed = location.hasSpeed() ? location.getSpeed() : Float.NaN;

      final float heading = location.hasBearing() ? location.getBearing() : Float.NaN;

      // nsGeoPositionCoords will convert NaNs to null for optional
      // properties of the JavaScript Coordinates object.
      GeckoAppShell.onLocationChanged(
          location.getLatitude(),
          location.getLongitude(),
          altitude,
          accuracy,
          altitudeAccuracy,
          heading,
          speed);
    }

    @Override
    public void onProviderDisabled(final String provider) {}

    @Override
    public void onProviderEnabled(final String provider) {}

    @Override
    public void onStatusChanged(final String provider, final int status, final Bundle extras) {}
  }

  private static final AndroidListeners sAndroidListeners = new AndroidListeners();

  private static SimpleArrayMap<String, PowerManager.WakeLock> sWakeLocks;

  /** Wake-lock for the CPU. */
  static final String WAKE_LOCK_CPU = "cpu";
  /** Wake-lock for the screen. */
  static final String WAKE_LOCK_SCREEN = "screen";
  /** Wake-lock for the audio-playing, eqaul to LOCK_CPU. */
  static final String WAKE_LOCK_AUDIO_PLAYING = "audio-playing";
  /** Wake-lock for the video-playing, eqaul to LOCK_SCREEN.. */
  static final String WAKE_LOCK_VIDEO_PLAYING = "video-playing";

  static final int WAKE_LOCKS_COUNT = 2;

  /** No one holds the wake-lock. */
  static final int WAKE_LOCK_STATE_UNLOCKED = 0;
  /** The wake-lock is held by a foreground window. */
  static final int WAKE_LOCK_STATE_LOCKED_FOREGROUND = 1;
  /** The wake-lock is held by a background window. */
  static final int WAKE_LOCK_STATE_LOCKED_BACKGROUND = 2;

  @SuppressLint("Wakelock") // We keep the wake lock independent from the function
  // scope, so we need to suppress the linter warning.
  private static void setWakeLockState(final String lock, final int state) {
    if (sWakeLocks == null) {
      sWakeLocks = new SimpleArrayMap<>(WAKE_LOCKS_COUNT);
    }

    PowerManager.WakeLock wl = sWakeLocks.get(lock);

    // we should still hold the lock for background audio.
    if (WAKE_LOCK_AUDIO_PLAYING.equals(lock) && state == WAKE_LOCK_STATE_LOCKED_BACKGROUND) {
      return;
    }

    if (state == WAKE_LOCK_STATE_LOCKED_FOREGROUND && wl == null) {
      final PowerManager pm =
          (PowerManager) getApplicationContext().getSystemService(Context.POWER_SERVICE);

      if (WAKE_LOCK_CPU.equals(lock) || WAKE_LOCK_AUDIO_PLAYING.equals(lock)) {
        wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lock);
      } else if (WAKE_LOCK_SCREEN.equals(lock) || WAKE_LOCK_VIDEO_PLAYING.equals(lock)) {
        // ON_AFTER_RELEASE is set, the user activity timer will be reset when the
        // WakeLock is released, causing the illumination to remain on a bit longer.
        wl =
            pm.newWakeLock(
                PowerManager.SCREEN_BRIGHT_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE, lock);
      } else {
        Log.w(LOGTAG, "Unsupported wake-lock: " + lock);
        return;
      }

      wl.acquire();
      sWakeLocks.put(lock, wl);
    } else if (state != WAKE_LOCK_STATE_LOCKED_FOREGROUND && wl != null) {
      wl.release();
      sWakeLocks.remove(lock);
    }
  }

  @SuppressWarnings("fallthrough")
  @WrapForJNI(calledFrom = "gecko")
  private static void enableSensor(final int aSensortype) {
    final SensorManager sm =
        (SensorManager) getApplicationContext().getSystemService(Context.SENSOR_SERVICE);

    switch (aSensortype) {
      case SENSOR_GAME_ROTATION_VECTOR:
        if (gGameRotationVectorSensor == null) {
          gGameRotationVectorSensor = sm.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);
        }
        if (gGameRotationVectorSensor != null) {
          sm.registerListener(
              sAndroidListeners, gGameRotationVectorSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        if (gGameRotationVectorSensor != null) {
          break;
        }
        // Fallthrough

      case SENSOR_ROTATION_VECTOR:
        if (gRotationVectorSensor == null) {
          gRotationVectorSensor = sm.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);
        }
        if (gRotationVectorSensor != null) {
          sm.registerListener(
              sAndroidListeners, gRotationVectorSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        if (gRotationVectorSensor != null) {
          break;
        }
        // Fallthrough

      case SENSOR_ORIENTATION:
        if (gOrientationSensor == null) {
          gOrientationSensor = sm.getDefaultSensor(Sensor.TYPE_ORIENTATION);
        }
        if (gOrientationSensor != null) {
          sm.registerListener(
              sAndroidListeners, gOrientationSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        break;

      case SENSOR_ACCELERATION:
        if (gAccelerometerSensor == null) {
          gAccelerometerSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        }
        if (gAccelerometerSensor != null) {
          sm.registerListener(
              sAndroidListeners, gAccelerometerSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        break;

      case SENSOR_LIGHT:
        if (gLightSensor == null) {
          gLightSensor = sm.getDefaultSensor(Sensor.TYPE_LIGHT);
        }
        if (gLightSensor != null) {
          sm.registerListener(sAndroidListeners, gLightSensor, SensorManager.SENSOR_DELAY_NORMAL);
        }
        break;

      case SENSOR_LINEAR_ACCELERATION:
        if (gLinearAccelerometerSensor == null) {
          gLinearAccelerometerSensor = sm.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION);
        }
        if (gLinearAccelerometerSensor != null) {
          sm.registerListener(
              sAndroidListeners, gLinearAccelerometerSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        break;

      case SENSOR_GYROSCOPE:
        if (gGyroscopeSensor == null) {
          gGyroscopeSensor = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        }
        if (gGyroscopeSensor != null) {
          sm.registerListener(
              sAndroidListeners, gGyroscopeSensor, SensorManager.SENSOR_DELAY_FASTEST);
        }
        break;

      default:
        Log.w(LOGTAG, "Error! Can't enable unknown SENSOR type " + aSensortype);
    }
  }

  @SuppressWarnings("fallthrough")
  @WrapForJNI(calledFrom = "gecko")
  private static void disableSensor(final int aSensortype) {
    final SensorManager sm =
        (SensorManager) getApplicationContext().getSystemService(Context.SENSOR_SERVICE);

    switch (aSensortype) {
      case SENSOR_GAME_ROTATION_VECTOR:
        if (gGameRotationVectorSensor != null) {
          sm.unregisterListener(sAndroidListeners, gGameRotationVectorSensor);
          break;
        }
        // Fallthrough

      case SENSOR_ROTATION_VECTOR:
        if (gRotationVectorSensor != null) {
          sm.unregisterListener(sAndroidListeners, gRotationVectorSensor);
          break;
        }
        // Fallthrough

      case SENSOR_ORIENTATION:
        if (gOrientationSensor != null) {
          sm.unregisterListener(sAndroidListeners, gOrientationSensor);
        }
        break;

      case SENSOR_ACCELERATION:
        if (gAccelerometerSensor != null) {
          sm.unregisterListener(sAndroidListeners, gAccelerometerSensor);
        }
        break;

      case SENSOR_LIGHT:
        if (gLightSensor != null) {
          sm.unregisterListener(sAndroidListeners, gLightSensor);
        }
        break;

      case SENSOR_LINEAR_ACCELERATION:
        if (gLinearAccelerometerSensor != null) {
          sm.unregisterListener(sAndroidListeners, gLinearAccelerometerSensor);
        }
        break;

      case SENSOR_GYROSCOPE:
        if (gGyroscopeSensor != null) {
          sm.unregisterListener(sAndroidListeners, gGyroscopeSensor);
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

  @WrapForJNI(calledFrom = "gecko")
  private static boolean hasHWVP8Encoder() {
    return HardwareCodecCapabilityUtils.hasHWVP8(true /* aIsEncoder */);
  }

  @WrapForJNI(calledFrom = "gecko")
  private static boolean hasHWVP8Decoder() {
    return HardwareCodecCapabilityUtils.hasHWVP8(false /* aIsEncoder */);
  }

  @WrapForJNI(calledFrom = "gecko")
  public static String getExtensionFromMimeType(final String aMimeType) {
    return MimeTypeMap.getSingleton().getExtensionFromMimeType(aMimeType);
  }

  @WrapForJNI(calledFrom = "gecko")
  public static String getMimeTypeFromExtensions(final String aFileExt) {
    final StringTokenizer st = new StringTokenizer(aFileExt, ".,; ");
    String type = null;
    String subType = null;
    while (st.hasMoreElements()) {
      final String ext = st.nextToken();
      final String mt = getMimeTypeFromExtension(ext);
      if (mt == null) continue;
      final int slash = mt.indexOf('/');
      final String tmpType = mt.substring(0, slash);
      if (!tmpType.equalsIgnoreCase(type)) type = type == null ? tmpType : "*";
      final String tmpSubType = mt.substring(slash + 1);
      if (!tmpSubType.equalsIgnoreCase(subType)) subType = subType == null ? tmpSubType : "*";
    }
    if (type == null) type = "*";
    if (subType == null) subType = "*";
    return type + "/" + subType;
  }

  @WrapForJNI(dispatchTo = "gecko")
  private static native void notifyAlertListener(String name, String topic, String cookie);

  /**
   * Called by the NotificationListener to notify Gecko that a previously shown notification has
   * been closed.
   */
  public static void onNotificationClose(final String name, final String cookie) {
    if (GeckoThread.isRunning()) {
      notifyAlertListener(name, "alertfinished", cookie);
    }
  }

  /**
   * Called by the NotificationListener to notify Gecko that a previously shown notification has
   * been clicked on.
   */
  public static void onNotificationClick(final String name, final String cookie) {
    if (GeckoThread.isRunning()) {
      notifyAlertListener(name, "alertclickcallback", cookie);
    } else {
      GeckoThread.queueNativeCallUntil(
          GeckoThread.State.PROFILE_READY,
          GeckoAppShell.class,
          "notifyAlertListener",
          name,
          "alertclickcallback",
          cookie);
    }
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
      sDensity = Float.valueOf(getApplicationContext().getResources().getDisplayMetrics().density);
    }

    return sDensity;
  }

  private static int sTotalRam;

  private static int getTotalRam(final Context context) {
    if (sTotalRam == 0) {
      final ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
      final ActivityManager am =
          (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
      am.getMemoryInfo(memInfo); // `getMemoryInfo()` returns a value in B. Convert to MB.
      sTotalRam = (int) (memInfo.totalMem / (1024 * 1024));
      Log.d(LOGTAG, "System memory: " + sTotalRam + "MB.");
    }

    return sTotalRam;
  }

  private static boolean isHighMemoryDevice(final Context context) {
    return getTotalRam(context) > HIGH_MEMORY_DEVICE_THRESHOLD_MB;
  }

  public static synchronized void useMaxScreenDepth(final boolean enable) {
    sUseMaxScreenDepth = enable;
  }

  /** Returns the colour depth of the default screen. This will either be 32, 24 or 16. */
  @WrapForJNI(calledFrom = "gecko")
  public static synchronized int getScreenDepth() {
    if (sScreenDepth == 0) {
      sScreenDepth = 16;
      final Context applicationContext = getApplicationContext();
      final PixelFormat info = new PixelFormat();
      final WindowManager wm =
          (WindowManager) applicationContext.getSystemService(Context.WINDOW_SERVICE);
      PixelFormat.getPixelFormatInfo(wm.getDefaultDisplay().getPixelFormat(), info);
      if (info.bitsPerPixel >= 24 && isHighMemoryDevice(applicationContext)) {
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
      final int[] pattern;
      if (aIsLongPress) {
        pattern = new int[] {0, 1, 20, 21};
      } else {
        pattern = new int[] {0, 10, 20, 30};
      }
      vibrateOnHapticFeedbackEnabled(pattern);
      sVibrationMaybePlaying = false;
      sVibrationEndTime = 0;
    }
  }

  private static Vibrator vibrator() {
    return (Vibrator) getApplicationContext().getSystemService(Context.VIBRATOR_SERVICE);
  }

  // Helper method to convert integer array to long array.
  private static long[] convertIntToLongArray(final int[] input) {
    final long[] output = new long[input.length];
    for (int i = 0; i < input.length; i++) {
      output[i] = input[i];
    }
    return output;
  }

  // Vibrate only if haptic feedback is enabled.
  private static void vibrateOnHapticFeedbackEnabled(final int[] milliseconds) {
    if (Settings.System.getInt(
            getApplicationContext().getContentResolver(),
            Settings.System.HAPTIC_FEEDBACK_ENABLED,
            0)
        > 0) {
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
    final int iterLen = pattern.length & ~1;
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

  private static ConnectivityManager sConnectivityManager;

  private static void ensureConnectivityManager() {
    if (sConnectivityManager == null) {
      sConnectivityManager =
          (ConnectivityManager)
              getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  private static boolean isNetworkLinkUp() {
    ensureConnectivityManager();
    try {
      final NetworkInfo info = sConnectivityManager.getActiveNetworkInfo();
      if (info == null || !info.isConnected()) return false;
    } catch (final SecurityException se) {
      return false;
    }
    return true;
  }

  @WrapForJNI(calledFrom = "gecko")
  private static boolean isNetworkLinkKnown() {
    ensureConnectivityManager();
    try {
      if (sConnectivityManager.getActiveNetworkInfo() == null) return false;
    } catch (final SecurityException se) {
      return false;
    }
    return true;
  }

  @WrapForJNI(calledFrom = "gecko")
  private static int getNetworkLinkType() {
    ensureConnectivityManager();
    final NetworkInfo info = sConnectivityManager.getActiveNetworkInfo();
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
        return LINK_TYPE_MOBILE;
      default:
        Log.w(LOGTAG, "Ignoring the current network type.");
        return LINK_TYPE_UNKNOWN;
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  private static String getDNSDomains() {
    if (Build.VERSION.SDK_INT < 23) {
      return "";
    }

    ensureConnectivityManager();
    final Network net = sConnectivityManager.getActiveNetwork();
    if (net == null) {
      return "";
    }

    final LinkProperties lp = sConnectivityManager.getLinkProperties(net);
    if (lp == null) {
      return "";
    }

    return lp.getDomains();
  }

  @WrapForJNI(calledFrom = "gecko")
  private static int[] getSystemColors() {
    // attrsAppearance[] must correspond to AndroidSystemColors structure in android/AndroidBridge.h
    final int[] attrsAppearance = {
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
      android.R.attr.panelColorBackground,
      Build.VERSION.SDK_INT >= 21 ? android.R.attr.colorAccent : 0,
    };

    final int[] result = new int[attrsAppearance.length];

    final ContextThemeWrapper contextThemeWrapper =
        new ContextThemeWrapper(getApplicationContext(), android.R.style.TextAppearance);

    final TypedArray appearance =
        contextThemeWrapper.getTheme().obtainStyledAttributes(attrsAppearance);

    if (appearance != null) {
      for (int i = 0; i < appearance.getIndexCount(); i++) {
        final int idx = appearance.getIndex(i);
        final int color = appearance.getColor(idx, 0);
        result[idx] = color;
      }
      appearance.recycle();
    }

    return result;
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

      final PackageManager pm = getApplicationContext().getPackageManager();
      Drawable icon = getDrawableForExtension(pm, resolvedExt);
      if (icon == null) {
        // Use a generic icon.
        icon =
            ResourcesCompat.getDrawable(
                getApplicationContext().getResources(),
                R.drawable.ic_generic_file,
                getApplicationContext().getTheme());
      }

      Bitmap bitmap = getBitmapFromDrawable(icon);
      if (bitmap.getWidth() != resolvedIconSize || bitmap.getHeight() != resolvedIconSize) {
        bitmap = Bitmap.createScaledBitmap(bitmap, resolvedIconSize, resolvedIconSize, true);
      }

      final ByteBuffer buf = ByteBuffer.allocate(resolvedIconSize * resolvedIconSize * 4);
      bitmap.copyPixelsToBuffer(buf);

      return buf.array();
    } catch (final Exception e) {
      Log.w(LOGTAG, "getIconForExtension failed.", e);
      return null;
    }
  }

  private static Bitmap getBitmapFromDrawable(final Drawable drawable) {
    if (drawable instanceof BitmapDrawable) {
      return ((BitmapDrawable) drawable).getBitmap();
    }

    int width = drawable.getIntrinsicWidth();
    width = width > 0 ? width : 1;
    int height = drawable.getIntrinsicHeight();
    height = height > 0 ? height : 1;

    final Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    final Canvas canvas = new Canvas(bitmap);
    drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
    drawable.draw(canvas);

    return bitmap;
  }

  public static String getMimeTypeFromExtension(final String ext) {
    final MimeTypeMap mtm = MimeTypeMap.getSingleton();
    return mtm.getMimeTypeFromExtension(ext);
  }

  private static Drawable getDrawableForExtension(final PackageManager pm, final String aExt) {
    final Intent intent = new Intent(Intent.ACTION_VIEW);
    final String mimeType = getMimeTypeFromExtension(aExt);
    if (mimeType != null && mimeType.length() > 0) intent.setType(mimeType);
    else return null;

    final List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
    if (list.size() == 0) return null;

    final ResolveInfo resolveInfo = list.get(0);

    if (resolveInfo == null) return null;

    final ActivityInfo activityInfo = resolveInfo.activityInfo;

    return activityInfo.loadIcon(pm);
  }

  @WrapForJNI(calledFrom = "gecko")
  private static boolean getShowPasswordSetting() {
    try {
      final int showPassword =
          Settings.System.getInt(
              getApplicationContext().getContentResolver(), Settings.System.TEXT_SHOW_PASSWORD, 1);
      return (showPassword > 0);
    } catch (final Exception e) {
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
    ThreadUtils.runOnUiThread(() -> GeckoNetworkManager.getInstance().enableNotifications());
  }

  @WrapForJNI(calledFrom = "gecko")
  private static void disableNetworkNotifications() {
    ThreadUtils.runOnUiThread(
        new Runnable() {
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
  private static void notifyWakeLockChanged(final String topic, final String state) {
    final int intState;
    if ("unlocked".equals(state)) {
      intState = WAKE_LOCK_STATE_UNLOCKED;
    } else if ("locked-foreground".equals(state)) {
      intState = WAKE_LOCK_STATE_LOCKED_FOREGROUND;
    } else if ("locked-background".equals(state)) {
      intState = WAKE_LOCK_STATE_LOCKED_BACKGROUND;
    } else {
      throw new IllegalArgumentException();
    }
    setWakeLockState(topic, intState);
  }

  @WrapForJNI(calledFrom = "gecko")
  private static String getProxyForURI(
      final String spec, final String scheme, final String host, final int port) {
    final ProxySelector ps = new ProxySelector();

    final Proxy proxy = ps.select(scheme, host);
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

  @WrapForJNI(calledFrom = "gecko")
  private static int getMaxTouchPoints() {
    final PackageManager pm = getApplicationContext().getPackageManager();
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
  private static final int NO_POINTER = 0x00000000;
  private static final int COARSE_POINTER = 0x00000001;
  private static final int FINE_POINTER = 0x00000002;
  private static final int HOVER_CAPABLE_POINTER = 0x00000004;

  private static int getPointerCapabilities(final InputDevice inputDevice) {
    int result = NO_POINTER;
    final int sources = inputDevice.getSources();

    // Blink checks fine pointer at first, then it check coarse pointer.
    // So, we should use same order for compatibility.
    // Also, if using Chrome OS, source may be SOURCE_MOUSE | SOURCE_TOUCHSCREEN | SOURCE_STYLUS
    // even if no touch screen. So we shouldn't check TOUCHSCREEN at first.

    if (hasInputDeviceSource(sources, InputDevice.SOURCE_MOUSE)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_STYLUS)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHPAD)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_TRACKBALL)) {
      result |= FINE_POINTER;
    } else if (hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHSCREEN)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_JOYSTICK)) {
      result |= COARSE_POINTER;
    }

    if (hasInputDeviceSource(sources, InputDevice.SOURCE_MOUSE)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_TOUCHPAD)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_TRACKBALL)
        || hasInputDeviceSource(sources, InputDevice.SOURCE_JOYSTICK)) {
      result |= HOVER_CAPABLE_POINTER;
    }

    return result;
  }

  @WrapForJNI(calledFrom = "gecko")
  // For any-pointer and any-hover media queries features.
  private static int getAllPointerCapabilities() {
    int result = NO_POINTER;

    for (final int deviceId : InputDevice.getDeviceIds()) {
      final InputDevice inputDevice = InputDevice.getDevice(deviceId);
      if (inputDevice == null || !InputDeviceUtils.isPointerTypeDevice(inputDevice)) {
        continue;
      }

      result |= getPointerCapabilities(inputDevice);
    }

    return result;
  }

  private static boolean hasInputDeviceSource(final int sources, final int inputDeviceSource) {
    return (sources & inputDeviceSource) == inputDeviceSource;
  }

  public static synchronized void setScreenSizeOverride(final Rect size) {
    sScreenSizeOverride = size;
  }

  static final ScreenCompat sScreenCompat;

  private interface ScreenCompat {
    Rect getScreenSize();
  }

  @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
  private static class JellyBeanScreenCompat implements ScreenCompat {
    public Rect getScreenSize() {
      final WindowManager wm =
          (WindowManager) getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
      final Display disp = wm.getDefaultDisplay();
      return new Rect(0, 0, disp.getWidth(), disp.getHeight());
    }
  }

  @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
  private static class JellyBeanMR1ScreenCompat implements ScreenCompat {
    public Rect getScreenSize() {
      final WindowManager wm =
          (WindowManager) getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
      final Display disp = wm.getDefaultDisplay();
      final Point size = new Point();
      disp.getRealSize(size);
      return new Rect(0, 0, size.x, size.y);
    }
  }

  @TargetApi(Build.VERSION_CODES.S)
  private static class AndroidSScreenCompat implements ScreenCompat {
    @SuppressLint("StaticFieldLeak")
    private static Context sWindowContext;

    private static Context getWindowContext() {
      if (sWindowContext == null) {
        final DisplayManager displayManager =
            (DisplayManager) getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
        final Display display = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
        sWindowContext =
            getApplicationContext()
                .createWindowContext(display, WindowManager.LayoutParams.TYPE_APPLICATION, null);
      }
      return sWindowContext;
    }

    public Rect getScreenSize() {
      final WindowManager windowManager = getWindowContext().getSystemService(WindowManager.class);
      return windowManager.getCurrentWindowMetrics().getBounds();
    }
  }

  static {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      sScreenCompat = new AndroidSScreenCompat();
    } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
      sScreenCompat = new JellyBeanMR1ScreenCompat();
    } else {
      sScreenCompat = new JellyBeanScreenCompat();
    }
  }

  /* package */ static Rect getScreenSizeIgnoreOverride() {
    return sScreenCompat.getScreenSize();
  }

  @WrapForJNI(calledFrom = "gecko")
  private static synchronized Rect getScreenSize() {
    if (sScreenSizeOverride != null) {
      return sScreenSizeOverride;
    }

    return getScreenSizeIgnoreOverride();
  }

  @WrapForJNI(calledFrom = "any")
  public static int getAudioOutputFramesPerBuffer() {
    final int DEFAULT = 512;

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
      return DEFAULT;
    }
    final AudioManager am =
        (AudioManager) getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
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
    final AudioManager am =
        (AudioManager) getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    if (am == null) {
      return DEFAULT;
    }
    final String prop = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    if (prop == null) {
      return DEFAULT;
    }
    return Integer.parseInt(prop);
  }

  @WrapForJNI(calledFrom = "any")
  public static void setCommunicationAudioModeOn(final boolean on) {
    final AudioManager am =
        (AudioManager) getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    if (am == null) {
      return;
    }

    try {
      if (on) {
        Log.e(LOGTAG, "Setting communication mode ON");
        am.startBluetoothSco();
        am.setBluetoothScoOn(true);
      } else {
        Log.e(LOGTAG, "Setting communication mode OFF");
        am.stopBluetoothSco();
        am.setBluetoothScoOn(false);
      }
    } catch (final SecurityException e) {
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
      final String[] locales = new String[localeList.size()];
      for (int i = 0; i < localeList.size(); i++) {
        locales[i] = localeList.get(i).toLanguageTag();
      }
      return locales;
    }
    final String[] locales = new String[1];
    final Locale locale = Locale.getDefault();
    if (Build.VERSION.SDK_INT >= 21) {
      locales[0] = locale.toLanguageTag();
      return locales;
    }

    locales[0] = getLanguageTag(locale);
    return locales;
  }

  @WrapForJNI
  public static boolean getIs24HourFormat() {
    final Context context = getApplicationContext();
    return DateFormat.is24HourFormat(context);
  }

  @WrapForJNI
  public static String getAppName() {
    final Context context = getApplicationContext();
    final ApplicationInfo info = context.getApplicationInfo();
    final int id = info.labelRes;
    return id == 0 ? info.nonLocalizedLabel.toString() : context.getString(id);
  }

  @WrapForJNI
  public static native boolean isParentProcess();

  /**
   * Returns a GeckoResult that will be completed to true if the GPU process is enabled and false if
   * it is disabled.
   */
  @WrapForJNI
  public static native GeckoResult<Boolean> isGpuProcessEnabled();
}
