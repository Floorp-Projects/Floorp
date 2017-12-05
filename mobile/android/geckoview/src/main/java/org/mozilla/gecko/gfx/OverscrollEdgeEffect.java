/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.widget.EdgeEffect;

import java.lang.reflect.Field;

public final class OverscrollEdgeEffect {
    // Used to index particular edges in the edges array
    private static final int TOP = 0;
    private static final int BOTTOM = 1;
    private static final int LEFT = 2;
    private static final int RIGHT = 3;

    /* package */ static final int AXIS_X = 0;
    /* package */ static final int AXIS_Y = 1;

    // All four edges of the screen
    private final EdgeEffect[] mEdges = new EdgeEffect[4];

    private final LayerSession mSession;
    private Runnable mInvalidationCallback;
    private int mWidth;
    private int mHeight;

    /* package */ OverscrollEdgeEffect(final LayerSession session) {
        mSession = session;
    }

    /**
     * Set the theme to use for overscroll from a given Context.
     *
     * @param context Context to use for the overscroll theme.
     */
    public void setTheme(final Context context) {
        ThreadUtils.assertOnUiThread();

        final PorterDuffXfermode mode = new PorterDuffXfermode(PorterDuff.Mode.SRC);
        Field paintField = null;

        if (Build.VERSION.SDK_INT >= 21) {
            try {
                paintField = EdgeEffect.class.getDeclaredField("mPaint");
                paintField.setAccessible(true);
            } catch (NoSuchFieldException e) {
            }
        }

        for (int i = 0; i < mEdges.length; i++) {
            mEdges[i] = new EdgeEffect(context);

            if (paintField == null) {
                continue;
            }

            try {
                final Paint p = (Paint) paintField.get(mEdges[i]);

                // The Android EdgeEffect class uses a mode of SRC_ATOP here, which means
                // it will only draw the effect where there are non-transparent pixels in
                // the destination. Since the LayerView itself is fully transparent, it
                // doesn't display at all. We need to use SRC instead.
                p.setXfermode(mode);
            } catch (IllegalAccessException e) {
            }
        }
    }

    /**
     * Set a Runnable that acts as a callback to invalidate the overscroll effect (for
     * example, as a response to user fling for example). The Runnbale should schedule a
     * future call to {@link #draw(Canvas)} as a result of the invalidation.
     *
     * @param runnable Invalidation Runnable.
     * @see #getInvalidationCallback()
     */
    public void setInvalidationCallback(final Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        mInvalidationCallback = runnable;
    }

    /**
     * Get the current invalidatation Runnable.
     *
     * @return Invalidation Runnable.
     * @see #setInvalidationCallback(Runnable)
     */
    public Runnable getInvalidationCallback() {
        ThreadUtils.assertOnUiThread();
        return mInvalidationCallback;
    }

    /* package */ void setSize(final int width, final int height) {
        mEdges[LEFT].setSize(height, width);
        mEdges[RIGHT].setSize(height, width);
        mEdges[TOP].setSize(width, height);
        mEdges[BOTTOM].setSize(width, height);

        mWidth = width;
        mHeight = height;
    }

    private EdgeEffect getEdgeForAxisAndSide(final int axis, final float side) {
        if (axis == AXIS_Y) {
            if (side < 0) {
                return mEdges[TOP];
            } else {
                return mEdges[BOTTOM];
            }
        } else {
            if (side < 0) {
                return mEdges[LEFT];
            } else {
                return mEdges[RIGHT];
            }
        }
    }

    /* package */ void setVelocity(final float velocity, final int axis) {
        final EdgeEffect edge = getEdgeForAxisAndSide(axis, velocity);

        // If we're showing overscroll already, start fading it out.
        if (!edge.isFinished()) {
            edge.onRelease();
        } else {
            // Otherwise, show an absorb effect
            edge.onAbsorb((int)velocity);
        }

        if (mInvalidationCallback != null) {
            mInvalidationCallback.run();
        }
    }

    /* package */ void setDistance(final float distance, final int axis) {
        // The first overscroll event often has zero distance. Throw it out
        if (distance == 0.0f) {
            return;
        }

        final EdgeEffect edge = getEdgeForAxisAndSide(axis, (int)distance);
        edge.onPull(distance / (axis == AXIS_X ? mWidth : mHeight));

        if (mInvalidationCallback != null) {
            mInvalidationCallback.run();
        }
    }

    /**
     * Draw the overscroll effect on a Canvas.
     *
     * @param canvas Canvas to draw on.
     */
    public void draw(final Canvas canvas) {
        ThreadUtils.assertOnUiThread();

        final Rect pageRect = new Rect();
        mSession.getSurfaceBounds(pageRect);

        // If we're pulling an edge, or fading it out, draw!
        boolean invalidate = false;
        if (!mEdges[TOP].isFinished()) {
            invalidate |= draw(mEdges[TOP], canvas, pageRect.left, pageRect.top, 0);
        }

        if (!mEdges[BOTTOM].isFinished()) {
            invalidate |= draw(mEdges[BOTTOM], canvas, pageRect.right, pageRect.bottom, 180);
        }

        if (!mEdges[LEFT].isFinished()) {
            invalidate |= draw(mEdges[LEFT], canvas, pageRect.left, pageRect.bottom, 270);
        }

        if (!mEdges[RIGHT].isFinished()) {
            invalidate |= draw(mEdges[RIGHT], canvas, pageRect.right, pageRect.top, 90);
        }

        // If the edge effect is animating off screen, invalidate.
        if (invalidate && mInvalidationCallback != null) {
            mInvalidationCallback.run();
        }
    }

    private static boolean draw(final EdgeEffect edge, final Canvas canvas, final float translateX, final float translateY, final float rotation) {
        final int state = canvas.save();
        canvas.translate(translateX, translateY);
        canvas.rotate(rotation);
        boolean invalidate = edge.draw(canvas);
        canvas.restoreToCount(state);

        return invalidate;
    }
}
