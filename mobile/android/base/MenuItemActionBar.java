/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ImageButton;

public class MenuItemActionBar extends ImageButton
                               implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionBar";

    private Context mContext;

    public MenuItemActionBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        DisplayMetrics metrics = new DisplayMetrics();
        ((Activity) context).getWindowManager().getDefaultDisplay().getMetrics(metrics);

        setLayoutParams(new ViewGroup.LayoutParams((int) (56 * metrics.density),
                                                   (int) (56 * metrics.density)));

        int padding = (int) (14 * metrics.density);
        setPadding(padding, padding, padding, padding);
        setBackgroundResource(R.drawable.action_bar_button);
        setScaleType(ImageView.ScaleType.FIT_XY);
    }

    @Override
    public View getLayout() {
        return this;
    }

    @Override
    public void setIcon(Drawable icon) {
        if (icon != null) {
            setImageDrawable(icon);
            setVisibility(VISIBLE);
        } else {
            setVisibility(GONE);
        }
    }

    @Override
    public void setIcon(int icon) {
        if (icon != 0) {
            setImageResource(icon);
            setVisibility(VISIBLE);
        } else {
            setVisibility(GONE);
        }
    }

    @Override
    public void setTitle(CharSequence title) {
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        setColorFilter(enabled ? 0 : 0xFF999999);
    }

    @Override
    public void setCheckable(boolean checkable) {
    }

    @Override
    public void setChecked(boolean checked) {
    }
}
