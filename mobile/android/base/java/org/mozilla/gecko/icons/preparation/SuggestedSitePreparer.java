/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.preparation;

import android.content.Context;
import android.database.Cursor;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.loader.SuggestedSiteLoader;

import java.util.HashSet;
import java.util.Set;

public class SuggestedSitePreparer implements Preparer {

    private boolean initialised = false;
    private final Set<String> siteFaviconMap = new HashSet<>();

    // Loading suggested sites (and iterating over them) is potentially slow. The number of suggested
    // sites is low, and a HashSet containing them is therefore likely to be exceedingly small.
    // Hence we opt to iterate over the list once, and do an immediate lookup every time a favicon
    // is requested:
    // Loading can fail if suggested sites haven't been initialised yet, only proceed if we return true.
    private boolean initialise(final Context context) {
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

    @Override
    public void prepare(final IconRequest request) {
        if (request.shouldSkipDisk()) {
            return;
        }

        if (!initialised) {
            initialised = initialise(request.getContext());

            if (!initialised) {
                // Early return: if we were unable to load suggested sites metdata, we won't be able
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
