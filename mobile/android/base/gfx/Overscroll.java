/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Canvas;

public interface Overscroll {
    // The axis to show overscroll on.
    public enum Axis {
        X,
        Y,
    };

    public void draw(final Canvas canvas, final ImmutableViewportMetrics metrics);
    public void setSize(final int width, final int height);
    public void setVelocity(final float velocity, final Axis axis);
    public void setDistance(final float distance, final Axis axis);
}
