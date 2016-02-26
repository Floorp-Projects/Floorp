/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.feeds.FeedService;
import org.mozilla.gecko.feeds.knownsites.KnownSiteBlogger;
import org.mozilla.gecko.feeds.knownsites.KnownSite;
import org.mozilla.gecko.feeds.knownsites.KnownSiteMedium;

/**
 * EnrollAction: Search for bookmarks of known sites we can subscribe to.
 */
public class EnrollAction implements BaseAction {
    private static final String LOGTAG = "FeedEnrollAction";

    private static final KnownSite[] knownSites = {
        new KnownSiteMedium(),
        new KnownSiteBlogger(),
    };

    private Context context;

    public EnrollAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(Intent intent) {
        Log.i(LOGTAG, "Searching for bookmarks to enroll in updates");

        BrowserDB db = GeckoProfile.get(context).getDB();

        for (KnownSite knownSite : knownSites) {
            searchFor(db, knownSite);
        }
    }

    @Override
    public boolean requiresNetwork() {
        return false;
    }

    @Override
    public boolean requiresPreferenceEnabled() {
        return true;
    }

    private void searchFor(BrowserDB db, KnownSite knownSite) {
        Cursor cursor = db.getBookmarksForPartialUrl(context.getContentResolver(), "://" + knownSite.getURLSearchString() + "/");
        if (cursor == null) {
            Log.d(LOGTAG, "Nothing found");
            return;
        }

        try {
            Log.d(LOGTAG, "Found " + cursor.getCount() + " websites");

            while (cursor.moveToNext()) {
                final String guid = cursor.getString(cursor.getColumnIndex(BrowserContract.Bookmarks.GUID));
                final String url = cursor.getString(cursor.getColumnIndex(BrowserContract.Bookmarks.URL));

                Log.d(LOGTAG, " (" + guid + ") " + url);

                String feedUrl = knownSite.getFeedFromURL(url);
                if (!TextUtils.isEmpty(feedUrl)) {
                    FeedService.subscribe(context, guid, feedUrl);
                }
            }
        } finally {
            cursor.close();
        }
    }
}
