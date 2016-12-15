/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Color;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewPager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStream.LabelCallback;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.home.activitystream.topsites.CirclePageIndicator;
import org.mozilla.gecko.home.activitystream.topsites.TopSitesPagerAdapter;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.util.DrawableUtil;
import org.mozilla.gecko.util.ViewUtil;
import org.mozilla.gecko.util.TouchTargetUtil;
import org.mozilla.gecko.widget.FaviconView;

import java.util.concurrent.Future;

import static org.mozilla.gecko.activitystream.ActivityStream.extractLabel;

public abstract class StreamItem extends RecyclerView.ViewHolder {
    public StreamItem(View itemView) {
        super(itemView);
    }

    public static class HighlightsTitle extends StreamItem {
        public static final int LAYOUT_ID = R.layout.activity_stream_main_highlightstitle;

        public HighlightsTitle(final View itemView) {
            super(itemView);
        }
    }

    public static class WelcomePanel
            extends StreamItem
            implements View.OnClickListener {
        public static final int LAYOUT_ID = R.layout.activity_stream_main_welcomepanel;

        public static final String PREF_WELCOME_DISMISSED = "activitystream.welcome_dismissed";

        private final RecyclerView.Adapter<StreamItem> adapter;
        private final Context context;

        public WelcomePanel(final View itemView, final RecyclerView.Adapter<StreamItem> adapter) {
            super(itemView);

            this.adapter = adapter;
            this.context = itemView.getContext();

            final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(itemView.getContext());

            if (!sharedPrefs.getBoolean(PREF_WELCOME_DISMISSED, false)) {
                final ViewStub welcomePanelStub = (ViewStub) itemView.findViewById(R.id.welcomepanel_stub);

                welcomePanelStub.inflate();

                final Button dismissButton = (Button) itemView.findViewById(R.id.dismiss_welcomepanel);

                dismissButton.setOnClickListener(this);
            }
        }

        @Override
        public void onClick(View v) {
            // To animate between item changes, RecyclerView keeps around the old version of the view,
            // and creates a new equivalent item (which is bound using the new data) - followed by
            // animating between those two versions. Hence we just need to make sure that
            // any future calls to onCreateViewHolder create a version of the Header Item
            // with the welcome panel hidden (i.e. we don't need to care about animations ourselves).
            // We communicate this state change via the pref.

            final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(context);

            sharedPrefs.edit()
                    .putBoolean(WelcomePanel.PREF_WELCOME_DISMISSED, true)
                    .apply();

            adapter.notifyItemChanged(getAdapterPosition());
        }
    }

    public static class TopPanel extends StreamItem {
        public static final int LAYOUT_ID = R.layout.activity_stream_main_toppanel;

        private final ViewPager topSitesPager;

        public TopPanel(View itemView, HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
            super(itemView);

            topSitesPager = (ViewPager) itemView.findViewById(R.id.topsites_pager);
            topSitesPager.setAdapter(new TopSitesPagerAdapter(itemView.getContext(), onUrlOpenListener, onUrlOpenInBackgroundListener));

            CirclePageIndicator indicator = (CirclePageIndicator) itemView.findViewById(R.id.topsites_indicator);
            indicator.setViewPager(topSitesPager);
        }

        public void bind(Cursor cursor, int tiles, int tilesWidth, int tilesHeight) {
            final TopSitesPagerAdapter adapter = (TopSitesPagerAdapter) topSitesPager.getAdapter();
            adapter.setTilesSize(tiles, tilesWidth, tilesHeight);
            adapter.swapCursor(cursor);

            final Resources resources = itemView.getResources();
            final int tilesMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);
            final int textHeight = resources.getDimensionPixelSize(R.dimen.activity_stream_top_sites_text_height);

