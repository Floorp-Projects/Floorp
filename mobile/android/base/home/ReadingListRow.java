/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.util.StringUtils;

import android.content.Context;
import android.database.Cursor;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class ReadingListRow extends LinearLayout {
    private TextView title;
    private TextView excerpt;
    private ImageView indicator;

    public ReadingListRow(Context context) {
        this(context, null);
    }

    public ReadingListRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        title = (TextView) findViewById(R.id.title);
        excerpt = (TextView) findViewById(R.id.excerpt);
        indicator = (ImageView) findViewById(R.id.indicator);
    }

    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        final boolean isUnread = cursor.getInt(cursor.getColumnIndexOrThrow(ReadingListItems.IS_UNREAD)) == 1;

        final String url = cursor.getString(cursor.getColumnIndexOrThrow(ReadingListItems.URL));

        final String titleText = cursor.getString(cursor.getColumnIndexOrThrow(ReadingListItems.TITLE));
        title.setText(TextUtils.isEmpty(titleText) ? StringUtils.stripCommonSubdomains(StringUtils.stripScheme(url)) : titleText);
        title.setTextAppearance(getContext(), isUnread ? R.style.Widget_ReadingListRow_Title_Unread : R.style.Widget_ReadingListRow_Title_Read);

        final String excerptText = cursor.getString(cursor.getColumnIndexOrThrow(ReadingListItems.EXCERPT));
        excerpt.setText(TextUtils.isEmpty(excerptText) ? url : excerptText);
        excerpt.setTextAppearance(getContext(), isUnread ? R.style.Widget_ReadingListRow_Title_Unread : R.style.Widget_ReadingListRow_Title_Read);

        indicator.setImageResource(isUnread ? R.drawable.reading_list_indicator_unread : R.drawable.reading_list_indicator_read);
    }

}
