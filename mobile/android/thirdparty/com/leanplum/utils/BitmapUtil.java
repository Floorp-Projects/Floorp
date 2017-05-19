/*
 * Copyright 2014, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum.utils;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.WindowManager;

import com.leanplum.internal.Log;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;

/**
 * Bitmap manipulation utilities.
 *
 * @author Martin Yanakiev, Anna Orlova
 */
public class BitmapUtil {
  // Layout_height for imageView is 192dp
  // https://android.googlesource.com/platform/frameworks/base
  // /+/6387d2f6dae27ba6e8481883325adad96d3010f4/core/res/res/layout
  // /notification_template_big_picture.xml
  private static final int BIG_PICTURE_MAX_HEIGHT_DP = 192;

  public static Bitmap getRoundedCornerBitmap(Bitmap bitmap, int pixels) {
    if (bitmap == null) {
      return null;
    }

    Bitmap output = Bitmap.createBitmap(bitmap.getWidth(), bitmap.getHeight(),
        Config.ARGB_8888);
    android.graphics.Canvas canvas = new android.graphics.Canvas(output);

    final int color = 0xff000000;
    final android.graphics.Paint paint = new android.graphics.Paint();
    final android.graphics.Rect rect = new android.graphics.Rect(0, 0,
        bitmap.getWidth(), bitmap.getHeight());
    final android.graphics.RectF rectF = new android.graphics.RectF(rect);

    paint.setAntiAlias(true);
    canvas.drawARGB(0, 0, 0, 0);
    paint.setColor(color);
    canvas.drawRoundRect(rectF, pixels, pixels, paint);

    paint.setXfermode(new android.graphics.PorterDuffXfermode(Mode.SRC_IN));
    canvas.drawBitmap(bitmap, rect, rect, paint);

    return output;
  }

  public static void stateBackgroundDarkerByPercentage(View v,
      int normalStateColor, int percentage) {
    int darker = getDarker(normalStateColor, percentage);
    stateBackground(v, normalStateColor, darker);
  }

  public static int getDarker(int color, int percentage) {
    if (percentage < 0 || percentage > 100)
      percentage = 0;
    double d = ((100 - percentage) / (double) 100);
    int a = (color >> 24) & 0xFF;
    int r = (int) (((color >> 16) & 0xFF) * d) & 0xFF;
    int g = (int) (((color >> 8) & 0xFF) * d) & 0xFF;
    int b = (int) ((color & 0xFF) * d) & 0xFF;
    a = a << 24;
    r = r << 16;
    g = g << 8;
    return a | r | g | b;
  }

  public static void stateBackground(View v, int normalStateColor,
      int pressedStateColor) {
    if (Build.VERSION.SDK_INT >= 16) {
      v.setBackground(getBackground(normalStateColor, pressedStateColor));
    } else {
      v.setBackgroundColor(normalStateColor);
    }
  }

  private static Drawable getBackground(int normalStateColor,
      int pressedStateColor) {
    StateListDrawable background = new StateListDrawable();
    int c = SizeUtil.dp10;
    float[] r = new float[] {c, c, c, c, c, c, c, c};
    RoundRectShape rr = new RoundRectShape(r, null, null);
    ShapeDrawable cd = new ShapeDrawable();
    cd.setShape(rr);
    cd.getPaint().setColor(pressedStateColor);
    background.addState(new int[] {android.R.attr.state_pressed,
        android.R.attr.state_focused}, cd);
    background.addState(new int[] {-android.R.attr.state_pressed,
        android.R.attr.state_focused}, cd);
    background.addState(new int[] {android.R.attr.state_pressed,
        -android.R.attr.state_focused}, cd);
    ShapeDrawable cd1 = new ShapeDrawable();
    cd1.setShape(rr);
    cd1.getPaint().setColor(normalStateColor);
    background.addState(new int[] {-android.R.attr.state_pressed,
        -android.R.attr.state_focused}, cd1);
    return background;
  }

  /**
   * Method to calculate a sample size value that is a power of two based on a target width and
   * height. From official Android documentation:
   * https://developer.android.com/training/displaying-bitmaps/load-bitmap.html
   *
   * @param reqWidth The requested width of the image.
   * @param reqHeight The requested height of the image.
   * @return The calculated inSampleSize - power of two.
   */
  private static int calculateInSampleSize(BitmapFactory.Options options, int reqWidth,
      int reqHeight) {
    // Raw height and width of image.
    final int height = options.outHeight;
    final int width = options.outWidth;
    int inSampleSize = 1;

    if (height > reqHeight || width > reqWidth) {
      final int halfHeight = height / 2;
      final int halfWidth = width / 2;

      // Calculate the largest inSampleSize value that is a power of 2 and keeps both
      // height and width larger than the requested height and width.
      while ((halfHeight / inSampleSize) >= reqHeight
          && (halfWidth / inSampleSize) >= reqWidth) {
        inSampleSize *= 2;
      }
    }

    return inSampleSize;
  }

  /**
   * Download a scaled bitmap.
   *
   * @param imageUrl The string of URL image.
   * @param width The requested width of the image.
   * @param height The requested height of the image.
   * @return The scaled bitmap downloaded form URL.
   */
  private static Bitmap getBitmapFromUrl(String imageUrl, int width, int height) {
    InputStream input = null;
    try {
      input = new URL(imageUrl).openStream();

      // First decode with inJustDecodeBounds=true to check dimensions.
      final BitmapFactory.Options options = new BitmapFactory.Options();
      options.inJustDecodeBounds = true;
      BitmapFactory.decodeStream(input, null, options);

      closeStream(input);

      input = new URL(imageUrl).openStream();
      options.inSampleSize = calculateInSampleSize(options, width, height);
      // Decode bitmap with inSampleSize set.
      options.inJustDecodeBounds = false;
      return BitmapFactory.decodeStream(input, null, options);
    } catch (IOException e) {
      Log.e(String.format("IOException in image download for URL: %s.", imageUrl), e);
      return null;
    } finally {
      closeStream(input);
    }
  }

  /**
   * Create a scaled bitmap.
   *
   * @param context The application context.
   * @param imageUrl The string of URL image.
   * @return The scaled bitmap.
   */
  public static Bitmap getScaledBitmap(Context context, String imageUrl) {
    // Processing an image depending on the current screen size to avoid central crop if the image
    // ratio is more than 2:1. Google aspect ~2:1 - page 78
    // http://commondatastorage.googleapis.com/io2012/presentations/live%20to%20website/105.pdf
    WindowManager windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
    DisplayMetrics displayMetrics = new DisplayMetrics();
    windowManager.getDefaultDisplay().getMetrics(displayMetrics);

    int pixelsHeight = Math.round(displayMetrics.density * BIG_PICTURE_MAX_HEIGHT_DP + 0.5f);
    int pixelsWidth = Math.min(2 * pixelsHeight, displayMetrics.widthPixels);

    Bitmap bitmap = getBitmapFromUrl(imageUrl, pixelsWidth, pixelsHeight);
    try {
      bitmap = Bitmap.createScaledBitmap(bitmap, pixelsWidth, pixelsHeight, true);
    } catch (Exception e) {
      Log.e("Failed on scale image " + imageUrl + " to (" + pixelsWidth + ", " + pixelsHeight + ")",
          e);
    }

    return bitmap;
  }

  /**
   * Method to close InputStream.
   *
   * @param inputStream The InputStream which must be closed.
   */
  private static void closeStream(InputStream inputStream) {
    try {
      if (inputStream != null) {
        inputStream.close();
      }
    } catch (IOException e) {
      Log.e("IOException during closing of image download stream.", e);
    }
  }
}
