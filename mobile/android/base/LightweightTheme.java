/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONObject;

import android.app.Application;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewParent;

import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

public class LightweightTheme implements GeckoEventListener {
    private static final String LOGTAG = "GeckoLightweightTheme";

    private Application mApplication;
    private Handler mHandler;

    private Bitmap mBitmap;
    private int mColor;
    private boolean mIsLight;

    public static interface OnChangeListener {
        // The View should change its background/text color. 
        public void onLightweightThemeChanged();

        // The View should reset to its default background/text color.
        public void onLightweightThemeReset();
    }

    private List<OnChangeListener> mListeners;
    
    public LightweightTheme(Application application) {
        mApplication = application;
        mHandler = new Handler(Looper.getMainLooper());
        mListeners = new ArrayList<OnChangeListener>();

        // unregister isn't needed as the lifetime is same as the application.
        GeckoAppShell.getEventDispatcher().registerEventListener("LightweightTheme:Update", this);
        GeckoAppShell.getEventDispatcher().registerEventListener("LightweightTheme:Disable", this);
    }

    public void addListener(final OnChangeListener listener) {
        // Don't inform the listeners that attached late.
        // Their onLayout() will take care of them before their onDraw();
        mListeners.add(listener);
    }

    public void removeListener(OnChangeListener listener) {
        mListeners.remove(listener);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("LightweightTheme:Update")) {
                JSONObject lightweightTheme = message.getJSONObject("data");
                String headerURL = lightweightTheme.getString("headerURL"); 
                int mark = headerURL.indexOf('?');
                if (mark != -1)
                    headerURL = headerURL.substring(0, mark);
                try {
                    // Get the image and convert it to a bitmap.
                    URL url = new URL(headerURL);
                    InputStream stream = url.openStream();
                    final Bitmap bitmap = BitmapFactory.decodeStream(stream);
                    stream.close();
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            setLightweightTheme(bitmap);
                        }
                    });
                } catch(MalformedURLException e) {
                } catch(IOException e) {
                }
            } else if (event.equals("LightweightTheme:Disable")) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        resetLightweightTheme();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    /**
     * Set a new lightweight theme with the given bitmap.
     * Note: This should be called on the UI thread to restrict accessing the
     * bitmap to a single thread.
     *
     * @param bitmap The bitmap used for the lightweight theme.
     */
    private void setLightweightTheme(Bitmap bitmap) {
        if (bitmap == null || bitmap.getWidth() == 0 || bitmap.getHeight() == 0) {
            mBitmap = null;
            return;
        }

        // To find the dominant color only once, take the bottom 25% of pixels.
        DisplayMetrics dm = mApplication.getResources().getDisplayMetrics();
        int maxWidth = Math.max(dm.widthPixels, dm.heightPixels);
        int height = (int) (bitmap.getHeight() * 0.25);

        // The lightweight theme image's width and height.
        int bitmapWidth = bitmap.getWidth();
        int bitmapHeight = bitmap.getHeight();

        // A cropped bitmap of the bottom 25% of pixels.
        Bitmap cropped = Bitmap.createBitmap(bitmap,
                                             bitmapWidth > maxWidth ? bitmapWidth - maxWidth : 0,
                                             bitmapHeight - height, 
                                             bitmapWidth > maxWidth ? maxWidth : bitmapWidth,
                                             height);

        // Dominant color based on the cropped bitmap.
        mColor = BitmapUtils.getDominantColor(cropped, false);

        // Calculate the luminance to determine if it's a light or a dark theme.
        double luminance = (0.2125 * ((mColor & 0x00FF0000) >> 16)) + 
                           (0.7154 * ((mColor & 0x0000FF00) >> 8)) + 
                           (0.0721 * (mColor &0x000000FF));
        mIsLight = (luminance > 110) ? true : false;

        // The bitmap image might be smaller than the device's width.
        // If it's smaller, fill the extra space on the left with the dominant color.
        if (bitmap.getWidth() >= maxWidth) {
            mBitmap = bitmap;
        } else {
            Paint paint = new Paint();
            paint.setAntiAlias(true);

            // Create a bigger image that can fill the device width.
            // By creating a canvas for the bitmap, anything drawn on the canvas
            // will be drawn on the bitmap.
            mBitmap = Bitmap.createBitmap(maxWidth, bitmapHeight, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(mBitmap);

            // Fill the canvas with dominant color.
            canvas.drawColor(mColor);

            // The image should be top-right aligned.
            Rect rect = new Rect();
            Gravity.apply(Gravity.TOP | Gravity.RIGHT,
                          bitmapWidth,
                          bitmapHeight,
                          new Rect(0, 0, maxWidth, bitmapHeight),
                          rect);

            // Draw the bitmap.
            canvas.drawBitmap(bitmap, null, rect, paint);
        }

        for (OnChangeListener listener : mListeners)
            listener.onLightweightThemeChanged();
    }

    /**
     * Reset the lightweight theme.
     * Note: This should be called on the UI thread to restrict accessing the
     * bitmap to a single thread.
     */
    private void resetLightweightTheme() {
        if (mBitmap != null) {
            // Reset the bitmap.
            mBitmap = null;

            for (OnChangeListener listener : mListeners)
                listener.onLightweightThemeReset();
        }
    }

    /**
     * A lightweight theme is enabled only if there is an active bitmap.
     *
     * @return True if the theme is enabled.
     */
    public boolean isEnabled() {
        return (mBitmap != null);
    }

    /**
     * Based on the luminance of the domanint color, a theme is classified as light or dark.
     *
     * @return True if the theme is light.
     */
    public boolean isLightTheme() {
        return mIsLight;
    }

    /**
     * Crop the image based on the position of the view on the window.
     * Either the View or one of its ancestors might have scrolled or translated.
     * This value should be taken into account while mapping the View to the Bitmap.
     *
     * @param view The view requesting a cropped bitmap.
     */
    private Bitmap getCroppedBitmap(View view) {
        if (mBitmap == null || view == null)
            return null;

        // Get the global position of the view on the entire screen.
        Rect rect = new Rect();
        view.getGlobalVisibleRect(rect);

        // Get the activity's window position. This does an IPC call, may be expensive.
        Rect window = new Rect();
        view.getWindowVisibleDisplayFrame(window);

        // Calculate the coordinates for the cropped bitmap.
        int screenWidth = view.getContext().getResources().getDisplayMetrics().widthPixels;
        int left = mBitmap.getWidth() - screenWidth + rect.left;
        int right = mBitmap.getWidth() - screenWidth + rect.right;
        int top = rect.top - window.top;
        int bottom = rect.bottom - window.top;

        int offsetX = 0;
        int offsetY = 0;

        // Find if this view or any of its ancestors has been translated or scrolled.
        ViewParent parent;
        View curView = view;
        do {
            if (Build.VERSION.SDK_INT >= 11) {
                offsetX += (int) curView.getTranslationX() - curView.getScrollX();
                offsetY += (int) curView.getTranslationY() - curView.getScrollY();
            } else {
                offsetX -= curView.getScrollX();
                offsetY -= curView.getScrollY();
            }

            parent = curView.getParent();

            if (parent instanceof View)
                curView = (View) parent;

        } while(parent instanceof View && parent != null);

        // Adjust the coordinates for the offset.
        left -= offsetX;
        right -= offsetX;
        top -= offsetY;
        bottom -= offsetY;

        // The either the required height may be less than the available image height or more than it.
        // If the height required is more, crop only the available portion on the image.
        int width = right - left;
        int height = (bottom > mBitmap.getHeight() ? mBitmap.getHeight() - top : bottom - top);

        // There is a chance that the view is not visible or doesn't fall within the phone's size.
        // In this case, 'rect' will have all values as '0'. Hence 'top' and 'bottom' may be negative,
        // and createBitmap() will fail.
        // The view will get a background in its next layout pass.
        try {
            return Bitmap.createBitmap(mBitmap, left, top, width, height);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Converts the cropped bitmap to a BitmapDrawable and returns the same.
     *
     * @param view The view for which a background drawable is required.
     * @return Either the cropped bitmap as a Drawable or null.
     */
    public Drawable getDrawable(View view) {
        Bitmap bitmap = getCroppedBitmap(view);
        if (bitmap == null)
            return null;

        BitmapDrawable drawable = new BitmapDrawable(view.getContext().getResources(), bitmap);
        drawable.setGravity(Gravity.TOP|Gravity.RIGHT);
        drawable.setTileModeXY(Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
        return drawable;
    }

    /**
     * Converts the cropped bitmap to a LightweightThemeDrawable, placing it over the dominant color.
     *
     * @param view The view for which a background drawable is required.
     * @return Either the cropped bitmap as a Drawable or null.
     */
     public LightweightThemeDrawable getColorDrawable(View view) {
         return getColorDrawable(view, mColor, false);
     }

    /**
     * Converts the cropped bitmap to a LightweightThemeDrawable, placing it over the required color.
     *
     * @param view The view for which a background drawable is required.
     * @param color The color over which the drawable should be drawn.
     * @return Either the cropped bitmap as a Drawable or null.
     */
    public LightweightThemeDrawable getColorDrawable(View view, int color) {
        return getColorDrawable(view, color, false);
    }

    /**
     * Converts the cropped bitmap to a LightweightThemeDrawable, placing it over the required color.
     *
     * @param view The view for which a background drawable is required.
     * @param color The color over which the drawable should be drawn.
     * @param needsDominantColor A layer of dominant color is needed or not.
     * @return Either the cropped bitmap as a Drawable or null.
     */
    public LightweightThemeDrawable getColorDrawable(View view, int color, boolean needsDominantColor) {
        Bitmap bitmap = getCroppedBitmap(view);
        if (bitmap == null)
            return null;

        LightweightThemeDrawable drawable = new LightweightThemeDrawable(view.getContext().getResources(), bitmap);
        if (needsDominantColor)
            drawable.setColorWithFilter(color, (mColor & 0x22FFFFFF));
        else
            drawable.setColor(color);

        return drawable;
    }
}
