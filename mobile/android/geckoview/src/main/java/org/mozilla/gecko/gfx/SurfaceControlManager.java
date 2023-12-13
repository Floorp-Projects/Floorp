/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.os.Build;
import android.view.Surface;
import android.view.SurfaceControl;
import androidx.annotation.RequiresApi;
import java.util.Iterator;
import java.util.Map;
import java.util.WeakHashMap;
import org.mozilla.gecko.annotation.WrapForJNI;

// A helper class that creates Surfaces from SurfaceControl objects, for the widget to render in to.
// Unlike the Surfaces provided to the widget directly from the application, these are suitable for
// use in the GPU process as well as the main process.
//
// The reason we must not render directly in to the Surface provided by the application from the GPU
// process is because of a bug on Android versions 12 and later: when the GPU process dies the
// Surface is not detached from the dead process' EGL surface, and any subsequent attempts to
// attach another EGL surface to the Surface will fail.
//
// The application is therefore required to provide the SurfaceControl object to a GeckoDisplay
// whenever rendering in to a SurfaceView. The widget will then obtain a Surface from that
// SurfaceControl using getChildSurface(). Internally, this creates another SurfaceControl as a
// child of the provided SurfaceControl, then creates the Surface from that child. If the GPU
// process dies we are able to simply destroy and recreate the child SurfaceControl objects, thereby
// avoiding the bug.
public class SurfaceControlManager {
  private static final String LOGTAG = "SurfaceControlManager";

  private static final SurfaceControlManager sInstance = new SurfaceControlManager();

  private final WeakHashMap<SurfaceControl, SurfaceControl> mChildSurfaceControls =
      new WeakHashMap<>();

  @WrapForJNI
  public static SurfaceControlManager getInstance() {
    return sInstance;
  }

  // Returns a Surface of the requested size that will be composited in to the specified
  // SurfaceControl.
  @RequiresApi(api = Build.VERSION_CODES.Q)
  @WrapForJNI(exceptionMode = "abort")
  public synchronized Surface getChildSurface(
      final SurfaceControl parent, final int width, final int height) {
    SurfaceControl child = mChildSurfaceControls.get(parent);
    if (child == null) {
      // We must periodically check if any of the SurfaceControls we are managing have been
      // destroyed, as we are unable to directly listen to their SurfaceViews' surfaceDestroyed
      // callbacks, and they may not be attached to any compositor when they are destroyed meaning
      // we cannot perform cleanup in response to the compositor being paused.
      // Doing so here, when we encounter a new SurfaceControl instance, is a reasonable guess as to
      // when a previous instance may have been released.
      final Iterator<Map.Entry<SurfaceControl, SurfaceControl>> it =
          mChildSurfaceControls.entrySet().iterator();
      while (it.hasNext()) {
        final Map.Entry<SurfaceControl, SurfaceControl> entry = it.next();
        if (!entry.getKey().isValid()) {
          it.remove();
        }
      }

      child = new SurfaceControl.Builder().setParent(parent).setName("GeckoSurface").build();
      mChildSurfaceControls.put(parent, child);
    }

    final SurfaceControl.Transaction transaction =
        new SurfaceControl.Transaction()
            .setVisibility(child, true)
            .setBufferSize(child, width, height);
    transaction.apply();
    transaction.close();

    return new Surface(child);
  }

  // Removes an existing parent SurfaceControl and its corresponding child from the manager. This
  // can be used when we require the next call to getChildSurface() for the specified parent to
  // create a new child rather than return the existing one.
  @RequiresApi(api = Build.VERSION_CODES.Q)
  @WrapForJNI(exceptionMode = "abort")
  public synchronized void removeSurface(final SurfaceControl parent) {
    final SurfaceControl child = mChildSurfaceControls.remove(parent);
    if (child != null) {
      child.release();
    }
  }

  // Must be called whenever the GPU process has died. This destroys all the child SurfaceControls
  // that have been created, meaning subsequent calls to getChildSurface() will create new ones.
  @RequiresApi(api = Build.VERSION_CODES.Q)
  @WrapForJNI(exceptionMode = "abort")
  public synchronized void onGpuProcessLoss() {
    for (final SurfaceControl child : mChildSurfaceControls.values()) {
      // Explicitly reparenting the old SurfaceControl to null ensures SurfaceFlinger does not hold
      // on to it. We used to not do this in order to avoid a blank screen until we resume rendering
      // in to a new SurfaceControl, but on some devices this was causing glitches.
      final SurfaceControl.Transaction transaction =
          new SurfaceControl.Transaction().reparent(child, null);
      transaction.apply();
      transaction.close();
      child.release();
    }
    mChildSurfaceControls.clear();
  }
}
