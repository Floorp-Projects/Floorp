/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.GeckoLinearLayout;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.widget.Divider;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

public class AboutHomeSection extends GeckoLinearLayout {
    private TextView mTitle;
    private TextView mSubtitle;
    private LinearLayout mItemsContainer;
    private LinkTextView mMoreText;

    public AboutHomeSection(Context context, AttributeSet attrs) {
        super(context, attrs);

        setOrientation(VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.abouthome_section, this);

        mTitle = (TextView) this.findViewById(R.id.title);
        mSubtitle = (TextView) this.findViewById(R.id.subtitle);
        mItemsContainer = (LinearLayout) this.findViewById(R.id.items_container);
        mMoreText = (LinkTextView) this.findViewById(R.id.more_text);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.AboutHomeSection);
        setTitle(a.getText(R.styleable.AboutHomeSection_title));
        setSubtitle(a.getText(R.styleable.AboutHomeSection_subtitle));
        setMoreText(a.getText(R.styleable.AboutHomeSection_more_text));
        a.recycle();
    }

    public LinearLayout getItemsContainer() {
        return mItemsContainer;
    }

    public void setTitle(CharSequence title) {
        if (!TextUtils.isEmpty(title)) {
            mTitle.setText(title);
            mTitle.setVisibility(View.VISIBLE);
        } else {
            mTitle.setVisibility(View.GONE);
        }
    }

    public void setSubtitle(CharSequence subtitle) {
        if (!TextUtils.isEmpty(subtitle)) {
            mSubtitle.setText(subtitle);
            mSubtitle.setVisibility(View.VISIBLE);
        } else {
            mSubtitle.setVisibility(View.GONE);
        }
    }

    public void setMoreText(CharSequence moreText) {
        if (!TextUtils.isEmpty(moreText)) {
            mMoreText.setText(moreText);
            mMoreText.setVisibility(View.VISIBLE);
        } else {
            mMoreText.setVisibility(View.GONE);
        }
    }

    public void setOnMoreTextClickListener(View.OnClickListener listener) {
        mMoreText.setOnClickListener(listener);
        mMoreText.setOnKeyListener(GamepadUtils.getClickDispatcher());
    }

    public void addItem(View item) {
        mItemsContainer.addView(item);

        Divider divider = new Divider(getContext(), null);
        divider.setBackgroundColor(0x3460666E);

        mItemsContainer.addView(divider);
    }

    public void clear() {
        mItemsContainer.removeAllViews();
    }

    public void show() {
        setVisibility(View.VISIBLE);
    }

    public void hide() {
        setVisibility(View.GONE);
    }

    public void showMoreText() {
        mMoreText.setVisibility(View.VISIBLE);
    }

    public void hideMoreText() {
        mMoreText.setVisibility(View.GONE);
    }
}
