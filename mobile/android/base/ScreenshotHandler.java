/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.RectUtils;
import org.mozilla.gecko.gfx.ScreenshotLayer;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.PrefsHelper;

import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.util.FloatMath;
import android.util.Log;

import java.nio.ByteBuffer;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

public final class ScreenshotHandler implements Runnable {
    public static final int SCREENSHOT_THUMBNAIL = 0;
    public static final int SCREENSHOT_CHECKERBOARD = 1;

    private static final String SCREENSHOT_DISABLED_PREF = "gfx.java.screenshot.enabled";

    private static final String LOGTAG = "GeckoScreenshotHandler";
    private static final int BYTES_FOR_16BPP = 2;
    private static final int MAX_PIXELS_PER_SLICE = 100000;

    private static boolean sDisableScreenshot;
    private static boolean sForceDisabled;
    private static ScreenshotHandler sInstance;

    private final int mMaxTextureSize;
    private final int mMinTextureSize;
    private final int mMaxPixels;

    private final Queue<PendingScreenshot> mPendingScreenshots;
    private ByteBuffer mBuffer;
    private int mBufferWidth;
    private int mBufferHeight;
    private RectF mPageRect;
    private float mWidthRatio;
    private float mHeightRatio;

    private int mTabId;
    private RectF mDirtyRect;
    private boolean mIsRepaintRunnablePosted;

    private static synchronized ScreenshotHandler getInstance() {
        if (sDisableScreenshot) {
            return null;
        }
        if (sInstance == null) {
            try {
                sInstance = new ScreenshotHandler();
            } catch (UnsupportedOperationException e) {
                // initialization failed, fall through and return null.
                // also set the disable flag so we don't try again.
                sDisableScreenshot = true;
            }
        }
        return sInstance;
    }

