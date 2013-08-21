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

public class BrowserToolbarBackground extends GeckoLinearLayout {
    private GeckoActivity mActivity;

    public BrowserToolbarBackground(Context context, AttributeSet attrs) {
        super(context, attrs);
        mActivity = (GeckoActivity) context;
    }

    @Override
    public void onLightweightThemeChanged() {
        Drawable drawable = mActivity.getLightweightTheme().getDrawable(this);
        if (drawable == null)
            return;

        StateListDrawable stateList = new StateListDrawable();
        stateList.addState(new int[] { R.attr.state_private }, new ColorDrawable(mActivity.getResources().getColor(R.color.background_private)));
        stateList.addState(new int[] {}, drawable);

        setBackgroundDrawable(stateList);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.url_bar_bg);
    }
}
