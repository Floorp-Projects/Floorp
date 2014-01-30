/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.GridView;

public class PanelGridView extends GridView implements DatasetBacked {
    private static final String LOGTAG = "GeckoPanelGridView";

    private final PanelGridViewAdapter mAdapter;

    public PanelGridView(Context context, ViewConfig viewConfig) {
        super(context, null, R.attr.homeGridViewStyle);
        mAdapter = new PanelGridViewAdapter(context);
        setAdapter(mAdapter);
        setNumColumns(AUTO_FIT);
    }

    @Override
    public void setDataset(Cursor cursor) {
        mAdapter.swapCursor(cursor);
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
}
