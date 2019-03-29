/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.Loader;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.activitystream.homepanel.model.TopStory;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPage;
import org.mozilla.gecko.activitystream.homepanel.topstories.PocketStoriesLoader;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.activitystream.homepanel.topsites.TopSitesPagerAdapter;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.Collections;
import java.util.List;

public class ActivityStreamPanel extends FrameLayout {
    private final StreamRecyclerAdapter adapter;

    private static final int LOADER_ID_HIGHLIGHTS = 0;
    private static final int LOADER_ID_TOPSITES = 1;
    private static final int LOADER_ID_POCKET = 2;

    /**
     * Number of database entries to consider and rank for finding highlights.
     */
    private static final int HIGHLIGHTS_CANDIDATES = 500;

    /**
     * Number of highlights that should be returned (max).
     */
    private static final int HIGHLIGHTS_LIMIT = 10;

    public static final String PREF_POCKET_ENABLED = "pref_activitystream_pocket_enabled";
    public static final String PREF_VISITED_ENABLED = "pref_activitystream_visited_enabled";
    public static final String PREF_BOOKMARKS_ENABLED = "pref_activitystream_recentbookmarks_enabled";

    private final RecyclerView contentRecyclerView;

    private int desiredTileWidth;
    private int tileMargin;
    private final SharedPreferences sharedPreferences;

    public ActivityStreamPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        setBackgroundColor(ContextCompat.getColor(context, R.color.photon_browser_toolbar_bg));

        inflate(context, R.layout.as_content, this);

        adapter = new StreamRecyclerAdapter();
        sharedPreferences = GeckoSharedPrefs.forProfile(context);

        contentRecyclerView = (RecyclerView) findViewById(R.id.activity_stream_main_recyclerview);
        contentRecyclerView.setAdapter(adapter);
        contentRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        contentRecyclerView.setHasFixedSize(true);
        // Override item animations to avoid horrible topsites refreshing
        contentRecyclerView.setItemAnimator(new StreamItemAnimator());
        contentRecyclerView.addItemDecoration(new HighlightsDividerItemDecoration(context));

        RecyclerViewClickSupport.addTo(contentRecyclerView)
                .setOnItemClickListener(adapter)
                .setOnItemLongClickListener(adapter);

        final Resources resources = getResources();
        desiredTileWidth = resources.getDimensionPixelSize(R.dimen.activity_stream_desired_tile_width);
        tileMargin = resources.getDimensionPixelSize(R.dimen.activity_stream_base_margin);

        ActivityStreamTelemetry.Extras.setGlobal(
                ActivityStreamTelemetry.Contract.FX_ACCOUNT_PRESENT,
                FirefoxAccounts.firefoxAccountsExist(context)
        );

