/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.util.Log;
import android.view.OrientationEventListener;
import android.view.Surface;

import java.util.Arrays;
import java.util.List;

import android.app.Activity;

public class GeckoScreenOrientationListener {
    private static final String LOGTAG = "GeckoScreenOrientationListener";

    static class OrientationEventListenerImpl extends OrientationEventListener {
        public OrientationEventListenerImpl(Context c) {
            super(c);
        }

        @Override
        public void onOrientationChanged(int aOrientation) {
            GeckoScreenOrientationListener.getInstance().updateScreenOrientation();
        }
    }

    static private GeckoScreenOrientationListener sInstance = null;

    // Make sure that any change in dom/base/ScreenOrientation.h happens here too.
    static public final short eScreenOrientation_None               = 0;
    static public final short eScreenOrientation_PortraitPrimary    = 1; // PR_BIT(0)
    static public final short eScreenOrientation_PortraitSecondary  = 2; // PR_BIT(1)
    static public final short eScreenOrientation_LandscapePrimary   = 4; // PR_BIT(2)
    static public final short eScreenOrientation_LandscapeSecondary = 8; // PR_BIT(3)
    static public final short eScreenOrientation_Default            = 16;// PR_BIT(4)

    static private final short DEFAULT_ORIENTATION = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    private short mOrientation;
    private OrientationEventListenerImpl mListener = null;

    // Whether the listener should be listening to changes.
    private boolean mShouldBeListening = false;
    // Whether the listener should notify Gecko that a change happened.
    private boolean mShouldNotify      = false;
    // The default orientation to use if nothing is specified
    private short mDefaultOrientation;

    private static final String DEFAULT_ORIENTATION_PREF = "app.orientation.default";

    private GeckoScreenOrientationListener() {
        mListener = new OrientationEventListenerImpl(GeckoAppShell.getContext());

        PrefsHelper.getPref(DEFAULT_ORIENTATION_PREF, new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                mDefaultOrientation = orientationFromStringArray(value);
                unlockScreenOrientation();
            }

            @Override
            public boolean isObserver() {
                return true;
            }
        });

        mDefaultOrientation = DEFAULT_ORIENTATION;
    }

    public static GeckoScreenOrientationListener getInstance() {
        if (sInstance == null) {
            sInstance = new GeckoScreenOrientationListener();
        }

        return sInstance;
    }

    public void start() {
        mShouldBeListening = true;
        updateScreenOrientation();

        if (mShouldNotify) {
            startListening();
        }
    }

    public void stop() {
        mShouldBeListening = false;

        if (mShouldNotify) {
            stopListening();
        }
    }

    public void enableNotifications() {
        updateScreenOrientation();
        mShouldNotify = true;

        if (mShouldBeListening) {
            startListening();
        }
    }

    public void disableNotifications() {
        mShouldNotify = false;

        if (mShouldBeListening) {
            stopListening();
        }
    }

    private void startListening() {
        mListener.enable();
    }

    private void stopListening() {
        mListener.disable();
    }

    private short orientationFromStringArray(String val) {
        List<String> orientations = Arrays.asList(val.split(","));
        // if nothing is listed, return unspecified
        if (orientations.size() == 0)
            return DEFAULT_ORIENTATION;

        // we dont' support multiple orientations yet. To avoid developer confusion,
        // just take the first one listed
        return orientationFromString(orientations.get(0));
    }

    private short orientationFromString(String val) {
        if ("portrait".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
        else if ("landscape".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
        else if ("portrait-primary".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        else if ("portrait-secondary".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        else if ("landscape-primary".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        else if ("landscape-secondary".equals(val))
            return (short)ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        return DEFAULT_ORIENTATION;
    }

    private void updateScreenOrientation() {
      Context context = GeckoAppShell.getContext();
      int rotation = mDefaultOrientation;
      if (context instanceof Activity) {
        rotation = ((Activity)context).getWindowManager().getDefaultDisplay().getRotation();
      }
        short previousOrientation = mOrientation;

        if (rotation == Surface.ROTATION_0) {
            mOrientation = eScreenOrientation_PortraitPrimary;
        } else if (rotation == Surface.ROTATION_180) {
            mOrientation = eScreenOrientation_PortraitSecondary;
        } else if (rotation == Surface.ROTATION_270) {
            mOrientation = eScreenOrientation_LandscapeSecondary;
        } else if (rotation == Surface.ROTATION_90) {
            mOrientation = eScreenOrientation_LandscapePrimary;
        } else {
            Log.e(LOGTAG, "Unexpected value received! (" + rotation + ")");
            return;
        }

        if (mShouldNotify && mOrientation != previousOrientation) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createScreenOrientationEvent(mOrientation));
        }
    }

    public short getScreenOrientation() {
        return mOrientation;
    }

    public void lockScreenOrientation(int aOrientation) {
        int orientation = 0;

        switch (aOrientation) {
        case eScreenOrientation_PortraitPrimary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            break;
        case eScreenOrientation_PortraitSecondary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
            break;
        case eScreenOrientation_PortraitPrimary | eScreenOrientation_PortraitSecondary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
            break;
        case eScreenOrientation_LandscapePrimary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            break;
        case eScreenOrientation_LandscapeSecondary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            break;
        case eScreenOrientation_LandscapePrimary | eScreenOrientation_LandscapeSecondary:
            orientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
            break;
        case eScreenOrientation_Default:
            orientation = ActivityInfo.SCREEN_ORIENTATION_NOSENSOR;
            break;
        default:
            Log.e(LOGTAG, "Unexpected value received! (" + aOrientation + ")");
            return;
        }
	if (GeckoAppShell.getContext() instanceof Activity)
	  ((Activity)GeckoAppShell.getContext()).setRequestedOrientation(orientation);
        updateScreenOrientation();
    }

    public void unlockScreenOrientation() {
      if (!(GeckoAppShell.getContext() instanceof Activity))
	return;
      if (((Activity)GeckoAppShell.getContext()).getRequestedOrientation() == mDefaultOrientation)
	return;

      ((Activity)GeckoAppShell.getContext()).setRequestedOrientation(mDefaultOrientation);
        updateScreenOrientation();
    }
}
