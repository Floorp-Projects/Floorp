/* -*- Mode: Java; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.util.Log;
import android.view.Display;

public class GeckoScreenChangeListener implements DisplayManager.DisplayListener {
  private static final String LOGTAG = "ScreenChangeListener";
  private static final boolean DEBUG = false;

  public GeckoScreenChangeListener() {}

  @Override
  public void onDisplayAdded(final int displayId) {}

  @Override
  public void onDisplayRemoved(final int displayId) {}

  @Override
  public void onDisplayChanged(final int displayId) {
    if (DEBUG) {
      Log.d(LOGTAG, "onDisplayChanged");
    }

    // Even if onDisplayChanged is called, Configuration may not updated yet.
    // So we use Display's data instead.
    if (displayId != Display.DEFAULT_DISPLAY) {
      if (DEBUG) {
        Log.d(LOGTAG, "Primary display is only supported");
      }
      return;
    }

    final DisplayManager displayManager = getDisplayManager();
    if (displayManager == null) {
      return;
    }

    if (GeckoScreenOrientation.getInstance().update(displayManager.getDisplay(displayId))) {
      // refreshScreenInfo is already called.
      return;
    }

    ScreenManagerHelper.refreshScreenInfo();
  }

  private static DisplayManager getDisplayManager() {
    return (DisplayManager)
        GeckoAppShell.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
  }

  public void enable() {
    final DisplayManager displayManager = getDisplayManager();
    if (displayManager == null) {
      return;
    }
    displayManager.registerDisplayListener(this, null);
  }

  public void disable() {
    final DisplayManager displayManager = getDisplayManager();
    if (displayManager == null) {
      return;
    }
    displayManager.unregisterDisplayListener(this);
  }
}
