/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.preparation;

import android.content.Context;
import android.database.Cursor;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
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
    private void initialise(final Context context) {
        final Cursor cursor = BrowserDB.from(context).getSuggestedSites().get(Integer.MAX_VALUE);
        try {
            final int urlColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL);

            while (cursor.moveToNext()) {
                final String url = cursor.getString(urlColumnIndex);
                siteFaviconMap.add(url);
            }
        } finally {
            cursor.close();
        }
    }

    @Override
    public void prepare(final IconRequest request) {
        if (request.shouldSkipDisk()) {
            return;
        }

        if (!initialised) {
            initialise(request.getContext());

            initialised = true;
        }

        final String siteURL = request.getPageUrl();

        if (siteFaviconMap.contains(siteURL)) {
            request.modify()
                    .icon(IconDescriptor.createBundledTileIcon(SuggestedSiteLoader.SUGGESTED_SITE_TOUCHTILE + request.getPageUrl()))
                    .deferBuild();
        }
    }
}
