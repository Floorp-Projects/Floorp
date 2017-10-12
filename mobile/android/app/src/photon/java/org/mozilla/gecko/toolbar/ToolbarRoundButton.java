/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.support.v4.content.ContextCompat;
import android.util.AttributeSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.lwt.LightweightTheme;

class ToolbarRoundButton extends ShapedButton {

    private boolean mShowLWTBackground;
    private Drawable mBackgroundDrawable;

    public ToolbarRoundButton(Context context) {
        this(context, null);
    }

    public ToolbarRoundButton(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ToolbarRoundButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        final TypedArray a = context.obtainStyledAttributes(attrs, new int[] { android.R.attr.background });
        mBackgroundDrawable = a.getDrawable(0);
        if (mBackgroundDrawable == null) {
            // Use default value if no background specified.
            mBackgroundDrawable = ContextCompat.getDrawable(context, R.drawable.url_bar_action_button);
        }
        a.recycle();

        final TypedArray ta = context.obtainStyledAttributes(attrs, R.styleable.ToolbarRoundButton);
        mShowLWTBackground = ta.getBoolean(R.styleable.ToolbarRoundButton_showLWTBackground, false);
        ta.recycle();

        setPrivateMode(false);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.setFillType(Path.FillType.INVERSE_EVEN_ODD);

        final Resources res = getResources();
        final int vSpace = res.getDimensionPixelSize(R.dimen.browser_toolbar_image_button_v_spacing);
        final int hSpace = res.getDimensionPixelSize(R.dimen.browser_toolbar_image_button_h_spacing);
        final RectF rect = new RectF(hSpace, vSpace, width - hSpace, height - vSpace);
        final int radius = res.getDimensionPixelSize(R.dimen.browser_toolbar_menu_radius);
        mPath.addRoundRect(rect, radius, radius, Path.Direction.CW);
    }

    @Override
    public void onLightweightThemeChanged() {
        final LightweightTheme lightweightTheme = getTheme();
        if (!lightweightTheme.isEnabled() || isPrivateMode()) {
            setBackground(mBackgroundDrawable);
        } else {
            final StateListDrawable stateList = new StateListDrawable();
            final int backgroundColorRes = lightweightTheme.isLightTheme()
                                                ? R.color.action_bar_item_bg_color_lwt_light_pressed
                                                : R.color.action_bar_item_bg_color_lwt_dark_pressed;
            stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(backgroundColorRes));
            stateList.addState(EMPTY_STATE_SET, getColorDrawable(android.R.color.transparent));
            setBackgroundDrawable(stateList);
        }
    }

    @Override
    public void onLightweightThemeReset() {
        setBackground(mBackgroundDrawable);
    }
}
