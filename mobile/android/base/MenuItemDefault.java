/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AbsListView;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MenuItemDefault extends LinearLayout
                             implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemDefault";

    private ImageView mIcon;
    private TextView mTitle;
    private CheckBox mCheck;

    private boolean mCheckable;
    private boolean mChecked;

    public MenuItemDefault(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources res = context.getResources();
        setLayoutParams(new AbsListView.LayoutParams((int) (res.getDimension(R.dimen.menu_item_row_width)),
                                                     (int) (res.getDimension(R.dimen.menu_item_row_height))));

        inflate(context, R.layout.menu_item, this);
        mIcon = (ImageView) findViewById(R.id.icon);
        mTitle = (TextView) findViewById(R.id.title);
        mCheck = (CheckBox) findViewById(R.id.check);

        mCheckable = false;
        mChecked = false;
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
        mCheckable = checkable;
        mCheck.setVisibility(mCheckable ? VISIBLE : GONE);
    }

    @Override
    public void setChecked(boolean checked) {
        mChecked = checked;
        mCheck.setChecked(mChecked);
    }
}
