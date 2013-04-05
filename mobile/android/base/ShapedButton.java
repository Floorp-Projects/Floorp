/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LevelListDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class ShapedButton extends GeckoImageButton
                          implements CanvasDelegate.DrawManager {
    protected GeckoActivity mActivity;

    private Path mPath;
    private CurveTowards mSide;

    protected CanvasDelegate mCanvasDelegate;

    private enum CurveTowards { NONE, LEFT, RIGHT };

    public ShapedButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        mActivity = (GeckoActivity) context;

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BrowserToolbarCurve);
        int curveTowards = a.getInt(R.styleable.BrowserToolbarCurve_curveTowards, 0x00);
        a.recycle();

        if (curveTowards == 0x00)
            mSide = CurveTowards.NONE;
        else if (curveTowards == 0x01)
            mSide = CurveTowards.LEFT;
        else
            mSide = CurveTowards.RIGHT;

        // Path is clipped.
        mPath = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_OUT);

        setWillNotDraw(false);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mSide == CurveTowards.NONE)
            return;

        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        int curve = (int) (height * 1.125f);

        mPath.reset();

        if (mSide == CurveTowards.RIGHT) {
            mPath.moveTo(0, 0);
            mPath.cubicTo(curve * 0.75f, 0,
                          curve * 0.25f, height,
                          curve, height);
            mPath.lineTo(0, height);
            mPath.lineTo(0, 0);
        } else if (mSide == CurveTowards.LEFT) {
            mPath.moveTo(width, 0);
            mPath.cubicTo((width - (curve * 0.75f)), 0,
                          (width - (curve * 0.25f)), height,
                          (width - curve), height);
            mPath.lineTo(width, height);
            mPath.lineTo(width, 0);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        if (mCanvasDelegate != null && mSide != CurveTowards.NONE)
            mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());
        else
            defaultDraw(canvas);
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }

    // The drawable is constructed as per @drawable/shaped_button.
    @Override
    public void onLightweightThemeChanged() {
        int background = mActivity.getResources().getColor(R.color.background_tabs);
        LightweightThemeDrawable lightWeight = mActivity.getLightweightTheme().getColorDrawable(this, background);

        if (lightWeight == null)
            return;

        lightWeight.setAlpha(34, 34);

        Resources resources = this.getContext().getResources();
        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(new int[] { android.R.attr.state_pressed }, new ColorDrawable(resources.getColor(R.color.highlight_shaped)));
        stateList.addState(new int[] { android.R.attr.state_focused }, new ColorDrawable(resources.getColor(R.color.highlight_shaped_focused)));
        stateList.addState(new int[] { R.attr.state_private }, new ColorDrawable(resources.getColor(R.color.background_tabs)));
        stateList.addState(new int[] {}, lightWeight);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.shaped_button);
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
}
