/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;

/**
 * Cursor loader callbacks that takes care loading favicons into memory.
 */
abstract class HomeCursorLoaderCallbacks implements LoaderCallbacks<Cursor> {

    // Cursor loader ID for favicons query
    private static final int LOADER_ID_FAVICONS = 100;

    private final Context mContext;
    private final LoaderManager mLoaderManager;

    public HomeCursorLoaderCallbacks(Context context, LoaderManager loaderManager) {
        mContext = context;
        mLoaderManager = loaderManager;
    }

    public void loadFavicons(Cursor cursor) {
        FaviconsLoader.restartFromCursor(mLoaderManager, LOADER_ID_FAVICONS, this, cursor);
    }

    @Override
    public Loader<Cursor> onCreateLoader(int id, Bundle args) {
        if (id == LOADER_ID_FAVICONS) {
            return FaviconsLoader.createInstance(mContext, args);
        }

        return null;
    }

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
        if (loader.getId() == LOADER_ID_FAVICONS) {
            onFaviconsLoaded();
        }
    }

    @Override
    public void onLoaderReset(Loader<Cursor> loader) {
        // Do nothing by default.
    }

    // Callback for favicons loaded in memory.
    public abstract void onFaviconsLoaded();
}
