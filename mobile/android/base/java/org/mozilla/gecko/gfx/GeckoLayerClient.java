/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.gfx.LayerView.DrawListener;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.AppConstants;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

class GeckoLayerClient implements LayerView.Listener, PanZoomTarget
{
    private static final String LOGTAG = "GeckoLayerClient";
    private static int sPaintSyncId = 1;

    private LayerRenderer mLayerRenderer;
    private boolean mLayerRendererInitialized;

    private final Context mContext;
    private IntSize mScreenSize;
    private IntSize mWindowSize;
    private DisplayPortMetrics mDisplayPort;

    private boolean mRecordDrawTimes;
    private final DrawTimingQueue mDrawTimingQueue;

    private VirtualLayer mRootLayer;

    /* The Gecko viewport as per the UI thread. Must be touched only on the UI thread.
     * If any events being sent to Gecko that are relative to the Gecko viewport position,
     * they must (a) be relative to this viewport, and (b) be sent on the UI thread to
     * avoid races. As long as these two conditions are satisfied, and the events being
     * sent to Gecko are processed in FIFO order, the events will properly be relative
     * to the Gecko viewport position. Note that if Gecko updates its viewport independently,
     * we get notified synchronously and also update this on the UI thread.
     */
    private ImmutableViewportMetrics mGeckoViewport;

    /*
     * The viewport metrics being used to draw the current frame. This is only
     * accessed by the compositor thread, and so needs no synchronisation.
     */
    private ImmutableViewportMetrics mFrameMetrics;

    private final List<DrawListener> mDrawListeners;

    /* Used as temporaries by syncViewportInfo */
    private final ViewTransform mCurrentViewTransform;

    /* Used as the return value of progressiveUpdateCallback */
    private final ProgressiveUpdateData mProgressiveUpdateData;
    private DisplayPortMetrics mProgressiveUpdateDisplayPort;
    private boolean mLastProgressiveUpdateWasLowPrecision;
    private boolean mProgressiveUpdateWasInDanger;

    private boolean mForceRedraw;

    /* The current viewport metrics.
     * This is volatile so that we can read and write to it from different threads.
     * We avoid synchronization to make getting the viewport metrics from
     * the compositor as cheap as possible. The viewport is immutable so
     * we don't need to worry about anyone mutating it while we're reading from it.
     * Specifically:
     * 1) reading mViewportMetrics from any thread is fine without synchronization
     * 2) writing to mViewportMetrics requires synchronizing on the layer controller object
     * 3) whenever reading multiple fields from mViewportMetrics without synchronization (i.e. in
     *    case 1 above) you should always first grab a local copy of the reference, and then use
     *    that because mViewportMetrics might get reassigned in between reading the different
     *    fields. */
    private volatile ImmutableViewportMetrics mViewportMetrics;

    private ZoomConstraints mZoomConstraints;

    private volatile boolean mGeckoIsReady;

    private final PanZoomController mPanZoomController;
    private final DynamicToolbarAnimator mToolbarAnimator;
    private final LayerView mView;

    /* This flag is true from the time that browser.js detects a first-paint is about to start,
     * to the time that we receive the first-paint composite notification from the compositor.
     * Note that there is a small race condition with this; if there are two paints that both
     * have the first-paint flag set, and the second paint happens concurrently with the
     * composite for the first paint, then this flag may be set to true prematurely. Fixing this
     * is possible but risky; see https://bugzilla.mozilla.org/show_bug.cgi?id=797615#c751
     */
    private volatile boolean mContentDocumentIsDisplayed;

    private SynthesizedEventState mPointerState;

    public GeckoLayerClient(Context context, LayerView view, EventDispatcher eventDispatcher) {
        // we can fill these in with dummy values because they are always written
        // to before being read
        mContext = context;
        mScreenSize = new IntSize(0, 0);
        mWindowSize = new IntSize(0, 0);
        mDisplayPort = new DisplayPortMetrics();
        mRecordDrawTimes = true;
        mDrawTimingQueue = new DrawTimingQueue();
        mCurrentViewTransform = new ViewTransform(0, 0, 1);
        mProgressiveUpdateData = new ProgressiveUpdateData();
        mProgressiveUpdateDisplayPort = new DisplayPortMetrics();

        mForceRedraw = true;
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        mViewportMetrics = new ImmutableViewportMetrics(displayMetrics)
                           .setViewportSize(view.getWidth(), view.getHeight());
        mZoomConstraints = new ZoomConstraints(false);

        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            mZoomConstraints = tab.getZoomConstraints();
            mViewportMetrics = mViewportMetrics.setIsRTL(tab.getIsRTL());
        }

        mFrameMetrics = mViewportMetrics;

