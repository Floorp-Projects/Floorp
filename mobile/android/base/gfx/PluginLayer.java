/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.PixelFormat;
import android.view.View;
import android.view.Surface;
import android.view.SurfaceView;
import android.widget.AbsoluteLayout;
import android.util.Log;
import android.opengl.GLES20;
import java.nio.FloatBuffer;
import java.util.Map;
import org.json.JSONArray;

import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.SurfaceBits;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;

public class PluginLayer extends TileLayer
{
    private static final String LOGTAG = "PluginLayer";
    private static final String PREF_PLUGIN_USE_PLACEHOLDER = "plugins.use_placeholder";

    private static boolean sUsePlaceholder = true;

    private View mView;
    private SurfaceView mSurfaceView;
    private PluginLayoutParams mLayoutParams;
    private AbsoluteLayout mContainer;

    private boolean mViewVisible;
    private boolean mShowPlaceholder;
    private boolean mDestroyed;

    private RectF mLastViewport;
    private float mLastZoomFactor;

    private ShowViewRunnable mShowViewRunnable;

    private static final float TEXTURE_MAP[] = {
                0.0f, 1.0f, // top left
                0.0f, 0.0f, // bottom left
                1.0f, 1.0f, // top right
                1.0f, 0.0f, // bottom right
    };

    public PluginLayer(View view, Rect rect, int maxDimension) {
        super(new BufferedCairoImage(null, 0, 0, 0), TileLayer.PaintMode.NORMAL);

        mView = view;
        mContainer = GeckoApp.mAppContext.getPluginContainer();
        mShowViewRunnable = new ShowViewRunnable(this);

        mView.setWillNotDraw(false);
        if (mView instanceof SurfaceView) {
            mSurfaceView = (SurfaceView)view;
            mSurfaceView.setZOrderOnTop(false);
            mSurfaceView.setZOrderMediaOverlay(true);
        }

        mLayoutParams = new PluginLayoutParams(rect, maxDimension);
    }

    static void addPrefNames(JSONArray prefs) {
        prefs.put(PREF_PLUGIN_USE_PLACEHOLDER);
    }

    static boolean setUsePlaceholder(Map<String, Integer> prefs) {
        Integer usePlaceholder = prefs.get(PREF_PLUGIN_USE_PLACEHOLDER);
        if (usePlaceholder == null) {
            return false;
        }

        sUsePlaceholder = (int)usePlaceholder == 1 ? true : false;
        Log.i(LOGTAG, "Using plugin placeholder: " + sUsePlaceholder);
        return true;
    }

    public void setVisible(boolean newVisible) {
        if (newVisible && !mShowPlaceholder) {
            showView();
        } else {
            hideView();
        }
    }

