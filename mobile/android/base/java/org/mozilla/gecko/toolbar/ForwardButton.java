/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.lwt.LightweightTheme;

public class ForwardButton extends NavButton {
    public ForwardButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.setFillType(Path.FillType.INVERSE_EVEN_ODD);

        final RectF rect = new RectF(0, 0, width, height);
        final int radius = getResources().getDimensionPixelSize(R.dimen.browser_toolbar_menu_radius);
        mPath.addRoundRect(rect, radius, radius, Path.Direction.CW);
    }

    @Override
    public void onLightweightThemeChanged() {
        final Drawable drawable = BrowserToolbar.getLightweightThemeDrawable(this, getTheme(), R.color.toolbar_grey);

        if (drawable == null) {
            return;
        }

        final StateListDrawable stateList = new StateListDrawable();

        final LightweightTheme lightweightTheme = getTheme();
        if (!lightweightTheme.isEnabled() || isPrivateMode()) {
            stateList.addState(PRIVATE_PRESSED_STATE_SET, getColorDrawable(R.color.action_bar_item_bg_color_private_pressed));
            stateList.addState(PRIVATE_STATE_SET, getColorDrawable(android.R.color.transparent));
            stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.action_bar_item_bg_color_pressed));
        } else {
            if (lightweightTheme.isLightTheme()) {
                stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.action_bar_item_bg_color_lwt_light_pressed));
            } else {
                stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.action_bar_item_bg_color_lwt_dark_pressed));
            }
        }

        stateList.addState(EMPTY_STATE_SET, drawable);
        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_forward_button);
    }
}