    private ScreenshotHandler() {
        int[] maxTextureSize = new int[1];
        GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, maxTextureSize, 0);
        mMaxTextureSize = maxTextureSize[0];
        if (mMaxTextureSize == 0) {
            throw new UnsupportedOperationException();
        }
        mMaxPixels = Math.min(ScreenshotLayer.getMaxNumPixels(), mMaxTextureSize * mMaxTextureSize);
        mMinTextureSize = (int)Math.ceil(mMaxPixels / mMaxTextureSize);
        mPendingScreenshots = new LinkedList<PendingScreenshot>();
        mBuffer = DirectBufferAllocator.allocate(mMaxPixels * BYTES_FOR_16BPP);
        mDirtyRect = new RectF();
        clearDirtyRect();
        PrefsHelper.getPref(SCREENSHOT_DISABLED_PREF,
             new PrefsHelper.PrefHandlerBase() {
                  @Override public void prefValue(String pref, boolean value) {
                      if (SCREENSHOT_DISABLED_PREF.equals(pref) && !value)
                          disableScreenshot(true);
                  }
             }
            );
    }

    private void cleanup() {
        synchronized (mPendingScreenshots) {
            if (mPendingScreenshots.isEmpty()) {
                // no screenshots are pending, its safe to free the buffer
                mBuffer = DirectBufferAllocator.free(mBuffer);
            } else {
                discardPendingScreenshots();
                mBuffer = null;
            }
        }
    }

    // Invoked via reflection from robocop test
    public static synchronized void disableScreenshot() {
        disableScreenshot(true);
    }

    // Invoked via reflection from robocop test
    public static synchronized void disableScreenshot(boolean forced) {
        if (sDisableScreenshot) {
            if (!sForceDisabled)
                sForceDisabled = forced;
            return;
        }

        sForceDisabled = forced;

        sDisableScreenshot = true;
        if (sInstance != null) {
            sInstance.cleanup();
            sInstance = null;
        }
        Log.i(LOGTAG, "Screenshotting disabled");
    }

    public static synchronized void enableScreenshot(boolean forced) {
        if (!sDisableScreenshot || (sForceDisabled && !forced)) {
            return;
        }
        sDisableScreenshot = false;
        sForceDisabled = false;
        Log.i(LOGTAG, "Screenshotting enabled");
    }

    public static void screenshotWholePage(Tab tab) {
        if (GeckoApp.mAppContext.isApplicationInBackground()) {
            return;
        }
        ScreenshotHandler handler = getInstance();
        if (handler == null) {
            return;
        }

        handler.screenshotWholePage(tab.getId());
    }

    private void discardPendingScreenshots() {
        synchronized (mPendingScreenshots) {
            for (Iterator<PendingScreenshot> i = mPendingScreenshots.iterator(); i.hasNext(); ) {
                i.next().discard();
            }
        }
    }

    private void screenshotWholePage(int tabId) {
        LayerView layerView = GeckoApp.mAppContext.getLayerView();
        if (layerView == null) {
            return;
        }
        ImmutableViewportMetrics viewport = layerView.getViewportMetrics();
        RectF pageRect = viewport.getCssPageRect();

        if (FloatUtils.fuzzyEquals(pageRect.width(), 0) || FloatUtils.fuzzyEquals(pageRect.height(), 0)) {
            return;
        }

        synchronized (this) {
            // if we're doing a full-page screenshot, toss any
            // dirty rects we have saved up and reset the tab id.
            mTabId = tabId;
            clearDirtyRect();
        }
        discardPendingScreenshots();

        int dstx = 0;
        int dsty = 0;
        float bestZoomFactor = (float)Math.sqrt(pageRect.width() * pageRect.height() / mMaxPixels);
        int dstw = IntSize.largestPowerOfTwoLessThan(pageRect.width() / bestZoomFactor);
        // clamp with min texture size so that the height doesn't exceed the sMaxTextureSize
        dstw = clamp(mMinTextureSize, dstw, mMaxTextureSize);
        int dsth = mMaxPixels / dstw;

        mPageRect = pageRect;
        mBufferWidth = dstw;
        mBufferHeight = dsth;
        mWidthRatio = dstw / pageRect.width();
        mHeightRatio = dsth / pageRect.height();

        scheduleCheckerboardScreenshotEvent(pageRect, dstx, dsty, dstw, dsth);
    }

    private static int clamp(int min, int val, int max) {
        return Math.max(Math.min(max, val), min);
    }

    // Called from native code by JNI
    public static void notifyPaintedRect(float top, float left, float bottom, float right) {
        ScreenshotHandler handler = getInstance();
        if (handler == null) {
            return;
        }

        handler.notifyPageUpdated(top, left, bottom, right);
    }

    private void notifyPageUpdated(float top, float left, float bottom, float right) {
        synchronized (this) {
            if (mPageRect == null || Tabs.getInstance().getSelectedTab().getId() != mTabId) {
                // if mPageRect is null, we haven't done a full-page
                // screenshot yet (or screenshotWholePage failed for some reason),
                // so ignore partial updates. also if the tab changed, ignore
                // partial updates until we do the next whole-page screenshot.
                return;
            }
            mDirtyRect.top = Math.max(mPageRect.top, Math.min(top, mDirtyRect.top));
            mDirtyRect.left = Math.max(mPageRect.left, Math.min(left, mDirtyRect.left));
            mDirtyRect.bottom = Math.min(mPageRect.bottom, Math.max(bottom, mDirtyRect.bottom));
            mDirtyRect.right = Math.min(mPageRect.right, Math.max(right, mDirtyRect.right));
            if (!mIsRepaintRunnablePosted) {
                GeckoAppShell.getHandler().postDelayed(this, 5000);
                mIsRepaintRunnablePosted = true;
            }
        }
    }

    private void clearDirtyRect() {
        mDirtyRect.set(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY,
                       Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY);
    }

    public void run() {
        // make a copy of the dirty rect to work with so we can keep
        // accumulating new dirty rects.
        RectF dirtyRect = new RectF();
        synchronized (this) {
            dirtyRect.set(mDirtyRect);
            clearDirtyRect();
            mIsRepaintRunnablePosted = false;
        }

        if (dirtyRect.width() <= 0 || dirtyRect.height() <= 0) {
            // we have nothing in the dirty rect, so nothing to do
            return;
        }

        Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab == null || selectedTab.getId() != mTabId) {
            // tab changed, so bail out before we start screenshotting
            // the wrong tab. note we must do this *after* resetting
            // mIsRepaintRunnablePosted above.
            return;
        }

        LayerView layerView = GeckoApp.mAppContext.getLayerView();
        if (layerView == null) {
            // we could be in the midst of an activity tear-down and re-start, so guard
            // against a null layer view
            return;
        }

        ImmutableViewportMetrics viewport = layerView.getViewportMetrics();
        if (RectUtils.fuzzyEquals(mPageRect, viewport.getCssPageRect())) {
            // the page size hasn't changed, so our dirty rect is still valid and we can just
            // repaint that area
            int dstx = (int)(mWidthRatio * (dirtyRect.left - viewport.cssPageRectLeft));
            int dsty = (int)(mHeightRatio * (dirtyRect.top - viewport.cssPageRectTop));
            int dstw = (int)(mWidthRatio * dirtyRect.width());
            int dsth = (int)(mHeightRatio * dirtyRect.height());
            scheduleCheckerboardScreenshotEvent(dirtyRect, dstx, dsty, dstw, dsth);
        } else {
            // the page size changed, so we need to re-screenshot the whole page
            screenshotWholePage(mTabId);
        }
    }

    private void scheduleCheckerboardScreenshotEvent(RectF srcRect, int dstx, int dsty, int dstw, int dsth) {
        int numSlices = (int)FloatMath.ceil(srcRect.width() * srcRect.height() / MAX_PIXELS_PER_SLICE);
        if (numSlices == 0 || dstw == 0 || dsth == 0) {
            return;
        }

        PendingScreenshot pending = new PendingScreenshot(mTabId);
        int sliceDstH = Math.max(1, dsth / numSlices);
        float sliceSrcH = sliceDstH * srcRect.height() / dsth;
        float srcY = srcRect.top;
        for (int i = 0; i < dsth; i += sliceDstH) {
            if (i + sliceDstH > dsth) {
                // the last slice may be smaller to account for rounding error.
                sliceDstH = dsth - i;
                sliceSrcH = sliceDstH * srcRect.height() / dsth;
            }
            GeckoEvent event = GeckoEvent.createScreenshotEvent(mTabId,
                (int)srcRect.left, (int)srcY, (int)srcRect.width(), (int)sliceSrcH,
                dstx, dsty + i, dstw, sliceDstH,
                mBufferWidth, mBufferHeight, SCREENSHOT_CHECKERBOARD, mBuffer);
            srcY += sliceSrcH;
            pending.addEvent(event);
        }
        synchronized (mPendingScreenshots) {
            mPendingScreenshots.add(pending);
            if (mPendingScreenshots.size() == 1) {
                sendNextEventToGecko();
            }
        }
    }

    private void sendNextEventToGecko() {
        synchronized (mPendingScreenshots) {
            while (!mPendingScreenshots.isEmpty()) {
                // some of the pending screenshots may have been discard()ed
                // so keep looping until we find a real one
                if (mPendingScreenshots.element().sendNextEventToGecko()) {
                    break;
                }
                mPendingScreenshots.remove();
            }
        }
    }

    // Called from native code by JNI
    synchronized public static void notifyScreenShot(final ByteBuffer data, final int tabId,
                                        final int left, final int top,
                                        final int right, final int bottom,
                                        final int bufferWidth, final int bufferHeight, final int token) {
        GeckoAppShell.getHandler().post(new Runnable() {
            public void run() {
                switch (token) {
                    case SCREENSHOT_CHECKERBOARD:
                    {
                        ScreenshotHandler handler = getInstance();
                        if (handler == null) {
                            // if the handler is null, we have a stale reference to the buffer, free it
                            DirectBufferAllocator.free(data);
                            break;
                        }
                        if (Tabs.getInstance().getSelectedTab().getId() == tabId) {
                            PendingScreenshot current;
                            synchronized (handler.mPendingScreenshots) {
                                current = handler.mPendingScreenshots.element();
                                current.slicePainted(left, top, right, bottom);
                                if (current.sendNextEventToGecko()) {
                                    break;
                                }
                                // this screenshot has all its slices done, so push it out
                                // to the layer renderer and remove it from the list
                            }
                            LayerView layerView = GeckoApp.mAppContext.getLayerView();
                            if (layerView != null) {
                                layerView.getRenderer().setCheckerboardBitmap(
                                    data, bufferWidth, bufferHeight, handler.mPageRect,
                                    current.getPaintedRegion());
                            }
                        }
                        synchronized (handler.mPendingScreenshots) {
                            handler.mPendingScreenshots.remove();
                            handler.sendNextEventToGecko();
                        }
                        break;
                    }
                    case SCREENSHOT_THUMBNAIL:
                    {
                        Tab tab = Tabs.getInstance().getTab(tabId);
                        if (tab != null) {
                            GeckoApp.mAppContext.handleThumbnailData(tab, data);
                        }
                        break;
                    }
                }
            }
        });
    }

    static class PendingScreenshot {
        private final int mTabId;
        private final LinkedList<GeckoEvent> mEvents;
        private final Rect mPainted;

        PendingScreenshot(int tabId) {
            mTabId = tabId;
            mEvents = new LinkedList<GeckoEvent>();
            mPainted = new Rect();
        }

        void addEvent(GeckoEvent event) {
            mEvents.add(event);
        }

        boolean sendNextEventToGecko() {
            if (!mEvents.isEmpty()) {
                GeckoAppShell.sendEventToGecko(mEvents.remove());
                return true;
            }
            return false;
        }

        void slicePainted(int left, int top, int right, int bottom) {
            mPainted.union(left, top, right, bottom);
        }

        Rect getPaintedRegion() {
            return mPainted;
        }

        void discard() {
            mEvents.clear();
        }
    }
}
