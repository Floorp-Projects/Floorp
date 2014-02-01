/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.PanelView;
import org.mozilla.gecko.db.BrowserContract.HomeItems;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;

import java.util.EnumSet;

public class PanelListView extends HomeListView
                           implements DatasetBacked, PanelView {

    private static final String LOGTAG = "GeckoPanelListView";

    private final PanelListAdapter mAdapter;
    private final ViewConfig mViewConfig;

    public PanelListView(Context context, ViewConfig viewConfig) {
        super(context);
        mViewConfig = viewConfig;
        mAdapter = new PanelListAdapter(context);
        setAdapter(mAdapter);
        setOnItemClickListener(new PanelListItemClickListener());
    }

    @Override
    public void setDataset(Cursor cursor) {
        Log.d(LOGTAG, "Setting dataset: " + mViewConfig.getDatasetId());
        mAdapter.swapCursor(cursor);
    }

    private class PanelListAdapter extends CursorAdapter {
        public PanelListAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            final PanelListRow row = (PanelListRow) view;
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(parent.getContext()).inflate(R.layout.panel_list_row, parent, false);
        }
    }

    private class PanelListItemClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            Cursor cursor = mAdapter.getCursor();
            if (cursor == null || !cursor.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            int urlIndex = cursor.getColumnIndexOrThrow(HomeItems.URL);
            final String url = cursor.getString(urlIndex);

            mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.OPEN_WITH_INTENT));
        }
    }
}
