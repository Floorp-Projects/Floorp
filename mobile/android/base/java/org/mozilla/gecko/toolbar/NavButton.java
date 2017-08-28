/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.res.TypedArray;
import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

abstract class NavButton extends ShapedButton {
    protected final Path mBorderPath;
    protected final Paint mBorderPaint;
    protected final float mBorderWidth;

    protected final int mBorderColor;
    protected final int mBorderColorPrivate;

    public NavButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.NavButton);
        mBorderColor = a.getColor(R.styleable.NavButton_borderColor,
                                  ContextCompat.getColor(context, R.color.disabled_grey));
        mBorderColorPrivate = a.getColor(R.styleable.NavButton_borderColorPrivate,
                                         ContextCompat.getColor(context, R.color.toolbar_icon_grey));
        a.recycle();

        mBorderWidth = getResources().getDimension(R.dimen.nav_button_border_width);

        // Paint to draw the border.
        mBorderPaint = new Paint();
        mBorderPaint.setAntiAlias(true);
        mBorderPaint.setStrokeWidth(mBorderWidth);
        mBorderPaint.setStyle(Paint.Style.STROKE);

        // Path is masked.
        mBorderPath = new Path();

        setPrivateMode(false);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        mBorderPaint.setColor(isPrivate ? mBorderColorPrivate : mBorderColor);
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        final double alphaRatio;
        if (getTheme().isEnabled()) {
            // When LightweightTheme is enabled, we don't want to show clear border.
            alphaRatio = 0.4;
        } else if (isEnabled()) {
            alphaRatio = 1;
        } else {
            // We also use low alpha value to present disabled state.
            alphaRatio = 0.05;
        }
        mBorderPaint.setAlpha((int) (255 * alphaRatio));

        // Draw the border on top.
        canvas.drawPath(mBorderPath, mBorderPaint);
    }

    // The drawable is constructed as per @drawable/url_bar_nav_button.
    @Override
    public void onLightweightThemeChanged() {
        final Drawable drawable = BrowserToolbar.getLightweightThemeDrawable(this, getTheme(), R.color.toolbar_grey);

        if (drawable == null) {
            return;
        }

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_PRESSED_STATE_SET, getColorDrawable(R.color.nav_button_bg_color_private_pressed));
        stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.nav_button_bg_color_pressed));
        stateList.addState(PRIVATE_FOCUSED_STATE_SET, getColorDrawable(R.color.nav_button_bg_color_private_focused));
        stateList.addState(FOCUSED_STATE_SET, getColorDrawable(R.color.nav_button_bg_color_focused));
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.nav_button_bg_color_private));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_nav_button);
    }
}
