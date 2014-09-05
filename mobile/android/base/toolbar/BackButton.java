/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class BackButton extends ShapedButton { 
    private final Path mBorderPath;
    private final Paint mBorderPaint;
    private final float mBorderWidth;

    public BackButton(Context context, AttributeSet attrs) {
        super(context, attrs);

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
        mBorderPaint.setColor(isPrivate ? 0xFF363B40 : 0xFFB5B5B5);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.addCircle(width/2, height/2, width/2, Path.Direction.CW);

        mBorderPath.reset();
        mBorderPath.addCircle(width/2, height/2, (width/2) - (mBorderWidth/2), Path.Direction.CW);
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());

        // Draw the border on top.
        canvas.drawPath(mBorderPath, mBorderPaint);
    }

    // The drawable is constructed as per @drawable/url_bar_nav_button.
    @Override
    public void onLightweightThemeChanged() {
        final Drawable drawable = mTheme.getDrawable(this);
        if (drawable == null)
            return;

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRIVATE_PRESSED_STATE_SET, getColorDrawable(R.color.highlight_nav_pb));
        stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.highlight_nav));
        stateList.addState(PRIVATE_FOCUSED_STATE_SET, getColorDrawable(R.color.highlight_nav_focused_pb));
        stateList.addState(FOCUSED_STATE_SET, getColorDrawable(R.color.highlight_nav_focused));
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.background_private));
        stateList.addState(EMPTY_STATE_SET, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_nav_button);
    }
}
