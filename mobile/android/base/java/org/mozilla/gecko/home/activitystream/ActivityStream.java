/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.home.HomeBanner;
import org.mozilla.gecko.home.HomeFragment;
import org.mozilla.gecko.home.HomeScreen;
import org.mozilla.gecko.home.SimpleCursorLoader;
import org.mozilla.gecko.home.activitystream.topsites.TopSitesPagerAdapter;

public class ActivityStream extends FrameLayout implements HomeScreen {
    private StreamRecyclerAdapter adapter;

    private static final int LOADER_ID_HIGHLIGHTS = 0;
    private static final int LOADER_ID_TOPSITES = 1;

    public ActivityStream(Context context, AttributeSet attrs) {
        super(context, attrs);

        inflate(context, R.layout.as_content, this);
    }

    @Override
    public boolean isVisible() {
        // This is dependent on the loading state - currently we're a dumb panel so we're always
        // "visible"
        return true;
    }

    @Override
    public void onToolbarFocusChange(boolean hasFocus) {
        // We don't care: this is HomePager specific
    }

    @Override
    public void showPanel(String panelId, Bundle restoreData) {
        // We could use this to restore Panel data. In practice this isn't likely to be relevant for
        // AS and can be ignore for now.
    }

    @Override
    public void setOnPanelChangeListener(OnPanelChangeListener listener) {
        // As with showPanel: not relevant yet, could be used for persistence (scroll position?)
    }

    @Override
    public void setPanelStateChangeListener(HomeFragment.PanelStateChangeListener listener) {
        // See setOnPanelChangeListener
    }

    @Override
    public void setBanner(HomeBanner banner) {
        // TODO: we should probably implement this to show snippets.
    }

    @Override
    public void load(LoaderManager lm, FragmentManager fm, String panelId, Bundle restoreData,
                     PropertyAnimator animator) {
        // Signal to load data from storage as needed, compare with HomePager
        RecyclerView rv = (RecyclerView) findViewById(R.id.activity_stream_main_recyclerview);

        // TODO: we need to retrieve BrowserApp and pass it in as onUrlOpenListener. That will
        // be simpler once we're a HomeFragment, but isn't so simple while we're still a View.
        adapter = new StreamRecyclerAdapter(lm, null);
        rv.setAdapter(adapter);
        rv.setLayoutManager(new LinearLayoutManager(getContext()));
        rv.setHasFixedSize(true);

        CursorLoaderCallbacks callbacks = new CursorLoaderCallbacks();
        lm.initLoader(LOADER_ID_HIGHLIGHTS, null, callbacks);
        lm.initLoader(LOADER_ID_TOPSITES, null, callbacks);
    }

    @Override
    public void unload() {
        adapter.swapHighlightsCursor(null);
        adapter.swapTopSitesCursor(null);
        // Signal to clear data that has been loaded, compare with HomePager
    }

    /**
     * This is a temporary cursor loader. We'll probably need a completely new query for AS,
     * at that time we can switch to the new CursorLoader, as opposed to using our outdated
     * SimpleCursorLoader.
     */
    private static class HistoryLoader extends SimpleCursorLoader {
        public HistoryLoader(Context context) {
            super(context);
        }

        @Override
        protected Cursor loadCursor() {
            final Context context = getContext();
            return GeckoProfile.get(context).getDB()
                    .getRecentHistory(context.getContentResolver(), 10);
        }
    }

    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (id == LOADER_ID_HIGHLIGHTS) {
                return new HistoryLoader(getContext());
            } else if (id == LOADER_ID_TOPSITES) {
                return GeckoProfile.get(getContext()).getDB().getActivityStreamTopSites(getContext(),
                        TopSitesPagerAdapter.TOTAL_ITEMS);
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