        mDrawListeners = new ArrayList<DrawListener>();
        mToolbarAnimator = new DynamicToolbarAnimator(this);
        mPanZoomController = PanZoomController.Factory.create(this, view, eventDispatcher);
        mView = view;
        mView.setListener(this);
        mContentDocumentIsDisplayed = true;
    }

    public void setOverscrollHandler(final Overscroll listener) {
        mPanZoomController.setOverscrollHandler(listener);
    }

    /** Attaches to root layer so that Gecko appears. */
    /* package */ boolean isGeckoReady() {
        return mGeckoIsReady;
    }

    @WrapForJNI
    private void onGeckoReady() {
        mGeckoIsReady = true;

        mRootLayer = new VirtualLayer(new IntSize(mView.getWidth(), mView.getHeight()));
        mLayerRenderer = mView.getRenderer();

        sendResizeEventIfNecessary(true, null);

        DisplayPortCalculator.initPrefs();

        // Gecko being ready is one of the two conditions (along with having an available
        // surface) that cause us to create the compositor. So here, now that we know gecko
        // is ready, call updateCompositor() to see if we can actually do the creation.
        // This needs to run on the UI thread so that the surface validity can't change on
        // us while we're in the middle of creating the compositor.
        mView.post(new Runnable() {
            @Override
            public void run() {
                mView.updateCompositor();
            }
        });
    }

    public void destroy() {
        mPanZoomController.destroy();
        mToolbarAnimator.destroy();
        mDrawListeners.clear();
    }

    /**
     * Returns true if this client is fine with performing a redraw operation or false if it
     * would prefer that the action didn't take place.
     */
    private boolean getRedrawHint() {
        if (mForceRedraw) {
            mForceRedraw = false;
            return true;
        }

        if (!mPanZoomController.getRedrawHint()) {
            return false;
        }

        return DisplayPortCalculator.aboutToCheckerboard(mViewportMetrics,
                mPanZoomController.getVelocityVector(), mDisplayPort);
    }

    Layer getRoot() {
        return mGeckoIsReady ? mRootLayer : null;
    }

    public LayerView getView() {
        return mView;
    }

    public FloatSize getViewportSize() {
        return mViewportMetrics.getSize();
    }

    /**
     * The view calls this function to indicate that the viewport changed size. It must hold the
     * monitor while calling it.
     *
     * TODO: Refactor this to use an interface. Expose that interface only to the view and not
     * to the layer client. That way, the layer client won't be tempted to call this, which might
     * result in an infinite loop.
     */
    boolean setViewportSize(int width, int height, PointF scrollChange) {
        if (mViewportMetrics.viewportRectWidth == width &&
            mViewportMetrics.viewportRectHeight == height &&
            (scrollChange == null || (scrollChange.x == 0 && scrollChange.y == 0))) {
            return false;
        }
        mViewportMetrics = mViewportMetrics.setViewportSize(width, height);
        if (scrollChange != null) {
            mViewportMetrics = mPanZoomController.adjustScrollForSurfaceShift(mViewportMetrics, scrollChange);
        }

        if (mGeckoIsReady) {
            // here we send gecko a resize message. The code in browser.js is responsible for
            // picking up on that resize event, modifying the viewport as necessary, and informing
            // us of the new viewport.
            sendResizeEventIfNecessary(true, scrollChange);

            // the following call also sends gecko a message, which will be processed after the resize
            // message above has updated the viewport. this message ensures that if we have just put
            // focus in a text field, we scroll the content so that the text field is in view.
            GeckoAppShell.viewSizeChanged();
        }
        return true;
    }

    PanZoomController getPanZoomController() {
        return mPanZoomController;
    }

    DynamicToolbarAnimator getDynamicToolbarAnimator() {
        return mToolbarAnimator;
    }

    /* Informs Gecko that the screen size has changed. */
    private void sendResizeEventIfNecessary(boolean force, PointF scrollChange) {
        DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();

        IntSize newScreenSize = new IntSize(metrics.widthPixels, metrics.heightPixels);
        IntSize newWindowSize = new IntSize(mViewportMetrics.viewportRectWidth,
                                            mViewportMetrics.viewportRectHeight);

        boolean screenSizeChanged = !mScreenSize.equals(newScreenSize);
        boolean windowSizeChanged = !mWindowSize.equals(newWindowSize);

        if (!force && !screenSizeChanged && !windowSizeChanged) {
            return;
        }

        mScreenSize = newScreenSize;
        mWindowSize = newWindowSize;

        if (screenSizeChanged) {
            Log.d(LOGTAG, "Screen-size changed to " + mScreenSize);
        }

        if (windowSizeChanged) {
            Log.d(LOGTAG, "Window-size changed to " + mWindowSize);
        }

        if (mView != null) {
            mView.notifySizeChanged(mWindowSize.width, mWindowSize.height,
                                    mScreenSize.width, mScreenSize.height);
        }

        String json = "";
        try {
            if (scrollChange != null) {
                int id = ++sPaintSyncId;
                if (id == 0) {
                    // never use 0 as that is the default value for "this is not
                    // a special transaction"
                    id = ++sPaintSyncId;
                }
                JSONObject jsonObj = new JSONObject();
                jsonObj.put("x", scrollChange.x / mViewportMetrics.zoomFactor);
                jsonObj.put("y", scrollChange.y / mViewportMetrics.zoomFactor);
                jsonObj.put("id", id);
                json = jsonObj.toString();
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Unable to convert point to JSON", e);
        }
        GeckoAppShell.notifyObservers("Window:Resize", json);
    }

    /** Sets the current page rect. You must hold the monitor while calling this. */
    private void setPageRect(RectF rect, RectF cssRect) {
        // Since the "rect" is always just a multiple of "cssRect" we don't need to
        // check both; this function assumes that both "rect" and "cssRect" are relative
        // the zoom factor in mViewportMetrics.
        if (mViewportMetrics.getCssPageRect().equals(cssRect))
            return;

        mViewportMetrics = mViewportMetrics.setPageRect(rect, cssRect);

        // Page size is owned by the layer client, so no need to notify it of
        // this change.

        post(new Runnable() {
            @Override
            public void run() {
                mPanZoomController.pageRectUpdated();
                mView.requestRender();
            }
        });
    }

    private void adjustViewport(DisplayPortMetrics displayPort) {
        // TODO: APZ For fennec might need margins information to deal with
        // the dynamic toolbar.
        if (AppConstants.MOZ_ANDROID_APZ)
            return;

        ImmutableViewportMetrics metrics = getViewportMetrics();
        ImmutableViewportMetrics clampedMetrics = metrics.clamp();

        if (displayPort == null) {
            displayPort = DisplayPortCalculator.calculate(metrics, mPanZoomController.getVelocityVector());
        }

        mDisplayPort = displayPort;
        mGeckoViewport = clampedMetrics;

        if (mRecordDrawTimes) {
            mDrawTimingQueue.add(displayPort);
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createViewportEvent(clampedMetrics, displayPort));
    }

    /** Aborts any pan/zoom animation that is currently in progress. */
    private void abortPanZoomAnimation() {
        if (mPanZoomController != null) {
            post(new Runnable() {
                @Override
                public void run() {
                    mPanZoomController.abortAnimation();
                }
            });
        }
    }

    /**
     * The different types of Viewport messages handled. All viewport events
     * expect a display-port to be returned, but can handle one not being
     * returned.
     */
    private enum ViewportMessageType {
        UPDATE,       // The viewport has changed and should be entirely updated
        PAGE_SIZE     // The viewport's page-size has changed
    }

    /** Viewport message handler. */
    private DisplayPortMetrics handleViewportMessage(ImmutableViewportMetrics messageMetrics, ViewportMessageType type) {
        synchronized (getLock()) {
            ImmutableViewportMetrics newMetrics;
            ImmutableViewportMetrics oldMetrics = getViewportMetrics();

            switch (type) {
            default:
            case UPDATE:
                // Keep the old viewport size
                newMetrics = messageMetrics.setViewportSize(oldMetrics.viewportRectWidth, oldMetrics.viewportRectHeight);
                if (mToolbarAnimator.isResizing()) {
                    // If we're in the middle of a resize, we don't want to clobber
                    // the scroll offset, so grab the one from the oldMetrics and
                    // keep using that. We also don't want to abort animations,
                    // because at that point we're guaranteed to not be animating
                    // anyway, and calling abortPanZoomAnimation has a nasty
                    // side-effect of clmaping and clobbering the metrics, which
                    // we don't want here.
                    newMetrics = newMetrics.setViewportOrigin(oldMetrics.viewportRectLeft, oldMetrics.viewportRectTop);
                    break;
                }
                if (!oldMetrics.fuzzyEquals(newMetrics)) {
                    abortPanZoomAnimation();
                }
                break;
            case PAGE_SIZE:
                // adjust the page dimensions to account for differences in zoom
                // between the rendered content (which is what Gecko tells us)
                // and our zoom level (which may have diverged).
                float scaleFactor = oldMetrics.zoomFactor / messageMetrics.zoomFactor;
                newMetrics = oldMetrics.setPageRect(RectUtils.scale(messageMetrics.getPageRect(), scaleFactor), messageMetrics.getCssPageRect());
                break;
            }

            // Update the Gecko-side viewport metrics. Make sure to do this
            // before modifying the metrics below.
            final ImmutableViewportMetrics geckoMetrics = newMetrics.clamp();
            post(new Runnable() {
                @Override
                public void run() {
                    mGeckoViewport = geckoMetrics;
                }
            });

            setViewportMetrics(newMetrics, type == ViewportMessageType.UPDATE);
            mDisplayPort = DisplayPortCalculator.calculate(getViewportMetrics(), null);
        }
        return mDisplayPort;
    }

    @WrapForJNI
    DisplayPortMetrics getDisplayPort(boolean pageSizeUpdate, boolean isBrowserContentDisplayed, int tabId, ImmutableViewportMetrics metrics) {
        Tabs tabs = Tabs.getInstance();
        if (isBrowserContentDisplayed && tabs.isSelectedTabId(tabId)) {
            // for foreground tabs, send the viewport update unless the document
            // displayed is different from the content document. In that case, just
            // calculate the display port.
            return handleViewportMessage(metrics, pageSizeUpdate ? ViewportMessageType.PAGE_SIZE : ViewportMessageType.UPDATE);
        } else {
            // for background tabs, request a new display port calculation, so that
            // when we do switch to that tab, we have the correct display port and
            // don't need to draw twice (once to allow the first-paint viewport to
            // get to java, and again once java figures out the display port).
            return DisplayPortCalculator.calculate(metrics, null);
        }
    }

    @WrapForJNI
    void contentDocumentChanged() {
        mContentDocumentIsDisplayed = false;
    }

    @WrapForJNI
    boolean isContentDocumentDisplayed() {
        return mContentDocumentIsDisplayed;
    }

    // This is called on the Gecko thread to determine if we're still interested
    // in the update of this display-port to continue. We can return true here
    // to abort the current update and continue with any subsequent ones. This
    // is useful for slow-to-render pages when the display-port starts lagging
    // behind enough that continuing to draw it is wasted effort.
    @WrapForJNI(allowMultithread = true)
    public ProgressiveUpdateData progressiveUpdateCallback(boolean aHasPendingNewThebesContent,
                                                           float x, float y, float width, float height,
                                                           float resolution, boolean lowPrecision) {
        // Reset the checkerboard risk flag when switching to low precision
        // rendering.
        if (lowPrecision && !mLastProgressiveUpdateWasLowPrecision) {
            // Skip low precision rendering until we're at risk of checkerboarding.
            if (!mProgressiveUpdateWasInDanger) {
                mProgressiveUpdateData.abort = true;
                return mProgressiveUpdateData;
            }
            mProgressiveUpdateWasInDanger = false;
        }
        mLastProgressiveUpdateWasLowPrecision = lowPrecision;

        // Grab a local copy of the last display-port sent to Gecko and the
        // current viewport metrics to avoid races when accessing them.
        DisplayPortMetrics displayPort = mDisplayPort;
        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        mProgressiveUpdateData.setViewport(viewportMetrics);
        mProgressiveUpdateData.abort = false;

        // Always abort updates if the resolution has changed. There's no use
        // in drawing at the incorrect resolution.
        if (!FloatUtils.fuzzyEquals(resolution, viewportMetrics.zoomFactor)) {
            Log.d(LOGTAG, "Aborting draw due to resolution change: " + resolution + " != " + viewportMetrics.zoomFactor);
            mProgressiveUpdateData.abort = true;
            return mProgressiveUpdateData;
        }

        // Store the high precision displayport for comparison when doing low
        // precision updates.
        if (!lowPrecision) {
            if (!FloatUtils.fuzzyEquals(resolution, mProgressiveUpdateDisplayPort.resolution) ||
                !FloatUtils.fuzzyEquals(x, mProgressiveUpdateDisplayPort.getLeft()) ||
                !FloatUtils.fuzzyEquals(y, mProgressiveUpdateDisplayPort.getTop()) ||
                !FloatUtils.fuzzyEquals(x + width, mProgressiveUpdateDisplayPort.getRight()) ||
                !FloatUtils.fuzzyEquals(y + height, mProgressiveUpdateDisplayPort.getBottom())) {
                mProgressiveUpdateDisplayPort =
                    new DisplayPortMetrics(x, y, x + width, y + height, resolution);
            }
        }

        // If we're not doing low precision draws and we're about to
        // checkerboard, enable low precision drawing.
        if (!lowPrecision && !mProgressiveUpdateWasInDanger) {
            if (DisplayPortCalculator.aboutToCheckerboard(viewportMetrics,
                  mPanZoomController.getVelocityVector(), mProgressiveUpdateDisplayPort)) {
                mProgressiveUpdateWasInDanger = true;
            }
        }

        // XXX All sorts of rounding happens inside Gecko that becomes hard to
        //     account exactly for. Given we align the display-port to tile
        //     boundaries (and so they rarely vary by sub-pixel amounts), just
        //     check that values are within a couple of pixels of the
        //     display-port bounds.

        // Never abort drawing if we can't be sure we've sent a more recent
        // display-port. If we abort updating when we shouldn't, we can end up
        // with blank regions on the screen and we open up the risk of entering
        // an endless updating cycle.
        if (Math.abs(displayPort.getLeft() - mProgressiveUpdateDisplayPort.getLeft()) <= 2 &&
            Math.abs(displayPort.getTop() - mProgressiveUpdateDisplayPort.getTop()) <= 2 &&
            Math.abs(displayPort.getBottom() - mProgressiveUpdateDisplayPort.getBottom()) <= 2 &&
            Math.abs(displayPort.getRight() - mProgressiveUpdateDisplayPort.getRight()) <= 2) {
            return mProgressiveUpdateData;
        }

        // Abort updates when the display-port no longer contains the visible
        // area of the page (that is, the viewport cropped by the page
        // boundaries).
        // XXX This makes the assumption that we never let the visible area of
        //     the page fall outside of the display-port.
        if (Math.max(viewportMetrics.viewportRectLeft, viewportMetrics.pageRectLeft) + 1 < x ||
            Math.max(viewportMetrics.viewportRectTop, viewportMetrics.pageRectTop) + 1 < y ||
            Math.min(viewportMetrics.viewportRectRight(), viewportMetrics.pageRectRight) - 1 > x + width ||
            Math.min(viewportMetrics.viewportRectBottom(), viewportMetrics.pageRectBottom) - 1 > y + height) {
            Log.d(LOGTAG, "Aborting update due to viewport not in display-port");
            mProgressiveUpdateData.abort = true;

            // Enable low-precision drawing, as we're likely to be in danger if
            // this situation has been encountered.
            mProgressiveUpdateWasInDanger = true;

            return mProgressiveUpdateData;
        }

        // Abort drawing stale low-precision content if there's a more recent
        // display-port in the pipeline.
        if (lowPrecision && !aHasPendingNewThebesContent) {
          mProgressiveUpdateData.abort = true;
        }
        return mProgressiveUpdateData;
    }

    void setZoomConstraints(ZoomConstraints constraints) {
        mZoomConstraints = constraints;
    }

    void setIsRTL(boolean aIsRTL) {
        synchronized (getLock()) {
            ImmutableViewportMetrics newMetrics = getViewportMetrics().setIsRTL(aIsRTL);
            setViewportMetrics(newMetrics, false);
        }
    }

    /** The compositor invokes this function just before compositing a frame where the document
      * is different from the document composited on the last frame. In these cases, the viewport
      * information we have in Java is no longer valid and needs to be replaced with the new
      * viewport information provided. setPageRect will never be invoked on the same frame that
      * this function is invoked on; and this function will always be called prior to syncViewportInfo.
      */
    @WrapForJNI(allowMultithread = true)
    public void setFirstPaintViewport(float offsetX, float offsetY, float zoom,
            float cssPageLeft, float cssPageTop, float cssPageRight, float cssPageBottom) {
        synchronized (getLock()) {
            ImmutableViewportMetrics currentMetrics = getViewportMetrics();

            Tab tab = Tabs.getInstance().getSelectedTab();

            RectF cssPageRect = new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom);
            RectF pageRect = RectUtils.scaleAndRound(cssPageRect, zoom);

            final ImmutableViewportMetrics newMetrics = currentMetrics
                .setViewportOrigin(offsetX, offsetY)
                .setZoomFactor(zoom)
                .setPageRect(pageRect, cssPageRect)
                .setIsRTL(tab != null ? tab.getIsRTL() : false);
            // Since we have switched to displaying a different document, we need to update any
            // viewport-related state we have lying around. This includes mGeckoViewport and
            // mViewportMetrics. Usually this information is updated via handleViewportMessage
            // while we remain on the same document.
            post(new Runnable() {
                @Override
                public void run() {
                    mGeckoViewport = newMetrics;
                }
            });

            setViewportMetrics(newMetrics);

            if (tab != null) {
                mView.setBackgroundColor(tab.getBackgroundColor());
                setZoomConstraints(tab.getZoomConstraints());
            }

            // At this point, we have just switched to displaying a different document than we
            // we previously displaying. This means we need to abort any panning/zooming animations
            // that are in progress and send an updated display port request to browser.js as soon
            // as possible. The call to PanZoomController.abortAnimation accomplishes this by calling the
            // forceRedraw function, which sends the viewport to gecko. The display port request is
            // actually a full viewport update, which is fine because if browser.js has somehow moved to
            // be out of sync with this first-paint viewport, then we force them back in sync.
            abortPanZoomAnimation();

            // Indicate that the document is about to be composited so the
            // LayerView background can be removed.
            if (mView.getPaintState() == LayerView.PAINT_START) {
                mView.setPaintState(LayerView.PAINT_BEFORE_FIRST);
            }
        }
        DisplayPortCalculator.resetPageState();
        mDrawTimingQueue.reset();

        mContentDocumentIsDisplayed = true;
    }

    /** The compositor invokes this function whenever it determines that the page rect
      * has changed (based on the information it gets from layout). If setFirstPaintViewport
      * is invoked on a frame, then this function will not be. For any given frame, this
      * function will be invoked before syncViewportInfo.
      */
    @WrapForJNI(allowMultithread = true)
    public void setPageRect(float cssPageLeft, float cssPageTop, float cssPageRight, float cssPageBottom) {
        synchronized (getLock()) {
            RectF cssPageRect = new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom);
            float ourZoom = getViewportMetrics().zoomFactor;
            setPageRect(RectUtils.scale(cssPageRect, ourZoom), cssPageRect);
            // Here the page size of the document has changed, but the document being displayed
            // is still the same. Therefore, we don't need to send anything to browser.js; any
            // changes we need to make to the display port will get sent the next time we call
            // adjustViewport().
        }
    }

    /** The compositor invokes this function on every frame to figure out what part of the
      * page to display, and to inform Java of the current display port. Since it is called
      * on every frame, it needs to be ultra-fast.
      * It avoids taking any locks or allocating any objects. We keep around a
      * mCurrentViewTransform so we don't need to allocate a new ViewTransform
      * every time we're called. NOTE: we might be able to return a ImmutableViewportMetrics
      * which would avoid the copy into mCurrentViewTransform.
      */
    @WrapForJNI(allowMultithread = true)
    public ViewTransform syncViewportInfo(int x, int y, int width, int height, float resolution, boolean layersUpdated,
                                          int paintSyncId) {
        // getViewportMetrics is thread safe so we don't need to synchronize.
        // We save the viewport metrics here, so we later use it later in
        // createFrame (which will be called by nsWindow::DrawWindowUnderlay on
        // the native side, by the compositor). The viewport
        // metrics can change between here and there, as it's accessed outside
        // of the compositor thread.
        mFrameMetrics = getViewportMetrics();

        if (paintSyncId == sPaintSyncId) {
            mToolbarAnimator.scrollChangeResizeCompleted();
        }
        mToolbarAnimator.populateViewTransform(mCurrentViewTransform, mFrameMetrics);

        if (mRootLayer != null) {
            mRootLayer.setPositionAndResolution(
                x, y, x + width, y + height,
                resolution);
        }

        if (layersUpdated && mRecordDrawTimes) {
            // If we got a layers update, that means a draw finished. Check to see if the area drawn matches
            // one of our requested displayports; if it does calculate the draw time and notify the
            // DisplayPortCalculator
            DisplayPortMetrics drawn = new DisplayPortMetrics(x, y, x + width, y + height, resolution);
            long time = mDrawTimingQueue.findTimeFor(drawn);
            if (time >= 0) {
                long now = SystemClock.uptimeMillis();
                time = now - time;
                mRecordDrawTimes = DisplayPortCalculator.drawTimeUpdate(time, width * height);
            }
        }

        if (layersUpdated) {
            for (DrawListener listener : mDrawListeners) {
                listener.drawFinished();
            }
        }

        return mCurrentViewTransform;
    }

    @WrapForJNI(allowMultithread = true)
    public ViewTransform syncFrameMetrics(float scrollX, float scrollY, float zoom,
                float cssPageLeft, float cssPageTop, float cssPageRight, float cssPageBottom,
                int dpX, int dpY, int dpWidth, int dpHeight, float paintedResolution,
                boolean layersUpdated, int paintSyncId)
    {
        // TODO: optimize this so it doesn't create so much garbage - it's a
        // hot path
        RectF cssPageRect = new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom);
        synchronized (getLock()) {
            mViewportMetrics = mViewportMetrics.setViewportOrigin(scrollX, scrollY)
                .setZoomFactor(zoom)
                .setPageRect(RectUtils.scale(cssPageRect, zoom), cssPageRect);
        }
        return syncViewportInfo(dpX, dpY, dpWidth, dpHeight, paintedResolution,
                    layersUpdated, paintSyncId);
    }

    class PointerInfo {
        // We reserve one pointer ID for the mouse, so that tests don't have
        // to worry about tracking pointer IDs if they just want to test mouse
        // event synthesization. If somebody tries to use this ID for a
        // synthesized touch event we'll throw an exception.
        public static final int RESERVED_MOUSE_POINTER_ID = 100000;

        public int pointerId;
        public int source;
        public int screenX;
        public int screenY;
        public double pressure;
        public int orientation;

        public MotionEvent.PointerCoords getCoords() {
            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.orientation = orientation;
            coords.pressure = (float)pressure;
            coords.x = screenX;
            coords.y = screenY;
            return coords;
        }
    }

    class SynthesizedEventState {
        public final ArrayList<PointerInfo> pointers;
        public long downTime;

        SynthesizedEventState() {
            pointers = new ArrayList<PointerInfo>();
        }

        int getPointerIndex(int pointerId) {
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).pointerId == pointerId) {
                    return i;
                }
            }
            return -1;
        }

        int addPointer(int pointerId, int source) {
            PointerInfo info = new PointerInfo();
            info.pointerId = pointerId;
            info.source = source;
            pointers.add(info);
            return pointers.size() - 1;
        }

        int getPointerCount(int source) {
            int count = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    count++;
                }
            }
            return count;
        }

        MotionEvent.PointerProperties[] getPointerProperties(int source) {
            MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    MotionEvent.PointerProperties p = new MotionEvent.PointerProperties();
                    p.id = pointers.get(i).pointerId;
                    switch (source) {
                        case InputDevice.SOURCE_TOUCHSCREEN:
                            p.toolType = MotionEvent.TOOL_TYPE_FINGER;
                            break;
                        case InputDevice.SOURCE_MOUSE:
                            p.toolType = MotionEvent.TOOL_TYPE_MOUSE;
                            break;
                    }
                    props[index++] = p;
                }
            }
            return props;
        }

        MotionEvent.PointerCoords[] getPointerCoords(int source) {
            MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    coords[index++] = pointers.get(i).getCoords();
                }
            }
            return coords;
        }
    }

    private void synthesizeNativePointer(int source, int pointerId,
            int eventType, int screenX, int screenY, double pressure,
            int orientation)
    {
        Log.d(LOGTAG, "Synthesizing pointer from " + source + " id " + pointerId + " at " + screenX + ", " + screenY);

        if (mPointerState == null) {
            mPointerState = new SynthesizedEventState();
        }

        // Find the pointer if it already exists
        int pointerIndex = mPointerState.getPointerIndex(pointerId);

        // Event-specific handling
        switch (eventType) {
            case MotionEvent.ACTION_POINTER_UP:
                if (pointerIndex < 0) {
                    Log.d(LOGTAG, "Requested synthesis of a pointer-up for a pointer that doesn't exist!");
                    return;
                }
                if (mPointerState.pointers.size() == 1) {
                    // Last pointer is going up
                    eventType = MotionEvent.ACTION_UP;
                }
                break;
            case MotionEvent.ACTION_CANCEL:
                if (pointerIndex < 0) {
                    Log.d(LOGTAG, "Requested synthesis of a pointer-cancel for a pointer that doesn't exist!");
                    return;
                }
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                if (pointerIndex < 0) {
                    // Adding a new pointer
                    pointerIndex = mPointerState.addPointer(pointerId, source);
                    if (pointerIndex == 0) {
                        // first pointer
                        eventType = MotionEvent.ACTION_DOWN;
                        mPointerState.downTime = SystemClock.uptimeMillis();
                    }
                } else {
                    // We're moving an existing pointer
                    eventType = MotionEvent.ACTION_MOVE;
                }
                break;
            case MotionEvent.ACTION_HOVER_MOVE:
                if (pointerIndex < 0) {
                    // Mouse-move a pointer without it going "down". However
                    // in order to send the right MotionEvent without a lot of
                    // duplicated code, we add the pointer to mPointerState,
                    // and then remove it at the bottom of this function.
                    pointerIndex = mPointerState.addPointer(pointerId, source);
                } else {
                    // We're moving an existing mouse pointer that went down.
                    eventType = MotionEvent.ACTION_MOVE;
                }
                break;
        }

        // Update the pointer with the new info
        PointerInfo info = mPointerState.pointers.get(pointerIndex);
        info.screenX = screenX;
        info.screenY = screenY;
        info.pressure = pressure;
        info.orientation = orientation;

        // Dispatch the event
        int action = (pointerIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
        action &= MotionEvent.ACTION_POINTER_INDEX_MASK;
        action |= (eventType & MotionEvent.ACTION_MASK);
        boolean isButtonDown = (source == InputDevice.SOURCE_MOUSE) &&
                               (eventType == MotionEvent.ACTION_DOWN || eventType == MotionEvent.ACTION_MOVE);
        final MotionEvent event = MotionEvent.obtain(
            /*downTime*/ mPointerState.downTime,
            /*eventTime*/ SystemClock.uptimeMillis(),
            /*action*/ action,
            /*pointerCount*/ mPointerState.getPointerCount(source),
            /*pointerProperties*/ mPointerState.getPointerProperties(source),
            /*pointerCoords*/ mPointerState.getPointerCoords(source),
            /*metaState*/ 0,
            /*buttonState*/ (isButtonDown ? MotionEvent.BUTTON_PRIMARY : 0),
            /*xPrecision*/ 0,
            /*yPrecision*/ 0,
            /*deviceId*/ 0,
            /*edgeFlags*/ 0,
            /*source*/ source,
            /*flags*/ 0);
        mView.post(new Runnable() {
            @Override
            public void run() {
                event.offsetLocation(0, mView.getSurfaceTranslation());
                mView.dispatchTouchEvent(event);
            }
        });

        // Forget about removed pointers
        if (eventType == MotionEvent.ACTION_POINTER_UP ||
            eventType == MotionEvent.ACTION_UP ||
            eventType == MotionEvent.ACTION_CANCEL ||
            eventType == MotionEvent.ACTION_HOVER_MOVE)
        {
            mPointerState.pointers.remove(pointerIndex);
        }
    }

    @WrapForJNI
    public void synthesizeNativeTouchPoint(int pointerId, int eventType, int screenX,
            int screenY, double pressure, int orientation)
    {
        if (pointerId == PointerInfo.RESERVED_MOUSE_POINTER_ID) {
            throw new IllegalArgumentException("Use a different pointer ID in your test, this one is reserved for mouse");
        }
        synthesizeNativePointer(InputDevice.SOURCE_TOUCHSCREEN, pointerId,
            eventType, screenX, screenY, pressure, orientation);
    }

    @WrapForJNI
    public void synthesizeNativeMouseEvent(int eventType, int screenX, int screenY) {
        synthesizeNativePointer(InputDevice.SOURCE_MOUSE, PointerInfo.RESERVED_MOUSE_POINTER_ID,
            eventType, screenX, screenY, 0, 0);
    }

    @WrapForJNI(allowMultithread = true)
    public LayerRenderer.Frame createFrame() {
        // Create the shaders and textures if necessary.
        if (!mLayerRendererInitialized) {
            if (mLayerRenderer == null) {
                return null;
            }
            mLayerRenderer.createDefaultProgram();
            mLayerRendererInitialized = true;
        }

        try {
            return mLayerRenderer.createFrame(mFrameMetrics);
        } catch (Exception e) {
            Log.w(LOGTAG, e);
            return null;
        }
    }

    @WrapForJNI(allowMultithread = true)
    public void activateProgram() {
        mLayerRenderer.activateDefaultProgram();
    }

    @WrapForJNI(allowMultithread = true)
    public void deactivateProgramAndRestoreState(boolean enableScissor,
            int scissorX, int scissorY, int scissorW, int scissorH)
    {
        mLayerRenderer.deactivateDefaultProgram();
        mLayerRenderer.restoreState(enableScissor, scissorX, scissorY, scissorW, scissorH);
    }

    private void geometryChanged(DisplayPortMetrics displayPort) {
        /* Let Gecko know if the screensize has changed */
        sendResizeEventIfNecessary(false, null);
        if (getRedrawHint()) {
            adjustViewport(displayPort);
        }
    }

    /** Implementation of LayerView.Listener */
    @Override
    public void renderRequested() {
        if (mView != null) {
            mView.invalidateAndScheduleComposite();
        }
    }

    /** Implementation of LayerView.Listener */
    @Override
    public void sizeChanged(int width, int height) {
        // We need to make sure a draw happens synchronously at this point,
        // but resizing the surface before the SurfaceView has resized will
        // cause a visible jump.
        mView.resumeCompositor(width, height);
    }

    /** Implementation of LayerView.Listener */
    @Override
    public void surfaceChanged(int width, int height) {
        IntSize viewportSize = mToolbarAnimator.getViewportSize();
        setViewportSize(viewportSize.width, viewportSize.height, null);
    }

    /** Implementation of PanZoomTarget */
    @Override
    public ImmutableViewportMetrics getViewportMetrics() {
        return mViewportMetrics;
    }

    /** Implementation of PanZoomTarget */
    @Override
    public ZoomConstraints getZoomConstraints() {
        return mZoomConstraints;
    }

    /** Implementation of PanZoomTarget */
    @Override
    public FullScreenState getFullScreenState() {
        return mView.getFullScreenState();
    }

    /** Implementation of PanZoomTarget */
    @Override
    public PointF getVisibleEndOfLayerView() {
        return mToolbarAnimator.getVisibleEndOfLayerView();
    }

    /** Implementation of PanZoomTarget */
    @Override
    public void setAnimationTarget(ImmutableViewportMetrics metrics) {
        if (mGeckoIsReady) {
            // We know what the final viewport of the animation is going to be, so
            // immediately request a draw of that area by setting the display port
            // accordingly. This way we should have the content pre-rendered by the
            // time the animation is done.
            DisplayPortMetrics displayPort = DisplayPortCalculator.calculate(metrics, null);
            adjustViewport(displayPort);
        }
    }

    /** Implementation of PanZoomTarget
     * You must hold the monitor while calling this.
     */
    @Override
    public void setViewportMetrics(ImmutableViewportMetrics metrics) {
        setViewportMetrics(metrics, true);
    }

    /*
     * You must hold the monitor while calling this.
     */
    private void setViewportMetrics(ImmutableViewportMetrics metrics, boolean notifyGecko) {
        // This class owns the viewport size and the fixed layer margins; don't let other pieces
        // of code clobber either of them. The only place the viewport size should ever be
        // updated is in GeckoLayerClient.setViewportSize, and the only place the margins should
        // ever be updated is in GeckoLayerClient.setFixedLayerMargins; both of these assign to
        // mViewportMetrics directly.
        metrics = metrics.setViewportSize(mViewportMetrics.viewportRectWidth, mViewportMetrics.viewportRectHeight);
        mViewportMetrics = metrics;

        viewportMetricsChanged(notifyGecko);
    }

    /*
     * You must hold the monitor while calling this.
     */
    private void viewportMetricsChanged(boolean notifyGecko) {
        mToolbarAnimator.onMetricsChanged(mViewportMetrics);

        mView.requestRender();
        if (notifyGecko && mGeckoIsReady) {
            geometryChanged(null);
        }
    }

    /*
     * Updates the viewport metrics, overriding the viewport size and margins
     * which are normally retained when calling setViewportMetrics.
     * You must hold the monitor while calling this.
     */
    void forceViewportMetrics(ImmutableViewportMetrics metrics, boolean notifyGecko, boolean forceRedraw) {
        if (forceRedraw) {
            mForceRedraw = true;
        }
        mViewportMetrics = metrics;
        viewportMetricsChanged(notifyGecko);
    }

    /** Implementation of PanZoomTarget
     * Scroll the viewport by a certain amount. This will take viewport margins
     * and margin animation into account. If margins are currently animating,
     * this will just go ahead and modify the viewport origin, otherwise the
     * delta will be applied to the margins and the remainder will be applied to
     * the viewport origin.
     *
     * You must hold the monitor while calling this.
     */
    @Override
    public void scrollBy(float dx, float dy) {
        // Set mViewportMetrics manually so the margin changes take.
        mViewportMetrics = mViewportMetrics.offsetViewportBy(dx, dy);
        viewportMetricsChanged(true);
    }

    /** Implementation of PanZoomTarget */
    @Override
    public void panZoomStopped() {
        mToolbarAnimator.onPanZoomStopped();
    }

    /** Implementation of PanZoomTarget */
    @Override
    public void forceRedraw(DisplayPortMetrics displayPort) {
        mForceRedraw = true;
        if (mGeckoIsReady) {
            geometryChanged(displayPort);
        }
    }

    /** Implementation of PanZoomTarget */
    @Override
    public boolean post(Runnable action) {
        return mView.post(action);
    }

    /** Implementation of PanZoomTarget */
    @Override
    public void postRenderTask(RenderTask task) {
        mView.postRenderTask(task);
    }

    /** Implementation of PanZoomTarget */
    @Override
    public void removeRenderTask(RenderTask task) {
        mView.removeRenderTask(task);
    }

    /** Implementation of PanZoomTarget */
    @Override
    public Object getLock() {
        return this;
    }

    /** Implementation of PanZoomTarget
     * Converts a point from layer view coordinates to layer coordinates. In other words, given a
     * point measured in pixels from the top left corner of the layer view, returns the point in
     * pixels measured from the last scroll position we sent to Gecko, in CSS pixels. Assuming the
     * events being sent to Gecko are processed in FIFO order, this calculation should always be
     * correct.
     */
    @Override
    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        if (!mGeckoIsReady) {
            return null;
        }

        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        PointF origin = viewportMetrics.getOrigin();
        float zoom = viewportMetrics.zoomFactor;
        ImmutableViewportMetrics geckoViewport = (AppConstants.MOZ_ANDROID_APZ ? mViewportMetrics : mGeckoViewport);
        PointF geckoOrigin = geckoViewport.getOrigin();
        float geckoZoom = geckoViewport.zoomFactor;

        // viewPoint + origin - offset gives the coordinate in device pixels from the top-left corner of the page.
        // Divided by zoom, this gives us the coordinate in CSS pixels from the top-left corner of the page.
        // geckoOrigin / geckoZoom is where Gecko thinks it is (scrollTo position) in CSS pixels from
        // the top-left corner of the page. Subtracting the two gives us the offset of the viewPoint from
        // the current Gecko coordinate in CSS pixels.
        PointF layerPoint = new PointF(
                ((viewPoint.x + origin.x) / zoom) - (geckoOrigin.x / geckoZoom),
                ((viewPoint.y + origin.y) / zoom) - (geckoOrigin.y / geckoZoom));

        return layerPoint;
    }

    @Override
    public Matrix getMatrixForLayerRectToViewRect() {
        if (!mGeckoIsReady) {
            return null;
        }

        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        PointF origin = viewportMetrics.getOrigin();
        float zoom = viewportMetrics.zoomFactor;
        ImmutableViewportMetrics geckoViewport = (AppConstants.MOZ_ANDROID_APZ ? mViewportMetrics : mGeckoViewport);
        PointF geckoOrigin = geckoViewport.getOrigin();
        float geckoZoom = geckoViewport.zoomFactor;

        Matrix matrix = new Matrix();
        matrix.postTranslate(geckoOrigin.x / geckoZoom, geckoOrigin.y / geckoZoom);
        matrix.postScale(zoom, zoom);
        matrix.postTranslate(-origin.x, -origin.y);
        return matrix;
    }

    @Override
    public void setScrollingRootContent(boolean isRootContent) {
        mToolbarAnimator.setScrollingRootContent(isRootContent);
    }

    public void addDrawListener(DrawListener listener) {
        mDrawListeners.add(listener);
    }

    public void removeDrawListener(DrawListener listener) {
        mDrawListeners.remove(listener);
    }
}
