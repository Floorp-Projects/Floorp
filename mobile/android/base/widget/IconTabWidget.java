/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.TabWidget;

public class IconTabWidget extends TabWidget {
    private OnTabChangedListener mListener;
    private final int mButtonLayoutId;
    private final boolean mIsIcon;

    public static interface OnTabChangedListener {
        public void onTabChanged(int tabIndex);
    }

    public IconTabWidget(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.IconTabWidget);
        mButtonLayoutId = a.getResourceId(R.styleable.IconTabWidget_android_layout, 0);
        mIsIcon = (a.getInt(R.styleable.IconTabWidget_display, 0x00) == 0x00);
        a.recycle();

        if (mButtonLayoutId == 0) {
            throw new RuntimeException("You must supply layout attribute");
        }
    }

    public void addTab(int imageResId, int stringResId) {
        View button = LayoutInflater.from(getContext()).inflate(mButtonLayoutId, this, false);
        if (mIsIcon) {
            ((ImageButton) button).setImageResource(imageResId);
            button.setContentDescription(getContext().getString(stringResId));
        } else {
            ((TextView) button).setText(getContext().getString(stringResId));
        }

        addView(button);
        button.setOnClickListener(new TabClickListener(getTabCount() - 1));
        button.setOnFocusChangeListener(this);
    }

    public void setTabSelectionListener(OnTabChangedListener listener) {
        mListener = listener;
    }

    @Override
    public void onFocusChange(View view, boolean hasFocus) {
    }

    private class TabClickListener implements OnClickListener {
        private final int mIndex;

        public TabClickListener(int index) {
            mIndex = index;
        }

        @Override
        public void onClick(View view) {
            if (mListener != null)
                mListener.onTabChanged(mIndex);
        }
    }
}
