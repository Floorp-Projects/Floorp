/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import org.mozilla.gecko.Driver;
import org.mozilla.gecko.tests.UITestContext;

import com.jayway.android.robotium.solo.Solo;

/**
 * Provides simplified gestures wrapping the Robotium gestures API.
 */
public final class GestureHelper {
    private static int DEFAULT_DRAG_STEP_COUNT = 10;

    private static Solo sSolo;
    private static Driver sDriver;

    private GestureHelper() { /* To disallow instantation. */ }

    public static void init(final UITestContext context) {
        sSolo = context.getSolo();
        sDriver = context.getDriver();
    }

    private static void swipeOnScreen(final int direction) {
        final int halfWidth = sDriver.getGeckoWidth() / 2;
        final int halfHeight = sDriver.getGeckoHeight() / 2;

        sSolo.drag(direction == Solo.LEFT ? halfWidth : 0,
                   direction == Solo.LEFT ? 0 : halfWidth,
                   halfHeight, halfHeight, DEFAULT_DRAG_STEP_COUNT);
    }

    public static void swipeLeft() {
        swipeOnScreen(Solo.LEFT);
    }

    public static void swipeRight() {
        swipeOnScreen(Solo.RIGHT);
    }
}
