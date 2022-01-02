/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;

class ScreenManagerHelper {

  /** Trigger a refresh of the cached screen information held by Gecko. */
  public static void refreshScreenInfo() {
    // Screen data is initialised automatically on startup, so no need to queue the call if
    // Gecko isn't running yet.
    if (GeckoThread.isRunning()) {
      nativeRefreshScreenInfo();
    }
  }

  @WrapForJNI(stubName = "RefreshScreenInfo", dispatchTo = "gecko")
  private static native void nativeRefreshScreenInfo();
}
