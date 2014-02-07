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
    private final TextView mTitle;
    private final TextView mDescription;

    public TwoLineRow(Context context) {
        this(context, null);
    }

    public TwoLineRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        setGravity(Gravity.CENTER_VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.two_line_row, this);
        mTitle = (TextView) findViewById(R.id.title);
        mDescription = (TextView) findViewById(R.id.description);
    }

    protected void setTitle(String text) {
        mTitle.setText(text);
    }

    protected void setDescription(String text) {
        mDescription.setText(text);
    }

    protected void setDescription(int stringId) {
        mDescription.setText(stringId);
    }

    public abstract void updateFromCursor(Cursor cursor);
}
