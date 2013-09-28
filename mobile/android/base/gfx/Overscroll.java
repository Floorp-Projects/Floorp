/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.content.Context;
import android.graphics.Canvas;
import android.support.v4.widget.EdgeEffectCompat;
import android.support.v4.view.ViewCompat;
import android.view.View;

public class Overscroll {
    // Used to index particular edges in the edges array
    private static final int TOP = 0;
    private static final int BOTTOM = 1;
    private static final int LEFT = 2;
    private static final int RIGHT = 3;

    // All four edges of the screen
    private final EdgeEffectCompat[] mEdges = new EdgeEffectCompat[4];

    // The view we're showing this overscroll on.
    private final View mView;

    // The axis to show overscroll on.
    public enum Axis {
        X,
        Y,
    };

    public Overscroll(final View v) {
        mView = v;
        Context context = v.getContext();
        for (int i = 0; i < 4; i++) {
            mEdges[i] = new EdgeEffectCompat(context);
        }
    }

    public void setSize(final int width, final int height) {
        mEdges[LEFT].setSize(height, width);
        mEdges[RIGHT].setSize(height, width);
        mEdges[TOP].setSize(width, height);
        mEdges[BOTTOM].setSize(width, height);
    }

    private EdgeEffectCompat getEdgeForAxisAndSide(final Axis axis, final float side) {
        if (axis == Axis.Y) {
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

    public void setVelocity(final float velocity, final Axis axis) {
        final EdgeEffectCompat edge = getEdgeForAxisAndSide(axis, velocity);

        // If we're showing overscroll already, start fading it out.
        if (!edge.isFinished()) {
            edge.onRelease();
        } else {
            // Otherwise, show an absorb effect
            edge.onAbsorb((int)velocity);
        }

        ViewCompat.postInvalidateOnAnimation(mView);
    }

    public void setDistance(final float distance, final Axis axis) {
        // The first overscroll event often has zero distance. Throw it out
        if (distance == 0.0f) {
            return;
        }

        final EdgeEffectCompat edge = getEdgeForAxisAndSide(axis, (int)distance);
        edge.onPull(distance / (axis == Axis.X ? mView.getWidth() : mView.getHeight()));
        ViewCompat.postInvalidateOnAnimation(mView);
    }

    public void draw(final Canvas canvas, final ImmutableViewportMetrics metrics) {
        if (metrics == null) {
            return;
        }

        // If we're pulling an edge, or fading it out, draw!
        boolean invalidate = false;
        if (!mEdges[TOP].isFinished()) {
            invalidate |= draw(mEdges[TOP], canvas, metrics.marginLeft, metrics.marginTop, 0);
        }

        if (!mEdges[BOTTOM].isFinished()) {
            invalidate |= draw(mEdges[BOTTOM], canvas, mView.getWidth(), mView.getHeight(), 180);
        }

        if (!mEdges[LEFT].isFinished()) {
            invalidate |= draw(mEdges[LEFT], canvas, metrics.marginLeft, mView.getHeight(), 270);
        }

        if (!mEdges[RIGHT].isFinished()) {
            invalidate |= draw(mEdges[RIGHT], canvas, mView.getWidth(), metrics.marginTop, 90);
        }

        // If the edge effect is animating off screen, invalidate.
        if (invalidate) {
            ViewCompat.postInvalidateOnAnimation(mView);
        }
    }

    public boolean draw(final EdgeEffectCompat edge, final Canvas canvas, final float translateX, final float translateY, final float rotation) {
        final int state = canvas.save();
        canvas.translate(translateX, translateY);
        canvas.rotate(rotation);
        boolean invalidate = edge.draw(canvas);
        canvas.restoreToCount(state);

        return invalidate;
    }
}
