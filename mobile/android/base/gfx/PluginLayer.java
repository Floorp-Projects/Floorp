/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.Rect;
import android.graphics.RectF;
import android.view.SurfaceView;
import android.view.View;
import android.widget.AbsoluteLayout;

public class PluginLayer extends TileLayer {
    private static final String LOGTAG = "PluginLayer";

    private View mView;
    private SurfaceView mSurfaceView;
    private PluginLayoutParams mLayoutParams;
    private AbsoluteLayout mContainer;

    private boolean mDestroyed;
    private boolean mViewVisible;

    private RectF mLastViewport;
    private float mLastZoomFactor;

    private static final float TEXTURE_MAP[] = {
                0.0f, 1.0f, // top left
                0.0f, 0.0f, // bottom left
                1.0f, 1.0f, // top right
                1.0f, 0.0f, // bottom right
    };

    public PluginLayer(View view, RectF rect, int maxDimension) {
        super(new BufferedCairoImage(null, 0, 0, 0), TileLayer.PaintMode.NORMAL);

        mView = view;
        mContainer = GeckoAppShell.getGeckoInterface().getPluginContainer();

        mView.setWillNotDraw(false);
        if (mView instanceof SurfaceView) {
            mSurfaceView = (SurfaceView)view;
            mSurfaceView.setZOrderOnTop(false);
            mSurfaceView.setZOrderMediaOverlay(true);
        }

        mLayoutParams = new PluginLayoutParams(rect, maxDimension);
    }

    public void setVisible(boolean visible) {
        if (visible) {
            showView();
        } else {
            hideView();
        }
    }

    private void hideView() {
        if (mViewVisible) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    mView.setVisibility(View.GONE);
                    mViewVisible = false;
                }
            });
        }
    }

    public void showView() {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (mContainer.indexOfChild(mView) < 0) {
                    mContainer.addView(mView, mLayoutParams);
                } else {
                    mContainer.updateViewLayout(mView, mLayoutParams);
                    mView.setVisibility(View.VISIBLE);
                }
                mViewVisible = true;
            }
        });
    }

    @Override
    public void destroy() {
        mDestroyed = true;

        mContainer.removeView(mView);
    }

    public void reset(RectF rect) {
        mLayoutParams.reset(rect);
    }

    @Override
    protected void performUpdates(RenderContext context) {
        if (mDestroyed)
            return;

        if (!RectUtils.fuzzyEquals(context.viewport, mLastViewport) ||
            !FloatUtils.fuzzyEquals(context.zoomFactor, mLastZoomFactor)) {

            mLastZoomFactor = context.zoomFactor;
            mLastViewport = context.viewport;
            mLayoutParams.reposition(context.viewport, context.zoomFactor);

            showView();
        }
    }

    @Override
    public void draw(RenderContext context) {
    }

    class PluginLayoutParams extends AbsoluteLayout.LayoutParams
    {
        private static final String LOGTAG = "GeckoApp.PluginLayoutParams";

        private RectF mRect;
        private int mMaxDimension;
        private float mLastResolution;

        public PluginLayoutParams(RectF rect, int maxDimension) {
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

        public void reset(RectF rect) {
            mRect = rect;
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
}
