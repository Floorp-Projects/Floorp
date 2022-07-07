/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import android.os.Binder;
import android.util.SparseArray;
import android.view.Surface;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.ICompositorSurfaceManager;
import org.mozilla.gecko.gfx.ISurfaceAllocator;
import org.mozilla.gecko.gfx.RemoteSurfaceAllocator;

public class GeckoServiceGpuProcess extends GeckoServiceChildProcess {
  private static final String LOGTAG = "ServiceGpuProcess";

  private static final class GpuProcessBinder extends GeckoServiceChildProcess.ChildProcessBinder {
    @Override
    public ICompositorSurfaceManager getCompositorSurfaceManager() {
      return RemoteCompositorSurfaceManager.getInstance();
    }

    @Override
    public ISurfaceAllocator getSurfaceAllocator(final int allocatorId) {
      return RemoteSurfaceAllocator.getInstance(allocatorId);
    }
  }

  @Override
  protected Binder createBinder() {
    return new GpuProcessBinder();
  }

  public static final class RemoteCompositorSurfaceManager extends ICompositorSurfaceManager.Stub {
    private static RemoteCompositorSurfaceManager mInstance;

    @WrapForJNI
    private static synchronized RemoteCompositorSurfaceManager getInstance() {
      if (mInstance == null) {
        mInstance = new RemoteCompositorSurfaceManager();
      }
      return mInstance;
    }

    private final SparseArray<Surface> mSurfaces = new SparseArray<Surface>();

    @Override
    public synchronized void onSurfaceChanged(final int widgetId, final Surface surface) {
      if (surface != null) {
        mSurfaces.put(widgetId, surface);
      } else {
        mSurfaces.remove(widgetId);
      }
    }

    @WrapForJNI
    public synchronized Surface getCompositorSurface(final int widgetId) {
      return mSurfaces.get(widgetId);
    }
  }
}
