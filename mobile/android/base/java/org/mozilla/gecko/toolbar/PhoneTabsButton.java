/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.View;

import org.mozilla.gecko.tabs.TabCurve;

public class PhoneTabsButton extends ShapedButton {
    public PhoneTabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        redrawTabs(width, height);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        redrawTabs(getWidth(), getHeight());
    }

    private void redrawTabs(int width, int height) {
        final int layoutDirection = ViewCompat.getLayoutDirection(this);

        Point[] nodes = getDirectionalNodes(width, height, layoutDirection);
        TabCurve.Direction directionalCurve = getDirectionalCurve(layoutDirection);

        mPath.reset();

        mPath.moveTo(nodes[0].x, nodes[0].y);
        TabCurve.drawFromTop(mPath, nodes[1].x, nodes[1].y, directionalCurve);
        mPath.lineTo(nodes[2].x, nodes[2].y);
        mPath.lineTo(nodes[3].x, nodes[3].y);
        mPath.lineTo(nodes[0].x, nodes[0].y);
    }

    private static TabCurve.Direction getDirectionalCurve(int direction) {
        if (direction == ViewCompat.LAYOUT_DIRECTION_RTL) {
            //  right to LEFT
            return TabCurve.Direction.LEFT;
        } else {
            //  left to RIGHT
            return TabCurve.Direction.RIGHT;
        }
    }

    private static Point[] getDirectionalNodes(int width, int height, int layoutDirection) {
        final Point[] nodes;
        if (layoutDirection == ViewCompat.LAYOUT_DIRECTION_RTL) {
            nodes = new Point[] {
                    new Point(width, 0)
                    , new Point(width, height)
                    , new Point(0, height)
                    , new Point(0, 0)
            };
        } else {
            nodes = new Point[]{
                    new Point(0, 0)
                    , new Point(0, height)
                    , new Point(width, height)
                    , new Point(width, 0)
            };
        }
        return nodes;
    }

}
