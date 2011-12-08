/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

