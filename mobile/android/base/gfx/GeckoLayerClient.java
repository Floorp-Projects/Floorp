/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoEventResponder;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.ui.PanZoomTarget;
import org.mozilla.gecko.ui.SimpleScaleGestureDetector;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.GestureDetector;

import java.util.HashMap;
import java.util.Map;

public class GeckoLayerClient
        implements GeckoEventResponder, LayerView.Listener, PanZoomTarget
{
    private static final String LOGTAG = "GeckoLayerClient";

    private LayerRenderer mLayerRenderer;
    private boolean mLayerRendererInitialized;

    private Context mContext;
    private IntSize mScreenSize;
    private IntSize mWindowSize;
    private DisplayPortMetrics mDisplayPort;
    private DisplayPortMetrics mReturnDisplayPort;

    private boolean mRecordDrawTimes;
    private final DrawTimingQueue mDrawTimingQueue;

    private VirtualLayer mRootLayer;

    /* The Gecko viewport as per the UI thread. Must be touched only on the UI thread. */
    private ViewportMetrics mGeckoViewport;

    /*
     * The viewport metrics being used to draw the current frame. This is only
     * accessed by the compositor thread, and so needs no synchronisation.
     */
    private ImmutableViewportMetrics mFrameMetrics;

    /* Used by robocop for testing purposes */
    private DrawListener mDrawListener;

    /* Used as a temporary ViewTransform by syncViewportInfo */
    private final ViewTransform mCurrentViewTransform;

    /* This is written by the compositor thread and read by the UI thread. */
    private volatile boolean mCompositorCreated;

    private boolean mForceRedraw;

    /* The current viewport metrics.
     * This is volatile so that we can read and write to it from different threads.
     * We avoid synchronization to make getting the viewport metrics from
     * the compositor as cheap as possible. The viewport is immutable so
     * we don't need to worry about anyone mutating it while we're reading from it.
     * Specifically:
     * 1) reading mViewportMetrics from any thread is fine without synchronization
     * 2) writing to mViewportMetrics requires synchronizing on the layer controller object
     * 3) whenver reading multiple fields from mViewportMetrics without synchronization (i.e. in
     *    case 1 above) you should always frist grab a local copy of the reference, and then use
     *    that because mViewportMetrics might get reassigned in between reading the different
     *    fields. */
    private volatile ImmutableViewportMetrics mViewportMetrics;

    private ZoomConstraints mZoomConstraints;

    private boolean mGeckoIsReady;

    /* The new color for the checkerboard. */
    private int mCheckerboardColor;
    private boolean mCheckerboardShouldShowChecks;

    private final PanZoomController mPanZoomController;
    private LayerView mView;

    public GeckoLayerClient(Context context) {
        // we can fill these in with dummy values because they are always written
        // to before being read
        mContext = context;
        mScreenSize = new IntSize(0, 0);
        mWindowSize = new IntSize(0, 0);
        mDisplayPort = new DisplayPortMetrics();
        mRecordDrawTimes = true;
        mDrawTimingQueue = new DrawTimingQueue();
        mCurrentViewTransform = new ViewTransform(0, 0, 1);

        mCompositorCreated = false;

        mForceRedraw = true;
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        mViewportMetrics = new ImmutableViewportMetrics(new ViewportMetrics(displayMetrics));
        mZoomConstraints = new ZoomConstraints(false);
        mCheckerboardColor = Color.WHITE;
        mCheckerboardShouldShowChecks = true;

        mPanZoomController = new PanZoomController(this);
    }

    public void setView(LayerView v) {
        mView = v;
        mView.connect(this);
    }

    /** Attaches to root layer so that Gecko appears. */
    public void notifyGeckoReady() {
        mGeckoIsReady = true;

        mRootLayer = new VirtualLayer(new IntSize(mView.getWidth(), mView.getHeight()));
        mLayerRenderer = new LayerRenderer(mView);

        GeckoAppShell.registerGeckoEventListener("Viewport:Update", this);
        GeckoAppShell.registerGeckoEventListener("Viewport:PageSize", this);
        GeckoAppShell.registerGeckoEventListener("Viewport:CalculateDisplayPort", this);
        GeckoAppShell.registerGeckoEventListener("Checkerboard:Toggle", this);
        GeckoAppShell.registerGeckoEventListener("Preferences:Data", this);

        mView.setListener(this);
        mView.setLayerRenderer(mLayerRenderer);

        sendResizeEventIfNecessary(true);

        JSONArray prefs = new JSONArray();
        DisplayPortCalculator.addPrefNames(prefs);
        PluginLayer.addPrefNames(prefs);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Preferences:Get", prefs.toString()));
    }

    public void destroy() {
        mPanZoomController.destroy();
        GeckoAppShell.unregisterGeckoEventListener("Viewport:Update", this);
        GeckoAppShell.unregisterGeckoEventListener("Viewport:PageSize", this);
        GeckoAppShell.unregisterGeckoEventListener("Viewport:CalculateDisplayPort", this);
        GeckoAppShell.unregisterGeckoEventListener("Checkerboard:Toggle", this);
        GeckoAppShell.unregisterGeckoEventListener("Preferences:Data", this);
    }

    /**
     * Returns true if this client is fine with performing a redraw operation or false if it
     * would prefer that the action didn't take place.
     */
    public boolean getRedrawHint() {
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

    public Layer getRoot() {
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
    public void setViewportSize(FloatSize size) {
        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.setSize(size);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        if (mGeckoIsReady) {
            viewportSizeChanged();
        }
    }

    public PanZoomController getPanZoomController() {
        return mPanZoomController;
    }

    public GestureDetector.OnGestureListener getGestureListener() {
        return mPanZoomController;
    }

    public SimpleScaleGestureDetector.SimpleScaleGestureListener getScaleGestureListener() {
        return mPanZoomController;
    }

    public GestureDetector.OnDoubleTapListener getDoubleTapListener() {
        return mPanZoomController;
    }

    /* Informs Gecko that the screen size has changed. */
    private void sendResizeEventIfNecessary(boolean force) {
        DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();

        IntSize newScreenSize = new IntSize(metrics.widthPixels, metrics.heightPixels);
        IntSize newWindowSize = new IntSize(mView.getWidth(), mView.getHeight());

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

        GeckoEvent event = GeckoEvent.createSizeChangedEvent(mWindowSize.width, mWindowSize.height,
                                                             mScreenSize.width, mScreenSize.height);
        GeckoAppShell.sendEventToGecko(event);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Window:Resize", ""));
    }

    void viewportSizeChanged() {
        // here we send gecko a resize message. The code in browser.js is responsible for
        // picking up on that resize event, modifying the viewport as necessary, and informing
        // us of the new viewport.
        sendResizeEventIfNecessary(true);
        // the following call also sends gecko a message, which will be processed after the resize
        // message above has updated the viewport. this message ensures that if we have just put
        // focus in a text field, we scroll the content so that the text field is in view.
        GeckoAppShell.viewSizeChanged();
    }

    /** Sets the current page rect. You must hold the monitor while calling this. */
    private void setPageRect(RectF rect, RectF cssRect) {
        // Since the "rect" is always just a multiple of "cssRect" we don't need to
        // check both; this function assumes that both "rect" and "cssRect" are relative
        // the zoom factor in mViewportMetrics.
        if (mViewportMetrics.getCssPageRect().equals(cssRect))
            return;

        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.setPageRect(rect, cssRect);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        // Page size is owned by the layer client, so no need to notify it of
        // this change.

        post(new Runnable() {
            public void run() {
                mPanZoomController.pageRectUpdated();
                mView.requestRender();
            }
        });
    }

    void adjustViewport(DisplayPortMetrics displayPort) {
        ImmutableViewportMetrics metrics = getViewportMetrics();

        ViewportMetrics clampedMetrics = new ViewportMetrics(metrics);
        clampedMetrics.setViewport(clampedMetrics.getClampedViewport());

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
    private void handleViewportMessage(JSONObject message, ViewportMessageType type) throws JSONException {
        ViewportMetrics messageMetrics = new ViewportMetrics(message);
        synchronized (this) {
            final ViewportMetrics newMetrics;
            ImmutableViewportMetrics oldMetrics = getViewportMetrics();

            switch (type) {
            default:
            case UPDATE:
                newMetrics = messageMetrics;
                // Keep the old viewport size
                newMetrics.setSize(oldMetrics.getSize());
                abortPanZoomAnimation();
                break;
            case PAGE_SIZE:
                // adjust the page dimensions to account for differences in zoom
                // between the rendered content (which is what Gecko tells us)
                // and our zoom level (which may have diverged).
                float scaleFactor = oldMetrics.zoomFactor / messageMetrics.getZoomFactor();
                newMetrics = new ViewportMetrics(oldMetrics);
                newMetrics.setPageRect(RectUtils.scale(messageMetrics.getPageRect(), scaleFactor), messageMetrics.getCssPageRect());
                break;
            }

            post(new Runnable() {
                public void run() {
                    mGeckoViewport = newMetrics;
                }
            });
            setViewportMetrics(newMetrics);
            mDisplayPort = DisplayPortCalculator.calculate(getViewportMetrics(), null);
        }
        mReturnDisplayPort = mDisplayPort;
    }

    /** Implementation of GeckoEventResponder/GeckoEventListener. */
    public void handleMessage(String event, JSONObject message) {
        try {
            if ("Viewport:Update".equals(event)) {
                handleViewportMessage(message, ViewportMessageType.UPDATE);
            } else if ("Viewport:PageSize".equals(event)) {
                handleViewportMessage(message, ViewportMessageType.PAGE_SIZE);
            } else if ("Viewport:CalculateDisplayPort".equals(event)) {
                ImmutableViewportMetrics newMetrics = new ImmutableViewportMetrics(new ViewportMetrics(message));
                mReturnDisplayPort = DisplayPortCalculator.calculate(newMetrics, null);
            } else if ("Checkerboard:Toggle".equals(event)) {
                boolean showChecks = message.getBoolean("value");
                setCheckerboardShowChecks(showChecks);
                Log.i(LOGTAG, "Showing checks: " + showChecks);
            } else if ("Preferences:Data".equals(event)) {
                JSONArray jsonPrefs = message.getJSONArray("preferences");
                Map<String, Integer> prefValues = new HashMap<String, Integer>();
                for (int i = jsonPrefs.length() - 1; i >= 0; i--) {
                    JSONObject pref = jsonPrefs.getJSONObject(i);
                    String name = pref.getString("name");
                    try {
                        prefValues.put(name, pref.getInt("value"));
                    } catch (JSONException je) {
                        // the pref value couldn't be parsed as an int. drop this pref
                        // and continue with the rest
                    }
                }
                // check return value from setStrategy to make sure that this is the
                // right batch of prefs, since other java code may also have sent requests
                // for prefs.
                if (DisplayPortCalculator.setStrategy(prefValues) && PluginLayer.setUsePlaceholder(prefValues)) {
                    GeckoAppShell.unregisterGeckoEventListener("Preferences:Data", this);
                }
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error decoding JSON in " + event + " handler", e);
        }
    }

    /** Implementation of GeckoEventResponder. */
    public String getResponse() {
        // We are responding to the events handled in handleMessage() above with the
        // display port we calculated. Different messages will generate different
        // display ports and put them in mReturnDisplayPort, so we just return that.
        // Note that mReturnDisplayPort is always touched on the Gecko thread, so
        // no synchronization is needed for it.
        if (mReturnDisplayPort == null) {
            return "";
        }
        try {
            return mReturnDisplayPort.toJSON();
        } finally {
            mReturnDisplayPort = null;
        }
    }

    /*
     * This function returns the last viewport that we sent to Gecko. If any additional events are
     * being sent to Gecko that are relative on the Gecko viewport position, they must (a) be relative
     * to this viewport, and (b) be sent on the UI thread to avoid races. As long as these two
     * conditions are satisfied, and the events being sent to Gecko are processed in FIFO order, the
     * events will properly be relative to the Gecko viewport position. Note that if Gecko updates
     * its viewport independently, we get notified synchronously and also update this on the UI thread.
     */
    public ViewportMetrics getGeckoViewportMetrics() {
        return mGeckoViewport;
    }

    public boolean checkerboardShouldShowChecks() {
        return mCheckerboardShouldShowChecks;
    }

    public int getCheckerboardColor() {
        return mCheckerboardColor;
    }

    public void setCheckerboardShowChecks(boolean showChecks) {
        mCheckerboardShouldShowChecks = showChecks;
        mView.requestRender();
    }

    public void setCheckerboardColor(int newColor) {
        mCheckerboardColor = newColor;
        mView.requestRender();
    }

    public void setZoomConstraints(ZoomConstraints constraints) {
        mZoomConstraints = constraints;
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature.
      * The compositor invokes this function just before compositing a frame where the document
      * is different from the document composited on the last frame. In these cases, the viewport
      * information we have in Java is no longer valid and needs to be replaced with the new
      * viewport information provided. setPageRect will never be invoked on the same frame that
      * this function is invoked on; and this function will always be called prior to syncViewportInfo.
      */
    public void setFirstPaintViewport(float offsetX, float offsetY, float zoom,
            float pageLeft, float pageTop, float pageRight, float pageBottom,
            float cssPageLeft, float cssPageTop, float cssPageRight, float cssPageBottom) {
        synchronized (this) {
            final ViewportMetrics currentMetrics = new ViewportMetrics(getViewportMetrics());
            currentMetrics.setOrigin(new PointF(offsetX, offsetY));
            currentMetrics.setZoomFactor(zoom);
            currentMetrics.setPageRect(new RectF(pageLeft, pageTop, pageRight, pageBottom),
                                       new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom));
            // Since we have switched to displaying a different document, we need to update any
            // viewport-related state we have lying around. This includes mGeckoViewport and
            // mViewportMetrics. Usually this information is updated via handleViewportMessage
            // while we remain on the same document.
            post(new Runnable() {
                public void run() {
                    mGeckoViewport = currentMetrics;
                }
            });
            setViewportMetrics(currentMetrics);

            Tab tab = Tabs.getInstance().getSelectedTab();
            setCheckerboardColor(tab.getCheckerboardColor());
            setZoomConstraints(tab.getZoomConstraints());

            // At this point, we have just switched to displaying a different document than we
            // we previously displaying. This means we need to abort any panning/zooming animations
            // that are in progress and send an updated display port request to browser.js as soon
            // as possible. We accomplish this by passing true to abortPanZoomAnimation, which
            // sends the request after aborting the animation. The display port request is actually
            // a full viewport update, which is fine because if browser.js has somehow moved to
            // be out of sync with this first-paint viewport, then we force them back in sync.
            abortPanZoomAnimation();
            mView.setPaintState(LayerView.PAINT_BEFORE_FIRST);
        }
        DisplayPortCalculator.resetPageState();
        mDrawTimingQueue.reset();
        mView.getRenderer().resetCheckerboard();
        GeckoAppShell.screenshotWholePage(Tabs.getInstance().getSelectedTab());
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature.
      * The compositor invokes this function whenever it determines that the page rect
      * has changed (based on the information it gets from layout). If setFirstPaintViewport
      * is invoked on a frame, then this function will not be. For any given frame, this
      * function will be invoked before syncViewportInfo.
      */
    public void setPageRect(float cssPageLeft, float cssPageTop, float cssPageRight, float cssPageBottom) {
        synchronized (this) {
            RectF cssPageRect = new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom);
            float ourZoom = getViewportMetrics().zoomFactor;
            setPageRect(RectUtils.scale(cssPageRect, ourZoom), cssPageRect);
            // Here the page size of the document has changed, but the document being displayed
            // is still the same. Therefore, we don't need to send anything to browser.js; any
            // changes we need to make to the display port will get sent the next time we call
            // adjustViewport().
        }
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature.
      * The compositor invokes this function on every frame to figure out what part of the
      * page to display, and to inform Java of the current display port. Since it is called
      * on every frame, it needs to be ultra-fast.
      * It avoids taking any locks or allocating any objects. We keep around a
      * mCurrentViewTransform so we don't need to allocate a new ViewTransform
      * everytime we're called. NOTE: we might be able to return a ImmutableViewportMetrics
      * which would avoid the copy into mCurrentViewTransform.
      */
    public ViewTransform syncViewportInfo(int x, int y, int width, int height, float resolution, boolean layersUpdated) {
        // getViewportMetrics is thread safe so we don't need to synchronize.
        // We save the viewport metrics here, so we later use it later in
        // createFrame (which will be called by nsWindow::DrawWindowUnderlay on
        // the native side, by the compositor). The viewport
        // metrics can change between here and there, as it's accessed outside
        // of the compositor thread.
        mFrameMetrics = getViewportMetrics();

        mCurrentViewTransform.x = mFrameMetrics.viewportRectLeft;
        mCurrentViewTransform.y = mFrameMetrics.viewportRectTop;
        mCurrentViewTransform.scale = mFrameMetrics.zoomFactor;

        mRootLayer.setPositionAndResolution(x, y, x + width, y + height, resolution);

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

        if (layersUpdated && mDrawListener != null) {
            /* Used by robocop for testing purposes */
            mDrawListener.drawFinished();
        }

        return mCurrentViewTransform;
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public LayerRenderer.Frame createFrame() {
        // Create the shaders and textures if necessary.
        if (!mLayerRendererInitialized) {
            mLayerRenderer.checkMonitoringEnabled();
            mLayerRenderer.createDefaultProgram();
            mLayerRendererInitialized = true;
        }

        return mLayerRenderer.createFrame(mFrameMetrics);
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void activateProgram() {
        mLayerRenderer.activateDefaultProgram();
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void deactivateProgram() {
        mLayerRenderer.deactivateDefaultProgram();
    }

    void geometryChanged() {
        /* Let Gecko know if the screensize has changed */
        sendResizeEventIfNecessary(false);
        if (getRedrawHint()) {
            adjustViewport(null);
        }
    }

    /** Implementation of LayerView.Listener */
    public void renderRequested() {
        GeckoAppShell.scheduleComposite();
    }

    /** Implementation of LayerView.Listener */
    public void compositionPauseRequested() {
        // We need to coordinate with Gecko when pausing composition, to ensure
        // that Gecko never executes a draw event while the compositor is paused.
        // This is sent synchronously to make sure that we don't attempt to use
        // any outstanding Surfaces after we call this (such as from a
        // surfaceDestroyed notification), and to make sure that any in-flight
        // Gecko draw events have been processed.  When this returns, composition is
        // definitely paused -- it'll synchronize with the Gecko event loop, which
        // in turn will synchronize with the compositor thread.
        if (mCompositorCreated) {
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createCompositorPauseEvent());
        }
    }

    /** Implementation of LayerView.Listener */
    public void compositionResumeRequested(int width, int height) {
        // Asking Gecko to resume the compositor takes too long (see
        // https://bugzilla.mozilla.org/show_bug.cgi?id=735230#c23), so we
        // resume the compositor directly. We still need to inform Gecko about
        // the compositor resuming, so that Gecko knows that it can now draw.
        if (mCompositorCreated) {
            GeckoAppShell.scheduleResumeComposition(width, height);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createCompositorResumeEvent());
        }
    }

    /** Implementation of LayerView.Listener */
    public void surfaceChanged(int width, int height) {
        setViewportSize(new FloatSize(width, height));

        // We need to make this call even when the compositor isn't currently
        // paused (e.g. during an orientation change), to make the compositor
        // aware of the changed surface.
        compositionResumeRequested(width, height);
        renderRequested();
    }

    /** Implementation of LayerView.Listener */
    public void compositorCreated() {
        mCompositorCreated = true;
    }

    /** Implementation of PanZoomTarget */
    public ImmutableViewportMetrics getViewportMetrics() {
        return mViewportMetrics;
    }

    /** Implementation of PanZoomTarget */
    public ZoomConstraints getZoomConstraints() {
        return mZoomConstraints;
    }

    /** Implementation of PanZoomTarget */
    public void setAnimationTarget(ViewportMetrics viewport) {
        if (mGeckoIsReady) {
            // We know what the final viewport of the animation is going to be, so
            // immediately request a draw of that area by setting the display port
            // accordingly. This way we should have the content pre-rendered by the
            // time the animation is done.
            ImmutableViewportMetrics metrics = new ImmutableViewportMetrics(viewport);
            DisplayPortMetrics displayPort = DisplayPortCalculator.calculate(metrics, null);
            adjustViewport(displayPort);
        }
    }

    /** Implementation of PanZoomTarget
     * You must hold the monitor while calling this.
     */
    public void setViewportMetrics(ViewportMetrics viewport) {
        mViewportMetrics = new ImmutableViewportMetrics(viewport);
        mView.requestRender();
        if (mGeckoIsReady) {
            geometryChanged();
        }
    }

    /** Implementation of PanZoomTarget */
    public void setForceRedraw() {
        mForceRedraw = true;
        if (mGeckoIsReady) {
            geometryChanged();
        }
    }

    /** Implementation of PanZoomTarget */
    public boolean post(Runnable action) {
        return mView.post(action);
    }

    /** Implementation of PanZoomTarget */
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
    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        if (!mGeckoIsReady) {
            return null;
        }

        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        PointF origin = viewportMetrics.getOrigin();
        float zoom = viewportMetrics.zoomFactor;
        ViewportMetrics geckoViewport = getGeckoViewportMetrics();
        PointF geckoOrigin = geckoViewport.getOrigin();
        float geckoZoom = geckoViewport.getZoomFactor();

        // viewPoint + origin gives the coordinate in device pixels from the top-left corner of the page.
        // Divided by zoom, this gives us the coordinate in CSS pixels from the top-left corner of the page.
        // geckoOrigin / geckoZoom is where Gecko thinks it is (scrollTo position) in CSS pixels from
        // the top-left corner of the page. Subtracting the two gives us the offset of the viewPoint from
        // the current Gecko coordinate in CSS pixels.
        PointF layerPoint = new PointF(
                ((viewPoint.x + origin.x) / zoom) - (geckoOrigin.x / geckoZoom),
                ((viewPoint.y + origin.y) / zoom) - (geckoOrigin.y / geckoZoom));

        return layerPoint;
    }

    /** Used by robocop for testing purposes. Not for production use! This is called via reflection by robocop. */
    public void setDrawListener(DrawListener listener) {
        mDrawListener = listener;
    }

    /** Used by robocop for testing purposes. Not for production use! This is used via reflection by robocop. */
    public interface DrawListener {
        public void drawFinished();
    }
}
