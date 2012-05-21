/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

