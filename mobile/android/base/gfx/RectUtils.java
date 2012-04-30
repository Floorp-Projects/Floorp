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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
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

import org.mozilla.gecko.FloatUtils;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import org.json.JSONException;
import org.json.JSONObject;

public final class RectUtils {
    public static Rect create(JSONObject json) {
        try {
            int x = json.getInt("x");
            int y = json.getInt("y");
            int width = json.getInt("width");
            int height = json.getInt("height");
            return new Rect(x, y, x + width, y + height);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public static String toJSON(RectF rect) {
        StringBuffer sb = new StringBuffer(256);
        sb.append("{ \"left\": ").append(rect.left)
          .append(", \"top\": ").append(rect.top)
          .append(", \"right\": ").append(rect.right)
          .append(", \"bottom\": ").append(rect.bottom)
          .append('}');
        return sb.toString();
    }

    public static RectF expand(RectF rect, float moreWidth, float moreHeight) {
        float halfMoreWidth = moreWidth / 2;
        float halfMoreHeight = moreHeight / 2;
        return new RectF(rect.left - halfMoreWidth,
                         rect.top - halfMoreHeight,
                         rect.right + halfMoreWidth,
                         rect.bottom + halfMoreHeight);
    }

    public static RectF contract(RectF rect, float lessWidth, float lessHeight) {
        float halfLessWidth = lessWidth / 2.0f;
        float halfLessHeight = lessHeight / 2.0f;
        return new RectF(rect.left + halfLessWidth,
                         rect.top + halfLessHeight,
                         rect.right - halfLessWidth,
                         rect.bottom - halfLessHeight);
    }

    public static RectF intersect(RectF one, RectF two) {
        float left = Math.max(one.left, two.left);
        float top = Math.max(one.top, two.top);
        float right = Math.min(one.right, two.right);
        float bottom = Math.min(one.bottom, two.bottom);
        return new RectF(left, top, Math.max(right, left), Math.max(bottom, top));
    }

    public static RectF scale(RectF rect, float scale) {
        float x = rect.left * scale;
        float y = rect.top * scale;
        return new RectF(x, y,
                         x + (rect.width() * scale),
                         y + (rect.height() * scale));
    }

    /** Returns the nearest integer rect of the given rect. */
    public static Rect round(RectF rect) {
        return new Rect(Math.round(rect.left), Math.round(rect.top),
                        Math.round(rect.right), Math.round(rect.bottom));
    }

    public static Rect roundIn(RectF rect) {
        return new Rect((int)Math.ceil(rect.left), (int)Math.ceil(rect.top),
                        (int)Math.floor(rect.right), (int)Math.floor(rect.bottom));
    }

    public static IntSize getSize(Rect rect) {
        return new IntSize(rect.width(), rect.height());
    }

    public static Point getOrigin(Rect rect) {
        return new Point(rect.left, rect.top);
    }

    public static PointF getOrigin(RectF rect) {
        return new PointF(rect.left, rect.top);
    }

    /*
     * Returns the rect that represents a linear transition between `from` and `to` at time `t`,
     * which is on the scale [0, 1).
     */
    public static RectF interpolate(RectF from, RectF to, float t) {
        return new RectF(FloatUtils.interpolate(from.left, to.left, t),
                         FloatUtils.interpolate(from.top, to.top, t),
                         FloatUtils.interpolate(from.right, to.right, t),
                         FloatUtils.interpolate(from.bottom, to.bottom, t));
    }

    public static boolean fuzzyEquals(RectF a, RectF b) {
        if (a == null && b == null)
            return true;
        else if ((a == null && b != null) || (a != null && b == null))
            return false;
        else
            return FloatUtils.fuzzyEquals(a.top, b.top)
                && FloatUtils.fuzzyEquals(a.left, b.left)
                && FloatUtils.fuzzyEquals(a.right, b.right)
                && FloatUtils.fuzzyEquals(a.bottom, b.bottom);
    }
}
