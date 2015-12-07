/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.util.SparseArray;
import android.view.View;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

/**
 * CursorAdapter for <code>HistoryPanel</code> to partition history items by <code>MostRecentSection</code> range headers.
 */
public class HistoryHeaderListCursorAdapter extends MultiTypeCursorAdapter implements HistoryPanel.HistoryUrlProvider {
    private static final int ROW_HEADER = 0;
    private static final int ROW_STANDARD = 1;

    private static final int[] VIEW_TYPES = new int[] { ROW_STANDARD, ROW_HEADER };
    private static final int[] LAYOUT_TYPES = new int[] { R.layout.home_item_row, R.layout.home_header_row };

    // Maps headers in the list with their respective sections
    private final SparseArray<HistoryPanel.MostRecentSection> mMostRecentSections;

    public HistoryHeaderListCursorAdapter(Context context) {
        super(context, null, VIEW_TYPES, LAYOUT_TYPES);

        // Initialize map of history sections
        mMostRecentSections = new SparseArray<>();
    }

    @Override
    public Object getItem(int position) {
        final int type = getItemViewType(position);

        // Header items are not in the cursor
        if (type == ROW_HEADER) {
            return null;
        }

        return super.getItem(position - getMostRecentSectionsCountBefore(position));
    }

    @Override
    public int getItemViewType(int position) {
        if (mMostRecentSections.get(position) != null) {
            return ROW_HEADER;
        }

        return ROW_STANDARD;
    }

    @Override
    public boolean isEnabled(int position) {
        return (getItemViewType(position) == ROW_STANDARD);
    }

    @Override
    public int getCount() {
        // Add the history section headers to the number of reported results.
        return super.getCount() + mMostRecentSections.size();
    }

    @Override
    public Cursor swapCursor(Cursor cursor) {
        loadMostRecentSections(cursor);
        Cursor oldCursor = super.swapCursor(cursor);
        return oldCursor;
    }

    @Override
    public void bindView(View view, Context context, int position) {
        final int type = getItemViewType(position);

        if (type == ROW_HEADER) {
            final HistoryPanel.MostRecentSection section = mMostRecentSections.get(position);
            final TextView row = (TextView) view;
            row.setText(HistoryPanel.getMostRecentSectionTitle(section));
        } else {
            // Account for the most recent section headers
            position -= getMostRecentSectionsCountBefore(position);
            final Cursor c = getCursor(position);
            final TwoLinePageRow row = (TwoLinePageRow) view;
            row.updateFromCursor(c);
        }
    }

    private int getMostRecentSectionsCountBefore(int position) {
        // Account for the number headers before the given position
        int sectionsBefore = 0;

        final int historySectionsCount = mMostRecentSections.size();
        for (int i = 0; i < historySectionsCount; i++) {
            final int sectionPosition = mMostRecentSections.keyAt(i);
            if (sectionPosition > position) {
                break;
            }

            sectionsBefore++;
        }

        return sectionsBefore;
    }

    private void loadMostRecentSections(Cursor c) {
        // Clear any history sections that may have been loaded before.
        mMostRecentSections.clear();

        if (c == null || !c.moveToFirst()) {
            return;
        }

        HistoryPanel.MostRecentSection section = null;

        do {
            final int position = c.getPosition();
            final long time = c.getLong(c.getColumnIndexOrThrow(BrowserContract.History.DATE_LAST_VISITED));
            final HistoryPanel.MostRecentSection itemSection = HistoryPanel.getMostRecentSectionForTime(time);

            if (section != itemSection) {
                section = itemSection;
                mMostRecentSections.append(position + mMostRecentSections.size(), section);
            }

            // Reached the last section, no need to continue
            if (section == HistoryPanel.MostRecentSection.OLDER_THAN_SIX_MONTHS) {
                break;
            }
        } while (c.moveToNext());
    }

    @Override
    public String getURL(int position) {
        position -= getMostRecentSectionsCountBefore(position);
        final Cursor c = getCursor(position);
        return c.getString(c.getColumnIndexOrThrow(BrowserContract.History.URL));
    }
}
