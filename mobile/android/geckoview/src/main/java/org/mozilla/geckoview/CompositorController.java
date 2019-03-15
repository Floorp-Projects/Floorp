/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.Color;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import java.util.ArrayList;
import java.util.List;

@UiThread
public final class CompositorController {
    private final GeckoSession.Compositor mCompositor;

    private List<Runnable> mDrawCallbacks;
    private int mDefaultClearColor = Color.WHITE;
    private Runnable mFirstPaintCallback;

    /* package */ CompositorController(final GeckoSession session) {
        mCompositor = session.mCompositor;
    }

    /* package */ void onCompositorReady() {
        mCompositor.setDefaultClearColor(mDefaultClearColor);
        mCompositor.enableLayerUpdateNotifications(
                mDrawCallbacks != null && !mDrawCallbacks.isEmpty());
    }

    /* package */ void onCompositorDetached() {
        if (mDrawCallbacks != null) {
            mDrawCallbacks.clear();
        }
    }

    /* package */ void notifyDrawCallbacks() {
        if (mDrawCallbacks != null) {
            for (final Runnable callback : mDrawCallbacks) {
                callback.run();
            }
        }
    }

    /**
     * Add a callback to run when drawing (layer update) occurs.
     *
     * @param callback Callback to add.
     */
    @RobocopTarget
    public void addDrawCallback(final @NonNull Runnable callback) {
        ThreadUtils.assertOnUiThread();

        if (mDrawCallbacks == null) {
            mDrawCallbacks = new ArrayList<Runnable>(2);
        }

        if (mDrawCallbacks.add(callback) && mDrawCallbacks.size() == 1 &&
                mCompositor.isReady()) {
            mCompositor.enableLayerUpdateNotifications(true);
        }
    }

    /**
     * Remove a previous draw callback.
     *
     * @param callback Callback to remove.
     */
    @RobocopTarget
    public void removeDrawCallback(final @NonNull Runnable callback) {
        ThreadUtils.assertOnUiThread();

        if (mDrawCallbacks == null) {
            return;
        }

        if (mDrawCallbacks.remove(callback) && mDrawCallbacks.isEmpty() &&
                mCompositor.isReady()) {
            mCompositor.enableLayerUpdateNotifications(false);
        }
    }

    /**
     * Get the current clear color when drawing.
     *
     * @return Curent clear color.
     */
    public int getClearColor() {
        ThreadUtils.assertOnUiThread();
        return mDefaultClearColor;
    }

    /**
     * Set the clear color when drawing. Default is Color.WHITE.
     *
     * @param color Clear color.
     */
    public void setClearColor(final int color) {
        ThreadUtils.assertOnUiThread();

        mDefaultClearColor = color;
        if (mCompositor.isReady()) {
            mCompositor.setDefaultClearColor(mDefaultClearColor);
        }
    }

    /**
     * Get the current first paint callback.
     *
     * @return Current first paint callback or null if not set.
     */
    public @Nullable Runnable getFirstPaintCallback() {
        ThreadUtils.assertOnUiThread();
        return mFirstPaintCallback;
    }

    /**
     * Set a callback to run when a document is first drawn.
     *
     * @param callback First paint callback.
     */
    public void setFirstPaintCallback(final @Nullable Runnable callback) {
        ThreadUtils.assertOnUiThread();
        mFirstPaintCallback = callback;
    }

    /* package */ void onFirstPaint() {
        if (mFirstPaintCallback != null) {
            mFirstPaintCallback.run();
        }
    }
}
