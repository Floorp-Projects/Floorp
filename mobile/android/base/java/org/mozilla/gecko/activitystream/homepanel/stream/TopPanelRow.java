/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.content.res.Resources;
import android.database.Cursor;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPage;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPagerAdapter;
import org.mozilla.gecko.home.HomePager;

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
        adapter.swapCursor(cursor, tilesSize);

        final Resources resources = itemView.getResources();
        final int tilesMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);

        final int rows;

        // The cursor is null when the view is first created. The view will layout with the number of rows we
        // set initially (while we load the Cursor) and these rows (with no content) will be briefly visible
        // to users. Since we expect 2 rows to be the most common for users, we show 2 rows at this point;
        // the RecyclerView will gracefully animate a collapse if we display fewer.
        if (cursor == null || cursor.getCount() > TopSitesPage.NUM_COLUMNS) {
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

    public interface OnCardLongClickListener {
        boolean onLongClick(TopSite topSite, int absolutePosition, View tabletContextMenuAnchor, int faviconWidth, int faviconHeight);
    }
}