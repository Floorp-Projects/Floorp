/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.Rect;
import android.opengl.GLES20;

/**
 * Base class for tile layers, which encapsulate the logic needed to draw textured tiles in OpenGL
 * ES.
 */
public abstract class TileLayer extends Layer {
    private static final String LOGTAG = "GeckoTileLayer";

    protected final BufferedImage mImage;

    public enum PaintMode { NORMAL, REPEAT, STRETCH };
    private PaintMode mPaintMode;

    public TileLayer(BufferedImage image, PaintMode paintMode) {
        super(image.getSize());

        mPaintMode = paintMode;
        mImage = image;
    }

    protected boolean repeats() { return mPaintMode == PaintMode.REPEAT; }
    protected boolean stretches() { return mPaintMode == PaintMode.STRETCH; }

    public abstract void destroy();

    public void setPaintMode(PaintMode mode) {
        mPaintMode = mode;
    }
}

