/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.util.Base64;
import android.util.Log;

public final class BitmapUtils {
    private static final String LOGTAG = "GeckoBitmapUtils";

    private BitmapUtils() {}

    public static Bitmap decodeByteArray(byte[] bytes) {
        return decodeByteArray(bytes, null);
    }

    public static Bitmap decodeByteArray(byte[] bytes, BitmapFactory.Options options) {
        if (bytes.length <= 0) {
            throw new IllegalArgumentException("bytes.length " + bytes.length
                                               + " must be a positive number");
        }

        Bitmap bitmap = null;
        try {
            bitmap = BitmapFactory.decodeByteArray(bytes, 0, bytes.length, options);
        } catch (OutOfMemoryError e) {
            Log.e(LOGTAG, "decodeByteArray(bytes.length=" + bytes.length
                          + ", options= " + options + ") OOM!", e);
            return null;
        }

        if (bitmap == null) {
            Log.w(LOGTAG, "decodeByteArray() returning null because BitmapFactory returned null");
            return null;
        }

        if (bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) {
            Log.w(LOGTAG, "decodeByteArray() returning null because BitmapFactory returned "
                          + "a bitmap with dimensions " + bitmap.getWidth()
                          + "x" + bitmap.getHeight());
            return null;
        }

        return bitmap;
    }

    public static int getDominantColor(Bitmap source) {
        return getDominantColor(source, true);
    }

    public static int getDominantColor(Bitmap source, boolean applyThreshold) {
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
          if (applyThreshold && hsv[1] <= 0.35f && hsv[2] <= 0.35f)
            continue;

          int h = Math.round(hsv[0] / 10.0f);
          int s = Math.round(hsv[1] * 10.0f);
          int v = Math.round(hsv[2] * 10.0f);
          colors[h]++;
          sat[s]++;
          val[v]++;

          // we only care about the most unique non white or black hue - if threshold is applied
          // we also store its saturation and value params to match the color better
          if (colors[h] > colors[maxH]) {
            maxH = h;
            maxS = s;
            maxV = v;
          }
        }
      }
      float[] hsv = new float[3];
      hsv[0] = maxH*10.0f;
      hsv[1] = (float)maxS/10.0f;
      hsv[2] = (float)maxV/10.0f;
      return Color.HSVToColor(hsv);
    }

    /**
     * Decodes a bitmap from a Base64 data URI.
     *
     * @param dataURI a Base64-encoded data URI string
     * @return        the decoded bitmap, or null if the data URI is invalid
     */
    public static Bitmap getBitmapFromDataURI(String dataURI) {
        String base64 = dataURI.substring(dataURI.indexOf(',') + 1);
        try {
            byte[] raw = Base64.decode(base64, Base64.DEFAULT);
            return BitmapUtils.decodeByteArray(raw);
        } catch (Exception e) {
            Log.e(LOGTAG, "exception decoding bitmap from data URI: " + dataURI, e);
        }
        return null;
    }
}

