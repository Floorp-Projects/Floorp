/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.processing;

import android.support.v7.graphics.Palette;
import android.util.Log;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * Processor implementation to extract the dominant color from the icon and attach it to the icon
 * response object.
 */
public class ColorProcessor implements Processor {
    private static final String LOGTAG = "GeckoColorProcessor";
    private static final int DEFAULT_COLOR = 0; // 0 == No color

    @Override
    public void process(IconRequest request, IconResponse response) {
        if (response.hasColor()) {
            return;
        }

        try {
            final Palette palette = Palette.from(response.getBitmap()).generate();
            response.updateColor(palette.getVibrantColor(DEFAULT_COLOR));
        } catch (ArrayIndexOutOfBoundsException e) {
            // We saw the palette library fail with an ArrayIndexOutOfBoundsException intermittently
            // in automation. In this case lets just swallow the exception and move on without a
            // color. This is a valid condition and callers should handle this gracefully (Bug 1318560).
            Log.e(LOGTAG, "Palette generation failed with ArrayIndexOutOfBoundsException", e);

            response.updateColor(DEFAULT_COLOR);
        }
    }
}
