/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.database.Cursor;
import android.database.CursorWrapper;
import android.support.annotation.UiThread;
import android.support.v7.widget.CardView;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

public class TopSitesPageAdapter extends RecyclerView.Adapter<TopSitesCard> {

    /**
     * Cursor wrapper that handles the offsets and limits that we expect.
     * This allows most of our code to completely ignore the fact that we're only touching part
     * of the cursor.
     */
    private static final class SubsetCursor extends CursorWrapper {
        private final int start;
        private final int count;

        public SubsetCursor(Cursor cursor, int start, int maxCount) {
            super(cursor);

            this.start = start;

            if (start + maxCount < cursor.getCount()) {
                count = maxCount;
            } else {
                count = cursor.getCount() - start;
            }
        }

        @Override
        public boolean moveToPosition(int position) {
            return super.moveToPosition(position + start);
        }

        @Override
        public int getCount() {
            return count;
        }
    }

    private Cursor cursor;

    /**
     *
     * @param cursor
     * @param startIndex The first item that this topsites group should show. This item, and the following
     * 3 items will be displayed by this adapter.
     */
    public void swapCursor(Cursor cursor, int startIndex) {
        if (cursor != null) {
            if (startIndex >= cursor.getCount()) {
                throw new IllegalArgumentException("startIndex must be within Cursor range");
            }

            this.cursor = new SubsetCursor(cursor, startIndex, TopSitesPagerAdapter.ITEMS_PER_PAGE);
        } else {
            this.cursor = null;
        }

        notifyDataSetChanged();
    }

    @Override
    public void onBindViewHolder(TopSitesCard holder, int position) {
        cursor.moveToPosition(position);
        holder.bind(cursor);
    }

    public TopSitesPageAdapter() {
        setHasStableIds(true);
    }

    @Override
    public TopSitesCard onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        final CardView card = (CardView) inflater.inflate(R.layout.activity_stream_topsites_card, parent, false);

        return new TopSitesCard(card);
    }

    @UiThread
    public String getURLForPosition(int position) {
        cursor.moveToPosition(position);

        return cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
    }

    @Override
    public int getItemCount() {
        if (cursor != null) {
            return cursor.getCount();
        } else {
            return 0;
        }
    }

    @Override
    @UiThread
    public long getItemId(int position) {
        cursor.moveToPosition(position);

        // The Combined View only contains pages that have been visited at least once, i.e. any
        // page in the TopSites query will contain a HISTORY_ID. _ID however will be 0 for all rows.
        return cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
    }
}
