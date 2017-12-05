/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.annotation.ColorInt;
import android.support.v7.graphics.Palette;
import android.util.Base64;
import android.util.Log;

public final class BitmapUtils {
    private static final String LOGTAG = "GeckoBitmapUtils";

    private BitmapUtils() {}

    public static Bitmap decodeByteArray(byte[] bytes) {
        return decodeByteArray(bytes, null);
    }

    public static Bitmap decodeByteArray(byte[] bytes, BitmapFactory.Options options) {
        return decodeByteArray(bytes, 0, bytes.length, options);
    }

    public static Bitmap decodeByteArray(byte[] bytes, int offset, int length) {
        return decodeByteArray(bytes, offset, length, null);
    }

    public static Bitmap decodeByteArray(byte[] bytes, int offset, int length, BitmapFactory.Options options) {
        if (bytes.length <= 0) {
            throw new IllegalArgumentException("bytes.length " + bytes.length
                                               + " must be a positive number");
        }

        Bitmap bitmap = null;
        try {
            bitmap = BitmapFactory.decodeByteArray(bytes, offset, length, options);
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

    public static Bitmap decodeStream(InputStream inputStream) {
        try {
            return BitmapFactory.decodeStream(inputStream);
        } catch (OutOfMemoryError e) {
            Log.e(LOGTAG, "decodeStream() OOM!", e);
            return null;
        }
    }

    public static Bitmap decodeUrl(Uri uri) {
        return decodeUrl(uri.toString());
    }

    public static Bitmap decodeUrl(String urlString) {
        URL url;

        try {
            url = new URL(urlString);
        } catch (MalformedURLException e) {
            Log.w(LOGTAG, "decodeUrl: malformed URL " + urlString);
            return null;
        }

        return decodeUrl(url);
    }

    public static Bitmap decodeUrl(URL url) {
        InputStream stream = null;

        try {
            stream = url.openStream();
        } catch (IOException e) {
            Log.w(LOGTAG, "decodeUrl: IOException downloading " + url);
            return null;
        }

        if (stream == null) {
            Log.w(LOGTAG, "decodeUrl: stream not found downloading " + url);
            return null;
        }

        Bitmap bitmap = decodeStream(stream);

        try {
            stream.close();
        } catch (IOException e) {
            Log.w(LOGTAG, "decodeUrl: IOException closing stream " + url, e);
        }

        return bitmap;
    }

    public static Bitmap decodeResource(Context context, int id) {
        return decodeResource(context, id, null);
    }

    public static Bitmap decodeResource(Context context, int id, BitmapFactory.Options options) {
        Resources resources = context.getResources();
        try {
            return BitmapFactory.decodeResource(resources, id, options);
        } catch (OutOfMemoryError e) {
            Log.e(LOGTAG, "decodeResource() OOM! Resource id=" + id, e);
            return null;
        }
    }

    public static @ColorInt int getDominantColor(Bitmap source, @ColorInt int defaultColor) {
        if (HardwareUtils.isX86System()) {
            // (Bug 1318667) We are running into crashes when using the palette library with
            // specific icons on x86 devices. They take down the whole VM and are not recoverable.
            // Unfortunately our release icon is triggering this crash. Until we can switch to a
            // newer version of the support library where this does not happen, we are using our
            // own slower implementation.
            return getDominantColorCustomImplementation(source, true, defaultColor);
        } else {
            try {
                final Palette palette = Palette.from(source).generate();
                return palette.getVibrantColor(defaultColor);
            } catch (ArrayIndexOutOfBoundsException e) {
                // We saw the palette library fail with an ArrayIndexOutOfBoundsException intermittently
                // in automation. In this case lets just swallow the exception and move on without a
                // color. This is a valid condition and callers should handle this gracefully (Bug 1318560).
                Log.e(LOGTAG, "Palette generation failed with ArrayIndexOutOfBoundsException", e);

                return defaultColor;
            }
        }
    }

    public static @ColorInt int getDominantColorCustomImplementation(Bitmap source) {
        return getDominantColorCustomImplementation(source, true, Color.WHITE);
    }

    public static @ColorInt int getDominantColorCustomImplementation(Bitmap source,
                                                                     boolean applyThreshold,
                                                                     @ColorInt int defaultColor) {
      if (source == null) {
          return defaultColor;
      }

      // Keep track of how many times a hue in a given bin appears in the image.
      // Hue values range [0 .. 360), so dividing by 10, we get 36 bins.
      int[] colorBins = new int[36];

      // The bin with the most colors. Initialize to -1 to prevent accidentally
      // thinking the first bin holds the dominant color.
      int maxBin = -1;

      // Keep track of sum hue/saturation/value per hue bin, which we'll use to
      // compute an average to for the dominant color.
      float[] sumHue = new float[36];
      float[] sumSat = new float[36];
      float[] sumVal = new float[36];
      float[] hsv = new float[3];

      int height = source.getHeight();
      int width = source.getWidth();
      int[] pixels = new int[width * height];
      source.getPixels(pixels, 0, width, 0, 0, width, height);
      for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
          int c = pixels[col + row * width];
          // Ignore pixels with a certain transparency.
          if (Color.alpha(c) < 128)
            continue;

          Color.colorToHSV(c, hsv);

          // If a threshold is applied, ignore arbitrarily chosen values for "white" and "black".
          if (applyThreshold && (hsv[1] <= 0.35f || hsv[2] <= 0.35f))
            continue;

          // We compute the dominant color by putting colors in bins based on their hue.
          int bin = (int) Math.floor(hsv[0] / 10.0f);

          // Update the sum hue/saturation/value for this bin.
          sumHue[bin] = sumHue[bin] + hsv[0];
          sumSat[bin] = sumSat[bin] + hsv[1];
          sumVal[bin] = sumVal[bin] + hsv[2];

          // Increment the number of colors in this bin.
          colorBins[bin]++;

          // Keep track of the bin that holds the most colors.
          if (maxBin < 0 || colorBins[bin] > colorBins[maxBin])
            maxBin = bin;
        }
      }

      // maxBin may never get updated if the image holds only transparent and/or black/white pixels.
      if (maxBin < 0) {
          return defaultColor;
      }

      // Return a color with the average hue/saturation/value of the bin with the most colors.
      hsv[0] = sumHue[maxBin] / colorBins[maxBin];
      hsv[1] = sumSat[maxBin] / colorBins[maxBin];
      hsv[2] = sumVal[maxBin] / colorBins[maxBin];
      return Color.HSVToColor(hsv);
    }

