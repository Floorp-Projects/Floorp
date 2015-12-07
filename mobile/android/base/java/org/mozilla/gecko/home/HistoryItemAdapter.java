/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.db.BrowserContract;

/**
 * A Cursor adapter that is used to populate the history list items in split plane mode.
 */
public class HistoryItemAdapter extends CursorAdapter implements HistoryPanel.HistoryUrlProvider {
    private final int resource;

    public HistoryItemAdapter(Context context, Cursor c, int resource) {
        super(context, c, false);
        this.resource = resource;
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        return LayoutInflater.from(context).inflate(resource, parent, false);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        return super.getView(position, convertView, parent);
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        final TwoLinePageRow row = (TwoLinePageRow) view;
        row.updateFromCursor(cursor);
    }

    @Override
    public String getURL(int position) {
        final Cursor cursor = getCursor();
        if (cursor == null || !cursor.moveToPosition(position)) {
            throw new IllegalStateException("Couldn't move cursor to position " + position);
        }

        return cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.History.URL));
    }
}
