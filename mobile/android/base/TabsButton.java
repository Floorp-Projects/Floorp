/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuffXfermode;
import android.graphics.PorterDuff.Mode;
import android.util.AttributeSet;
import android.widget.ImageButton;

public class TabsButton extends ImageButton 
                        implements CanvasDelegate.DrawManager { 
    Paint mPaint;

    Path mPath;
    Path mBackgroundPath;
    Path mLeftCurve;
    Path mRightCurve;

    boolean mCropped;
    int mFullWidth;
    CurveTowards mSide;
    CanvasDelegate mCanvasDelegate;

    private enum CurveTowards { NONE, LEFT, RIGHT };

    public TabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.BrowserToolbarCurve);
        int curveTowards = a.getInt(R.styleable.BrowserToolbarCurve_curveTowards, 0x02);
        a.recycle();

        a = context.obtainStyledAttributes(attrs, R.styleable.TabsButton);
        mCropped = a.getBoolean(R.styleable.TabsButton_cropped, false);
        a.recycle();

        if (curveTowards == 0x00)
            mSide = CurveTowards.NONE;
        else if (curveTowards == 0x01)
            mSide = CurveTowards.LEFT;
        else
            mSide = CurveTowards.RIGHT;

        // Paint to draw the background.
        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setColor(0xFF000000);

        // Path is masked.
        mPath = new Path();
        mBackgroundPath = new Path();
        mLeftCurve = new Path();
        mRightCurve = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_IN);

        // Path might extend beyond the screen for smaller tabs button.
        mFullWidth = (int) context.getResources().getDimension(R.dimen.tabs_button_full_width);
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
            right = mFullWidth;
            top = 0;
            bottom = height;
        } else {
            left = width - mFullWidth;
            right = width;
            top = height;
            bottom = 0;
        }

        mLeftCurve.reset();
        mLeftCurve.moveTo(left, top);

        if (mCropped && mSide == CurveTowards.LEFT) {
            mLeftCurve.cubicTo(left + curve, top,
                               left, bottom,
                               left + curve, bottom);
        } else {
            mLeftCurve.cubicTo(left + (curve * 0.75f), top,
                               left + (curve * 0.25f), bottom,
                               left + curve, bottom);
        }

        mRightCurve.reset();
        mRightCurve.moveTo(right, bottom);

        if (mCropped && mSide == CurveTowards.RIGHT) {
            mRightCurve.cubicTo(right - curve, bottom,
                                right, top,
                                right - curve, top);
        } else {
            mRightCurve.cubicTo(right - (curve * 0.75f), bottom,
                                right - (curve * 0.25f), top,
                                right - curve, top);
        }

        mPath.reset();

        // Level 2: for phones: transparent.
        //          for tablets: only one curve.
        Drawable background = getBackground();

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

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }
}
