/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.os.RemoteException;
import android.util.Log;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.process.GeckoProcessManager;
import org.mozilla.gecko.process.GeckoServiceChildProcess;

/* package */ final class SurfaceAllocator {
  private static final String LOGTAG = "SurfaceAllocator";

  private static ISurfaceAllocator sAllocator;

  private static synchronized void ensureConnection() throws Exception {
    if (sAllocator != null) {
      return;
    }

    if (GeckoAppShell.isParentProcess()) {
      sAllocator = GeckoProcessManager.getInstance().getSurfaceAllocator();
    } else {
      sAllocator = GeckoServiceChildProcess.getSurfaceAllocator();
    }

    if (sAllocator == null) {
      throw new Exception("Failed to connect to surface allocator service!");
    }
  }

  @WrapForJNI
  public static GeckoSurface acquireSurface(
      final int width, final int height, final boolean singleBufferMode) {
    try {
      ensureConnection();

      if (singleBufferMode && !GeckoSurfaceTexture.isSingleBufferSupported()) {
        return null;
      }
      final GeckoSurface surface = sAllocator.acquireSurface(width, height, singleBufferMode);
      if (surface != null && !surface.inProcess()) {
        sAllocator.configureSync(surface.initSyncSurface(width, height));
      }
      return surface;
    } catch (final Exception e) {
      Log.w(LOGTAG, "Failed to acquire GeckoSurface", e);
      return null;
    }
  }

  @WrapForJNI
  public static void disposeSurface(final GeckoSurface surface) {
    try {
      ensureConnection();
    } catch (final Exception e) {
      Log.w(LOGTAG, "Failed to dispose surface, no connection");
      return;
    }

    // Release the SurfaceTexture on the other side
    try {
      sAllocator.releaseSurface(surface.getHandle());
    } catch (final RemoteException e) {
      Log.w(LOGTAG, "Failed to release surface texture", e);
    }

    // And now our Surface
    try {
      surface.release();
    } catch (final Exception e) {
      Log.w(LOGTAG, "Failed to release surface", e);
    }
  }

  public static void sync(final int upstream) {
    try {
      ensureConnection();
    } catch (final Exception e) {
      Log.w(LOGTAG, "Failed to sync texture, no connection");
      return;
    }

    // Release the SurfaceTexture on the other side
    try {
      sAllocator.sync(upstream);
    } catch (final RemoteException e) {
      Log.w(LOGTAG, "Failed to sync texture", e);
    }
  }
}