        updateSharedPreferencesGlobalExtras(context, sharedPreferences);
    }

    private void updateSharedPreferencesGlobalExtras(final Context context, final SharedPreferences sharedPreferences) {
        ActivityStreamTelemetry.Extras.setGlobal(
                ActivityStreamTelemetry.Contract.AS_USER_PREFERENCES,
                ActivityStreamTelemetry.getASUserPreferencesValue(context, sharedPreferences)
        );
    }

    void setOnUrlOpenListeners(HomePager.OnUrlOpenListener onUrlOpenListener, HomePager.OnUrlOpenInBackgroundListener onUrlOpenInBackgroundListener) {
        adapter.setOnUrlOpenListeners(onUrlOpenListener, onUrlOpenInBackgroundListener);
    }

    public void load(LoaderManager lm) {
        lm.initLoader(LOADER_ID_TOPSITES, null, new TopSitesCallback());
        if (sharedPreferences.getBoolean(PREF_BOOKMARKS_ENABLED, true) || sharedPreferences.getBoolean(PREF_VISITED_ENABLED, true)) {
            lm.initLoader(LOADER_ID_HIGHLIGHTS, null, new HighlightsCallbacks());
        }

        // TODO: we should get the default values from resources for the other Top Sites sections above too.
        if (ActivityStreamConfiguration.isPocketRecommendingTopSites(getContext())) {
            lm.initLoader(LOADER_ID_POCKET, null, new PocketStoriesCallbacks());
        }

    }

    public void unload() {
        adapter.swapHighlights(Collections.<Highlight>emptyList());

        adapter.swapTopSitesCursor(null);
    }

    public void reload(final LoaderManager lm, final Context context, final SharedPreferences sharedPreferences) {
        adapter.clearAndInit();

        updateSharedPreferencesGlobalExtras(context, sharedPreferences);

        // Destroy loaders so they don't restart loading when returning.
        lm.destroyLoader(LOADER_ID_HIGHLIGHTS);
        lm.destroyLoader(LOADER_ID_POCKET);

        load(lm);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        // This code does two things:
        //  * Calculate the size of the tiles we want to display (using a desired width as anchor point)
        //  * Add padding to the left/right side if there's too much space that we do not need


        // Calculate how many tiles fit into the available horizontal space if we are using our
        // desired tile width.
        int fittingTiles = (w - tileMargin) / (desiredTileWidth + tileMargin);

        if (fittingTiles <= TopSitesPage.NUM_COLUMNS) {
            // We can't fit all tiles (or they fit exactly) if we are using the desired tiles width.
            // We will still show all tiles but they might be smaller than what we would like them to be.
            contentRecyclerView.setPadding(0, 0, 0, 0);
        } else if (fittingTiles > TopSitesPage.NUM_COLUMNS) {
            // We can fit more tiles than we want to display. Calculate how much space we need and
            // use the remaining space as padding on the left and right.
            int needed = TopSitesPage.NUM_COLUMNS * (desiredTileWidth + tileMargin) + tileMargin;
            int padding = (w - needed) / 2;

            // With the padding applied we have less space available for the tiles
            w = needed;

            contentRecyclerView.setPadding(padding, 0, padding, 0);
        }

        // Now calculate how large an individual tile is
        final int tilesSize = (w - (TopSitesPage.NUM_COLUMNS * tileMargin) - tileMargin) / TopSitesPage.NUM_COLUMNS;

        adapter.setTileSize(tilesSize);
    }

    private class HighlightsCallbacks implements LoaderManager.LoaderCallbacks<List<Highlight>> {
        @Override
        public Loader<List<Highlight>> onCreateLoader(int id, Bundle args) {
            return new HighlightsLoader(getContext(), HIGHLIGHTS_CANDIDATES, HIGHLIGHTS_LIMIT);
        }

        @Override
        public void onLoadFinished(Loader<List<Highlight>> loader, List<Highlight> data) {
            adapter.swapHighlights(data);
        }

        @Override
        public void onLoaderReset(Loader<List<Highlight>> loader) {
            adapter.swapHighlights(Collections.<Highlight>emptyList());
        }
    }

    private class TopSitesCallback implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final int topSitesPerPage = TopSitesPage.NUM_COLUMNS * TopSitesPage.NUM_ROWS;
            // Top Sites limit multiplier needed to account for duplicated base URLs which will be ignored.
            // Speculative value. May be changed depending on user feedback.
            final int limitMultiplier = 3;

            final Context context = getContext();
            return BrowserDB.from(context).getActivityStreamTopSites(
                    context,
                    topSitesPerPage * TopSitesPagerAdapter.SUGGESTED_SITES_MAX_PAGES * limitMultiplier,
                    topSitesPerPage * TopSitesPagerAdapter.PAGES * limitMultiplier);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor data) {
            adapter.swapTopSitesCursor(data);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            adapter.swapTopSitesCursor(null);
        }
    }

    private class PocketStoriesCallbacks implements LoaderManager.LoaderCallbacks<List<TopStory>> {

        @Override
        public Loader<List<TopStory>> onCreateLoader(int id, Bundle args) {
            return new PocketStoriesLoader(getContext());
        }

        @Override
        public void onLoadFinished(Loader<List<TopStory>> loader, List<TopStory> data) {
            adapter.swapTopStories(data);
        }

        @Override
        public void onLoaderReset(Loader<List<TopStory>> loader) {
            adapter.swapTopStories(Collections.<TopStory>emptyList());
        }


    }
}
