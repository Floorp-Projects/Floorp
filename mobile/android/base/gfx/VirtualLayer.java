/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Rect;

public class VirtualLayer extends Layer {
    public VirtualLayer(IntSize size) {
        super(size);
    }

    @Override
    public void draw(RenderContext context) {
        // No-op.
    }

    void setPositionAndResolution(int left, int top, int right, int bottom, float newResolution) {
        // This is an optimized version of the following code:
        // beginTransaction();
        // try {
        //     setPosition(new Rect(left, top, right, bottom));
        //     setResolution(newResolution);
        //     performUpdates(null);
        // } finally {
        //     endTransaction();
        // }

        // it is safe to drop the transaction lock in this instance (i.e. for the
        // VirtualLayer that is just a shadow of what gecko is painting) because
        // the position and resolution of this layer are always touched on the compositor
        // thread, and therefore do not require synchronization.
        mPosition.set(left, top, right, bottom);
        mResolution = newResolution;
    }
}
