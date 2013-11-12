/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import org.mozilla.gecko.util.FloatUtils;

import android.graphics.RectF;

/*
 * This class keeps track of the area we request Gecko to paint, as well
 * as the resolution of the paint. The area may be different from the visible
 * area of the page, and the resolution may be different from the resolution
 * used in the compositor to render the page. This is so that we can ask Gecko
 * to paint a much larger area without using extra memory, and then render some
 * subsection of that with compositor scaling.
 */
public final class DisplayPortMetrics {
    @WrapElementForJNI
    public final float resolution;
    @WrapElementForJNI
    private final RectF mPosition;

    public DisplayPortMetrics() {
        this(0, 0, 0, 0, 1);
    }

    @WrapElementForJNI
    public DisplayPortMetrics(float left, float top, float right, float bottom, float resolution) {
        this.resolution = resolution;
        mPosition = new RectF(left, top, right, bottom);
    }

    public float getLeft() {
        return mPosition.left;
    }

    public float getTop() {
        return mPosition.top;
    }

    public float getRight() {
        return mPosition.right;
    }

    public float getBottom() {
        return mPosition.bottom;
    }

    public boolean contains(RectF rect) {
        return mPosition.contains(rect);
    }

    public boolean fuzzyEquals(DisplayPortMetrics metrics) {
        return RectUtils.fuzzyEquals(mPosition, metrics.mPosition)
            && FloatUtils.fuzzyEquals(resolution, metrics.resolution);
    }

    public String toJSON() {
        StringBuilder sb = new StringBuilder(256);
        sb.append("{ \"left\": ").append(mPosition.left)
          .append(", \"top\": ").append(mPosition.top)
          .append(", \"right\": ").append(mPosition.right)
          .append(", \"bottom\": ").append(mPosition.bottom)
          .append(", \"resolution\": ").append(resolution)
          .append('}');
        return sb.toString();
    }

    @Override
    public String toString() {
        return "DisplayPortMetrics v=(" + mPosition.left + "," + mPosition.top + "," + mPosition.right + ","
                + mPosition.bottom + ") z=" + resolution;
    }
}
