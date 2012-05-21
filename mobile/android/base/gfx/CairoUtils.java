/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.CairoImage;
import android.graphics.Bitmap;
import javax.microedition.khronos.opengles.GL10;

/**
 * Utility methods useful when displaying Cairo bitmaps using OpenGL ES.
 */
public class CairoUtils {
    private CairoUtils() { /* Don't call me. */ }

    public static int bitsPerPixelForCairoFormat(int cairoFormat) {
        switch (cairoFormat) {
        case CairoImage.FORMAT_A1:          return 1;
        case CairoImage.FORMAT_A8:          return 8;
        case CairoImage.FORMAT_RGB16_565:   return 16;
        case CairoImage.FORMAT_RGB24:       return 24;
        case CairoImage.FORMAT_ARGB32:      return 32;
        default:
            throw new RuntimeException("Unknown Cairo format");
        }
    }

    public static int bitmapConfigToCairoFormat(Bitmap.Config config) {
        if (config == null)
            return CairoImage.FORMAT_ARGB32;    /* Droid Pro fix. */

        switch (config) {
        case ALPHA_8:   return CairoImage.FORMAT_A8;
        case ARGB_4444: throw new RuntimeException("ARGB_444 unsupported");
        case ARGB_8888: return CairoImage.FORMAT_ARGB32;
        case RGB_565:   return CairoImage.FORMAT_RGB16_565;
        default:        throw new RuntimeException("Unknown Skia bitmap config");
        }
    }

    public static Bitmap.Config cairoFormatTobitmapConfig(int format) {
        switch (format) {
        case CairoImage.FORMAT_A8:        return Bitmap.Config.ALPHA_8;
        case CairoImage.FORMAT_ARGB32:    return Bitmap.Config.ARGB_8888;
        case CairoImage.FORMAT_RGB16_565: return Bitmap.Config.RGB_565;
        default:
            throw new RuntimeException("Unknown CairoImage format");
        }
    }
}

