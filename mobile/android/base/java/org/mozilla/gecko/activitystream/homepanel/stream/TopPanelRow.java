/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.content.res.Resources;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.homepanel.model.Metadata;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPage;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPagerAdapter;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashSet;
import java.util.Set;

public class TopPanelRow extends StreamViewHolder {
    public static final int LAYOUT_ID = R.layout.activity_stream_main_toppanel;

    private final ViewPager topSitesPager;

    private static class SwipeListener extends ViewPager.SimpleOnPageChangeListener {
        int currentPosition = 0;

        @Override
        public void onPageSelected(int newPosition) {
            final String extra;
            if (newPosition > currentPosition) {
                extra = "swipe_forward";
            } else if (newPosition < currentPosition) {
                extra = "swipe_back";
            } else {
                // Selected the same page - this could happen if framework behaviour differs across
                // Android versions of manufacturers.
                return;
            }

            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.LIST, extra);
            currentPosition = newPosition;
        }
    }

    private final SwipeListener swipeListener = new SwipeListener();

    /**
     * @param onCardLongClickListener A listener for when a card is long-clicked.
     */
    public TopPanelRow(final View itemView, final HomePager.OnUrlOpenListener onUrlOpenListener,
            final OnCardLongClickListener onCardLongClickListener) {
        super(itemView);

        topSitesPager = (ViewPager) itemView.findViewById(R.id.topsites_pager);
        topSitesPager.setAdapter(new TopSitesPagerAdapter(itemView.getContext(), onUrlOpenListener, onCardLongClickListener));
        topSitesPager.addOnPageChangeListener(swipeListener);
    }

    public void bind(Cursor cursor, int tilesSize) {
        final TopSitesPagerAdapter adapter = (TopSitesPagerAdapter) topSitesPager.getAdapter();
        final Cursor filteredCursor = getCursorForUniqueBaseUrls(cursor);
        adapter.swapCursor(filteredCursor, tilesSize);

        final Resources resources = itemView.getResources();
        final int tilesMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);

        final int rows;

        // The cursor is null when the view is first created. The view will layout with the number of rows we
        // set initially (while we load the Cursor) and these rows (with no content) will be briefly visible
        // to users. Since we expect 2 rows to be the most common for users, we show 2 rows at this point;
        // the RecyclerView will gracefully animate a collapse if we display fewer.
        if (filteredCursor == null || filteredCursor.getCount() > TopSitesPage.NUM_COLUMNS) {
            rows = 2;
        } else if (cursor.getCount() > 0) {
            rows = 1;
        } else {
            // The user has deleted history and removed all suggested sites.
            rows = 0;
        }

        ViewGroup.LayoutParams layoutParams = topSitesPager.getLayoutParams();
        layoutParams.height = rows > 0 ? (tilesSize * rows) + (tilesMargin * 2) : 0;
        topSitesPager.setLayoutParams(layoutParams);

        // Reset the page position: binding a new Cursor means that topsites reverts to the first page,
        // no event is sent in that case, but we need to know the right page number to send correct
        // page swipe events
        swipeListener.currentPosition = 0;
    }

    @Nullable
    private Cursor getCursorForUniqueBaseUrls(final Cursor cursor) {
        if (cursor == null || !cursor.moveToFirst()) {
            return cursor;
        }

        final String[] projection = new String[] {
                BrowserContract.Combined._ID,
                BrowserContract.Combined.BOOKMARK_ID,
                BrowserContract.Combined.HISTORY_ID,
                BrowserContract.Combined.URL,
                BrowserContract.Combined.TITLE,
                BrowserContract.Highlights.POSITION,
                BrowserContract.TopSites.TYPE,
                BrowserContract.TopSites.PAGE_METADATA_JSON,
        };
        final int limit = TopSitesPage.NUM_TILES * TopSitesPagerAdapter.PAGES;
        final MatrixCursor filteredCursor = new MatrixCursor(projection);
        final Set<String> urls = new HashSet<String>(limit);
        do {
            String baseUrl = getBaseForUrl(cursor.getString(cursor.getColumnIndex(BrowserContract.Combined.URL)));
            boolean isPinned = isTopSitePinned(cursor.getInt(cursor.getColumnIndex(BrowserContract.TopSites.TYPE)));
            if (isPinned || !urls.contains(baseUrl)) {
                final Object[] originalColumns = new Object[] {
                        cursor.getLong(cursor.getColumnIndex(BrowserContract.Combined._ID)),
                        getBookmarkId(cursor),
                        cursor.getLong(cursor.getColumnIndex(BrowserContract.Combined.HISTORY_ID)),
                        cursor.getString(cursor.getColumnIndex(BrowserContract.Combined.URL)),
                        cursor.getString(cursor.getColumnIndex(BrowserContract.Combined.TITLE)),
                        cursor.getInt(cursor.getColumnIndex(BrowserContract.Highlights.POSITION)),
                        cursor.getInt(cursor.getColumnIndex(BrowserContract.TopSites.TYPE)),
                        Metadata.fromCursor(cursor, BrowserContract.TopSites.PAGE_METADATA_JSON)
                };
                // match the original cursor fields
                filteredCursor.addRow(originalColumns);
                urls.add(baseUrl);
            }
        } while (filteredCursor.getCount() < limit && cursor.moveToNext());

        return filteredCursor;
    }

    private Long getBookmarkId(Cursor cursor) {
        // If the value of BOOKMARK_ID column it's null that means the url is not bookmarked
        final int columnIndex = cursor.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID);
        return cursor.isNull(columnIndex) ? null : cursor.getLong(columnIndex);
    }

    private boolean isTopSitePinned(final int topSiteType) {
        return topSiteType == BrowserContract.TopSites.TYPE_PINNED;
    }

    private String getBaseForUrl(final String url) {
        try {
            return new URL(url).getHost();
        } catch (MalformedURLException e) {
            return null;
        }
    }

    public interface OnCardLongClickListener {
        boolean onLongClick(TopSite topSite, int absolutePosition, View tabletContextMenuAnchor, int faviconWidth, int faviconHeight);
    }
}