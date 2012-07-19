/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.widget.ImageView;

public class MenuItemDefault extends LinearLayout
                             implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemDefault";

    private Context mContext;

    private ImageView mIcon;
    private TextView mTitle;
    private ImageView mCheck;

    public MenuItemDefault(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        DisplayMetrics metrics = new DisplayMetrics();
        ((Activity) context).getWindowManager().getDefaultDisplay().getMetrics(metrics);

        setLayoutParams(new LayoutParams((int) (240 * metrics.density),
                                         (int) (44 * metrics.density)));
        setBackgroundResource(R.drawable.menu_item_bg);

        inflate(context, R.layout.menu_item, this);
        mIcon = (ImageView) findViewById(R.id.icon);
        mTitle = (TextView) findViewById(R.id.title);
        mCheck = (ImageView) findViewById(R.id.check);
    }

    @Override
    public View getLayout() {
        return this;
    }

    @Override
    public void setIcon(Drawable icon) {
        if (icon != null) {
            mIcon.setImageDrawable(icon);
            mIcon.setVisibility(VISIBLE);
        } else {
            mIcon.setVisibility(GONE);
        }
    }

    @Override
    public void setIcon(int icon) {
        if (icon != 0) {
            mIcon.setImageResource(icon);
            mIcon.setVisibility(VISIBLE);
        } else {
            mIcon.setVisibility(GONE);
        }
    }

    @Override
    public void setTitle(CharSequence title) {
        mTitle.setText(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mTitle.setEnabled(enabled);
        mCheck.setEnabled(enabled);
        mIcon.setColorFilter(enabled ? 0 : 0xFF999999);
    }

    @Override
    public void setCheckable(boolean checkable) {
        mCheck.setVisibility(checkable ? VISIBLE : GONE);
    }

    @Override
    public void setChecked(boolean checked) {
        mCheck.setVisibility(checked ? VISIBLE : GONE);
    }
}
