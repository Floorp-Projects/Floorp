/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.Loader;
import android.support.v4.graphics.ColorUtils;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.widget.FrameLayout;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.activitystream.topsites.TopSitesPagerAdapter;
import org.mozilla.gecko.util.ContextUtils;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

public class ActivityStream extends FrameLayout {
    private final StreamRecyclerAdapter adapter;

    private static final int LOADER_ID_HIGHLIGHTS = 0;
    private static final int LOADER_ID_TOPSITES = 1;

    private static final int MINIMUM_TILES = 4;
    private static final int MAXIMUM_TILES = 6;

    private int desiredTileWidth;
    private int desiredTilesHeight;
    private int tileMargin;

    public ActivityStream(Context context, AttributeSet attrs) {
        super(context, attrs);

        setBackgroundColor(ContextCompat.getColor(context, R.color.about_page_header_grey));

        inflate(context, R.layout.as_content, this);

        adapter = new StreamRecyclerAdapter();

        final RecyclerView rv = (RecyclerView) findViewById(R.id.activity_stream_main_recyclerview);

        rv.setAdapter(adapter);
        rv.setLayoutManager(new LinearLayoutManager(getContext()));
        rv.setHasFixedSize(true);

        RecyclerViewClickSupport.addTo(rv)
                .setOnItemClickListener(adapter);

        final Resources resources = getResources();
        desiredTileWidth = resources.getDimensionPixelSize(R.dimen.activity_stream_desired_tile_width);
        desiredTilesHeight = resources.getDimensionPixelSize(R.dimen.activity_stream_desired_tile_height);
        tileMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);
    }

    void setOnUrlOpenListeners(HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        adapter.setOnUrlOpenListeners(onUrlOpenListener, onUrlOpenInBackgroundListener);
    }

    public void load(LoaderManager lm) {
        CursorLoaderCallbacks callbacks = new CursorLoaderCallbacks();

        lm.initLoader(LOADER_ID_HIGHLIGHTS, null, callbacks);
        lm.initLoader(LOADER_ID_TOPSITES, null, callbacks);
    }

    public void unload() {
        adapter.swapHighlightsCursor(null);
        adapter.swapTopSitesCursor(null);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        int tiles = (w - tileMargin) / (desiredTileWidth + tileMargin);

        if (tiles < MINIMUM_TILES) {
            tiles = MINIMUM_TILES;

            setPadding(0, 0, 0, 0);
        } else if (tiles > MAXIMUM_TILES) {
            tiles = MAXIMUM_TILES;

            // Use the remaining space as padding
            int needed = tiles * (desiredTileWidth + tileMargin) + tileMargin;
            int padding = (w - needed) / 2;
            w = needed;

            setPadding(padding, 0, padding, 0);
        } else {
            setPadding(0, 0, 0, 0);
        }

        final float ratio = (float) desiredTilesHeight / (float) desiredTileWidth;
        final int tilesWidth = (w - (tiles * tileMargin) - tileMargin) / tiles;
        final int tilesHeight = (int) (ratio * tilesWidth);

        adapter.setTileSize(tiles, tilesWidth, tilesHeight);
    }

    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final Context context = getContext();
            if (id == LOADER_ID_HIGHLIGHTS) {
                return BrowserDB.from(context).getHighlights(context, 10);
            } else if (id == LOADER_ID_TOPSITES) {
                return BrowserDB.from(context).getActivityStreamTopSites(
                        context, TopSitesPagerAdapter.PAGES * MAXIMUM_TILES);
            } else {
                throw new IllegalArgumentException("Can't handle loader id " + id);
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor data) {
            if (loader.getId() == LOADER_ID_HIGHLIGHTS) {
                adapter.swapHighlightsCursor(data);
            } else if (loader.getId() == LOADER_ID_TOPSITES) {
                adapter.swapTopSitesCursor(data);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (loader.getId() == LOADER_ID_HIGHLIGHTS) {
                adapter.swapHighlightsCursor(null);
            } else if (loader.getId() == LOADER_ID_TOPSITES) {
                adapter.swapTopSitesCursor(null);
            }
        }
    }
}
