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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
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

import android.graphics.Color;
import android.graphics.Bitmap;
import java.lang.Math;
import android.util.Log;

public final class BitmapUtils {
    public static int getDominantColor(Bitmap source) {
      int[] colors = new int[37];
      int[] sat = new int[11];
      int[] val = new int[11];
      int maxH = 0;
      int maxS = 0;
      int maxV = 0;
      if (source == null)
        return Color.argb(255,255,255,255);

      for (int row = 0; row < source.getHeight(); row++) {
        for (int col = 0; col < source.getWidth(); col++) {
          int c = source.getPixel(col, row);
          if (Color.alpha(c) < 128)
            continue;

          float[] hsv = new float[3];
          Color.colorToHSV(c, hsv);

          // arbitrarily chosen values for "white" and "black"
          if (hsv[1] > 0.35f && hsv[2] > 0.35f) {
            int h = Math.round(hsv[0] / 10.0f);
            int s = Math.round(hsv[1] * 10.0f);
            int v = Math.round(hsv[2] * 10.0f);
            colors[h]++;
            sat[s]++;
            val[v]++;
            // we only care about the most unique non white or black hue, but also
            // store its saturation and value params to match the color better
            if (colors[h] > colors[maxH]) {
              maxH = h;
              maxS = s;
              maxV = v;
            }
          }
        }
      }
      float[] hsv = new float[3];
      hsv[0] = maxH*10.0f;
      hsv[1] = (float)maxS/10.0f;
      hsv[2] = (float)maxV/10.0f;
      return Color.HSVToColor(hsv);
    }
}

