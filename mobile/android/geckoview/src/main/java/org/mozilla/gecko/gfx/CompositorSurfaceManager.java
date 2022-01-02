/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.os.RemoteException;
import android.view.Surface;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class CompositorSurfaceManager {
  private static final String LOGTAG = "CompSurfManager";

  private ICompositorSurfaceManager mManager;

  public CompositorSurfaceManager(final ICompositorSurfaceManager aManager) {
    mManager = aManager;
  }

  @WrapForJNI(exceptionMode = "nsresult")
  public synchronized void onSurfaceChanged(final int widgetId, final Surface surface)
      throws RemoteException {
    mManager.onSurfaceChanged(widgetId, surface);
  }
}
