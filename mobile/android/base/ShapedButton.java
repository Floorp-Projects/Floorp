/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public abstract class ShapedButton extends GeckoImageButton
                                   implements CanvasDelegate.DrawManager,
                                              LightweightTheme.OnChangeListener { 
    protected GeckoActivity mActivity;
    protected Path mPath;
    protected CurveTowards mSide;
    protected CanvasDelegate mCanvasDelegate;

    protected enum CurveTowards { NONE, LEFT, RIGHT };

    abstract public void onLightweightThemeChanged();
    abstract public void onLightweightThemeReset();

    public ShapedButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        mActivity = (GeckoActivity) context;

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BrowserToolbarCurve);
        int curveTowards = a.getInt(R.styleable.BrowserToolbarCurve_curveTowards, 0x02);
        a.recycle();

        if (curveTowards == 0x00)
            mSide = CurveTowards.NONE;
        else if (curveTowards == 0x01)
            mSide = CurveTowards.LEFT;
        else
            mSide = CurveTowards.RIGHT;

        setWillNotDraw(false);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mActivity.getLightweightTheme().addListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mActivity.getLightweightTheme().removeListener(this);
    }

    @Override
    public void draw(Canvas canvas) {
        if (mCanvasDelegate != null)
            mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());
        else
            defaultDraw(canvas);
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }

    @Override
    public void setBackgroundDrawable(Drawable drawable) {
        if (getBackground() == null || drawable == null) {
            super.setBackgroundDrawable(drawable);
            return;
        }

        int[] padding =  new int[] { getPaddingLeft(),
                                     getPaddingTop(),
                                     getPaddingRight(),
                                     getPaddingBottom()
                                   };
        drawable.setLevel(getBackground().getLevel());
        super.setBackgroundDrawable(drawable);

        setPadding(padding[0], padding[1], padding[2], padding[3]);
    }

    @Override
    public void setBackgroundResource(int resId) {
        if (getBackground() == null || resId == 0) {
            super.setBackgroundResource(resId);
            return;
        }

        setBackgroundDrawable(getContext().getResources().getDrawable(resId));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }

}
