/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.Shader;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class BackButton extends ShapedButton { 
    private Path mPath;
    private Path mBorderPath;
    private Paint mBorderPaint;
    private Paint mBorderPrivatePaint;

    public BackButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Paint to draw the border.
        mBorderPaint = new Paint();
        mBorderPaint.setAntiAlias(true);
        mBorderPaint.setColor(0xFF000000);
        mBorderPaint.setStyle(Paint.Style.STROKE);

        mBorderPrivatePaint = new Paint(mBorderPaint);

        // Path is masked.
        mPath = new Path();
        mBorderPath = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_IN);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.addCircle(width/2, height/2, width/2, Path.Direction.CW);

        float borderWidth = getContext().getResources().getDimension(R.dimen.nav_button_border_width);
        mBorderPaint.setStrokeWidth(borderWidth);
        mBorderPrivatePaint.setStrokeWidth(borderWidth);

        mBorderPath.reset();
        mBorderPath.addCircle(width/2, height/2, (width/2) - borderWidth, Path.Direction.CW);

        mBorderPaint.setShader(new LinearGradient(0, 0, 
                                                  0, height, 
                                                  0xFFB5BBC1, 0xFFFAFBFC,
                                                  Shader.TileMode.CLAMP));

        mBorderPrivatePaint.setShader(new LinearGradient(0, 0, 
                                                         0, height, 
                                                         0xFF040607, 0xFF0B0D0E,
                                                         Shader.TileMode.CLAMP));
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());

        // Draw the border on top.
        canvas.drawPath(mBorderPath, isPrivateMode() ? mBorderPrivatePaint : mBorderPaint);
    }

    // The drawable is constructed as per @drawable/address_bar_nav_button.
    @Override
    public void onLightweightThemeChanged() {
        Drawable drawable = mActivity.getLightweightTheme().getDrawable(this);
        if (drawable == null)
            return;

        Resources resources = getContext().getResources();
        StateListDrawable stateList = new StateListDrawable();

        stateList.addState(new int[] { R.attr.state_private, android.R.attr.state_pressed }, new ColorDrawable(resources.getColor(R.color.highlight_nav_pb)));
        stateList.addState(new int[] { android.R.attr.state_pressed }, new ColorDrawable(resources.getColor(R.color.highlight_nav)));
        stateList.addState(new int[] { R.attr.state_private, android.R.attr.state_focused }, new ColorDrawable(resources.getColor(R.color.highlight_nav_focused_pb)));
        stateList.addState(new int[] { android.R.attr.state_focused }, new ColorDrawable(resources.getColor(R.color.highlight_nav_focused)));
        stateList.addState(new int[] { R.attr.state_private }, new ColorDrawable(resources.getColor(R.color.background_private)));
        stateList.addState(new int[] {}, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.address_bar_nav_button);
    }
}
