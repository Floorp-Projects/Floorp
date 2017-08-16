/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.activitystream.homepanel.stream.HighlightItem;
import org.mozilla.gecko.activitystream.homepanel.stream.HighlightsTitle;
import org.mozilla.gecko.activitystream.homepanel.stream.StreamItem;
import org.mozilla.gecko.activitystream.homepanel.stream.TopPanel;
import org.mozilla.gecko.activitystream.homepanel.stream.WelcomePanel;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.EnumSet;
import java.util.LinkedList;
import java.util.List;

/**
 * The adapter for the Activity Stream panel.
 *
 * Every item is in a single adapter: Top Sites, Welcome panel, Highlights.
 */
public class StreamRecyclerAdapter extends RecyclerView.Adapter<StreamItem> implements RecyclerViewClickSupport.OnItemClickListener,
        RecyclerViewClickSupport.OnItemLongClickListener, StreamHighlightItemContextMenuListener {

    private static final String LOGTAG = StringUtils.safeSubstring("Gecko" + StreamRecyclerAdapter.class.getSimpleName(), 0, 23);

    private Cursor topSitesCursor;
    private List<RowItem> recyclerViewModel; // List of item types backing this RecyclerView.

    private final RowItemType[] FIXED_ROWS = {RowItemType.TOP_PANEL, RowItemType.WELCOME, RowItemType.HIGHLIGHTS_TITLE};
    private static final int HIGHLIGHTS_OFFSET = 3; // Topsites, Welcome, Highlights Title

    private HomePager.OnUrlOpenListener onUrlOpenListener;
    private HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    private int tiles;
    private int tilesSize;

    public interface RowItem {
        RowItemType getRowItemType();
    }

    public enum RowItemType {
        TOP_PANEL (-2), // RecyclerView.NO_ID is -1, so start hard-coded stableIds at -2.
        WELCOME (-3),
        HIGHLIGHTS_TITLE (-4),
        HIGHLIGHT_ITEM (-1); // There can be multiple Highlight Items so caller should handle as a special case.

        public final int stableId;

        RowItemType(int stableId) {
            this.stableId = stableId;
        }

        int getViewType() {
            return this.ordinal();
        }
    }

    private static RowItem makeRowItemFromType(final RowItemType type) {
        return new RowItem() {
            @Override
            public RowItemType getRowItemType() {
                return type;
            }
        };
    }

    public StreamRecyclerAdapter() {
        setHasStableIds(true);
        recyclerViewModel = new LinkedList<>();
        for (RowItemType type : FIXED_ROWS) {
            recyclerViewModel.add(makeRowItemFromType(type));
        }
    }

    void setOnUrlOpenListeners(HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    public void setTileSize(int tiles, int tilesSize) {
        this.tilesSize = tilesSize;
        this.tiles = tiles;

        notifyDataSetChanged();
    }

    @Override
    public int getItemViewType(int position) {
        if (position >= recyclerViewModel.size()) {
            throw new IllegalArgumentException("Requested position does not exist");
        }
        return recyclerViewModel.get(position).getRowItemType().getViewType();
    }

    @Override
    public StreamItem onCreateViewHolder(ViewGroup parent, final int type) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (type == RowItemType.TOP_PANEL.getViewType()) {
            return new TopPanel(inflater.inflate(TopPanel.LAYOUT_ID, parent, false), onUrlOpenListener, onUrlOpenInBackgroundListener);
        } else if (type == RowItemType.WELCOME.getViewType()) {
            return new WelcomePanel(inflater.inflate(WelcomePanel.LAYOUT_ID, parent, false), this);
        } else if (type == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            return new HighlightItem(inflater.inflate(HighlightItem.LAYOUT_ID, parent, false), this);
        } else if (type == RowItemType.HIGHLIGHTS_TITLE.getViewType()) {
            return new HighlightsTitle(inflater.inflate(HighlightsTitle.LAYOUT_ID, parent, false));
        } else {
            throw new IllegalStateException("Missing inflation for ViewType " + type);
        }
    }

    private int getHighlightsOffsetFromRVPosition(int position) {
        return position - HIGHLIGHTS_OFFSET;
    }

    @Override
    public void onBindViewHolder(StreamItem holder, int position) {
        int type = getItemViewType(position);
        if (type == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            final Highlight highlight = (Highlight) recyclerViewModel.get(position);
            ((HighlightItem) holder).bind(highlight, position, tilesSize);
        } else if (type == RowItemType.TOP_PANEL.getViewType()) {
            ((TopPanel) holder).bind(topSitesCursor, tiles, tilesSize);
        }
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (!onItemClickIsValidHighlightItem(position)) {
            return;
        }

        final Highlight highlight = (Highlight) recyclerViewModel.get(position);

        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .forHighlightSource(highlight.getSource())
                .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS)
                .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, getHighlightsOffsetFromRVPosition(position))
                .set(ActivityStreamTelemetry.Contract.COUNT, recyclerViewModel.size() - FIXED_ROWS.length);

        Telemetry.sendUIEvent(
                TelemetryContract.Event.LOAD_URL,
                TelemetryContract.Method.LIST_ITEM,
                extras.build()
        );

        // NB: This is hacky. We need to process telemetry data first, otherwise we run a risk of
        // not having a cursor to work with once url is opened and BrowserApp closes A-S home screen
        // and clears its resources (read: cursors). See Bug 1326018.
        onUrlOpenListener.onUrlOpen(highlight.getUrl(), EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
    }

    @Override
    public boolean onItemLongClicked(final RecyclerView recyclerView, final int position, final View v) {
        if (!onItemClickIsValidHighlightItem(position)) {
            return false;
        }

        final HighlightItem highlightItem = (HighlightItem) recyclerView.getChildViewHolder(v);
        openContextMenu(highlightItem, position, ActivityStreamTelemetry.Contract.INTERACTION_LONG_CLICK);
        return true;
    }

    private boolean onItemClickIsValidHighlightItem(final int position) {
        if (getItemViewType(position) != RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            // Headers (containing topsites and/or the highlights title) do their own click handling as needed
            return false;
        }

        // The position this method receives is from RecyclerView.ViewHolder.getAdapterPosition, whose docs state:
        // "Note that if you've called notifyDataSetChanged(), until the next layout pass, the return value of this
        // method will be NO_POSITION."
        //
        // At the time of writing, we call notifyDataSetChanged for:
        // - swapHighlights (can't do anything about this)
        // - setTileSize (in theory, we might be able to do something hacky to get the existing highlights list)
        //
        // Given the low crash rate (34 crashes in 23 days), I don't think it's worth investigating further
        // or adding a hack.
        if (position == RecyclerView.NO_POSITION) {
            Log.w(LOGTAG, "onItemClicked: received NO_POSITION. Returning");
            return false;
        }

        return true;
    }

    @Override
    public void openContextMenu(final HighlightItem highlightItem, final int position, @NonNull final String interactionExtra) {
        final Highlight highlight = (Highlight) recyclerViewModel.get(position);

        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS)
                .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, position - HIGHLIGHTS_OFFSET)
                .set(ActivityStreamTelemetry.Contract.INTERACTION, interactionExtra)
                .forHighlightSource(highlight.getSource());

        ActivityStreamContextMenu.show(highlightItem.itemView.getContext(),
                highlightItem.getContextMenuAnchor(),
                extras,
                ActivityStreamContextMenu.MenuMode.HIGHLIGHT,
                highlight,
                /* shouldOverrideWithImageProvider */ true, // we use image providers in HighlightItem.pageIconLayout.
                onUrlOpenListener, onUrlOpenInBackgroundListener,
                highlightItem.getTileWidth(), highlightItem.getTileHeight());

        Telemetry.sendUIEvent(
                TelemetryContract.Event.SHOW,
                TelemetryContract.Method.CONTEXT_MENU,
                extras.build()
        );
    }

    @Override
    public int getItemCount() {
        return recyclerViewModel.size();
    }

    public void swapHighlights(List<Highlight> highlights) {
        recyclerViewModel = recyclerViewModel.subList(0, HIGHLIGHTS_OFFSET);
        recyclerViewModel.addAll(highlights);
        notifyDataSetChanged();
    }

    public void swapTopSitesCursor(Cursor cursor) {
        this.topSitesCursor = cursor;
        notifyItemChanged(0);
    }

    @Override
    public long getItemId(int position) {
        final int viewType = getItemViewType(position);
        if (viewType == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            // Highlights are always picked from recent history - So using the history id should
            // give us a unique (positive) id.
            final Highlight highlight = (Highlight) recyclerViewModel.get(position);
            return highlight.getHistoryId();
        } else {
            return recyclerViewModel.get(position).getRowItemType().stableId;
        }
    }
}
