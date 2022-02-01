/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import org.mozilla.gecko.util.ThreadUtils;

public class OrientationController {
  private OrientationDelegate mDelegate;

  OrientationController() {}

  /**
   * Sets the {@link OrientationDelegate} for this instance.
   *
   * @param delegate The {@link OrientationDelegate} instance.
   */
  @UiThread
  public void setDelegate(final @Nullable OrientationDelegate delegate) {
    ThreadUtils.assertOnUiThread();
    mDelegate = delegate;
  }

  /**
   * Gets the {@link OrientationDelegate} for this instance.
   *
   * @return delegate The {@link OrientationDelegate} instance.
   */
  @UiThread
  @Nullable
  public OrientationDelegate getDelegate() {
    ThreadUtils.assertOnUiThread();
    return mDelegate;
  }

  /** This delegate will be called whenever an orientation lock is called. */
  @UiThread
  public interface OrientationDelegate {
    /**
     * Called whenever the orientation should be locked.
     *
     * @param aOrientation The desired orientation such as ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
     * @return A {@link GeckoResult} which resolves to a {@link AllowOrDeny}
     */
    @Nullable
    default GeckoResult<AllowOrDeny> onOrientationLock(@NonNull final int aOrientation) {
      return null;
    }

    /** Called whenever the orientation should be unlocked. */
    @Nullable
    default void onOrientationUnlock() {}
  }
}
