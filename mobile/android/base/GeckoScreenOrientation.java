/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.util.Log;
import android.view.Surface;
import android.app.Activity;

import java.util.Arrays;
import java.util.List;

/*
 * Updates, locks and unlocks the screen orientation.
 *
 * Note: Replaces the OnOrientationChangeListener to avoid redundant rotation
 * event handling.
 */
public class GeckoScreenOrientation {
    private static final String LOGTAG = "GeckoScreenOrientation";

    // Make sure that any change in dom/base/ScreenOrientation.h happens here too.
    public enum ScreenOrientation {
        NONE(0),
        PORTRAIT_PRIMARY(1 << 0),
        PORTRAIT_SECONDARY(1 << 1),
        LANDSCAPE_PRIMARY(1 << 2),
        LANDSCAPE_SECONDARY(1 << 3),
        DEFAULT(1 << 4);

        public final short value;

        private ScreenOrientation(int value) {
            this.value = (short)value;
        }

        public static ScreenOrientation get(short value) {
            switch (value) {
                case (1 << 0): return PORTRAIT_PRIMARY;
                case (1 << 1): return PORTRAIT_SECONDARY;
                case (1 << 2): return LANDSCAPE_PRIMARY;
                case (1 << 3): return LANDSCAPE_SECONDARY;
                case (1 << 4): return DEFAULT;
                default: return NONE;
            }
        }
    }

    // Singleton instance.
    private static GeckoScreenOrientation sInstance;
    // Default screen orientation, used for initialization and unlocking.
    private static final ScreenOrientation DEFAULT_SCREEN_ORIENTATION = ScreenOrientation.DEFAULT;
    // Default rotation, used when device rotation is unknown.
    private static final int DEFAULT_ROTATION = Surface.ROTATION_0;
    // Default orientation, used if screen orientation is unspecified.
    private ScreenOrientation mDefaultScreenOrientation;
    // Last updated screen orientation.
    private ScreenOrientation mScreenOrientation;
    // Whether the update should notify Gecko about screen orientation changes.
    private boolean mShouldNotify = true;
    // Configuration screen orientation preference path.
    private static final String DEFAULT_SCREEN_ORIENTATION_PREF = "app.orientation.default";

