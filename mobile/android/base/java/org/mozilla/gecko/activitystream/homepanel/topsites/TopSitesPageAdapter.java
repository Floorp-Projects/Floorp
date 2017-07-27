/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel.topsites;

import android.content.Context;
import android.database.Cursor;
import android.support.annotation.UiThread;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.model.TopSite;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

/* package-local */ class TopSitesPageAdapter extends RecyclerView.Adapter<TopSitesCard> implements RecyclerViewClickSupport.OnItemClickListener {
    private List<TopSite> topSites;
    private final int pageNumber;
    private int tiles;
    private int tilesWidth;
    private int tilesHeight;
    private int textHeight;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    /* package-local */ TopSitesPageAdapter(Context context, int pageNumber, int tiles, int tilesWidth, int tilesHeight,
                               HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        setHasStableIds(true);

        this.topSites = new ArrayList<>();
        this.pageNumber = pageNumber;
        this.tiles = tiles;
        this.tilesWidth = tilesWidth;
        this.tilesHeight = tilesHeight;
        this.textHeight = context.getResources().getDimensionPixelSize(R.dimen.activity_stream_top_sites_text_height);

        this.onUrlOpenListener = onUrlOpenListener;
        this.onUrlOpenInBackgroundListener = onUrlOpenInBackgroundListener;
    }

    /**
     * @param startIndex The first item that this topsites group should show. This item, and the following
     * 3 items will be displayed by this adapter.
     */
    public void swapCursor(Cursor cursor, int startIndex) {
        topSites.clear();

        if (cursor == null) {
            return;
        }

        for (int i = 0; i < tiles && startIndex + i < cursor.getCount(); i++) {
            cursor.moveToPosition(startIndex + i);

            topSites.add(TopSite.fromCursor(cursor));
        }

        notifyDataSetChanged();
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        final TopSite topSite = topSites.get(position);
        final int absolutePosition = getTopSiteAbsolutePosition(position);

        ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                .forTopSite(topSite)
                .set(ActivityStreamTelemetry.Contract.PAGE_NUMBER, pageNumber)
                .set(ActivityStreamTelemetry.Contract.ACTION_POSITION, absolutePosition);

        Telemetry.sendUIEvent(
                TelemetryContract.Event.LOAD_URL,
                TelemetryContract.Method.LIST_ITEM,
                extras.build()
        );

        onUrlOpenListener.onUrlOpen(StringUtils.decodeUserEnteredUrl(topSite.getUrl()), EnumSet.noneOf(HomePager.OnUrlOpenListener.Flags.class));
    }

    @Override
    public void onBindViewHolder(TopSitesCard holder, int position) {
        holder.bind(topSites.get(position), getTopSiteAbsolutePosition(position));
    }

    @Override
    public TopSitesCard onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        final FrameLayout card = (FrameLayout) inflater.inflate(R.layout.activity_stream_topsites_card, parent, false);

        ViewGroup.LayoutParams layoutParams = card.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = tilesHeight + textHeight;
        card.setLayoutParams(layoutParams);

        return new TopSitesCard(card, onUrlOpenListener, onUrlOpenInBackgroundListener);
    }

    @Override
    public int getItemCount() {
        return topSites.size();
    }

    @Override
    @UiThread
    public long getItemId(int position) {
        return topSites.get(position).getId();
    }

    /**
     * This assumes that every TopSite page up to the current one has the same number of tiles.
     * relativePosition must range from 0 to {number of tiles on the current page}.
     */
    private int getTopSiteAbsolutePosition(int relativePosition) {
        if (relativePosition < 0 || relativePosition > tiles) {
            throw new IllegalArgumentException("Illegal relative top site position encountered");
        }
        return relativePosition + pageNumber * tiles;
    }
}
