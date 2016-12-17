/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

/**
 * A <code>ScreenOrientationDelegate</code> is responsible for setting the screen orientation.
 * <p>
 * A browser that wants to support the <a href="https://w3c.github.io/screen-orientation/">Screen
 * Orientation API</a> MUST implement these methods.  A GeckoView consumer MAY implement these
 * methods.
 * <p> To implement, consider registering an
 * {@link android.app.Application.ActivityLifecycleCallbacks} handler to track the current
 * foreground {@link android.app.Activity}.
 */
public interface ScreenOrientationDelegate {
    /**
     * If possible, set the current screen orientation.
     *
     * @param requestedActivityInfoOrientation An orientation constant as used in {@link android.content.pm.ActivityInfo#screenOrientation}.
     * @return true if screen orientation could be set; false otherwise.
     */
    boolean setRequestedOrientationForCurrentActivity(int requestedActivityInfoOrientation);
}
