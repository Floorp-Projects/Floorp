/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations;

import java.text.DateFormat;
import java.text.FieldPosition;
import java.util.Date;

/**
 * An entry of the screenshot list in the bookmarks panel.
 */
class BookmarkScreenshotRow extends LinearLayout {
    private TextView titleView;
    private TextView dateView;

    // This DateFormat uses the current locale at instantiation time, which won't get updated if the locale is changed.
    // Since it's just a date, it's probably not worth the code complexity to fix that.
    private static final DateFormat dateFormat = DateFormat.getDateInstance(DateFormat.LONG);
    private static final DateFormat timeFormat = DateFormat.getTimeInstance(DateFormat.SHORT);

    // This parameter to DateFormat.format has no impact on the result but rather gets mutated by the method to
    // identify where a certain field starts and ends (by index). This is useful if you want to later modify the String;
    // I'm not sure why this argument isn't optional.
    private static final FieldPosition dummyFieldPosition = new FieldPosition(-1);

    public BookmarkScreenshotRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        titleView = (TextView) findViewById(R.id.title);
        dateView = (TextView) findViewById(R.id.date);
    }

    public void updateFromCursor(final Cursor c) {
        titleView.setText(getTitleFromCursor(c));
        dateView.setText(getDateFromCursor(c));
    }

    private static String getTitleFromCursor(final Cursor c) {
        final int index = c.getColumnIndexOrThrow(UrlAnnotations.URL);
        return c.getString(index);
    }

    private static String getDateFromCursor(final Cursor c) {
        final long timestamp = c.getLong(c.getColumnIndexOrThrow(UrlAnnotations.DATE_CREATED));
        final Date date = new Date(timestamp);
        final StringBuffer sb = new StringBuffer();
        dateFormat.format(date, sb, dummyFieldPosition)
                .append(" - ");
        timeFormat.format(date, sb, dummyFieldPosition);
        return sb.toString();
    }
}
