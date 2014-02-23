/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ItemHandler;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.PanelView;

import android.content.Context;
import android.database.Cursor;
import android.view.View;
import android.widget.AdapterView;
import android.widget.GridView;

public class PanelGridView extends GridView
                           implements DatasetBacked, PanelView {
    private static final String LOGTAG = "GeckoPanelGridView";

    private final ViewConfig mViewConfig;
    private final PanelViewAdapter mAdapter;
    protected OnUrlOpenListener mUrlOpenListener;

    public PanelGridView(Context context, ViewConfig viewConfig) {
        super(context, null, R.attr.panelGridViewStyle);
        mViewConfig = viewConfig;
        mAdapter = new PanelViewAdapter(context, viewConfig.getItemType());
        setAdapter(mAdapter);
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

    private class PanelGridItemClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            Cursor cursor = mAdapter.getCursor();
            if (cursor == null || !cursor.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            int urlIndex = cursor.getColumnIndexOrThrow(HomeItems.URL);
            final String url = cursor.getString(urlIndex);

            EnumSet<OnUrlOpenListener.Flags> flags = EnumSet.noneOf(OnUrlOpenListener.Flags.class);
            if (mViewConfig.getItemHandler() == ItemHandler.INTENT) {
                flags.add(OnUrlOpenListener.Flags.OPEN_WITH_INTENT);
            }

            mUrlOpenListener.onUrlOpen(url, flags);
        }
    }
}
