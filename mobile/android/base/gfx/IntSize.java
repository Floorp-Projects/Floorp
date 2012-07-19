/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.util.FloatMath;
import org.json.JSONException;
import org.json.JSONObject;

public class IntSize {
    public final int width, height;

    public IntSize(IntSize size) { width = size.width; height = size.height; }
    public IntSize(int inWidth, int inHeight) { width = inWidth; height = inHeight; }

    public IntSize(FloatSize size) {
        width = Math.round(size.width);
        height = Math.round(size.height);
    }

    public IntSize(JSONObject json) {
        try {
            width = json.getInt("width");
            height = json.getInt("height");
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public int getArea() {
        return width * height;
    }

    public boolean equals(IntSize size) {
        return ((size.width == width) && (size.height == height));
    }

    public boolean isPositive() {
        return (width > 0 && height > 0);
    }

    @Override
    public String toString() { return "(" + width + "," + height + ")"; }

    public IntSize scale(float factor) {
        return new IntSize(Math.round(width * factor),
                           Math.round(height * factor));
    }

    /* Returns the power of two that is greater than or equal to value */
    public static int nextPowerOfTwo(int value) {
        // code taken from http://acius2.blogspot.com/2007/11/calculating-next-power-of-2.html
        if (0 == value--) {
            return 1;
        }
        value = (value >> 1) | value;
        value = (value >> 2) | value;
        value = (value >> 4) | value;
        value = (value >> 8) | value;
        value = (value >> 16) | value;
        return value + 1;
    }

    public IntSize nextPowerOfTwo() {
        return new IntSize(nextPowerOfTwo(width), nextPowerOfTwo(height));
    }

    public static boolean isPowerOfTwo(int value) {
        if (value == 0)
            return false;
        return (value & (value - 1)) == 0;
    }

    public static int largestPowerOfTwoLessThan(float value) {
        int val = (int)FloatMath.floor(value);
        if (val <= 0) {
            throw new IllegalArgumentException("Error: value must be > 0");
        }
        // keep dropping the least-significant set bits until only one is left
        int bestVal = val;
        while (val != 0) {
            bestVal = val;
            val &= (val - 1);
        }
        return bestVal;
    }
}

