/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

public class FlowLayout extends ViewGroup {
    private static final int DEFAULT_SPACING = 5;
    private int mSpacing = DEFAULT_SPACING;

    public FlowLayout(Context context) {
        super(context);
    }

    public FlowLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, org.mozilla.gecko.R.styleable.FlowLayout);
        mSpacing = a.getDimensionPixelSize(R.styleable.FlowLayout_spacing, DEFAULT_SPACING);
        a.recycle();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int parentWidth = MeasureSpec.getSize(widthMeasureSpec);
        final int childCount = getChildCount();
        int rowWidth = 0;
        int totalWidth = 0;
        int totalHeight = 0;
        boolean firstChild = true;

        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() == GONE)
                continue;

            measureChild(child, widthMeasureSpec, heightMeasureSpec);

            final int childWidth = child.getMeasuredWidth();
            final int childHeight = child.getMeasuredHeight();

            if (firstChild || (rowWidth + childWidth > parentWidth)) {
                rowWidth = 0;
                totalHeight += childHeight;
                if (!firstChild)
                    totalHeight += mSpacing;
                firstChild = false;
            }

            rowWidth += childWidth;

            if (rowWidth > totalWidth)
                totalWidth = rowWidth;

            rowWidth += mSpacing;
        }

        setMeasuredDimension(totalWidth, totalHeight);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        final int childCount = getChildCount();
        final int totalWidth = r - l;
        int x = 0;
        int y = 0;
        int prevChildHeight = 0;

        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() == GONE)
                continue;

            final int childWidth = child.getMeasuredWidth();
            final int childHeight = child.getMeasuredHeight();
            if (x + childWidth > totalWidth) {
                x = 0;
                y += prevChildHeight + mSpacing;
            }
            prevChildHeight = childHeight;
            child.layout(x, y, x + childWidth, y + childHeight);
            x += childWidth + mSpacing;
        }
    }
}
