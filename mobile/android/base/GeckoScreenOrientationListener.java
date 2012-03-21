/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.Log;
import android.view.OrientationEventListener;
import android.view.Surface;

public class GeckoScreenOrientationListener
{
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
  static public final short eScreenOrientation_PortraitPrimary    = 1;
  static public final short eScreenOrientation_PortraitSecondary  = 2;
  static public final short eScreenOrientation_LandscapePrimary   = 4;
  static public final short eScreenOrientation_LandscapeSecondary = 8;

  private short mOrientation;
  private OrientationEventListenerImpl mListener = null;

  // Whether the listener should be listening to changes.
  private boolean mShouldBeListening = false;
  // Whether the listener should notify Gecko that a change happened.
  private boolean mShouldNotify      = false;

  private GeckoScreenOrientationListener() {
    mListener = new OrientationEventListenerImpl(GeckoApp.mAppContext);
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

  // NOTE: this is public so OrientationEventListenerImpl can access it.
  // Unfortunately, Java doesn't know about friendship.
  public void updateScreenOrientation() {
    int rotation = GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getRotation();
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
      Log.e("GeckoScreenOrientationListener", "Unexpected value received! (" + rotation + ")");
      return;
    }

    if (mShouldNotify && mOrientation != previousOrientation) {
      GeckoAppShell.sendEventToGecko(GeckoEvent.createScreenOrientationEvent(mOrientation));
    }
  }

  public short getScreenOrientation() {
    return mOrientation;
  }
}