    public GeckoScreenOrientation() {
        PrefsHelper.getPref(DEFAULT_SCREEN_ORIENTATION_PREF, new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                // Read and update the configuration default preference.
                mDefaultScreenOrientation = screenOrientationFromArrayString(value);
                setRequestedOrientation(mDefaultScreenOrientation);
            }
        });

        mDefaultScreenOrientation = DEFAULT_SCREEN_ORIENTATION;
        update();
    }

    public static GeckoScreenOrientation getInstance() {
        if (sInstance == null) {
            sInstance = new GeckoScreenOrientation();
        }
        return sInstance;
    }

    /*
     * Enable Gecko screen orientation events on update.
     */
    public void enableNotifications() {
        update();
        mShouldNotify = true;
    }

    /*
     * Disable Gecko screen orientation events on update.
     */
    public void disableNotifications() {
        mShouldNotify = false;
    }

    /*
     * Update screen orientation.
     * Retrieve orientation and rotation via GeckoAppShell.
     *
     * @return Whether the screen orientation has changed.
     */
    public boolean update() {
        Activity activity = GeckoAppShell.getGeckoInterface().getActivity();
        if (activity == null) {
            return false;
        }
        Configuration config = activity.getResources().getConfiguration();
        return update(config.orientation);
    }

    /*
     * Update screen orientation given the android orientation.
     * Retrieve rotation via GeckoAppShell.
     *
     * @param aAndroidOrientation
     *        Android screen orientation from Configuration.orientation.
     *
     * @return Whether the screen orientation has changed.
     */
    public boolean update(int aAndroidOrientation) {
        return update(getScreenOrientation(aAndroidOrientation, getRotation()));
    }

    /*
     * Update screen orientation given the screen orientation.
     *
     * @param aScreenOrientation
     *        Gecko screen orientation based on android orientation and rotation.
     *
     * @return Whether the screen orientation has changed.
     */
    public boolean update(ScreenOrientation aScreenOrientation) {
        if (mScreenOrientation == aScreenOrientation) {
            return false;
        }
        mScreenOrientation = aScreenOrientation;
        Log.d(LOGTAG, "updating to new orientation " + mScreenOrientation);
        if (mShouldNotify) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createScreenOrientationEvent(mScreenOrientation.value));
        }
        return true;
    }

    /*
     * @return The Android orientation (Configuration.orientation).
     */
    public int getAndroidOrientation() {
        return screenOrientationToAndroidOrientation(getScreenOrientation());
    }

    /*
     * @return The Gecko screen orientation derived from Android orientation and
     *         rotation.
     */
    public ScreenOrientation getScreenOrientation() {
        return mScreenOrientation;
    }

    /*
     * Lock screen orientation given the Android orientation.
     * Retrieve rotation via GeckoAppShell.
     *
     * @param aAndroidOrientation
     *        The Android orientation provided by Configuration.orientation.
     */
    public void lock(int aAndroidOrientation) {
        lock(getScreenOrientation(aAndroidOrientation, getRotation()));
    }

    /*
     * Lock screen orientation given the Gecko screen orientation.
     * Retrieve rotation via GeckoAppShell.
     *
     * @param aScreenOrientation
     *        Gecko screen orientation derived from Android orientation and
     *        rotation.
     *
     * @return Whether the locking was successful.
     */
    public boolean lock(ScreenOrientation aScreenOrientation) {
        Log.d(LOGTAG, "locking to " + aScreenOrientation);
        update(aScreenOrientation);
        return setRequestedOrientation(aScreenOrientation);
    }

    /*
     * Unlock and update screen orientation.
     *
     * @return Whether the unlocking was successful.
     */
    public boolean unlock() {
        Log.d(LOGTAG, "unlocking");
        setRequestedOrientation(mDefaultScreenOrientation);
        return update();
    }

    /*
     * Set the given requested orientation for the current activity.
     * This is essentially an unlock without an update.
     *
     * @param aScreenOrientation
     *        Gecko screen orientation.
     *
     * @return Whether the requested orientation was set. This can only fail if
     *         the current activity cannot be retrieved vie GeckoAppShell.
     *
     */
    private boolean setRequestedOrientation(ScreenOrientation aScreenOrientation) {
        int activityOrientation = screenOrientationToActivityInfoOrientation(aScreenOrientation);
        Activity activity = GeckoAppShell.getGeckoInterface().getActivity();
        if (activity == null) {
            Log.w(LOGTAG, "setRequestOrientation: failed to get activity");
        }
        if (activity.getRequestedOrientation() == activityOrientation) {
            return false;
        }
        activity.setRequestedOrientation(activityOrientation);
        return true;
    }

    /*
     * Combine the Android orientation and rotation to the Gecko orientation.
     *
     * @param aAndroidOrientation
     *        Android orientation from Configuration.orientation.
     * @param aRotation
     *        Device rotation from Display.getRotation().
     *
     * @return Gecko screen orientation.
     */
    private ScreenOrientation getScreenOrientation(int aAndroidOrientation, int aRotation) {
        boolean isPrimary = aRotation == Surface.ROTATION_0 || aRotation == Surface.ROTATION_90;
        if (aAndroidOrientation == Configuration.ORIENTATION_PORTRAIT) {
            if (isPrimary) {
                // Non-rotated portrait device or landscape device rotated
                // to primary portrait mode counter-clockwise.
                return ScreenOrientation.PORTRAIT_PRIMARY;
            }
            return ScreenOrientation.PORTRAIT_SECONDARY;
        }
        if (aAndroidOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            if (isPrimary) {
                // Non-rotated landscape device or portrait device rotated
                // to primary landscape mode counter-clockwise.
                return ScreenOrientation.LANDSCAPE_PRIMARY;
            }
            return ScreenOrientation.LANDSCAPE_SECONDARY;
        }
        return ScreenOrientation.NONE;
    }

    /*
     * @return Device rotation from Display.getRotation().
     */
    private int getRotation() {
        Activity activity = GeckoAppShell.getGeckoInterface().getActivity();
        if (activity == null) {
            Log.w(LOGTAG, "getRotation: failed to get activity");
            return DEFAULT_ROTATION;
        }
        return activity.getWindowManager().getDefaultDisplay().getRotation();
    }

    /*
     * Retrieve the screen orientation from an array string.
     *
     * @param aArray
     *        String containing comma-delimited strings.
     *
     * @return First parsed Gecko screen orientation.
     */
    public static ScreenOrientation screenOrientationFromArrayString(String aArray) {
        List<String> orientations = Arrays.asList(aArray.split(","));
        if (orientations.size() == 0) {
            // If nothing is listed, return default.
            Log.w(LOGTAG, "screenOrientationFromArrayString: no orientation in string");
            return DEFAULT_SCREEN_ORIENTATION;
        }

        // We don't support multiple orientations yet. To avoid developer
        // confusion, just take the first one listed.
        return screenOrientationFromString(orientations.get(0));
    }

    /*
     * Retrieve the scren orientation from a string.
     *
     * @param aStr
     *        String hopefully containing a screen orientation name.
     * @return Gecko screen orientation if matched, DEFAULT_SCREEN_ORIENTATION
     *         otherwise.
     */
    public static ScreenOrientation screenOrientationFromString(String aStr) {
        switch (aStr) {
            case "portrait":
                return ScreenOrientation.PORTRAIT_PRIMARY;
            case "landscape":
                return ScreenOrientation.LANDSCAPE_PRIMARY;
            case "portrait-primary":
                return ScreenOrientation.PORTRAIT_PRIMARY;
            case "portrait-secondary":
                return ScreenOrientation.PORTRAIT_SECONDARY;
            case "landscape-primary":
                return ScreenOrientation.LANDSCAPE_PRIMARY;
            case "landscape-secondary":
                return ScreenOrientation.LANDSCAPE_SECONDARY;
        }

        Log.w(LOGTAG, "screenOrientationFromString: unknown orientation string");
        return DEFAULT_SCREEN_ORIENTATION;
    }

    /*
     * Convert Gecko screen orientation to Android orientation.
     *
     * @param aScreenOrientation
     *        Gecko screen orientation.
     * @return Android orientation. This conversion is lossy, the Android
     *         orientation does not differentiate between primary and secondary
     *         orientations.
     */
    public static int screenOrientationToAndroidOrientation(ScreenOrientation aScreenOrientation) {
        switch (aScreenOrientation) {
            case PORTRAIT_PRIMARY:
            case PORTRAIT_SECONDARY:
                return Configuration.ORIENTATION_PORTRAIT;
            case LANDSCAPE_PRIMARY:
            case LANDSCAPE_SECONDARY:
                return Configuration.ORIENTATION_LANDSCAPE;
            case NONE:
            case DEFAULT:
            default:
                return Configuration.ORIENTATION_UNDEFINED;
        }
    }


    /*
     * Convert Gecko screen orientation to Android ActivityInfo orientation.
     * This is yet another orientation used by Android, but it's more detailed
     * than the Android orientation.
     * It is required for screen orientation locking and unlocking.
     *
     * @param aScreenOrientation
     *        Gecko screen orientation.
     * @return Android ActivityInfo orientation.
     */
    public static int screenOrientationToActivityInfoOrientation(ScreenOrientation aScreenOrientation) {
        switch (aScreenOrientation) {
            case PORTRAIT_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            case PORTRAIT_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
            case LANDSCAPE_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case LANDSCAPE_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            case DEFAULT:
            case NONE:
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            default:
                return ActivityInfo.SCREEN_ORIENTATION_NOSENSOR;
        }
    }
}
