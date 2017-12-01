/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.activitystream.homepanel.model.RowModel;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageModel;
import org.mozilla.gecko.activitystream.homepanel.model.WebpageRowModel;
import org.mozilla.gecko.activitystream.homepanel.stream.HighlightsEmptyStateRow;
import org.mozilla.gecko.activitystream.homepanel.stream.LearnMoreRow;
import org.mozilla.gecko.activitystream.homepanel.stream.TopPanelRow;
import org.mozilla.gecko.activitystream.homepanel.model.TopStory;
import org.mozilla.gecko.activitystream.homepanel.topstories.PocketStoriesLoader;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.activitystream.homepanel.stream.WebpageItemRow;
import org.mozilla.gecko.activitystream.homepanel.stream.StreamTitleRow;
import org.mozilla.gecko.activitystream.homepanel.stream.StreamViewHolder;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.Collections;
import java.util.EnumSet;
import java.util.LinkedList;
import java.util.List;

/**
 * The adapter for the Activity Stream panel.
 *
 * Every item is in a single adapter: Top Sites, Welcome panel, Highlights.
 */
public class StreamRecyclerAdapter extends RecyclerView.Adapter<StreamViewHolder> implements RecyclerViewClickSupport.OnItemClickListener,
        RecyclerViewClickSupport.OnItemLongClickListener {

    private static final String LOGTAG = StringUtils.safeSubstring("Gecko" + StreamRecyclerAdapter.class.getSimpleName(), 0, 23);

    private Cursor topSitesCursor;
    private List<RowModel> recyclerViewModel; // List of item types backing this RecyclerView.
    private List<TopStory> topStoriesQueue;

    // Content sections available on the Activity Stream page. These may be hidden if the sections are disabled.
    private final RowItemType[] ACTIVITY_STREAM_SECTIONS =
            { RowItemType.TOP_PANEL, RowItemType.TOP_STORIES_TITLE, RowItemType.HIGHLIGHTS_TITLE, RowItemType.LEARN_MORE_LINK };
    public static final int MAX_TOP_STORIES = 3;
    private static final String LINK_MORE_POCKET = "https://getpocket.com/explore/trending?src=ff_android&cdn=0";

    private HomePager.OnUrlOpenListener onUrlOpenListener;
    private HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    private int tilesSize;

    public enum RowItemType {
        TOP_PANEL (-2), // RecyclerView.NO_ID is -1, so start hard-coded stableIds at -2.
        TOP_STORIES_TITLE(-3),
        TOP_STORIES_ITEM(-1), // There can be multiple Top Stories items so caller should handle as a special case.
        HIGHLIGHTS_TITLE (-4),
        HIGHLIGHTS_EMPTY_STATE(-5),
        HIGHLIGHT_ITEM (-1), // There can be multiple Highlight Items so caller should handle as a special case.
        LEARN_MORE_LINK(-6);

        public final int stableId;

        RowItemType(int stableId) {
            this.stableId = stableId;
        }

        int getViewType() {
            return this.ordinal();
        }
    }

    private static RowModel makeRowModelFromType(final RowItemType type) {
        return new RowModel() {
            @Override
            public RowItemType getRowItemType() {
                return type;
            }
        };
    }

    public StreamRecyclerAdapter() {
        setHasStableIds(true);
        recyclerViewModel = new LinkedList<>();

        clearAndInit();
    }

    public void clearAndInit() {
        recyclerViewModel.clear();
        for (RowItemType type : ACTIVITY_STREAM_SECTIONS) {
            recyclerViewModel.add(makeRowModelFromType(type));
        }
        topStoriesQueue = Collections.emptyList();
    }

    void setOnUrlOpenListeners(HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    public void setTileSize(int tilesSize) {
        this.tilesSize = tilesSize;
        notifyDataSetChanged();
    }

    @Override
    public int getItemViewType(int position) {
        if (position >= recyclerViewModel.size()) {
            throw new IllegalArgumentException("Requested position, " + position + ", does not exist. Size is :" +
                    recyclerViewModel.size());
        }
        return recyclerViewModel.get(position).getRowItemType().getViewType();
    }

    @Override
    public StreamViewHolder onCreateViewHolder(final ViewGroup parent, final int type) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (type == RowItemType.TOP_PANEL.getViewType()) {
            return new TopPanelRow(inflater.inflate(TopPanelRow.LAYOUT_ID, parent, false), onUrlOpenListener, new TopPanelRow.OnCardLongClickListener() {
                @Override
                public boolean onLongClick(final TopSite topSite, final int absolutePosition,
                        final View tabletContextMenuAnchor, final int faviconWidth, final int faviconHeight) {
                    openContextMenuForTopSite(topSite, absolutePosition, tabletContextMenuAnchor, parent, faviconWidth, faviconHeight);
                    return true;
                }
            });
        } else if (type == RowItemType.TOP_STORIES_TITLE.getViewType()) {
            return new StreamTitleRow(inflater.inflate(StreamTitleRow.LAYOUT_ID, parent, false), R.string.activity_stream_topstories, R.string.activity_stream_link_more, LINK_MORE_POCKET, onUrlOpenListener);
        } else if (type == RowItemType.TOP_STORIES_ITEM.getViewType() ||
                type == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            return new WebpageItemRow(inflater.inflate(WebpageItemRow.LAYOUT_ID, parent, false), new WebpageItemRow.OnMenuButtonClickListener() {
                @Override
                public void onMenuButtonClicked(final WebpageItemRow row, final int position) {
                    openContextMenuForWebpageItemRow(row, position, parent, ActivityStreamTelemetry.Contract.INTERACTION_MENU_BUTTON);
                }
            });
        } else if (type == RowItemType.HIGHLIGHTS_TITLE.getViewType()) {
            return new StreamTitleRow(inflater.inflate(StreamTitleRow.LAYOUT_ID, parent, false), R.string.activity_stream_highlights);
        } else if (type == RowItemType.HIGHLIGHTS_EMPTY_STATE.getViewType()) {
            return new HighlightsEmptyStateRow(inflater.inflate(HighlightsEmptyStateRow.LAYOUT_ID, parent, false));
        } else if (type == RowItemType.LEARN_MORE_LINK.getViewType()) {
            return new LearnMoreRow(inflater.inflate(LearnMoreRow.LAYOUT_ID, parent, false));
        } else {
            throw new IllegalStateException("Missing inflation for ViewType " + type);
        }
    }

    /**
     * Returns the index of an item within highlights.
     * @param position position in adapter
     * @return index of item within highlights
     */
    private int getHighlightsIndexFromAdapterPosition(int position) {
        if (getItemViewType(position) != RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            throw new IllegalArgumentException("Item is not a highlight!");
        }
        return position - indexOfType(RowItemType.HIGHLIGHT_ITEM, recyclerViewModel);
    }

    /**
     * Returns the index of an item within top stories.
     * @param position position in adapter
     * @return index of item within top stories
     */
    private int getTopStoriesIndexFromAdapterPosition(int position) {
        if (getItemViewType(position) != RowItemType.TOP_STORIES_ITEM.getViewType()) {
            throw new IllegalArgumentException("Item is not a topstory!");
        }
        return position - indexOfType(RowItemType.TOP_STORIES_ITEM, recyclerViewModel);
    }

    @Override
    public void onBindViewHolder(StreamViewHolder holder, int position) {
        int type = getItemViewType(position);
        if (type == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            final Highlight highlight = (Highlight) recyclerViewModel.get(position);
            ((WebpageItemRow) holder).bind(highlight, position, tilesSize);
        } else if (type == RowItemType.TOP_PANEL.getViewType()) {
            ((TopPanelRow) holder).bind(topSitesCursor, tilesSize);
        } else if (type == RowItemType.TOP_STORIES_ITEM.getViewType()) {
            final TopStory story = (TopStory) recyclerViewModel.get(position);
            ((WebpageItemRow) holder).bind(story, position, tilesSize);
        } else if (type == RowItemType.HIGHLIGHTS_TITLE.getViewType()
                || type == RowItemType.HIGHLIGHTS_EMPTY_STATE.getViewType()) {
            final Context context = holder.itemView.getContext();
            final SharedPreferences sharedPreferences = GeckoSharedPrefs.forProfile(context);
            final boolean bookmarksEnabled = sharedPreferences.getBoolean(ActivityStreamPanel.PREF_BOOKMARKS_ENABLED,
                    context.getResources().getBoolean(R.bool.pref_activitystream_recentbookmarks_enabled_default));
            final boolean visitedEnabled = sharedPreferences.getBoolean(ActivityStreamPanel.PREF_VISITED_ENABLED,
                    context.getResources().getBoolean(R.bool.pref_activitystream_visited_enabled_default));
            setViewVisible(bookmarksEnabled || visitedEnabled, holder.itemView);
        } else if (type == RowItemType.TOP_STORIES_TITLE.getViewType()) {
            final Context context = holder.itemView.getContext();
            final boolean pocketEnabled = ActivityStreamConfiguration.isPocketEnabledByLocale(context) &&
                    GeckoSharedPrefs.forProfile(context).getBoolean(ActivityStreamPanel.PREF_POCKET_ENABLED,
                    context.getResources().getBoolean(R.bool.pref_activitystream_pocket_enabled_default));
            setViewVisible(pocketEnabled, holder.itemView);
        }
    }

    /**
     * This sets a child view of the adapter visible or hidden.
     *
     * This only applies to children whose height and width are WRAP_CONTENT and MATCH_PARENT
     * respectively.
     *
     * NB: This is a hack for the views that are included in the RecyclerView adapter even if
     * they shouldn't be shown, such as the section title views or the empty view for highlights.
     *
     * A more correct implementation would dynamically add/remove these title views rather than
     * showing and hiding them.
     *
     * @param toShow true if the view is to be shown, false to be hidden
     * @param view child View whose visibility is to be changed
     */
    private static void setViewVisible(boolean toShow, final View view) {
        view.setVisibility(toShow ? View.VISIBLE : View.GONE);
        // We also need to set the layout height and width to 0 for the RecyclerView child.
        final RecyclerView.LayoutParams layoutParams = (RecyclerView.LayoutParams) view.getLayoutParams();
        if (toShow) {
            layoutParams.height = RecyclerView.LayoutParams.WRAP_CONTENT;
            layoutParams.width = RecyclerView.LayoutParams.MATCH_PARENT;
        } else {
            layoutParams.height = 0;
            layoutParams.width = 0;
        }
        view.setLayoutParams(layoutParams);
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (!onItemClickIsValidRowItem(position)) {
            return;
        }

        final WebpageRowModel model = (WebpageRowModel) recyclerViewModel.get(position);

        final String sourceType;
        final int actionPosition;
        final int size;
        final String referrerUri;
        final int viewType = getItemViewType(position);

        final ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder();
        if (viewType == RowItemType.HIGHLIGHT_ITEM.getViewType()) {
            extras.forHighlightSource(model.getSource());
            sourceType = ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS;
            actionPosition = getHighlightsIndexFromAdapterPosition(position);
            size = getNumOfTypeShown(RowItemType.HIGHLIGHT_ITEM);
            referrerUri = null;
        } else {
            sourceType = ActivityStreamTelemetry.Contract.TYPE_POCKET;
            actionPosition = getTopStoriesIndexFromAdapterPosition(position);
            size = getNumOfTypeShown(RowItemType.TOP_STORIES_ITEM);
            referrerUri = PocketStoriesLoader.POCKET_REFERRER_URI;
        }

        extras.set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, sourceType)
              .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, actionPosition)
              .set(ActivityStreamTelemetry.Contract.COUNT, size);

        Telemetry.sendUIEvent(
                TelemetryContract.Event.LOAD_URL,
                TelemetryContract.Method.LIST_ITEM,
                extras.build()
        );

        // NB: This is hacky. We need to process telemetry data first, otherwise we run a risk of
        // not having a cursor to work with once url is opened and BrowserApp closes A-S home screen
        // and clears its resources (read: cursors). See Bug 1326018.
        onUrlOpenListener.onUrlOpenWithReferrer(model.getUrl(), referrerUri,
                EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
    }

    @Override
    public boolean onItemLongClicked(final RecyclerView recyclerView, final int position, final View v) {
        if (!onItemClickIsValidRowItem(position)) {
            return false;
        }

        final WebpageItemRow highlightItem = (WebpageItemRow) recyclerView.getChildViewHolder(v);
        openContextMenuForWebpageItemRow(highlightItem, position, recyclerView, ActivityStreamTelemetry.Contract.INTERACTION_LONG_CLICK);
        return true;
    }

    private boolean onItemClickIsValidRowItem(final int position) {
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

        final int viewType = getItemViewType(position);
        if (viewType != RowItemType.HIGHLIGHT_ITEM.getViewType()
                && viewType != RowItemType.TOP_STORIES_ITEM.getViewType()) {
            // Headers (containing topsites and/or the highlights title) do their own click handling as needed
            return false;
        }

        return true;
    }

    private void openContextMenuForWebpageItemRow(final WebpageItemRow webpageItemRow, final int position, final View snackbarAnchor,
            @NonNull final String interactionExtra) {
        final WebpageRowModel model = (WebpageRowModel) recyclerViewModel.get(position);

        final String sourceType;
        final int actionPosition;
        final ActivityStreamContextMenu.MenuMode menuMode;

        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder();
        if (model.getRowItemType() == RowItemType.HIGHLIGHT_ITEM) {
            extras.forHighlightSource(model.getSource());
            sourceType = ActivityStreamTelemetry.Contract.TYPE_HIGHLIGHTS;
            actionPosition = getHighlightsIndexFromAdapterPosition(position);
            menuMode = ActivityStreamContextMenu.MenuMode.HIGHLIGHT;
        } else {
            sourceType = ActivityStreamTelemetry.Contract.TYPE_POCKET;
            actionPosition = getTopStoriesIndexFromAdapterPosition(position);
            menuMode = ActivityStreamContextMenu.MenuMode.TOPSTORY;
        }

        extras.set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, sourceType)
              .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, actionPosition)
              .set(ActivityStreamTelemetry.Contract.INTERACTION, interactionExtra);

        openContextMenuInner(webpageItemRow.getTabletContextMenuAnchor(), snackbarAnchor, extras, menuMode, model,
                /* shouldOverrideWithImageProvider */ true, // we use image providers in HighlightItem.pageIconLayout.
                webpageItemRow.getTileWidth(), webpageItemRow.getTileHeight());
    }

    private void openContextMenuForTopSite(final TopSite topSite, final int absolutePosition, final View tabletContextMenuAnchor,
            final View snackbarAnchor, final int faviconWidth, final int faviconHeight) {
        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .forTopSite(topSite)
                .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, absolutePosition);

        openContextMenuInner(tabletContextMenuAnchor, snackbarAnchor, extras, ActivityStreamContextMenu.MenuMode.TOPSITE, topSite,
                /* shouldOverrideWithImageProvider */ false, // we only use favicons for top sites.
                faviconWidth, faviconHeight);
    }

    /**
     * @param snackbarAnchor See {@link ActivityStreamContextMenu#show(View, View, ActivityStreamTelemetry.Extras.Builder, ActivityStreamContextMenu.MenuMode, WebpageModel, boolean, HomePager.OnUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener, int, int)} )}
     *                       for additional details.
     */
    private void openContextMenuInner(final View tabletContextMenuAnchor, final View snackbarAnchor,
            final ActivityStreamTelemetry.Extras.Builder extras,
            final ActivityStreamContextMenu.MenuMode menuMode, final WebpageModel webpageModel,
            final boolean shouldOverrideWithImageProvider,
            final int faviconWidth, final int faviconHeight) {
        ActivityStreamContextMenu.show(tabletContextMenuAnchor, snackbarAnchor,
                extras,
                menuMode,
                webpageModel,
                shouldOverrideWithImageProvider,
                onUrlOpenListener, onUrlOpenInBackgroundListener,
                faviconWidth, faviconHeight);

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
        final int insertionIndex = indexOfType(RowItemType.HIGHLIGHTS_TITLE, recyclerViewModel) + 1;
        if (getNumOfTypeShown(RowItemType.HIGHLIGHTS_EMPTY_STATE) > 0) {
            recyclerViewModel.remove(insertionIndex); // remove empty state.
        } else {
            int numHighlights = getNumOfTypeShown(RowItemType.HIGHLIGHT_ITEM);
            while (numHighlights > 0) {
                recyclerViewModel.remove(insertionIndex);
                numHighlights--;
            }
        }

        if (!highlights.isEmpty()) {
            recyclerViewModel.addAll(insertionIndex, highlights);
        } else {
            recyclerViewModel.add(insertionIndex, makeRowModelFromType(RowItemType.HIGHLIGHTS_EMPTY_STATE));
        }
        notifyDataSetChanged();
    }

    public void swapTopStories(List<TopStory> newStories) {
        final int insertionIndex = indexOfType(RowItemType.TOP_STORIES_TITLE, recyclerViewModel) + 1;
        int numOldStories = getNumOfTypeShown(RowItemType.TOP_STORIES_ITEM);
        while (numOldStories > 0) {
            recyclerViewModel.remove(insertionIndex);
            numOldStories--;
        }

        topStoriesQueue = newStories;
        for (int i = 0; i < Math.min(MAX_TOP_STORIES, topStoriesQueue.size()); i++) {
            recyclerViewModel.add(insertionIndex + i, topStoriesQueue.get(i));
        }

        notifyDataSetChanged();
    }

    /**
     * Returns the index of the first item of the type found.
     * @param type viewType of RowItemType
     * @param rowModelList List to be indexed into
     * @return index of first item of the type, or -1 if it none exist.
     */
    private static int indexOfType(RowItemType type, List<RowModel> rowModelList) {
        for (int i = 0; i < rowModelList.size(); i++) {
            if (rowModelList.get(i).getRowItemType() == type) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Returns the number of consecutive items in the adapter of the item type specified.
     *
     * This is intended to be used for counting the items that have a dynamic count
     * (such as Highlights or TopStory)
     *
     * @param type RowItemType to be counted
     * @return The number of items shown.
     */
    private int getNumOfTypeShown(RowItemType type) {
        final int startIndex = indexOfType(type, recyclerViewModel);
        if (startIndex == -1) {
            return 0;
        }
        int count = 0;
        for (int i = startIndex; i < recyclerViewModel.size(); i++) {
            if (getItemViewType(i) == type.getViewType()) {
                count++;
            } else {
                break;
            }
        }
        return count;
    }

    public void swapTopSitesCursor(Cursor cursor) {
        this.topSitesCursor = cursor;
        notifyItemChanged(0);
    }

    @Override
    public long getItemId(int position) {
        final int viewType = getItemViewType(position);
        if (viewType == RowItemType.HIGHLIGHT_ITEM.getViewType()
                || viewType == RowItemType.TOP_STORIES_ITEM.getViewType()) {
            // Highlights are always picked from recent history - So using the history id should
            // give us a unique (positive) id.
            final WebpageRowModel model = (WebpageRowModel) recyclerViewModel.get(position);
            return model.getUniqueId();
        } else {
            return recyclerViewModel.get(position).getRowItemType().stableId;
        }
    }
}
