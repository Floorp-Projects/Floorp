/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream;

import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.stream.HighlightItem;
import org.mozilla.gecko.home.activitystream.stream.HighlightsTitle;
import org.mozilla.gecko.home.activitystream.stream.StreamItem;
import org.mozilla.gecko.home.activitystream.stream.TopPanel;
import org.mozilla.gecko.home.activitystream.stream.WelcomePanel;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.EnumSet;

public class StreamRecyclerAdapter extends RecyclerView.Adapter<StreamItem> implements RecyclerViewClickSupport.OnItemClickListener {
    private Cursor highlightsCursor;
    private Cursor topSitesCursor;

    private HomePager.OnUrlOpenListener onUrlOpenListener;
    private HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    private int tiles;
    private int tilesWidth;
    private int tilesHeight;

    public StreamRecyclerAdapter() {
        setHasStableIds(true);
    }

    void setOnUrlOpenListeners(HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    public void setTileSize(int tiles, int tilesWidth, int tilesHeight) {
        this.tilesWidth = tilesWidth;
        this.tilesHeight = tilesHeight;
        this.tiles = tiles;

        notifyDataSetChanged();
    }

    @Override
    public int getItemViewType(int position) {
        if (position == 0) {
            return TopPanel.LAYOUT_ID;
        } else if (position == 1) {
            return WelcomePanel.LAYOUT_ID;
        } else if (position == 2) {
            return HighlightsTitle.LAYOUT_ID;
        } else if (position < getItemCount()) {
            return HighlightItem.LAYOUT_ID;
        } else {
            throw new IllegalArgumentException("Requested position does not exist");
        }
    }

    @Override
    public StreamItem onCreateViewHolder(ViewGroup parent, final int type) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (type == TopPanel.LAYOUT_ID) {
            return new TopPanel(inflater.inflate(type, parent, false), onUrlOpenListener, onUrlOpenInBackgroundListener);
        } else if (type == WelcomePanel.LAYOUT_ID) {
            return new WelcomePanel(inflater.inflate(type, parent, false), this);
        } else if (type == HighlightItem.LAYOUT_ID) {
            return new HighlightItem(inflater.inflate(type, parent, false), onUrlOpenListener, onUrlOpenInBackgroundListener);
        } else if (type == HighlightsTitle.LAYOUT_ID) {
            return new HighlightsTitle(inflater.inflate(type, parent, false));
        } else {
            throw new IllegalStateException("Missing inflation for ViewType " + type);
        }
    }

    private int translatePositionToCursor(int position) {
        if (getItemViewType(position) != HighlightItem.LAYOUT_ID) {
            throw new IllegalArgumentException("Requested cursor position for invalid item");
        }

        // We have three blank panels at the top, hence remove that to obtain the cursor position
        return position - 3;
    }

    @Override
    public void onBindViewHolder(StreamItem holder, int position) {
        int type = getItemViewType(position);

        if (type == HighlightItem.LAYOUT_ID) {
            final int cursorPosition = translatePositionToCursor(position);

            highlightsCursor.moveToPosition(cursorPosition);
            ((HighlightItem) holder).bind(highlightsCursor, tilesWidth,  tilesHeight);
        } else if (type == TopPanel.LAYOUT_ID) {
            ((TopPanel) holder).bind(topSitesCursor, tiles, tilesWidth, tilesHeight);
        }
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (getItemViewType(position) != HighlightItem.LAYOUT_ID) {
            // Headers (containing topsites and/or the highlights title) do their own click handling as needed
            return;
        }

        int actualPosition = translatePositionToCursor(position);
        highlightsCursor.moveToPosition(actualPosition);

        final String url = highlightsCursor.getString(
                highlightsCursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));

        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .forHighlightSource(Utils.highlightSource(highlightsCursor))
                .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS)
                .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, actualPosition)
                .set(ActivityStreamTelemetry.Contract.COUNT, highlightsCursor.getCount());

        Telemetry.sendUIEvent(
                TelemetryContract.Event.LOAD_URL,
                TelemetryContract.Method.LIST_ITEM,
                extras.build()
        );

        // NB: This is hacky. We need to process telemetry data first, otherwise we run a risk of
        // not having a cursor to work with once url is opened and BrowserApp closes A-S home screen
        // and clears its resources (read: cursors). See Bug 1326018.
        onUrlOpenListener.onUrlOpen(url, EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
    }

    @Override
    public int getItemCount() {
        final int highlightsCount;

        if (highlightsCursor != null) {
            highlightsCount = highlightsCursor.getCount();
        } else {
            highlightsCount = 0;
        }

        return highlightsCount + 3;
    }

    public void swapHighlightsCursor(Cursor cursor) {
        highlightsCursor = cursor;

        notifyDataSetChanged();
    }

    public void swapTopSitesCursor(Cursor cursor) {
        this.topSitesCursor = cursor;

        notifyItemChanged(0);
    }

    @Override
    public long getItemId(int position) {
        final int type = getItemViewType(position);

        // To avoid having clashing IDs, we:
        // - use history ID's as is
        // - use hardcoded negative ID's for fixed panels
        // - multiply bookmark ID's by -1 to not clash with history, and add an offset to not
        //   clash with the fixed panels above
        final int offset = -10;

        // RecyclerView.NO_ID is -1, so start our hard-coded IDs at -2.
        switch (type) {
            case TopPanel.LAYOUT_ID:
                return -2;
            case WelcomePanel.LAYOUT_ID:
                return -3;
            case HighlightsTitle.LAYOUT_ID:
                return -4;
            case HighlightItem.LAYOUT_ID:
                final int cursorPosition = translatePositionToCursor(position);
                highlightsCursor.moveToPosition(cursorPosition);

                final long historyID = highlightsCursor.getLong(highlightsCursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
                final boolean isHistory = -1 != historyID;

                if (isHistory) {
                    return historyID;
                }

                final long bookmarkID = highlightsCursor.getLong(highlightsCursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID));
                final boolean isBookmark = -1 != bookmarkID;

                if (isBookmark) {
                    return -1 * bookmarkID + offset;
                }

                throw new IllegalArgumentException("Unhandled highlight type in getItemId - has no history or bookmark ID");
            default:
                throw new IllegalArgumentException("StreamItem with LAYOUT_ID=" + type + " not handled in getItemId()");
        }
    }
}
