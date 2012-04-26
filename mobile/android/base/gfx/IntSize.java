/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.FloatSize;
import org.json.JSONException;
import org.json.JSONObject;
import java.lang.Math;

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

    public static int nextPowerOfTwo(float value) {
        return nextPowerOfTwo((int) value);
    }

    public IntSize nextPowerOfTwo() {
        return new IntSize(nextPowerOfTwo(width), nextPowerOfTwo(height));
    }
}

