/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Path;
import android.util.AttributeSet;
import android.widget.ImageButton;

public class ShapedButton extends ImageButton
                          implements CanvasDelegate.DrawManager { 
    protected Path mPath;
    protected CurveTowards mSide;
    protected CanvasDelegate mCanvasDelegate;

    protected enum CurveTowards { NONE, LEFT, RIGHT };

    public ShapedButton(Context context, AttributeSet attrs) {
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
    }

    @Override
    public void draw(Canvas canvas) {
        mCanvasDelegate.draw(canvas, mPath, getWidth(), getHeight());
    }

    @Override
    public void defaultDraw(Canvas canvas) {
        super.draw(canvas);
    }
}
