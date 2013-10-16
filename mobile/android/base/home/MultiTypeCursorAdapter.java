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

/**
 * MultiTypeCursorAdapter wraps a cursor and any meta data associated with it.
 * A set of view types (corresponding to the cursor and its meta data)
 * are mapped to a set of layouts.
 */
abstract class MultiTypeCursorAdapter extends CursorAdapter {
    private final int[] mViewTypes;
    private final int[] mLayouts;

    // Bind the view for the given position.
    abstract public void bindView(View view, Context context, int position);

    public MultiTypeCursorAdapter(Context context, Cursor cursor, int[] viewTypes, int[] layouts) {
        super(context, cursor, 0);

        if (viewTypes.length != layouts.length) {
            throw new IllegalStateException("The view types and the layouts should be of same size");
        }

        mViewTypes = viewTypes;
        mLayouts = layouts;
    }

    @Override
    public final int getViewTypeCount() {
        return mViewTypes.length;
    }

    /**
     * @return Cursor for the given position.
     */
    public final Cursor getCursor(int position) {
        final Cursor cursor = getCursor();
        if (cursor == null || !cursor.moveToPosition(position)) {
            throw new IllegalStateException("Couldn't move cursor to position " + position);
        }

        return cursor;
    }

    @Override
    public final View getView(int position, View convertView, ViewGroup parent) {
        final Context context = parent.getContext();
        if (convertView == null) {
            convertView = newView(context, position, parent);
        }

        bindView(convertView, context, position);
        return convertView;
    }

    @Override
    public final void bindView(View view, Context context, Cursor cursor) {
        // Do nothing.
    }

    @Override
    public final View newView(Context context, Cursor cursor, ViewGroup parent) {
        return null;
    }

    /**
     * Inflate a new view from a set of view types and layouts based on the position.
     *
     * @param context Context for inflating the view.
     * @param position Position of the view.
     * @param parent Parent view group that will hold this view.
     */
    private View newView(Context context, int position, ViewGroup parent) {
        final int type = getItemViewType(position);
        final int count = mViewTypes.length;

        for (int i = 0; i < count; i++) {
            if (mViewTypes[i] == type) {
                return LayoutInflater.from(context).inflate(mLayouts[i], parent, false);
            }
        }

        return null;
    }
}
