/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import android.graphics.Bitmap;
import android.support.annotation.ColorInt;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.util.BitmapUtils;

/**
 * Processor implementation to extract the dominant color from the icon and attach it to the icon
 * response object.
 */
public class ColorProcessor implements Processor {
    private static final String LOGTAG = "GeckoColorProcessor";
    private static final int DEFAULT_COLOR = 0xFFB1B1B3; // 0 == No color, here we use photon color

    @Override
    public void process(IconRequest request, IconResponse response) {
        if (response.hasColor()) {
            return;
        }

        final Bitmap bitmap = response.getBitmap();

        final @ColorInt Integer edgeColor = getEdgeColor(bitmap);
        if (edgeColor != null) {
            response.updateColor(edgeColor);
            return;
        }

        final @ColorInt int dominantColor = BitmapUtils.getDominantColor(response.getBitmap(), DEFAULT_COLOR);
        response.updateColor(dominantColor & 0x7FFFFFFF);
    }

    /**
     * If a bitmap has a consistent edge colour (i.e. if all the border pixels have the same colour),
     * return that colour.
     * @param bitmap
     * @return The edge colour. null if there is no consistent edge color.
     */
    private @ColorInt Integer getEdgeColor(final Bitmap bitmap) {
        final int width = bitmap.getWidth();
        final int height = bitmap.getHeight();

        // Only allocate an array once, with the max width we need once, to minimise the number
        // of allocations.
        @ColorInt int[] edge = new int[Math.max(width, height)];

        // Top:
        bitmap.getPixels(edge, 0, width, 0, 0, width, 1);
        final @ColorInt Integer edgecolor = getEdgeColorFromSingleDimension(edge, width);
        if (edgecolor == null) {
            return null;
        }

        // Bottom:
        bitmap.getPixels(edge, 0, width, 0, height - 1, width, 1);
        if (!edgecolor.equals(getEdgeColorFromSingleDimension(edge, width))) {
            return null;
        }

        // Left:
        bitmap.getPixels(edge, 0, 1, 0, 0, 1, height);
        if (!edgecolor.equals(getEdgeColorFromSingleDimension(edge, height))) {
            return null;
        }

        // Right:
        bitmap.getPixels(edge, 0, 1, width - 1, 0, 1, height);
        if (!edgecolor.equals(getEdgeColorFromSingleDimension(edge, height))) {
            return null;
        }

        return edgecolor;
    }

    /**
     * Obtain the colour for a given edge if all colors are the same.
     *
     * @param edge An array containing the color values of the pixels constituting the edge of a bitmap.
     * @param length The length of the array to be traversed. Must be smaller than, or equal to
     * the total length of the array.
     * @return The colour contained within the array, or null if colours vary.
     */
    private @ColorInt Integer getEdgeColorFromSingleDimension(@ColorInt int[] edge, int length) {
        @ColorInt int color = edge[0];

        for (int i = 1; i < length; ++i) {
            if (edge[i] != color) {
                return null;
            }
        }

        return color;
    }
}
