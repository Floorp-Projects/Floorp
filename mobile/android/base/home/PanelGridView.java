/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.PanelView;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.GridView;

import java.util.EnumSet;

public class PanelGridView extends GridView
                           implements DatasetBacked, PanelView {
    private static final String LOGTAG = "GeckoPanelGridView";

    private final PanelGridViewAdapter mAdapter;
    protected OnUrlOpenListener mUrlOpenListener;

    public PanelGridView(Context context, ViewConfig viewConfig) {
        super(context, null, R.attr.panelGridViewStyle);
        mAdapter = new PanelGridViewAdapter(context);
        setAdapter(mAdapter);
        setNumColumns(AUTO_FIT);
        setOnItemClickListener(new PanelGridItemClickListener());
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mUrlOpenListener = null;
    }

    @Override
    public void setDataset(Cursor cursor) {
        mAdapter.swapCursor(cursor);
    }

    @Override
    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    private class PanelGridViewAdapter extends CursorAdapter {

        public PanelGridViewAdapter(Context context) {
            super(context, null, 0);
        }

        @Override
        public void bindView(View bindView, Context context, Cursor cursor) {
            final PanelGridItemView item = (PanelGridItemView) bindView;
            item.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return new PanelGridItemView(context);
        }
    }

    private class PanelGridItemClickListener implements AdapterView.OnItemClickListener {
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
