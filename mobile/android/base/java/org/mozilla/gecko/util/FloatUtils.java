/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.PointF;

import java.lang.IllegalArgumentException;

public final class FloatUtils {
    private FloatUtils() {}

    public static boolean fuzzyEquals(float a, float b) {
        return (Math.abs(a - b) < 1e-6);
    }

    public static boolean fuzzyEquals(PointF a, PointF b) {
        return fuzzyEquals(a.x, b.x) && fuzzyEquals(a.y, b.y);
    }

    /*
     * Returns the value that represents a linear transition between `from` and `to` at time `t`,
     * which is on the scale [0, 1). Thus with t = 0.0f, this returns `from`; with t = 1.0f, this
     * returns `to`; with t = 0.5f, this returns the value halfway from `from` to `to`.
     */
    public static float interpolate(float from, float to, float t) {
        return from + (to - from) * t;
    }

    /**
     * Returns 'value', clamped so that it isn't any lower than 'low', and it
     * isn't any higher than 'high'.
     */
    public static float clamp(float value, float low, float high) {
        if (high < low) {
          throw new IllegalArgumentException(
              "clamp called with invalid parameters (" + high + " < " + low + ")" );
        }
        return Math.max(low, Math.min(high, value));
    }
}
