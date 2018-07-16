/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.FilterFlags;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;

/**
 * Encapsulates the implementation of the search cursor loader.
 */
class SearchLoader {
    public static final String LOGTAG = "GeckoSearchLoader";

    private static final String KEY_SEARCH_TERM = "search_term";
    private static final String KEY_FILTER_FLAGS = "flags";

    private SearchLoader() {
    }

    @SuppressWarnings("unchecked")
    public static Loader<Cursor> createInstance(Context context, Bundle args) {
        if (args != null) {
            final String searchTerm = args.getString(KEY_SEARCH_TERM);
            final EnumSet<FilterFlags> flags =
                    (EnumSet<FilterFlags>) args.getSerializable(KEY_FILTER_FLAGS);
            return new SearchCursorLoader(context, searchTerm, flags);
        } else {
            return new SearchCursorLoader(context, "", EnumSet.noneOf(FilterFlags.class));
        }
    }

    private static Bundle createArgs(String searchTerm, EnumSet<FilterFlags> flags) {
        Bundle args = new Bundle();
        args.putString(SearchLoader.KEY_SEARCH_TERM, searchTerm);
        args.putSerializable(SearchLoader.KEY_FILTER_FLAGS, flags);

        return args;
    }

    public static void init(LoaderManager manager, int loaderId,
                            LoaderCallbacks<Cursor> callbacks, String searchTerm) {
        init(manager, loaderId, callbacks, searchTerm, EnumSet.noneOf(FilterFlags.class));
    }

    public static void init(LoaderManager manager, int loaderId,
                            LoaderCallbacks<Cursor> callbacks, String searchTerm,
                            EnumSet<FilterFlags> flags) {
        final Bundle args = createArgs(searchTerm, flags);
        manager.initLoader(loaderId, args, callbacks);
    }

    public static void restart(LoaderManager manager, int loaderId,
                               LoaderCallbacks<Cursor> callbacks, String searchTerm) {
        restart(manager, loaderId, callbacks, searchTerm, EnumSet.noneOf(FilterFlags.class));
    }

    public static void restart(LoaderManager manager, int loaderId,
                               LoaderCallbacks<Cursor> callbacks, String searchTerm,
                               EnumSet<FilterFlags> flags) {
        manager.destroyLoader(loaderId);
        final Bundle args = createArgs(searchTerm, flags);
        manager.restartLoader(loaderId, args, callbacks);
    }

    public static class SearchCursorLoader extends SimpleCursorLoader {
        private static final String TELEMETRY_HISTOGRAM_LOAD_CURSOR = "FENNEC_SEARCH_LOADER_TIME_MS";

        // Max number of search results.
        private static final int SEARCH_LIMIT = 100;

        // The target search term associated with the loader.
        private final String mSearchTerm;

        // The filter flags associated with the loader.
        private final EnumSet<FilterFlags> mFlags;
        private final GeckoProfile mProfile;

        public SearchCursorLoader(Context context, String searchTerm, EnumSet<FilterFlags> flags) {
            super(context);
            mSearchTerm = searchTerm;
            mFlags = flags;
            mProfile = GeckoProfile.get(context);
        }

        @Override
        public Cursor loadCursor() {
            final long start = SystemClock.uptimeMillis();
            final Cursor cursor = BrowserDB.from(mProfile).filter(getContext().getContentResolver(), mSearchTerm, SEARCH_LIMIT, mFlags);
            final long end = SystemClock.uptimeMillis();
            final long took = end - start;
            Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_LOAD_CURSOR, (int) Math.min(took, Integer.MAX_VALUE));
            return cursor;
        }

        public String getSearchTerm() {
            return mSearchTerm;
        }
    }

}
