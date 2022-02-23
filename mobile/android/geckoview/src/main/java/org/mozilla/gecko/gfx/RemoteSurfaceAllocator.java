/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

public final class RemoteSurfaceAllocator extends ISurfaceAllocator.Stub {
  private static final String LOGTAG = "RemoteSurfaceAllocator";

  private static RemoteSurfaceAllocator mInstance;

  public static synchronized RemoteSurfaceAllocator getInstance() {
    if (mInstance == null) {
      mInstance = new RemoteSurfaceAllocator();
    }
    return mInstance;
  }

  @Override
  public GeckoSurface acquireSurface(
      final int width, final int height, final boolean singleBufferMode) {
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.acquire(singleBufferMode, 0);

    if (gst == null) {
      return null;
    }

    if (width > 0 && height > 0) {
      gst.setDefaultBufferSize(width, height);
    }

    return new GeckoSurface(gst);
  }

  @Override
  public void releaseSurface(final int handle) {
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(handle);
    if (gst != null) {
      gst.decrementUse();
    }
  }

  @Override
  public void configureSync(final SyncConfig config) {
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(config.sourceTextureHandle);
    if (gst != null) {
      gst.configureSnapshot(config.targetSurface, config.width, config.height);
    }
  }

  @Override
  public void sync(final int handle) {
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(handle);
    if (gst != null) {
      gst.takeSnapshot();
    }
  }
}