    private void hideView() {
        if (mViewVisible) {
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    mView.setVisibility(View.GONE);
                }
            });

            mViewVisible = false;
        }
    }

    private void suspendView() {
        // Right now we can only show a placeholder ("suspend") a 
        // SurfaceView plugin (Flash)
        if (mSurfaceView != null) {
            hideView();

            GeckoApp.mAppContext.mMainHandler.removeCallbacks(mShowViewRunnable);
            GeckoApp.mAppContext.mMainHandler.postDelayed(mShowViewRunnable, 250);
        }
    }

    public void updateView() {
        showView(true);
    }

    public void showView() {
        showView(false);
    }

    public void showView(boolean forceUpdate) {
        if (!mViewVisible || forceUpdate) {
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    if (mContainer.indexOfChild(mView) < 0) {
                        mContainer.addView(mView, mLayoutParams);
                    } else {
                        mContainer.updateViewLayout(mView, mLayoutParams);
                        mView.setVisibility(View.VISIBLE);
                    }
                }
            });
            mViewVisible = true;
            mShowPlaceholder = false;
        }
    }

    public void destroy() {
        mDestroyed = true;

        mContainer.removeView(mView);
        GeckoApp.mAppContext.mMainHandler.removeCallbacks(mShowViewRunnable);
    }

    public void reset(Rect rect) {
        mLayoutParams.reset(rect);
    }

    @Override
    protected void performUpdates(RenderContext context) {
        if (mDestroyed)
            return;

        if (!RectUtils.fuzzyEquals(context.viewport, mLastViewport) ||
            !FloatUtils.fuzzyEquals(context.zoomFactor, mLastZoomFactor)) {

            // Viewport has changed from the last update
            
            // Attempt to figure out if this is a full page plugin or near to it. If so, we don't show the placeholder because
            // it just performs badly (flickering).
            boolean fullPagePlugin = (mLayoutParams.width >= (context.viewport.width() * 0.90f) ||
                                      mLayoutParams.height >= (context.viewport.height() * 0.90f));

            if (!fullPagePlugin && mLastViewport != null && mSurfaceView != null && !mShowPlaceholder && sUsePlaceholder) {
                // We have a SurfaceView that we can snapshot for a placeholder, and we are
                // not currently showing a placeholder.

                Surface surface = mSurfaceView.getHolder().getSurface();
                SurfaceBits bits = GeckoAppShell.getSurfaceBits(surface);
                if (bits != null) {
                    int cairoFormat = -1;
                    switch (bits.format) {
                    case PixelFormat.RGBA_8888:
                        cairoFormat = CairoImage.FORMAT_ARGB32;
                        break;
                    case PixelFormat.RGB_565:
                        cairoFormat = CairoImage.FORMAT_RGB16_565;
                        break;
                    default:
                        Log.w(LOGTAG, "Unable to handle format " + bits.format);
                        break;
                    }

                    if (cairoFormat >= 0) {
                        BufferedCairoImage image = (BufferedCairoImage)mImage;
                        image.setBuffer(bits.buffer, bits.width, bits.height, cairoFormat);

                        mPosition = new Rect(mLayoutParams.x, mLayoutParams.y, mLayoutParams.x + bits.width, mLayoutParams.y + bits.height);
                        mPosition.offset(Math.round(mLastViewport.left), Math.round(mLastViewport.top));

                        mResolution = mLastZoomFactor;

                        // We've uploaded the snapshot to the texture now (or will in super.performUpdates)
                        // so we can draw the placeholder
                        mShowPlaceholder = true;
                        super.performUpdates(context);
                    }
                }
            }

            mLastZoomFactor = context.zoomFactor;
            mLastViewport = context.viewport;
            mLayoutParams.reposition(context.viewport, context.zoomFactor);

            if (mShowPlaceholder) {
                suspendView();
            } else {
                // We aren't showing the placeholder so we need to update the view position immediately
                updateView();
            }
        }
    }

    @Override
    public void draw(RenderContext context) {
        if (!mShowPlaceholder || mDestroyed || !initialized())
            return;

        RectF bounds;
        Rect position = getPosition();
        RectF viewport = context.viewport;

        bounds = getBounds(context);

        float height = mLayoutParams.height;
        float left = bounds.left - viewport.left;
        float top = viewport.height() - (bounds.top + height - viewport.top);

        float[] coords = {
            //x, y, z, texture_x, texture_y
            left/viewport.width(), top/viewport.height(), 0,
            0.0f, mLayoutParams.height / bounds.height(),

            left/viewport.width(), (top+height)/viewport.height(), 0,
            0.0f, 0.0f,

            (left+mLayoutParams.width)/viewport.width(), top/viewport.height(), 0,
            mLayoutParams.width / bounds.width(), mLayoutParams.height / bounds.height(),

            (left+mLayoutParams.width)/viewport.width(), (top+height)/viewport.height(), 0,
            mLayoutParams.width / bounds.width(), 0.0f
        };

        FloatBuffer coordBuffer = context.coordBuffer;
        int positionHandle = context.positionHandle;
        int textureHandle = context.textureHandle;

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, getTextureID());

        // Make sure we are at position zero in the buffer
        coordBuffer.position(0);
        coordBuffer.put(coords);

        // Unbind any the current array buffer so we can use client side buffers
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);

        // Vertex coordinates are x,y,z starting at position 0 into the buffer.
        coordBuffer.position(0);
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 20, coordBuffer);

        // Texture coordinates are texture_x, texture_y starting at position 3 into the buffer.
        coordBuffer.position(3);
        GLES20.glVertexAttribPointer(textureHandle, 2, GLES20.GL_FLOAT, false, 20, coordBuffer);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    class PluginLayoutParams extends AbsoluteLayout.LayoutParams
    {
        private static final String LOGTAG = "GeckoApp.PluginLayoutParams";

        private RectF mRect;
        private int mMaxDimension;
        private float mLastResolution;

        public PluginLayoutParams(Rect rect, int maxDimension) {
            super(0, 0, 0, 0);

            mMaxDimension = maxDimension;
            reset(rect);
        }

        private void clampToMaxSize() {
            if (width > mMaxDimension || height > mMaxDimension) {
                if (width > height) {
                    height = Math.round(((float)height/(float)width) * mMaxDimension);
                    width = mMaxDimension;
                } else {
                    width = Math.round(((float)width/(float)height) * mMaxDimension);
                    height = mMaxDimension;
                }
            }
        }

        public void reset(Rect rect) {
            mRect = new RectF(rect);
        }

        public void reposition(RectF viewport, float zoomFactor) {

            RectF scaled = RectUtils.scale(mRect, zoomFactor);

            this.x = Math.round(scaled.left - viewport.left);
            this.y = Math.round(scaled.top - viewport.top);

            if (!FloatUtils.fuzzyEquals(mLastResolution, zoomFactor)) {
                width = Math.round(mRect.width() * zoomFactor);
                height = Math.round(mRect.height() * zoomFactor);
                mLastResolution = zoomFactor;

                clampToMaxSize();
            }
        }
    }

    class ShowViewRunnable implements Runnable {

        private PluginLayer mLayer;

        public ShowViewRunnable(PluginLayer layer) {
            mLayer = layer;
        }

        public void run() {
            mLayer.showView();
        }
    }
}
