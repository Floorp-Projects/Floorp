/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PanZoomController;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.OvershootInterpolator;
import android.view.animation.ScaleAnimation;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.nio.ByteBuffer;
import java.text.DecimalFormat;

public class ZoomedView extends FrameLayout implements LayerView.DynamicToolbarListener,
        LayerView.ZoomedViewListener, GeckoEventListener {
    private static final String LOGTAG = "Gecko" + ZoomedView.class.getSimpleName();

    private static final float[] ZOOM_FACTORS_LIST = {2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 1.5f};
    private static final int W_CAPTURED_VIEW_IN_PERCENT = 50;
    private static final int H_CAPTURED_VIEW_IN_PERCENT = 50;
    private static final int MINIMUM_DELAY_BETWEEN_TWO_RENDER_CALLS_NS = 1000000;
    private static final int DELAY_BEFORE_NEXT_RENDER_REQUEST_MS = 2000;
    private static final int OPENING_ANIMATION_DURATION_MS = 250;
    private static final int CLOSING_ANIMATION_DURATION_MS = 150;
    private static final float OVERSHOOT_INTERPOLATOR_TENSION = 1.5f;

    private float zoomFactor;
    private int currentZoomFactorIndex;
    private boolean isSimplifiedUI;
    private int defaultZoomFactor;
    private int prefDefaultZoomFactorObserverId;
    private int prefSimplifiedUIObserverId;

    private ImageView zoomedImageView;
    private LayerView layerView;
    private int viewWidth;
    private int viewHeight; // Only the zoomed view height, no toolbar, no shadow ...
    private int viewContainerWidth;
    private int viewContainerHeight; // Zoomed view height with toolbar and other elements like shadow, ...
    private int containterSize; // shadow, margin, ...
    private Point lastPosition;
    private boolean shouldSetVisibleOnUpdate;
    private boolean isBlockedFromAppearing; // Prevent the display of the zoomedview while FormAssistantPopup is visible
    private PointF returnValue;
    private final PointF animationStart;
    private ImageView closeButton;
    private TextView changeZoomFactorButton;
    private boolean toolbarOnTop;
    private float offsetDueToToolBarPosition;
    private int toolbarHeight;
    private int cornerRadius;
    private float dynamicToolbarOverlap;

    private boolean stopUpdateView;

    private int lastOrientation;

    private ByteBuffer buffer;
    private Runnable requestRenderRunnable;
    private long startTimeReRender;
    private long lastStartTimeReRender;

    private ZoomedViewTouchListener touchListener;

    private enum StartPointUpdate {
        GECKO_POSITION, CENTER, NO_CHANGE
    }

    private class RoundedBitmapDrawable extends BitmapDrawable {
        private Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        final float cornerRadius;
        final boolean squareOnTopOfDrawable;

        RoundedBitmapDrawable(Resources res, Bitmap bitmap, boolean squareOnTop, int radius) {
            super(res, bitmap);
            squareOnTopOfDrawable = squareOnTop;
            final BitmapShader shader = new BitmapShader(bitmap, Shader.TileMode.CLAMP,
                Shader.TileMode.CLAMP);
            paint.setAntiAlias(true);
            paint.setShader(shader);
            cornerRadius = radius;
        }

        @Override
        public void draw(Canvas canvas) {
            int height = getBounds().height();
            int width = getBounds().width();
            RectF rect = new RectF(0.0f, 0.0f, width, height);
            canvas.drawRoundRect(rect, cornerRadius, cornerRadius, paint);

            //draw rectangles over the corners we want to be square
            if (squareOnTopOfDrawable) {
                canvas.drawRect(0, 0, cornerRadius, cornerRadius, paint);
                canvas.drawRect(width - cornerRadius, 0, width, cornerRadius, paint);
            } else {
                canvas.drawRect(0, height - cornerRadius, cornerRadius, height, paint);
                canvas.drawRect(width - cornerRadius, height - cornerRadius, width, height, paint);
            }
        }
    }

    private class ZoomedViewTouchListener implements View.OnTouchListener {
        private float originRawX;
        private float originRawY;
        private boolean dragged;
        private MotionEvent actionDownEvent;

        @Override
        public boolean onTouch(View view, MotionEvent event) {
            if (layerView == null) {
                return false;
            }

            switch (event.getAction()) {
            case MotionEvent.ACTION_MOVE:
                if (moveZoomedView(event)) {
                    dragged = true;
                }
                break;

            case MotionEvent.ACTION_UP:
                if (dragged) {
                    dragged = false;
                } else {
                    if (isClickInZoomedView(event.getY())) {
                        GeckoEvent eClickInZoomedView = GeckoEvent.createBroadcastEvent("Gesture:ClickInZoomedView", "");
                        GeckoAppShell.sendEventToGecko(eClickInZoomedView);
                        layerView.dispatchTouchEvent(actionDownEvent);
                        actionDownEvent.recycle();
                        PointF convertedPosition = getUnzoomedPositionFromPointInZoomedView(event.getX(), event.getY());
                        // the LayerView expects the coordinates relative to the window, not the surface, so we need
                        // to adjust that here.
                        convertedPosition.y += layerView.getSurfaceTranslation();
                        MotionEvent e = MotionEvent.obtain(event.getDownTime(), event.getEventTime(),
                                MotionEvent.ACTION_UP, convertedPosition.x, convertedPosition.y,
                                event.getMetaState());
                        layerView.dispatchTouchEvent(e);
                        e.recycle();
                    }
                }
                break;

            case MotionEvent.ACTION_DOWN:
                dragged = false;
                originRawX = event.getRawX();
                originRawY = event.getRawY();
                PointF convertedPosition = getUnzoomedPositionFromPointInZoomedView(event.getX(), event.getY());
                // the LayerView expects the coordinates relative to the window, not the surface, so we need
                // to adjust that here.
                convertedPosition.y += layerView.getSurfaceTranslation();
                actionDownEvent = MotionEvent.obtain(event.getDownTime(), event.getEventTime(),
                        MotionEvent.ACTION_DOWN, convertedPosition.x, convertedPosition.y,
                        event.getMetaState());
                break;
            }
            return true;
        }

        private boolean isClickInZoomedView(float y) {
            return ((toolbarOnTop && y > toolbarHeight) ||
                (!toolbarOnTop && y < ZoomedView.this.viewHeight));
        }

        private boolean moveZoomedView(MotionEvent event) {
            RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) ZoomedView.this.getLayoutParams();
            if ((!dragged) && (Math.abs((int) (event.getRawX() - originRawX)) < PanZoomController.CLICK_THRESHOLD)
                    && (Math.abs((int) (event.getRawY() - originRawY)) < PanZoomController.CLICK_THRESHOLD)) {
                // When the user just touches the screen ACTION_MOVE can be detected for a very small delta on position.
                // In this case, the move is ignored if the delta is lower than 1 unit.
                return false;
            }

            float newLeftMargin = params.leftMargin + event.getRawX() - originRawX;
            float newTopMargin = params.topMargin + event.getRawY() - originRawY;
            ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
            ZoomedView.this.moveZoomedView(metrics, newLeftMargin, newTopMargin, StartPointUpdate.CENTER);
            originRawX = event.getRawX();
            originRawY = event.getRawY();
            return true;
        }
    }

    public ZoomedView(Context context) {
        this(context, null, 0);
    }

    public ZoomedView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ZoomedView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        isSimplifiedUI = true;
        isBlockedFromAppearing = false;
        getPrefs();
        currentZoomFactorIndex = 0;
        returnValue = new PointF();
        animationStart = new PointF();
        requestRenderRunnable = new Runnable() {
            @Override
            public void run() {
                requestZoomedViewRender();
            }
        };
        touchListener = new ZoomedViewTouchListener();
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
                "Gesture:clusteredLinksClicked", "Window:Resize", "Content:LocationChange",
                "Gesture:CloseZoomedView", "Browser:ZoomToPageWidth", "Browser:ZoomToRect",
                "FormAssist:AutoComplete", "FormAssist:Hide");
    }

    void destroy() {
        PrefsHelper.removeObserver(prefDefaultZoomFactorObserverId);
        PrefsHelper.removeObserver(prefSimplifiedUIObserverId);
        ThreadUtils.removeCallbacksFromUiThread(requestRenderRunnable);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
                "Gesture:clusteredLinksClicked", "Window:Resize", "Content:LocationChange",
                "Gesture:CloseZoomedView", "Browser:ZoomToPageWidth", "Browser:ZoomToRect",
                "FormAssist:AutoComplete", "FormAssist:Hide");
    }

    // This method (onFinishInflate) is called only when the zoomed view class is used inside
    // an xml structure <org.mozilla.gecko.ZoomedView ...
    // It won't be called if the class is used from java code like "new  ZoomedView(context);"
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        closeButton = (ImageView) findViewById(R.id.dialog_close);
        changeZoomFactorButton = (TextView) findViewById(R.id.change_zoom_factor);
        zoomedImageView = (ImageView) findViewById(R.id.zoomed_image_view);

        setTextInZoomFactorButton(zoomFactor);

        toolbarHeight = getResources().getDimensionPixelSize(R.dimen.zoomed_view_toolbar_height);
        containterSize = getResources().getDimensionPixelSize(R.dimen.drawable_dropshadow_size);
        cornerRadius = getResources().getDimensionPixelSize(R.dimen.standard_corner_radius);

        moveToolbar(true);
    }

    private void setListeners() {
        closeButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                stopZoomDisplay(true);
            }
        });

        changeZoomFactorButton.setOnTouchListener(new  OnTouchListener() {
            public boolean onTouch(View v, MotionEvent event) {

                if (event.getAction() == MotionEvent.ACTION_UP) {
                    if (event.getX() >= (changeZoomFactorButton.getLeft() + changeZoomFactorButton.getWidth() / 2)) {
                        changeZoomFactor(true);
                    } else {
                        changeZoomFactor(false);
                    }
                }
                return true;
            }
        });

        setOnTouchListener(touchListener);
    }

    private void removeListeners() {
        closeButton.setOnClickListener(null);

        changeZoomFactorButton.setOnTouchListener(null);

        setOnTouchListener(null);
    }
    /*
     * Convert a click from ZoomedView. Return the position of the click in the
     * LayerView
     */
    private PointF getUnzoomedPositionFromPointInZoomedView(float x, float y) {
        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        final float parentWidth = metrics.getWidth();
        final float parentHeight = metrics.getHeight();
        RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) getLayoutParams();

        // The number of unzoomed content pixels that can be displayed in the
        // zoomed area.
        float visibleContentPixels = viewWidth / zoomFactor;
        // The offset in content pixels of the leftmost zoomed pixel from the
        // layerview's left edge when the zoomed view is moved to the right as
        // far as it can go.
        float maxContentOffset = parentWidth - visibleContentPixels;
        // The maximum offset in screen pixels that the zoomed view can have
        float maxZoomedViewOffset = parentWidth - viewContainerWidth;

        // The above values allow us to compute the term
        //   maxContentOffset / maxZoomedViewOffset
        // which is the number of content pixels that we should move over by
        // for every screen pixel that the zoomed view is moved over by.
        // This allows a smooth transition from when the zoomed view is at the
        // leftmost extent to when it is at the rightmost extent.

        // This is the offset in content pixels of the leftmost zoomed pixel
        // visible in the zoomed view. This value is relative to the layerview
        // edge.
        float zoomedContentOffset = ((float)params.leftMargin) * maxContentOffset / maxZoomedViewOffset;
        returnValue.x = (int)(zoomedContentOffset + (x / zoomFactor));

        // Same comments here vertically
        visibleContentPixels = viewHeight / zoomFactor;
        maxContentOffset = parentHeight - visibleContentPixels;
        maxZoomedViewOffset = parentHeight - (viewContainerHeight - toolbarHeight);
        float zoomedAreaOffset = (float)params.topMargin + offsetDueToToolBarPosition - layerView.getSurfaceTranslation();
        zoomedContentOffset = zoomedAreaOffset * maxContentOffset / maxZoomedViewOffset;
        returnValue.y = (int)(zoomedContentOffset + ((y - offsetDueToToolBarPosition) / zoomFactor));

        return returnValue;
    }

    /*
     * A touch point (x,y) occurs in LayerView, this point should be displayed
     * in the center of the zoomed view. The returned point is the position of
     * the Top-Left zoomed view point on the screen device
     */
    private PointF getZoomedViewTopLeftPositionFromTouchPosition(float x, float y) {
        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        final float parentWidth = metrics.getWidth();
        final float parentHeight = metrics.getHeight();

        // See comments in getUnzoomedPositionFromPointInZoomedView, but the
        // transformations here are largely the reverse of that function.

        float visibleContentPixels = viewWidth / zoomFactor;
        float maxContentOffset = parentWidth - visibleContentPixels;
        float maxZoomedViewOffset = parentWidth - viewContainerWidth;
        float contentPixelOffset = x - (visibleContentPixels / 2.0f);
        returnValue.x = (int)(contentPixelOffset * (maxZoomedViewOffset / maxContentOffset));

        visibleContentPixels = viewHeight / zoomFactor;
        maxContentOffset = parentHeight - visibleContentPixels;
        maxZoomedViewOffset = parentHeight - (viewContainerHeight - toolbarHeight);
        contentPixelOffset = y - (visibleContentPixels / 2.0f);
        float unscaledViewOffset = layerView.getSurfaceTranslation() - offsetDueToToolBarPosition;
        returnValue.y = (int)((contentPixelOffset * (maxZoomedViewOffset / maxContentOffset)) + unscaledViewOffset);

        return returnValue;
    }

    private void moveZoomedView(ImmutableViewportMetrics metrics, float newLeftMargin, float newTopMargin,
            StartPointUpdate animateStartPoint) {
        RelativeLayout.LayoutParams newLayoutParams = (RelativeLayout.LayoutParams) getLayoutParams();
        newLayoutParams.leftMargin = (int) newLeftMargin;
        newLayoutParams.topMargin = (int) newTopMargin;
        int topMarginMin = (int)(layerView.getSurfaceTranslation() + dynamicToolbarOverlap);
        int topMarginMax = layerView.getHeight() - viewContainerHeight;
        int leftMarginMin = 0;
        int leftMarginMax = layerView.getWidth() - viewContainerWidth;

        if (newTopMargin < topMarginMin) {
            newLayoutParams.topMargin = topMarginMin;
        } else if (newTopMargin > topMarginMax) {
            newLayoutParams.topMargin = topMarginMax;
        }

        if (newLeftMargin < leftMarginMin) {
            newLayoutParams.leftMargin = leftMarginMin;
        } else if (newLeftMargin > leftMarginMax) {
            newLayoutParams.leftMargin = leftMarginMax;
        }

        if (newLayoutParams.topMargin < topMarginMin + 1) {
            moveToolbar(false);
        } else if (newLayoutParams.topMargin > topMarginMax - 1) {
            moveToolbar(true);
        }

        if (animateStartPoint == StartPointUpdate.GECKO_POSITION) {
            // Before this point, the animationStart point is relative to the layerView.
            // The value is initialized in startZoomDisplay using the click point position coming from Gecko.
            // The position of the zoomed view is now calculated, so the position of the animation
            // can now be correctly set relative to the zoomed view
            animationStart.x = animationStart.x - newLayoutParams.leftMargin;
            animationStart.y = animationStart.y - newLayoutParams.topMargin;
        } else if (animateStartPoint == StartPointUpdate.CENTER) {
            // At this point, the animationStart point is no more valid probably because
            // the zoomed view has been moved by the user.
            // In this case, the animationStart point is set to the center point of the zoomed view.
            PointF convertedPosition = getUnzoomedPositionFromPointInZoomedView(viewContainerWidth / 2, viewContainerHeight / 2);
            animationStart.x = convertedPosition.x - newLayoutParams.leftMargin;
            animationStart.y = convertedPosition.y - newLayoutParams.topMargin;
        }

        setLayoutParams(newLayoutParams);
        PointF convertedPosition = getUnzoomedPositionFromPointInZoomedView(0, offsetDueToToolBarPosition);
        lastPosition = PointUtils.round(convertedPosition);
        requestZoomedViewRender();
    }

    private void moveToolbar(boolean moveTop) {
        if (toolbarOnTop == moveTop) {
            return;
        }
        toolbarOnTop = moveTop;
        if (toolbarOnTop) {
            offsetDueToToolBarPosition = toolbarHeight;
        } else {
            offsetDueToToolBarPosition = 0;
        }

        RelativeLayout.LayoutParams p = (RelativeLayout.LayoutParams) zoomedImageView.getLayoutParams();
        RelativeLayout.LayoutParams pChangeZoomFactorButton = (RelativeLayout.LayoutParams) changeZoomFactorButton.getLayoutParams();
        RelativeLayout.LayoutParams pCloseButton = (RelativeLayout.LayoutParams) closeButton.getLayoutParams();

        if (moveTop) {
            p.addRule(RelativeLayout.BELOW, R.id.change_zoom_factor);
            pChangeZoomFactorButton.addRule(RelativeLayout.BELOW, 0);
            pCloseButton.addRule(RelativeLayout.BELOW, 0);
        } else {
            p.addRule(RelativeLayout.BELOW, 0);
            pChangeZoomFactorButton.addRule(RelativeLayout.BELOW, R.id.zoomed_image_view);
            pCloseButton.addRule(RelativeLayout.BELOW, R.id.zoomed_image_view);
        }
        pChangeZoomFactorButton.addRule(RelativeLayout.ALIGN_LEFT, R.id.zoomed_image_view);
        pCloseButton.addRule(RelativeLayout.ALIGN_RIGHT, R.id.zoomed_image_view);
        zoomedImageView.setLayoutParams(p);
        changeZoomFactorButton.setLayoutParams(pChangeZoomFactorButton);
        closeButton.setLayoutParams(pCloseButton);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // In case of orientation change, the zoomed view update is stopped until the orientation change
        // is completed. At this time, the function onMetricsChanged is called and the
        // zoomed view update is restarted again.
        if (lastOrientation != newConfig.orientation) {
            shouldBlockUpdate(true);
            lastOrientation = newConfig.orientation;
        }
    }

    private void refreshZoomedViewSize(ImmutableViewportMetrics viewport) {
        if (layerView == null) {
            return;
        }

        RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) getLayoutParams();
        setCapturedSize(viewport);
        moveZoomedView(viewport, params.leftMargin, params.topMargin, StartPointUpdate.NO_CHANGE);
    }

    private void setCapturedSize(ImmutableViewportMetrics metrics) {
        float parentMinSize = Math.min(metrics.getWidth(), metrics.getHeight());
        viewWidth = (int) ((parentMinSize * W_CAPTURED_VIEW_IN_PERCENT / (zoomFactor * 100.0)) * zoomFactor);
        viewHeight = (int) ((parentMinSize * H_CAPTURED_VIEW_IN_PERCENT / (zoomFactor * 100.0)) * zoomFactor);
        viewContainerHeight = viewHeight + toolbarHeight +
                2 * containterSize; // Top and bottom shadows
        viewContainerWidth = viewWidth +
                2 * containterSize; // Right and left shadows
        // Display in zoomedview is corrupted when width is an odd number
        // More details about this issue here: bug 776906 comment 11
        viewWidth &= ~0x1;
    }

    private void shouldBlockUpdate(boolean shouldBlockUpdate) {
        stopUpdateView = shouldBlockUpdate;
    }

    private Bitmap.Config getBitmapConfig() {
        return (GeckoAppShell.getScreenDepth() == 24) ? Bitmap.Config.ARGB_8888 : Bitmap.Config.RGB_565;
    }

    private void getPrefs() {
        prefSimplifiedUIObserverId = PrefsHelper.getPref("ui.zoomedview.simplified", new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean simplified) {
                isSimplifiedUI = simplified;
                if (simplified) {
                    changeZoomFactorButton.setVisibility(View.INVISIBLE);
                    zoomFactor = (float) defaultZoomFactor;
                } else {
                    zoomFactor = ZOOM_FACTORS_LIST[currentZoomFactorIndex];
                    setTextInZoomFactorButton(zoomFactor);
                    changeZoomFactorButton.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public boolean isObserver() {
                return true;
            }
        });

        prefDefaultZoomFactorObserverId = PrefsHelper.getPref("ui.zoomedview.defaultZoomFactor", new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, int defaultZoomFactorFromSettings) {
                defaultZoomFactor = defaultZoomFactorFromSettings;
                if (isSimplifiedUI) {
                    zoomFactor = (float) defaultZoomFactor;
                } else {
                    zoomFactor = ZOOM_FACTORS_LIST[currentZoomFactorIndex];
                    setTextInZoomFactorButton(zoomFactor);
                }
            }

            @Override
            public boolean isObserver() {
                return true;
            }
        });
    }

    private void startZoomDisplay(LayerView aLayerView, final int leftFromGecko, final int topFromGecko) {
        if (isBlockedFromAppearing) {
            return;
        }
        if (layerView == null) {
            layerView = aLayerView;
            layerView.addZoomedViewListener(this);
            layerView.getDynamicToolbarAnimator().addTranslationListener(this);
            ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
            setCapturedSize(metrics);
        }
        startTimeReRender = 0;
        shouldSetVisibleOnUpdate = true;

        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        // At this point, the start point is relative to the layerView.
        // Later, it will be converted relative to the zoomed view as soon as
        // the position of the zoomed view will be calculated.
        animationStart.x = (float) leftFromGecko * metrics.zoomFactor;
        animationStart.y = (float) topFromGecko * metrics.zoomFactor + layerView.getSurfaceTranslation();

        moveUsingGeckoPosition(leftFromGecko, topFromGecko);
    }

    public void stopZoomDisplay(boolean withAnimation) {
        // If "startZoomDisplay" is running and not totally completed (Gecko thread is still
        // running and "showZoomedView" has not yet been called), the zoomed view will be
        // displayed after this call and it should not.
        // Force the stop of the zoomed view, changing the shouldSetVisibleOnUpdate flag
        // before the test of the visibility.
        shouldSetVisibleOnUpdate = false;
        if (getVisibility() == View.VISIBLE) {
            hideZoomedView(withAnimation);
            ThreadUtils.removeCallbacksFromUiThread(requestRenderRunnable);
            if (layerView != null) {
                layerView.getDynamicToolbarAnimator().removeTranslationListener(this);
                layerView.removeZoomedViewListener(this);
                layerView = null;
            }
        }
    }

    private void changeZoomFactor(boolean zoomIn) {
        if (zoomIn && currentZoomFactorIndex < ZOOM_FACTORS_LIST.length - 1) {
            currentZoomFactorIndex++;
        } else if (zoomIn && currentZoomFactorIndex >= ZOOM_FACTORS_LIST.length - 1) {
            currentZoomFactorIndex = 0;
        } else if (!zoomIn && currentZoomFactorIndex > 0) {
            currentZoomFactorIndex--;
        } else {
            currentZoomFactorIndex = ZOOM_FACTORS_LIST.length - 1;
        }
        zoomFactor = ZOOM_FACTORS_LIST[currentZoomFactorIndex];

        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        refreshZoomedViewSize(metrics);
        setTextInZoomFactorButton(zoomFactor);
    }

    private void setTextInZoomFactorButton(float zoom) {
        final String percentageValue = Integer.toString((int) (100*zoom));
        changeZoomFactorButton.setText("- " + getResources().getString(R.string.percent, percentageValue) + " +");
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (event.equals("Gesture:clusteredLinksClicked")) {
                        final JSONObject clickPosition = message.getJSONObject("clickPosition");
                        int left = clickPosition.getInt("x");
                        int top = clickPosition.getInt("y");
                        // Start to display inside the zoomedView
                        LayerView geckoAppLayerView = GeckoAppShell.getLayerView();
                        if (geckoAppLayerView != null) {
                            startZoomDisplay(geckoAppLayerView, left, top);
                        }
                    } else if (event.equals("Window:Resize")) {
                        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
                        refreshZoomedViewSize(metrics);
                    } else if (event.equals("Content:LocationChange")) {
                        stopZoomDisplay(false);
                    } else if (event.equals("Gesture:CloseZoomedView") ||
                            event.equals("Browser:ZoomToPageWidth") ||
                            event.equals("Browser:ZoomToRect")) {
                        stopZoomDisplay(true);
                    } else if (event.equals("FormAssist:AutoComplete")) {
                        isBlockedFromAppearing = true;
                        stopZoomDisplay(true);
                    } else if (event.equals("FormAssist:Hide")) {
                        isBlockedFromAppearing = false;
                    }
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON exception", e);
                }
            }
        });
    }

    private void moveUsingGeckoPosition(int leftFromGecko, int topFromGecko) {
        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        final float parentHeight = metrics.getHeight();
        // moveToolbar is called before getZoomedViewTopLeftPositionFromTouchPosition in order to
        // correctly center vertically the zoomed area
        moveToolbar((topFromGecko * metrics.zoomFactor > parentHeight / 2));
        PointF convertedPosition = getZoomedViewTopLeftPositionFromTouchPosition((leftFromGecko * metrics.zoomFactor),
                (topFromGecko * metrics.zoomFactor));
        moveZoomedView(metrics, convertedPosition.x, convertedPosition.y, StartPointUpdate.GECKO_POSITION);
    }

    @Override
    public void onTranslationChanged(float aToolbarTranslation, float aLayerViewTranslation) {
        ThreadUtils.assertOnUiThread();
        if (layerView != null) {
            dynamicToolbarOverlap = aLayerViewTranslation - aToolbarTranslation;
            refreshZoomedViewSize(layerView.getViewportMetrics());
        }
    }

    @Override
    public void onMetricsChanged(final ImmutableViewportMetrics viewport) {
        // It can be called from a Gecko thread (forceViewportMetrics in GeckoLayerClient).
        // Post to UI Thread to avoid Exception:
        //    "Only the original thread that created a view hierarchy can touch its views."
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                shouldBlockUpdate(false);
                refreshZoomedViewSize(viewport);
            }
        });
    }

    @Override
    public void onPanZoomStopped() {
    }

    @Override
    public void updateView(ByteBuffer data) {
        final Bitmap sb3 = Bitmap.createBitmap(viewWidth, viewHeight, getBitmapConfig());
        if (sb3 != null) {
            data.rewind();
            try {
                sb3.copyPixelsFromBuffer(data);
            } catch (Exception iae) {
                Log.w(LOGTAG, iae.toString());
            }
            if (zoomedImageView != null) {
                RoundedBitmapDrawable ob3 = new RoundedBitmapDrawable(getResources(), sb3, toolbarOnTop, cornerRadius);
                zoomedImageView.setImageDrawable(ob3);
            }
        }
        if (shouldSetVisibleOnUpdate) {
            this.showZoomedView();
        }
        lastStartTimeReRender = startTimeReRender;
        startTimeReRender = 0;
    }

    private void showZoomedView() {
        // no animation if the zoomed view is already visible
        if (getVisibility() != View.VISIBLE) {
            final Animation anim = new ScaleAnimation(
                    0f, 1f, // Start and end values for the X axis scaling
                    0f, 1f, // Start and end values for the Y axis scaling
                    Animation.ABSOLUTE, animationStart.x, // Pivot point of X scaling
                    Animation.ABSOLUTE, animationStart.y); // Pivot point of Y scaling
            anim.setFillAfter(true); // Needed to keep the result of the animation
            anim.setDuration(OPENING_ANIMATION_DURATION_MS);
            anim.setInterpolator(new OvershootInterpolator(OVERSHOOT_INTERPOLATOR_TENSION));
            anim.setAnimationListener(new AnimationListener() {
                public void onAnimationEnd(Animation animation) {
                    setListeners();
                }
                public void onAnimationRepeat(Animation animation) {
                }
                public void onAnimationStart(Animation animation) {
                    removeListeners();
                }
            });
            setAnimation(anim);
        }
        setVisibility(View.VISIBLE);
        shouldSetVisibleOnUpdate = false;
    }

    private void hideZoomedView(boolean withAnimation) {
        if (withAnimation) {
            final Animation anim = new ScaleAnimation(
                1f, 0f, // Start and end values for the X axis scaling
                1f, 0f, // Start and end values for the Y axis scaling
                Animation.ABSOLUTE, animationStart.x, // Pivot point of X scaling
                Animation.ABSOLUTE, animationStart.y); // Pivot point of Y scaling
            anim.setFillAfter(true); // Needed to keep the result of the animation
            anim.setDuration(CLOSING_ANIMATION_DURATION_MS);
            anim.setAnimationListener(new AnimationListener() {
                public void onAnimationEnd(Animation animation) {
                }
                public void onAnimationRepeat(Animation animation) {
                }
                public void onAnimationStart(Animation animation) {
                    removeListeners();
                }
            });
            setAnimation(anim);
        } else {
            removeListeners();
            setAnimation(null);
        }
        setVisibility(View.GONE);
        shouldSetVisibleOnUpdate = false;
    }

    private void updateBufferSize() {
        int pixelSize = (GeckoAppShell.getScreenDepth() == 24) ? 4 : 2;
        int capacity = viewWidth * viewHeight * pixelSize;
        if (buffer == null || buffer.capacity() != capacity) {
            buffer = DirectBufferAllocator.free(buffer);
            buffer = DirectBufferAllocator.allocate(capacity);
        }
    }

    private boolean isRendering() {
        return (startTimeReRender != 0);
    }

    private boolean renderFrequencyTooHigh() {
        return ((System.nanoTime() - lastStartTimeReRender) < MINIMUM_DELAY_BETWEEN_TWO_RENDER_CALLS_NS);
    }

    @Override
    public void requestZoomedViewRender() {
        if (stopUpdateView) {
            return;
        }
        // remove pending runnable
        ThreadUtils.removeCallbacksFromUiThread(requestRenderRunnable);

        // "requestZoomedViewRender" can be called very often by Gecko (endDrawing in LayerRender) without
        // any thing changed in the zoomed area (useless calls from the "zoomed area" point of view).
        // "requestZoomedViewRender" can take time to re-render the zoomed view, it depends of the complexity
        // of the html on this area.
        // To avoid to slow down the application, the 2 following cases are tested:

        // 1- Last render is still running, plan another render later.
        if (isRendering()) {
            // post a new runnable DELAY_BEFORE_NEXT_RENDER_REQUEST_MS later
            // We need to post with a delay to be sure that the last call to requestZoomedViewRender will be done.
            // For a static html page WITHOUT any animation/video, there is a last call to endDrawing and we need to make
            // the zoomed render on this last call.
            ThreadUtils.postDelayedToUiThread(requestRenderRunnable, DELAY_BEFORE_NEXT_RENDER_REQUEST_MS);
            return;
        }

        // 2- Current render occurs too early, plan another render later.
        if (renderFrequencyTooHigh()) {
            // post a new runnable DELAY_BEFORE_NEXT_RENDER_REQUEST_MS later
            // We need to post with a delay to be sure that the last call to requestZoomedViewRender will be done.
            // For a page WITH animation/video, the animation/video can be stopped, and we need to make
            // the zoomed render on this last call.
            ThreadUtils.postDelayedToUiThread(requestRenderRunnable, DELAY_BEFORE_NEXT_RENDER_REQUEST_MS);
            return;
        }

        startTimeReRender = System.nanoTime();
        // Allocate the buffer if it's the first call.
        // Change the buffer size if it's not the right size.
        updateBufferSize();

        int tabId = Tabs.getInstance().getSelectedTab().getId();

        ImmutableViewportMetrics metrics = layerView.getViewportMetrics();
        PointF origin = metrics.getOrigin();

        final int xPos = (int)origin.x + lastPosition.x;
        final int yPos = (int)origin.y + lastPosition.y;

        GeckoEvent e = GeckoEvent.createZoomedViewEvent(tabId, xPos, yPos, viewWidth,
                viewHeight, zoomFactor * metrics.zoomFactor, buffer);
        GeckoAppShell.sendEventToGecko(e);
    }

}
