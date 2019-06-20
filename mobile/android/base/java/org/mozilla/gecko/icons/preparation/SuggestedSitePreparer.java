/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.preparation;

import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.loader.SuggestedSiteLoader;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.HashSet;
import java.util.Set;

public class SuggestedSitePreparer implements Preparer {
    private boolean initialised = false;
    private final Set<String> siteFaviconMap = new HashSet<>();

    private boolean initialise(final Context context) {
        registerForSuggestedSitesUpdated(context.getApplicationContext());

        return refreshSiteFaviconMap(context);
    }

    // Suggested sites can change at runtime - for example when a distribution is (down)loaded. In
    // this case we need to refresh our local cache of suggested sites' favicons.
    private void registerForSuggestedSitesUpdated(final Context context) {
        context.getContentResolver().registerContentObserver(
                BrowserContract.SuggestedSites.CONTENT_URI,
                false,
                new ContentObserver(null) {
                    @Override
                    public void onChange(boolean selfChange) {
                        // The list of suggested sites has changed. We need to update our local
                        // mapping.
                        refreshSiteFaviconMapOnBackgroundThread(context);
                    }
                });
    }

    private void refreshSiteFaviconMapOnBackgroundThread(final Context context) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                refreshSiteFaviconMap(context);
            }
        });
    }

    // Loading suggested sites (and iterating over them) is potentially slow. The number of suggested
    // sites is low, and a HashSet containing them is therefore likely to be exceedingly small.
    // Hence we opt to iterate over the list once, and do an immediate lookup every time a favicon
    // is requested:
    // Loading can fail if suggested sites haven't been initialised yet, only proceed if we return true.
    private synchronized boolean refreshSiteFaviconMap(Context context) {
        siteFaviconMap.clear();

        final SuggestedSites suggestedSites = BrowserDB.from(context).getSuggestedSites();

        // suggestedSites may not have been initialised yet if BrowserApp isn't running yet. Suggested
        // Sites are initialised in BrowserApp.onCreate(), but we might need to load icons when running
        // custom tabs, as geckoview, etc. (But we don't care as much about the bundled icons in those
        // scenarios.)
        if (suggestedSites == null) {
            return false;
        }

        final Cursor cursor = suggestedSites.get(Integer.MAX_VALUE);
        try {
            final int urlColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL);

            while (cursor.moveToNext()) {
                final String url = cursor.getString(urlColumnIndex);
                siteFaviconMap.add(url);
            }
        } finally {
            cursor.close();
        }

        return true;
    }

    // Synchronized so that asynchronous updates of the map (via content observer) are visible
    // immediately and completely.
    @Override
    public synchronized void prepare(final IconRequest request) {
        if (request.shouldSkipDisk()) {
            return;
        }

        // A locale change could mean different suggested top sites that needs to be loaded.
        if (!initialised || BrowserLocaleManager.getInstance().systemLocaleDidChange()) {
            initialised = initialise(request.getContext());

            if (!initialised) {
                // Early return: if we were unable to load suggested sites metadata, we won't be able
                // to provide sites (but we'll still try again next time).
                return;
            }
        }

        final String siteURL = request.getPageUrl();

        if (siteFaviconMap.contains(siteURL)) {
            request.modify()
                    .icon(IconDescriptor.createBundledTileIcon(SuggestedSiteLoader.SUGGESTED_SITE_TOUCHTILE + request.getPageUrl()))
                    .deferBuild();
        }
    }
}
