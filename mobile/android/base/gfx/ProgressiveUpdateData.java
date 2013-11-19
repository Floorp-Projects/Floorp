/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

/**
 * This is the data structure that's returned by the progressive tile update
 * callback function. It encompasses the current viewport and a boolean value
 * representing whether the front-end is interested in the current progressive
 * update continuing.
 */
public class ProgressiveUpdateData {
    public float x;
    public float y;
    public float width;
    public float height;
    public float scale;
    public boolean abort;

    public void setViewport(ImmutableViewportMetrics viewport) {
        this.x = viewport.viewportRectLeft;
        this.y = viewport.viewportRectTop;
        this.width = viewport.viewportRectRight - this.x;
        this.height = viewport.viewportRectBottom - this.y;
        this.scale = viewport.zoomFactor;
    }
}

