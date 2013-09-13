/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;

import java.util.ArrayList;

/**
 * Encapsulates the implementation of the favicons cursorloader.
 */
class FaviconsLoader {
    // Argument containing list of urls for the favicons loader
    private static final String FAVICONS_LOADER_URLS_ARG = "urls";

    private FaviconsLoader() {
    }

    private static ArrayList<String> getUrlsWithoutFavicon(Cursor c) {
        ArrayList<String> urls = new ArrayList<String>();

        if (c == null || !c.moveToFirst()) {
            return urls;
        }

        do {
            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

            // We only want to load favicons from DB if they are not in the
            // memory cache yet. The url is null for bookmark folders.
            if (url == null || Favicons.getFaviconFromMemCache(url) != null) {
                continue;
            }

            urls.add(url);
        } while (c.moveToNext());

        return urls;
    }

    private static void storeFaviconsInMemCache(Cursor c) {
        if (c == null || !c.moveToFirst()) {
            return;
        }

        do {
            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
            final byte[] b = c.getBlob(c.getColumnIndexOrThrow(URLColumns.FAVICON));

            if (b == null) {
                continue;
            }

            Bitmap favicon = BitmapUtils.decodeByteArray(b);
            if (favicon == null) {
                continue;
            }

            favicon = Favicons.scaleImage(favicon);
            Favicons.putFaviconInMemCache(url, favicon);
        } while (c.moveToNext());
    }

    public static void restartFromCursor(LoaderManager manager, int loaderId,
            LoaderCallbacks<Cursor> callbacks, Cursor c) {
        // If there urls without in-memory favicons, trigger a new loader
        // to load the images from disk to memory.
        ArrayList<String> urls = getUrlsWithoutFavicon(c);
        if (urls.size() > 0) {
            Bundle args = new Bundle();
            args.putStringArrayList(FAVICONS_LOADER_URLS_ARG, urls);

            manager.restartLoader(loaderId, args, callbacks);
        }
    }

    public static Loader<Cursor> createInstance(Context context, Bundle args) {
        final ArrayList<String> urls = args.getStringArrayList(FAVICONS_LOADER_URLS_ARG);
        return new FaviconsCursorLoader(context, urls);
    }

    private static class FaviconsCursorLoader extends SimpleCursorLoader {
        private final ArrayList<String> mUrls;

        public FaviconsCursorLoader(Context context, ArrayList<String> urls) {
            super(context);
            mUrls = urls;
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();

            Cursor c = BrowserDB.getFaviconsForUrls(cr, mUrls);
            storeFaviconsInMemCache(c);

            return c;
        }
    }
}
