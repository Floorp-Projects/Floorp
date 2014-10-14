/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

public class ReadingListRow extends LinearLayout {

    private final Resources resources;

    private final TextView title;
    private final TextView excerpt;
    private final TextView readTime;

    // Average reading speed in words per minute.
    private static final int AVERAGE_READING_SPEED = 250;

    // Length of average word.
    private static final float AVERAGE_WORD_LENGTH = 5.1f;


    public ReadingListRow(Context context) {
        this(context, null);
    }

    public ReadingListRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.reading_list_row_view, this);

        setOrientation(LinearLayout.VERTICAL);

        resources = context.getResources();

        title = (TextView) findViewById(R.id.title);
        excerpt = (TextView) findViewById(R.id.excerpt);
        readTime = (TextView) findViewById(R.id.read_time);
    }

    public void updateFromCursor(Cursor cursor) {
        if (cursor == null) {
            return;
        }

        final int titleIndex = cursor.getColumnIndexOrThrow(ReadingListItems.TITLE);
        title.setText(cursor.getString(titleIndex));

        final int excerptIndex = cursor.getColumnIndexOrThrow(ReadingListItems.EXCERPT);
        excerpt.setText(cursor.getString(excerptIndex));

        final int lengthIndex = cursor.getColumnIndexOrThrow(ReadingListItems.LENGTH);
        final int minutes = getEstimatedReadTime(cursor.getInt(lengthIndex));
        if (minutes <= 60) {
            readTime.setText(resources.getString(R.string.reading_list_time_minutes, minutes));
        } else {
            readTime.setText(resources.getString(R.string.reading_list_time_over_an_hour));
        }
    }

    /**
     * Calculates the estimated time to read an article based on its length.
     *
     * @param length of the article (in characters)
     * @return estimated time to read the article (in minutes)
     */
    private static int getEstimatedReadTime(int length) {
        final int minutes = (int) Math.ceil((length / AVERAGE_WORD_LENGTH) / AVERAGE_READING_SPEED);

        // Minimum of one minute.
        return Math.max(minutes, 1);
    }
}