    /**
     * Decodes a bitmap from a Base64 data URI.
     *
     * @param dataURI a Base64-encoded data URI string
     * @return        the decoded bitmap, or null if the data URI is invalid
     */
    public static Bitmap getBitmapFromDataURI(String dataURI) {
        if (dataURI == null) {
            return null;
        }

        byte[] raw = getBytesFromDataURI(dataURI);
        if (raw == null || raw.length == 0) {
            return null;
        }

        return decodeByteArray(raw);
    }

    /**
     * Return a byte[] containing the bytes in a given base64 string, or null if this is not a valid
     * base64 string.
     */
    public static byte[] getBytesFromBase64(String base64) {
        try {
            return Base64.decode(base64, Base64.DEFAULT);
        } catch (Exception e) {
            Log.e(LOGTAG, "exception decoding bitmap from data URI: " + base64, e);
        }

        return null;
    }

    public static byte[] getBytesFromDataURI(String dataURI) {
        final String base64 = dataURI.substring(dataURI.indexOf(',') + 1);
        return getBytesFromBase64(base64);
    }

    public static Bitmap getBitmapFromDrawable(Drawable drawable) {
        if (drawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) drawable).getBitmap();
        }

        int width = drawable.getIntrinsicWidth();
        width = width > 0 ? width : 1;
        int height = drawable.getIntrinsicHeight();
        height = height > 0 ? height : 1;

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);

        return bitmap;
    }

    public static int getResource(final Context context, final Uri resourceUrl) {
        final String scheme = resourceUrl.getScheme();
        if (!"drawable".equals(scheme)) {
            // Return a "not found" default icon that's easy to spot.
            return android.R.drawable.sym_def_app_icon;
        }

        String resource = resourceUrl.getSchemeSpecificPart();
        if (resource.startsWith("//")) {
            resource = resource.substring(2);
        }

        final Resources res = context.getResources();
        int id = res.getIdentifier(resource, "drawable", context.getPackageName());
        if (id != 0) {
            return id;
        }

        // For backwards compatibility, we also search in system resources.
        id = res.getIdentifier(resource, "drawable", "android");
        if (id != 0) {
            return id;
        }

        Log.w(LOGTAG, "Cannot find drawable/" + resource);
        // Return a "not found" default icon that's easy to spot.
        return android.R.drawable.sym_def_app_icon;
    }
}
