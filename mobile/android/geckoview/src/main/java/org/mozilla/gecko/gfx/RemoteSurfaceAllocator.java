/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

public final class RemoteSurfaceAllocator extends ISurfaceAllocator.Stub {
  private static final String LOGTAG = "RemoteSurfaceAllocator";

  private static RemoteSurfaceAllocator mInstance;

  private final int mAllocatorId;
  /// Monotonically increasing counter used to generate unique handles
  /// for each SurfaceTexture by combining with mAllocatorId.
  private static volatile int sNextHandle = 1;

  /**
   * Retrieves the singleton allocator instance for this process.
   *
   * @param allocatorId A unique ID identifying the process this instance belongs to, which must be
   *     0 for the parent process instance.
   */
  public static synchronized RemoteSurfaceAllocator getInstance(final int allocatorId) {
    if (mInstance == null) {
      mInstance = new RemoteSurfaceAllocator(allocatorId);
    }
    return mInstance;
  }

  private RemoteSurfaceAllocator(final int allocatorId) {
    mAllocatorId = allocatorId;
  }

  @Override
  public GeckoSurface acquireSurface(
      final int width, final int height, final boolean singleBufferMode) {
    final long handle = ((long) mAllocatorId << 32) | sNextHandle++;
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.acquire(singleBufferMode, handle);

    if (gst == null) {
      return null;
    }

    if (width > 0 && height > 0) {
      gst.setDefaultBufferSize(width, height);
    }

    return new GeckoSurface(gst);
  }

  @Override
  public void releaseSurface(final long handle) {
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
  public void sync(final long handle) {
    final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(handle);
    if (gst != null) {
      gst.takeSnapshot();
    }
  }
}
