/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class BrowserToolbarBackground extends GeckoLinearLayout
                                      implements CanvasDelegate.DrawManager {
    private GeckoActivity mActivity;
    private Path mPath;
    private CurveTowards mSide;
    private CanvasDelegate mCanvasDelegate;

    public enum CurveTowards { NONE, LEFT, RIGHT };

    public BrowserToolbarBackground(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BrowserToolbarCurve);
        int curveTowards = a.getInt(R.styleable.BrowserToolbarCurve_curveTowards, 0x02);
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
        mActivity = (GeckoActivity) context;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        int curve = (int) (height * 1.125f);

        mPath.reset();

        if (mSide == CurveTowards.LEFT) {
            mPath.moveTo(0, height);
            mPath.cubicTo(curve * 0.75f, height,
                          curve * 0.25f, 0,
                          curve, 0);
            mPath.lineTo(0, 0);
            mPath.lineTo(0, height);
        } else if (mSide == CurveTowards.RIGHT) {
            mPath.moveTo(width, height);
            mPath.cubicTo((width - (curve * 0.75f)), height,
                          (width - (curve * 0.25f)), 0,
                          (width - curve), 0);
            mPath.lineTo(width, 0);
            mPath.lineTo(width, height);
        }
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }

    @Override
    public void onLightweightThemeChanged() {
        Drawable drawable = mActivity.getLightweightTheme().getDrawable(this);
        if (drawable == null)
            return;

        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(new int[] { R.attr.state_private }, new ColorDrawable(mActivity.getResources().getColor(R.color.background_private)));
        stateList.addState(new int[] {}, drawable);

        int[] padding =  new int[] { getPaddingLeft(),
                                     getPaddingTop(),
                                     getPaddingRight(),
                                     getPaddingBottom()
                                   };
        setBackgroundDrawable(stateList);
        setPadding(padding[0], padding[1], padding[2], padding[3]);
    }

    @Override
    public void onLightweightThemeReset() {
        int[] padding =  new int[] { getPaddingLeft(),
                                     getPaddingTop(),
                                     getPaddingRight(),
                                     getPaddingBottom()
                                   };
        setBackgroundResource(R.drawable.address_bar_bg);
        setPadding(padding[0], padding[1], padding[2], padding[3]);
    }

    public CurveTowards getCurveTowards() {
        return mSide;
    }

    public void setCurveTowards(CurveTowards side) {
        if (side == mSide)
            return;

        mSide = side;
        requestLayout();
    }
}
