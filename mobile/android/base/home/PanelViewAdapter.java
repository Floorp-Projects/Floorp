/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.ItemType;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.view.View;
import android.view.ViewGroup;

class PanelViewAdapter extends CursorAdapter {
	private final Context mContext;
	private final ItemType mItemType;

    public PanelViewAdapter(Context context, ItemType itemType) {
        super(context, null, 0);
        mContext = context;
        mItemType = itemType;
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        final PanelItemView item = (PanelItemView) view;
        item.updateFromCursor(cursor);
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        return PanelItemView.create(mContext, mItemType);
    }
}
