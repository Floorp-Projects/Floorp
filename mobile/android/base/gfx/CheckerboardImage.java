/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import android.graphics.Color;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;
import java.util.Arrays;

/** A Cairo image that displays a tinted checkerboard. */
public class CheckerboardImage extends CairoImage {
    // The width and height of the checkerboard tile.
    private static final int SIZE = 16;
    // The pixel format of the checkerboard tile.
    private static final int FORMAT = CairoImage.FORMAT_RGB16_565;
    // The color to mix in to tint the background color.
    private static final int TINT_COLOR = Color.GRAY;
    // The amount to mix in.
    private static final float TINT_OPACITY = 0.4f;

    private ByteBuffer mBuffer;
    private int mMainColor;
    private boolean mShowChecks;

    /** Creates a new checkerboard image. */
    public CheckerboardImage() {
        int bpp = CairoUtils.bitsPerPixelForCairoFormat(FORMAT);
        mBuffer = GeckoAppShell.allocateDirectBuffer(SIZE * SIZE * bpp / 8);
        update(true, Color.WHITE);
    }

    /** Returns the current color of the checkerboard. */
    public int getColor() {
        return mMainColor;
    }

    /** Returns whether or not we are currently showing checks on the checkerboard. */
    public boolean getShowChecks() {
        return mShowChecks;
    }

    /** Updates the checkerboard image. If showChecks is true, then create a
        checkerboard image that is tinted to the color. Otherwise just return a flat
        image of the color. */
    public void update(boolean showChecks, int color) {
        mMainColor = color;
        mShowChecks = showChecks;

        short mainColor16 = convertTo16Bit(mMainColor);

        mBuffer.rewind();
        ShortBuffer shortBuffer = mBuffer.asShortBuffer();

        if (!mShowChecks) {
            short color16 = convertTo16Bit(mMainColor);
            short[] fillBuffer = new short[SIZE];
            Arrays.fill(fillBuffer, color16);

            for (int i = 0; i < SIZE; i++) {
                shortBuffer.put(fillBuffer);
            }

            return;
        }

        short tintColor16 = convertTo16Bit(tint(mMainColor));

        short[] mainPattern = new short[SIZE / 2], tintPattern = new short[SIZE / 2];
        Arrays.fill(mainPattern, mainColor16);
        Arrays.fill(tintPattern, tintColor16);

        // The checkerboard pattern looks like this:
        //
        // +---+---+
        // | N | T |  N = normal
        // +---+---+  T = tinted
        // | T | N |
        // +---+---+

        for (int i = 0; i < SIZE / 2; i++) {
            shortBuffer.put(mainPattern);
            shortBuffer.put(tintPattern);
        }
        for (int i = SIZE / 2; i < SIZE; i++) {
            shortBuffer.put(tintPattern);
            shortBuffer.put(mainPattern);
        }
    }

    // Tints the given color appropriately and returns the tinted color.
    private int tint(int color) {
        float negTintOpacity = 1.0f - TINT_OPACITY;
        float r = Color.red(color) * negTintOpacity + Color.red(TINT_COLOR) * TINT_OPACITY;
        float g = Color.green(color) * negTintOpacity + Color.green(TINT_COLOR) * TINT_OPACITY;
        float b = Color.blue(color) * negTintOpacity + Color.blue(TINT_COLOR) * TINT_OPACITY;
        return Color.rgb(Math.round(r), Math.round(g), Math.round(b));
    }

    // Converts a 32-bit ARGB color to 16-bit R5G6B5, truncating values and discarding the alpha
    // channel.
    private short convertTo16Bit(int color) {
        int r = Color.red(color) >> 3, g = Color.green(color) >> 2, b = Color.blue(color) >> 3;
        int c = ((r << 11) | (g << 5) | b);
        // Swap endianness.
        return (short)((c >> 8) | ((c & 0xff) << 8));
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mBuffer != null) {
                GeckoAppShell.freeDirectBuffer(mBuffer);
                mBuffer = null;
            }
        } finally {
            super.finalize();
        }
    }

    @Override
    public ByteBuffer getBuffer() {
        return mBuffer;
    }

    @Override
    public IntSize getSize() {
        return new IntSize(SIZE, SIZE);
    }

    @Override
    public int getFormat() {
        return FORMAT;
    }
}

