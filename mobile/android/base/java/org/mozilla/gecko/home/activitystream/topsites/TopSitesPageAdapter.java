/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home.activitystream.topsites;

import android.content.Context;
import android.database.Cursor;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.model.TopSite;

import java.util.ArrayList;
import java.util.List;

public class TopSitesPageAdapter extends RecyclerView.Adapter<TopSitesCard> {
    private List<TopSite> topSites;
    private int tiles;
    private int tilesWidth;
    private int tilesHeight;
    private int textHeight;

    private final HomePager.OnUrlOpenListener onUrlOpenListener;
    private final HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener;

    public TopSitesPageAdapter(Context context, int tiles, int tilesWidth, int tilesHeight,
                               HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        setHasStableIds(true);

        this.topSites = new ArrayList<>();
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
    public void onBindViewHolder(TopSitesCard holder, int position) {
        holder.bind(topSites.get(position));
    }

    @Override
    public TopSitesCard onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        final FrameLayout card = (FrameLayout) inflater.inflate(R.layout.activity_stream_topsites_card, parent, false);
        final View content = card.findViewById(R.id.content);

        ViewGroup.LayoutParams layoutParams = content.getLayoutParams();
        layoutParams.width = tilesWidth;
        layoutParams.height = tilesHeight + textHeight;
        content.setLayoutParams(layoutParams);

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
}
