/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.R;
import org.mozilla.gecko.lwt.LightweightThemeDrawable;
import org.mozilla.gecko.widget.themed.ThemedFrameLayout;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;

/** A FrameLayout with lightweight theme support. Note that {@link ShapedButton}'s lwt support is basically the same so
 * if you change it here, you should probably change it there. Note also that this doesn't have ShapedButton's path code
 * so shouldn't have "ShapedButton" in the name, but I wanted to make the connection apparent so I left it.
 */
public class ShapedButtonFrameLayout extends ThemedFrameLayout {

    public ShapedButtonFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // The drawable is constructed as per @drawable/shaped_button.
    @Override
    public void onLightweightThemeChanged() {
        final int background = ContextCompat.getColor(getContext(), R.color.text_and_tabs_tray_grey);
        final LightweightThemeDrawable lightWeight = getTheme().getColorDrawable(this, background);

        if (lightWeight == null)
            return;

        lightWeight.setAlpha(34, 34);

        final StateListDrawable stateList = new StateListDrawable();
        stateList.addState(PRESSED_ENABLED_STATE_SET, getColorDrawable(R.color.highlight_shaped));
        stateList.addState(FOCUSED_STATE_SET, getColorDrawable(R.color.highlight_shaped_focused));
        stateList.addState(PRIVATE_STATE_SET, getColorDrawable(R.color.text_and_tabs_tray_grey));
        stateList.addState(EMPTY_STATE_SET, lightWeight);

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
        setBackgroundDrawable(getResources().getDrawable(resId));
    }
}
