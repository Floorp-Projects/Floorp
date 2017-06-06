/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.graphics.Color;

public class ColorUtils {
    public static int getReadableTextColor(final int backgroundColor) {
        final int greyValue = grayscaleFromRGB(backgroundColor);
        // 186 chosen rather than the seemingly obvious 128 because of gamma.
        if (greyValue < 186) {
            return Color.WHITE;
        } else {
            return Color.BLACK;
        }
    }

    private static int grayscaleFromRGB(final int color) {
        final int red = Color.red(color);
        final int green = Color.green(color);
        final int blue = Color.blue(color);
        // Magic weighting taken from a stackoverflow post, supposedly related to how
        // humans perceive color.
        return (int) (0.299 * red + 0.587 * green + 0.114 * blue);
    }
}
