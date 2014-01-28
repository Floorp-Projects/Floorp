/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.lang.ref.WeakReference;

public abstract class TwoLineRow extends LinearLayout {
    protected static final int NO_ICON = 0;

    private final TextView mPrimaryText;
    private int mPrimaryIconId;

    private final TextView mSecondaryText;
    private int mSecondaryIconId;

    public TwoLineRow(Context context) {
        this(context, null);
    }

    public TwoLineRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        mSecondaryIconId = NO_ICON;
        mPrimaryIconId = NO_ICON;

        LayoutInflater.from(context).inflate(R.layout.two_line_row, this);
        mPrimaryText = (TextView) findViewById(R.id.primary_text);
        mSecondaryText = (TextView) findViewById(R.id.secondary_text);
    }

    protected void setPrimaryText(String text) {
        mPrimaryText.setText(text);
    }

    protected void setSecondaryText(String text) {
        mSecondaryText.setText(text);
    }

    protected void setSecondaryText(int stringId) {
        mSecondaryText.setText(stringId);
    }

    protected void setPrimaryIcon(int iconId) {
        if (mPrimaryIconId == iconId) {
            return;
        }

        mPrimaryIconId = iconId;
        mSecondaryText.setCompoundDrawablesWithIntrinsicBounds(mSecondaryIconId, 0, mPrimaryIconId, 0);
    }

    protected void setSecondaryIcon(int iconId) {
        if (mSecondaryIconId == iconId) {
            return;
        }

        mSecondaryIconId = iconId;
        mSecondaryText.setCompoundDrawablesWithIntrinsicBounds(mSecondaryIconId, 0, mPrimaryIconId, 0);
    }

    public abstract void updateFromCursor(Cursor cursor);
}
