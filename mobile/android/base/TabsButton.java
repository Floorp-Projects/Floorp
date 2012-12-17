/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuffXfermode;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.LevelListDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;

public class TabsButton extends ShapedButton { 
    private Paint mPaint;

    private Path mBackgroundPath;
    private Path mLeftCurve;
    private Path mRightCurve;

    private boolean mCropped;
    private boolean mSideBar;

    public TabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsButton);
        mCropped = a.getBoolean(R.styleable.TabsButton_cropped, false);
        a.recycle();

        a = context.obtainStyledAttributes(attrs, R.styleable.TabsPanel);
        mSideBar = a.getBoolean(R.styleable.TabsPanel_sidebar, false);
        a.recycle();

        // Paint to draw the background.
        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setColor(0xFF000000);
        mPaint.setStrokeWidth(0.0f);

        // Path is masked.
        mPath = new Path();
        mBackgroundPath = new Path();
        mLeftCurve = new Path();
        mRightCurve = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_IN);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        float curve = height * 1.125f;

        // The bounds for the rectangle to carve the curves.
        float left;
        float right;
        float top;
        float bottom;

        if (mSide == CurveTowards.RIGHT) {
            left = 0;
            right = width;
            top = 0;
            bottom = height;
        } else {
            left = 0;
            right = width;
            top = height;
            bottom = 0;
        }

        mLeftCurve.reset();
        mLeftCurve.moveTo(left, top);

        if (mCropped && mSide == CurveTowards.LEFT) {
            mLeftCurve.lineTo(left, top/2);
            mLeftCurve.quadTo(left * 0.95f, top * 0.05f, left + curve/2, bottom);
        } else {
            mLeftCurve.cubicTo(left + (curve * 0.75f), top,
                               left + (curve * 0.25f), bottom,
                               left + curve, bottom);
        }

        mRightCurve.reset();
        mRightCurve.moveTo(right, bottom);

        if (mCropped && mSide == CurveTowards.RIGHT) {
            mRightCurve.lineTo(right, bottom/2);
            mRightCurve.quadTo(right * 0.95f, bottom * 0.05f, right - (curve/2), top);
        } else {
            mRightCurve.cubicTo(right - (curve * 0.75f), bottom,
                                right - (curve * 0.25f), top,
                                right - curve, top);
        }

        mPath.reset();

        // Level 2: for phones: transparent.
        //          for tablets: only one curve.
        Drawable background = getBackground();
        if (background == null)
            return;

        if (!(background.getCurrent() instanceof ColorDrawable)) {
            if (background.getLevel() == 2) {
                mPath.moveTo(left, top);
                mPath.lineTo(left, bottom);
                mPath.lineTo(right, bottom);
                mPath.addPath(mRightCurve);
                mPath.lineTo(left, top);
            } else {
                mPath.moveTo(left, top);
                mPath.addPath(mLeftCurve);
                mPath.lineTo(right, bottom);
                mPath.addPath(mRightCurve);
                mPath.lineTo(left, top);
            }
        }

        if (mCropped) {
            mBackgroundPath.reset();

            if (mSide == CurveTowards.RIGHT) {
                mBackgroundPath.moveTo(right, bottom);
                mBackgroundPath.addPath(mRightCurve);
                mBackgroundPath.lineTo(right, top);
                mBackgroundPath.lineTo(right, bottom);
            } else {
                mBackgroundPath.moveTo(left, top);
                mBackgroundPath.addPath(mLeftCurve);
                mBackgroundPath.lineTo(left, bottom);
                mBackgroundPath.lineTo(left, top);
            }
        }
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());

        Drawable background = getBackground();
        if (background.getCurrent() instanceof ColorDrawable)
            return;

        // Additionally draw a black curve for cropped button's default level.
        if (mCropped && background.getLevel() != 2)
            canvas.drawPath(mBackgroundPath, mPaint);
    }

    // The drawable is constructed as per @drawable/tabs_button.
    @Override
    public void onLightweightThemeChanged() {
        LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getTextureDrawable(this, R.drawable.tabs_tray_bg_repeat);
        if (drawable == null)
            return;

        drawable.setAlpha(34, 34);

        Resources resources = this.getContext().getResources();
        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(new int[] { android.R.attr.state_pressed }, resources.getDrawable(R.drawable.highlight));
        stateList.addState(new int[] { R.attr.state_private }, resources.getDrawable(R.drawable.tabs_tray_bg_repeat));
        stateList.addState(new int[] {}, drawable);

        LevelListDrawable levelList = new LevelListDrawable();
        levelList.addLevel(0, 1, stateList);

        // If there is a side bar, the expanded state will have a filled button.
        if (mSideBar)
            levelList.addLevel(2, 2, stateList);
        else
            levelList.addLevel(2, 2, new ColorDrawable(Color.TRANSPARENT));

        setBackgroundDrawable(levelList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.tabs_button);
    }
}
