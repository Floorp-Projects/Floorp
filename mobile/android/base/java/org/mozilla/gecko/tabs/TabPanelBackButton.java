/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.ImageButton;

public class TabPanelBackButton extends ImageButton {

    private int dividerWidth = 0;

    private final Drawable divider;
    private final int dividerPadding;

    public TabPanelBackButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabPanelBackButton);
        divider = a.getDrawable(R.styleable.TabPanelBackButton_rightDivider);
        dividerPadding = (int) a.getDimension(R.styleable.TabPanelBackButton_dividerVerticalPadding, 0);
        a.recycle();

        if (divider != null) {
            dividerWidth = divider.getIntrinsicWidth();
        }
        //  Support RTL
        DrawableCompat.setAutoMirrored(getDrawable(), true);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        setMeasuredDimension(getMeasuredWidth() + dividerWidth, getMeasuredHeight());
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (divider != null) {
            final ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) getLayoutParams();

            if (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_RTL) {
                final int start = getLeft() + lp.getMarginStart();
                divider.setBounds(
                        start,
                        getPaddingTop() + dividerPadding,
                        start + dividerWidth,
                        getHeight() - getPaddingBottom() - dividerPadding
                );
                divider.draw(canvas);
            } else {
                final int left = getRight() - lp.rightMargin - dividerWidth;

                divider.setBounds(left, getPaddingTop() + dividerPadding,
                        left + dividerWidth, getHeight() - getPaddingBottom() - dividerPadding);
                divider.draw(canvas);
            }
        }
    }
}
