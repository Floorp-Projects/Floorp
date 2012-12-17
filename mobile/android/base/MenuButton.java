/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Path;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.LevelListDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

public class MenuButton extends ShapedButton {

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Path is clipped.
        mPath = new Path();
        mCanvasDelegate = new CanvasDelegate(this, Mode.DST_OUT);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        float curve = height * 1.125f;

        mPath.reset();

        // Clipping happens on opposite side for menu.
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

    // The drawable is constructed as per @drawable/menu_button.
    @Override
    public void onLightweightThemeChanged() {
        LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getColorDrawable(this, Color.BLACK);
        if (drawable == null)
            return;

        drawable.setAlpha(34, 34);

        Resources resources = getContext().getResources();
        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(new int[] { R.attr.state_private }, new ColorDrawable(Color.BLACK));
        stateList.addState(new int[] { android.R.attr.state_pressed }, resources.getDrawable(R.drawable.highlight));
        stateList.addState(new int[] {}, drawable);

        LevelListDrawable levelList = new LevelListDrawable();
        levelList.addLevel(0, 1, stateList);
        levelList.addLevel(2, 2, new ColorDrawable(Color.TRANSPARENT));

        setBackgroundDrawable(levelList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.menu_button);
    }
}
