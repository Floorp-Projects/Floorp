/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.Color;

public class ColorUtil {
    public static int darken(final int color, final double fraction) {
        int red = Color.red(color);
        int green = Color.green(color);
        int blue = Color.blue(color);
        red = darkenColor(red, fraction);
        green = darkenColor(green, fraction);
        blue = darkenColor(blue, fraction);
        final int alpha = Color.alpha(color);
        return Color.argb(alpha, red, green, blue);
    }

    private static int darkenColor(final int color, final double fraction) {
        return (int) Math.max(color - (color * fraction), 0);
    }
}
