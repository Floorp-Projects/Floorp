/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * Interface for registering the external VR context with WebVR. The context must be registered
 * before Gecko is started. This API is not intended for external consumption. To see an example of
 * how it is used please see the <a href="https://github.com/MozillaReality/FirefoxReality"
 * target="_blank">Firefox Reality browser</a>.
 *
 * @see <a href="https://searchfox.org/mozilla-central/source/gfx/vr/external_api/moz_external_vr.h"
 *     target="_blank">External VR Context</a>
 */
public class GeckoVRManager {
  private static long mExternalContext;

  private GeckoVRManager() {}

  @WrapForJNI
  private static synchronized long getExternalContext() {
    return mExternalContext;
  }

  /**
   * Sets the external VR context. The external VR context is defined <a
   * href="https://searchfox.org/mozilla-central/source/gfx/vr/external_api/moz_external_vr.h"
   * target="_blank">here</a>.
   *
   * @param externalContext A pointer to the external VR context.
   */
  @AnyThread
  public static synchronized void setExternalContext(final long externalContext) {
    mExternalContext = externalContext;
  }
}
