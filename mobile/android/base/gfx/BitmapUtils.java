/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.net.MalformedURLException;
import java.net.URL;

import org.mozilla.gecko.R;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ThumbnailHelper;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

public final class BitmapUtils {
    private static final String LOGTAG = "GeckoBitmapUtils";

    private BitmapUtils() {}

    public interface BitmapLoader {
        public void onBitmapFound(Drawable d);
    }

    private static void runOnBitmapFoundOnUiThread(final BitmapLoader loader, final Drawable d) {
        if (ThreadUtils.isOnUiThread()) {
            loader.onBitmapFound(d);
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                loader.onBitmapFound(d);
            }
        });
    }

    /**
     * Attempts to find a drawable associated with a given string, using its URI scheme to determine
     * how to load the drawable. The BitmapLoader's `onBitmapFound` method is always called, and
     * will be called with `null` if no drawable is found.
     *
     * The BitmapLoader `onBitmapFound` method always runs on the UI thread.
     */
    public static void getDrawable(final Context context, final String data, final BitmapLoader loader) {
        if (TextUtils.isEmpty(data)) {
            runOnBitmapFoundOnUiThread(loader, null);
            return;
        }

        if (data.startsWith("data")) {
            final BitmapDrawable d = new BitmapDrawable(context.getResources(), getBitmapFromDataURI(data));
            runOnBitmapFoundOnUiThread(loader, d);
            return;
        }

        if (data.startsWith("thumbnail:")) {
            getThumbnailDrawable(context, data, loader);
            return;
        }

        if (data.startsWith("jar:") || data.startsWith("file://")) {
            (new UiAsyncTask<Void, Void, Drawable>(ThreadUtils.getBackgroundHandler()) {
                @Override
                public Drawable doInBackground(Void... params) {
                    try {
                        if (data.startsWith("jar:jar")) {
                            return GeckoJarReader.getBitmapDrawable(context.getResources(), data);
                        }

                        // Don't attempt to validate the JAR signature when loading an add-on icon
                        if (data.startsWith("jar:file")) {
                            return GeckoJarReader.getBitmapDrawable(context.getResources(), Uri.decode(data));
                        }

                        final URL url = new URL(data);
                        final InputStream is = (InputStream) url.getContent();
                        try {
                            return Drawable.createFromStream(is, "src");
                        } finally {
                            is.close();
                        }
                    } catch (Exception e) {
                        Log.w(LOGTAG, "Unable to set icon", e);
                    }
                    return null;
                }

                @Override
                public void onPostExecute(Drawable drawable) {
                    loader.onBitmapFound(drawable);
                }
            }).execute();
            return;
        }

        if (data.startsWith("-moz-icon://")) {
            final Uri imageUri = Uri.parse(data);
            final String ssp = imageUri.getSchemeSpecificPart();
            final String resource = ssp.substring(ssp.lastIndexOf('/') + 1);

            try {
                final Drawable d = context.getPackageManager().getApplicationIcon(resource);
                runOnBitmapFoundOnUiThread(loader, d);
            } catch(Exception ex) { }

            return;
        }

        if (data.startsWith("drawable://")) {
            final Uri imageUri = Uri.parse(data);
            final int id = getResource(imageUri, R.drawable.ic_status_logo);
            final Drawable d = context.getResources().getDrawable(id);

            runOnBitmapFoundOnUiThread(loader, d);
            return;
        }

        runOnBitmapFoundOnUiThread(loader, null);
    }

    public static void getThumbnailDrawable(final Context context, final String data, final BitmapLoader loader) {
         int id = Integer.parseInt(data.substring(10), 10);
         final Tab tab = Tabs.getInstance().getTab(id);
         runOnBitmapFoundOnUiThread(loader, tab.getThumbnail());
         Tabs.registerOnTabsChangedListener(new Tabs.OnTabsChangedListener() {
                 public void onTabChanged(Tab t, Tabs.TabEvents msg, Object data) {
                     if (tab == t && msg == Tabs.TabEvents.THUMBNAIL) {
                         Tabs.unregisterOnTabsChangedListener(this);
                         runOnBitmapFoundOnUiThread(loader, t.getThumbnail());
                     }
                 }
             });
         ThumbnailHelper.getInstance().getAndProcessThumbnailFor(tab);
    }

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
        } catch(MalformedURLException e) {
            Log.w(LOGTAG, "decodeUrl: malformed URL " + urlString);
            return null;
        }

        return decodeUrl(url);
    }

    public static Bitmap decodeUrl(URL url) {
        InputStream stream = null;

        try {
            stream = url.openStream();
        } catch(IOException e) {
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
        } catch(IOException e) {
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

    public static int getDominantColor(Bitmap source) {
        return getDominantColor(source, true);
    }

    public static int getDominantColor(Bitmap source, boolean applyThreshold) {
      if (source == null)
        return Color.argb(255,255,255,255);

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
      if (maxBin < 0)
        return Color.argb(255,255,255,255);

      // Return a color with the average hue/saturation/value of the bin with the most colors.
      hsv[0] = sumHue[maxBin]/colorBins[maxBin];
      hsv[1] = sumSat[maxBin]/colorBins[maxBin];
      hsv[2] = sumVal[maxBin]/colorBins[maxBin];
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

    public static int getResource(Uri resourceUrl, int defaultIcon) {
        int icon = defaultIcon;

        final String scheme = resourceUrl.getScheme();
        if ("drawable".equals(scheme)) {
            String resource = resourceUrl.getSchemeSpecificPart();
            resource = resource.substring(resource.lastIndexOf('/') + 1);

            try {
                return Integer.parseInt(resource);
            } catch(NumberFormatException ex) {
                // This isn't a resource id, try looking for a string
            }

            try {
                final Class<R.drawable> drawableClass = R.drawable.class;
                final Field f = drawableClass.getField(resource);
                icon = f.getInt(null);
            } catch (final NoSuchFieldException e1) {

                // just means the resource doesn't exist for fennec. Check in Android resources
                try {
                    final Class<android.R.drawable> drawableClass = android.R.drawable.class;
                    final Field f = drawableClass.getField(resource);
                    icon = f.getInt(null);
                } catch (final NoSuchFieldException e2) {
                    // This drawable doesn't seem to exist...
                } catch(Exception e3) {
                    Log.i(LOGTAG, "Exception getting drawable", e3);
                }

            } catch (Exception e4) {
              Log.i(LOGTAG, "Exception getting drawable", e4);
            }

            resourceUrl = null;
        }
        return icon;
    }
}