            ViewGroup.LayoutParams layoutParams = topSitesPager.getLayoutParams();
            layoutParams.height = tilesHeight + tilesMargin + textHeight;
            topSitesPager.setLayoutParams(layoutParams);
        }
    }

    public static class HighlightItem extends StreamItem implements IconCallback {
        public static final int LAYOUT_ID = R.layout.activity_stream_card_history_item;

        enum HighlightSource {
            VISITED,
            BOOKMARKED
        }

        String title;
        String url;

        @Nullable Boolean isPinned;
        @Nullable Boolean isBookmarked;

        final FaviconView vIconView;
        final TextView vLabel;
        final TextView vTimeSince;
        final TextView vSourceView;
        final TextView vPageView;
        final ImageView vSourceIconView;

        private Future<IconResponse> ongoingIconLoad;
        private int tilesMargin;

        public HighlightItem(final View itemView,
                             final HomePager.OnUrlOpenListener onUrlOpenListener,
                             final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
            super(itemView);

            tilesMargin = itemView.getResources().getDimensionPixelSize(R.dimen.activity_stream_base_margin);

            vLabel = (TextView) itemView.findViewById(R.id.card_history_label);
            vTimeSince = (TextView) itemView.findViewById(R.id.card_history_time_since);
            vIconView = (FaviconView) itemView.findViewById(R.id.icon);
            vSourceView = (TextView) itemView.findViewById(R.id.card_history_source);
            vPageView = (TextView) itemView.findViewById(R.id.page);
            vSourceIconView = (ImageView) itemView.findViewById(R.id.source_icon);

            final ImageView menuButton = (ImageView) itemView.findViewById(R.id.menu);

            menuButton.setImageDrawable(
                    DrawableUtil.tintDrawable(menuButton.getContext(), R.drawable.menu, Color.LTGRAY));

            TouchTargetUtil.ensureTargetHitArea(menuButton, itemView);

            menuButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    ActivityStreamContextMenu.show(v.getContext(),
                            menuButton,
                            ActivityStreamContextMenu.MenuMode.HIGHLIGHT,
                            title, url, isBookmarked, isPinned,
                            onUrlOpenListener, onUrlOpenInBackgroundListener,
                            vIconView.getWidth(), vIconView.getHeight());
                }
            });

            ViewUtil.enableTouchRipple(menuButton);
        }

        public void bind(Cursor cursor, int tilesWidth, int tilesHeight) {

            final long time = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.DATE));
            final String ago = DateUtils.getRelativeTimeSpanString(time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, 0).toString();

            title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.History.TITLE));
            url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));

            vLabel.setText(title);
            vTimeSince.setText(ago);

            ViewGroup.LayoutParams layoutParams = vIconView.getLayoutParams();
            layoutParams.width = tilesWidth - tilesMargin;
            layoutParams.height = tilesHeight;
            vIconView.setLayoutParams(layoutParams);

            final HighlightSource source = highlightSource(cursor);

            updateStateForSource(source, cursor);
            updateUiForSource(source);
            updatePage(url);

            if (ongoingIconLoad != null) {
                ongoingIconLoad.cancel(true);
            }

            ongoingIconLoad = Icons.with(itemView.getContext())
                    .pageUrl(url)
                    .skipNetwork()
                    .build()
                    .execute(this);
        }

        private void updateStateForSource(HighlightSource source, Cursor cursor) {
            // We can only be certain of bookmark state if an item is a bookmark item.
            // Otherwise, due to the underlying highlights query, we have to look up states when
            // menus are displayed.
            switch (source) {
                case BOOKMARKED:
                    isBookmarked = true;
                    isPinned = null;
                    break;
                case VISITED:
                    isBookmarked = null;
                    isPinned = null;
                    break;
                default:
                    throw new IllegalArgumentException("Unknown source: " + source);
            }
        }

        private void updateUiForSource(HighlightSource source) {
            switch (source) {
                case BOOKMARKED:
                    vSourceView.setText(R.string.activity_stream_highlight_label_bookmarked);
                    vSourceView.setVisibility(View.VISIBLE);
                    vSourceIconView.setImageResource(R.drawable.ic_as_bookmarked);
                    break;
                case VISITED:
                    vSourceView.setText(R.string.activity_stream_highlight_label_visited);
                    vSourceView.setVisibility(View.VISIBLE);
                    vSourceIconView.setImageResource(R.drawable.ic_as_visited);
                    break;
                default:
                    vSourceView.setVisibility(View.INVISIBLE);
                    vSourceIconView.setImageResource(0);
                    break;
            }

            // TODO Why?
            // vSourceView.setText(vSourceView.getText());
        }

        private void updatePage(final String url) {
            extractLabel(itemView.getContext(), url, false, new LabelCallback() {
                @Override
                public void onLabelExtracted(String label) {
                    vPageView.setText(TextUtils.isEmpty(label) ? url : label);
                }
            });
        }

        @Override
        public void onIconResponse(IconResponse response) {
            vIconView.updateImage(response);
        }
    }

    private static HighlightItem.HighlightSource highlightSource(final Cursor cursor) {
        if (-1 != cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID))) {
            return HighlightItem.HighlightSource.BOOKMARKED;
        }

        if (-1 != cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID))) {
            return HighlightItem.HighlightSource.VISITED;
        }

        throw new IllegalArgumentException("Unknown highlight source.");
    }
}
