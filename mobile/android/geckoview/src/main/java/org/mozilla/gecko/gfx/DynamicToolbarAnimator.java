/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.Bitmap;
import android.graphics.PointF;
import android.util.Log;
import android.view.MotionEvent;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;

public class DynamicToolbarAnimator {
    private static final String LOGTAG = "GeckoDynamicToolbarAnimator";

    public static enum PinReason {
        DISABLED(0),
        RELAYOUT(1),
        ACTION_MODE(2),
        FULL_SCREEN(3),
        CARET_DRAG(4),
        PAGE_LOADING(5),
        CUSTOM_TAB(6);

        public final int value;
        PinReason(final int aValue) {
            value = aValue;
        }
    }

    public interface MetricsListener {
        public void onMetricsChanged(ImmutableViewportMetrics viewport);
    }

    public interface ToolbarChromeProxy {
        public Bitmap getBitmapOfToolbarChrome();
        public boolean isToolbarChromeVisible();
        public void toggleToolbarChrome(boolean aShow);
    }

    private final Set<PinReason> mPinFlags = Collections.synchronizedSet(EnumSet.noneOf(PinReason.class));

    private final GeckoLayerClient mTarget;
    private LayerView.Compositor mCompositor;
    private final List<MetricsListener> mListeners;
    private ToolbarChromeProxy mToolbarChromeProxy;
    private int mMaxToolbarHeight;

    public DynamicToolbarAnimator(GeckoLayerClient aTarget) {
        mTarget = aTarget;
        mListeners = new ArrayList<MetricsListener>();
    }

    public void addMetricsListener(MetricsListener aListener) {
        mListeners.add(aListener);
    }

    public void removeMetricsListener(MetricsListener aListener) {
        mListeners.remove(aListener);
    }

    public void setToolbarChromeProxy(ToolbarChromeProxy aToolbarChromeProxy) {
        mToolbarChromeProxy = aToolbarChromeProxy;
    }

    /* package-private */ void onToggleChrome(boolean aShow) {
        if (mToolbarChromeProxy != null) {
            mToolbarChromeProxy.toggleToolbarChrome(aShow);
        }
    }

    /* package-private */ void onMetricsChanged(ImmutableViewportMetrics aMetrics) {
        for (MetricsListener listener : mListeners) {
            listener.onMetricsChanged(aMetrics);
        }
    }

    public void setMaxToolbarHeight(int maxToolbarHeight) {
        ThreadUtils.assertOnUiThread();
        mMaxToolbarHeight = maxToolbarHeight;
        if (isCompositorReady()) {
            mCompositor.setMaxToolbarHeight(mMaxToolbarHeight);
        }
    }

    public int getCurrentToolbarHeight() {
        if ((mToolbarChromeProxy != null) && mToolbarChromeProxy.isToolbarChromeVisible()) {
            return mMaxToolbarHeight;
        }
        return 0;
    }

    /**
     * If true, scroll changes will not affect translation.
     */
    public boolean isPinned() {
        return !mPinFlags.isEmpty();
    }

    public boolean isPinnedBy(PinReason reason) {
        return mPinFlags.contains(reason);
    }

    /* package */ void sendPinValueToCompositor(final boolean pinned, final PinReason reason) {
        if (isCompositorReady()) {
             mCompositor.setPinned(pinned, reason.value);
        }
    }

    public void setPinned(final boolean pinned, final PinReason reason) {
        // setPinned may be called from the main thread but compositor can only be accessed on UI thread
        if (pinned != mPinFlags.contains(reason)) {
            if (ThreadUtils.isOnUiThread() == true) {
                sendPinValueToCompositor(pinned, reason);
            } else {
                ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                    public void run() {
                        sendPinValueToCompositor(pinned, reason);
                    }
                });
            }
        }

        if (pinned) {
            mPinFlags.add(reason);
        } else {
            mPinFlags.remove(reason);
        }
    }

    public void showToolbar(boolean immediately) {
        if (isCompositorReady()) {
            mCompositor.sendToolbarAnimatorMessage(immediately ?
                LayerView.REQUEST_SHOW_TOOLBAR_IMMEDIATELY : LayerView.REQUEST_SHOW_TOOLBAR_ANIMATED);
        }
    }

    public void hideToolbar(boolean immediately) {
        if (isCompositorReady()) {
            mCompositor.sendToolbarAnimatorMessage(immediately ?
                LayerView.REQUEST_HIDE_TOOLBAR_IMMEDIATELY : LayerView.REQUEST_HIDE_TOOLBAR_ANIMATED);
        }
    }

    /* package-private */ IntSize getViewportSize() {
        ThreadUtils.assertOnUiThread();

        int viewWidth = mTarget.getView().getWidth();
        int viewHeight = mTarget.getView().getHeight();
        if ((mToolbarChromeProxy != null) && mToolbarChromeProxy.isToolbarChromeVisible()) {
          viewHeight -= mMaxToolbarHeight;
        }
        return new IntSize(viewWidth, viewHeight);
    }

    public PointF getVisibleEndOfLayerView() {
        return new PointF(mTarget.getView().getWidth(),
            mTarget.getView().getHeight());
    }

    /* package-private */ void updateCompositor() {
        if (isCompositorReady()) {
            mCompositor.setMaxToolbarHeight(mMaxToolbarHeight);
            if ((mToolbarChromeProxy != null) && mToolbarChromeProxy.isToolbarChromeVisible()) {
                mCompositor.sendToolbarAnimatorMessage(LayerView.REQUEST_SHOW_TOOLBAR_IMMEDIATELY);
            } else {
                mCompositor.sendToolbarAnimatorMessage(LayerView.REQUEST_HIDE_TOOLBAR_IMMEDIATELY);
            }
            for (PinReason reason : PinReason.values()) {
                mCompositor.setPinned(mPinFlags.contains(reason), reason.value);
            }
        }
    }

    /* package-private */ void notifyCompositorCreated(LayerView.Compositor aCompositor) {
        ThreadUtils.assertOnUiThread();
        mCompositor = aCompositor;
    }

    private boolean isCompositorReady() {
        return ((mCompositor != null) && (mCompositor.isReady()));
    }


    /* package-private */ Bitmap getBitmapOfToolbarChrome() {
        if (mToolbarChromeProxy != null) {
            return mToolbarChromeProxy.getBitmapOfToolbarChrome();
        }
        return null;
    }
}
