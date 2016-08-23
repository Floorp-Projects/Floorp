/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream;

import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.mozilla.gecko.home.activitystream.StreamItem.BottomPanel;
import org.mozilla.gecko.home.activitystream.StreamItem.CompactItem;
import org.mozilla.gecko.home.activitystream.StreamItem.HighlightItem;
import org.mozilla.gecko.home.activitystream.StreamItem.TopPanel;

public class StreamRecyclerAdapter extends RecyclerView.Adapter<StreamItem> {
    private Cursor highlightsCursor;

    @Override
    public int getItemViewType(int position) {
        if (position == 0) {
            return TopPanel.LAYOUT_ID;
        } else if (position == getItemCount() - 1) {
            return BottomPanel.LAYOUT_ID;
        } else {
            // TODO: in future we'll want to create different items for some results, tbc?
            // For now let's show a detailed view for these two positions...
            if (position == 2 || position == 6) {
                return HighlightItem.LAYOUT_ID;
            }
            return CompactItem.LAYOUT_ID;
        }
    }

    @Override
    public StreamItem onCreateViewHolder(ViewGroup parent, final int type) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (type == TopPanel.LAYOUT_ID) {
            return new TopPanel(inflater.inflate(type, parent, false));
        } else if (type == BottomPanel.LAYOUT_ID) {
                return new BottomPanel(inflater.inflate(type, parent, false));
        } else if (type == CompactItem.LAYOUT_ID) {
            return new CompactItem(inflater.inflate(type, parent, false));
        } else if (type == HighlightItem.LAYOUT_ID) {
            return new HighlightItem(inflater.inflate(type, parent, false));
        } else {
            throw new IllegalStateException("Missing inflation for ViewType " + type);
        }
    }

    private int translatePositionToCursor(int position) {
        if (position == 0 ||
            position == getItemCount() - 1) {
            throw new IllegalArgumentException("Requested cursor position for invalid item");
        }

        // We have one blank panel at the top, hence remove that to obtain the cursor position
        return position - 1;
    }

    @Override
    public void onBindViewHolder(StreamItem holder, int position) {
        int type = getItemViewType(position);

        if (type == CompactItem.LAYOUT_ID ||
            type == HighlightItem.LAYOUT_ID) {

            final int cursorPosition = translatePositionToCursor(position);

            highlightsCursor.moveToPosition(cursorPosition);
            holder.bind(highlightsCursor);
        }
    }

    @Override
    public int getItemCount() {
        final int highlightsCount;
        if (highlightsCursor != null) {
            highlightsCount = highlightsCursor.getCount();
        } else {
            highlightsCount = 0;
        }

        return 2 + highlightsCount;
    }

    public void swapCursor(Cursor cursor) {
        highlightsCursor = cursor;

        notifyDataSetChanged();
    }
}
