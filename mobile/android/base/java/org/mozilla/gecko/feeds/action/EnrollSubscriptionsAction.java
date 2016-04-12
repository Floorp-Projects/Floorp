/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.text.TextUtils;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.feeds.FeedService;
import org.mozilla.gecko.feeds.knownsites.KnownSiteBlogger;
import org.mozilla.gecko.feeds.knownsites.KnownSite;
import org.mozilla.gecko.feeds.knownsites.KnownSiteMedium;
import org.mozilla.gecko.feeds.knownsites.KnownSiteTumblr;
import org.mozilla.gecko.feeds.knownsites.KnownSiteWordpress;

/**
 * EnrollSubscriptionsAction: Search for bookmarks of known sites we can subscribe to.
 */
public class EnrollSubscriptionsAction extends FeedAction {
    private static final String LOGTAG = "FeedEnrollAction";

    private static final KnownSite[] knownSites = {
        new KnownSiteMedium(),
        new KnownSiteBlogger(),
        new KnownSiteWordpress(),
        new KnownSiteTumblr(),
    };

    private Context context;

    public EnrollSubscriptionsAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(BrowserDB db, Intent intent) {
        log("Searching for bookmarks to enroll in updates");

        final ContentResolver contentResolver = context.getContentResolver();

        for (KnownSite knownSite : knownSites) {
            searchFor(db, contentResolver, knownSite);
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

    private void searchFor(BrowserDB db, ContentResolver contentResolver, KnownSite knownSite) {
        final UrlAnnotations urlAnnotations = db.getUrlAnnotations();

        final Cursor cursor = db.getBookmarksForPartialUrl(contentResolver, knownSite.getURLSearchString());
        if (cursor == null) {
            log("Nothing found (" + knownSite.getClass().getSimpleName() + ")");
            return;
        }

        try {
            log("Found " + cursor.getCount() + " websites");

            while (cursor.moveToNext()) {

                final String url = cursor.getString(cursor.getColumnIndex(BrowserContract.Bookmarks.URL));

                log(" URL: " + url);

                String feedUrl = knownSite.getFeedFromURL(url);
                if (TextUtils.isEmpty(feedUrl)) {
                    log("Could not determine feed for URL: " + url);
                    return;
                }

                if (!urlAnnotations.hasFeedUrlForWebsite(contentResolver, url)) {
                    urlAnnotations.insertFeedUrl(contentResolver, url, feedUrl);
                }

                if (!urlAnnotations.hasFeedSubscription(contentResolver, feedUrl)) {
                    FeedService.subscribe(context, feedUrl);
                }
            }
        } finally {
            cursor.close();
        }
    }

}
