/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.db.BrowserContract.ExpirePriority;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.StringUtils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;

/**
 * A utility wrapper for accessing a static {@link LocalBrowserDB},
 * manually initialized with a particular profile, and similarly
 * managing a static instance of {@link SuggestedSites}.
 *
 * Be careful using this class: if you're not BrowserApp, you probably
 * want to manually instantiate and use LocalBrowserDB itself.
 *
 * Also manages some flags.
 */
public class BrowserDB {
    public static enum FilterFlags {
        EXCLUDE_PINNED_SITES
    }

    private static volatile LocalBrowserDB sDb;
    private static volatile SuggestedSites sSuggestedSites;
    private static volatile boolean sAreContentProvidersEnabled = true;

    public static void initialize(String profile) {
        sDb = new LocalBrowserDB(profile);
    }

    public static int addDefaultBookmarks(Context context, ContentResolver cr, final int offset) {
        return sDb.addDefaultBookmarks(context, cr, offset);
    }

    public static int addDistributionBookmarks(ContentResolver cr, Distribution distribution, int offset) {
        return sDb.addDistributionBookmarks(cr, distribution, offset);
    }

    public static void setSuggestedSites(SuggestedSites suggestedSites) {
        sSuggestedSites = suggestedSites;
    }

    public static boolean hideSuggestedSite(String url) {
        return sSuggestedSites.hideSite(url);
    }

    public static void invalidateCachedState() {
        sDb.invalidateCachedState();
    }

    @RobocopTarget
    public static Cursor filter(ContentResolver cr, CharSequence constraint, int limit,
                                EnumSet<FilterFlags> flags) {
        return sDb.filter(cr, constraint, limit, flags);
    }

    private static void appendUrlsFromCursor(List<String> urls, Cursor c) {
        c.moveToPosition(-1);
        while (c.moveToNext()) {
            String url = c.getString(c.getColumnIndex(History.URL));

            // Do a simpler check before decoding to avoid parsing
            // all URLs unnecessarily.
            if (StringUtils.isUserEnteredUrl(url)) {
                url = StringUtils.decodeUserEnteredUrl(url);
            }

            urls.add(url);
        };
    }

    public static Cursor getTopSites(ContentResolver cr, int minLimit, int maxLimit) {
        // Note this is not a single query anymore, but actually returns a mixture
        // of two queries, one for topSites and one for pinned sites.
        Cursor pinnedSites = sDb.getPinnedSites(cr, minLimit);

        int pinnedCount = pinnedSites.getCount();
        Cursor topSites = sDb.getTopSites(cr, maxLimit - pinnedCount);
        int topCount = topSites.getCount();

        Cursor suggestedSites = null;
        if (sSuggestedSites != null) {
            final int count = minLimit - pinnedCount - topCount;
            if (count > 0) {
                final List<String> excludeUrls = new ArrayList<String>(pinnedCount + topCount);
                appendUrlsFromCursor(excludeUrls, pinnedSites);
                appendUrlsFromCursor(excludeUrls, topSites);

                suggestedSites = sSuggestedSites.get(count, excludeUrls);
            }
        }

        return new TopSitesCursorWrapper(pinnedSites, topSites, suggestedSites, minLimit);
    }

    public static void updateVisitedHistory(ContentResolver cr, String uri) {
        if (sAreContentProvidersEnabled) {
            sDb.updateVisitedHistory(cr, uri);
        }
    }

    public static void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        if (sAreContentProvidersEnabled) {
            sDb.updateHistoryTitle(cr, uri, title);
        }
    }

    @RobocopTarget
    public static Cursor getAllVisitedHistory(ContentResolver cr) {
        return (sAreContentProvidersEnabled ? sDb.getAllVisitedHistory(cr) : null);
    }

    public static Cursor getRecentHistory(ContentResolver cr, int limit) {
        return sDb.getRecentHistory(cr, limit);
    }

    public static void expireHistory(ContentResolver cr, ExpirePriority priority) {
        if (sDb == null) {
            return;
        }

        sDb.expireHistory(cr, priority == null ? ExpirePriority.NORMAL : priority);
    }

    @RobocopTarget
    public static void removeHistoryEntry(ContentResolver cr, String url) {
        sDb.removeHistoryEntry(cr, url);
    }

    @RobocopTarget
    public static void clearHistory(ContentResolver cr) {
        sDb.clearHistory(cr);
    }

    @RobocopTarget
    public static Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        return sDb.getBookmarksInFolder(cr, folderId);
    }

    @RobocopTarget
    public static Cursor getReadingList(ContentResolver cr) {
        return sDb.getReadingList(cr);
    }

    public static String getUrlForKeyword(ContentResolver cr, String keyword) {
        return sDb.getUrlForKeyword(cr, keyword);
    }

    @RobocopTarget
    public static boolean isBookmark(ContentResolver cr, String uri) {
        return (sAreContentProvidersEnabled && sDb.isBookmark(cr, uri));
    }

    public static boolean isReadingListItem(ContentResolver cr, String uri) {
        return (sAreContentProvidersEnabled && sDb.isReadingListItem(cr, uri));
    }

    public static void addBookmark(ContentResolver cr, String title, String uri) {
        sDb.addBookmark(cr, title, uri);
    }

    @RobocopTarget
    public static void removeBookmarksWithURL(ContentResolver cr, String uri) {
        sDb.removeBookmarksWithURL(cr, uri);
    }

    @RobocopTarget
    public static void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword) {
        sDb.updateBookmark(cr, id, uri, title, keyword);
    }

    public static void addReadingListItem(ContentResolver cr, ContentValues values) {
        sDb.addReadingListItem(cr, values);
    }

    public static void removeReadingListItemWithURL(ContentResolver cr, String uri) {
        sDb.removeReadingListItemWithURL(cr, uri);
    }

    public static LoadFaviconResult getFaviconForFaviconUrl(ContentResolver cr, String faviconURL) {
        return sDb.getFaviconForUrl(cr, faviconURL);
    }

    /**
     * Try to find a usable favicon URL in the history or bookmarks table.
     */
    public static String getFaviconURLFromPageURL(ContentResolver cr, String url) {
        return sDb.getFaviconURLFromPageURL(cr, url);
    }

    public static void updateFaviconForUrl(ContentResolver cr, String pageUri, byte[] encodedFavicon, String faviconUri) {
        sDb.updateFaviconForUrl(cr, pageUri, encodedFavicon, faviconUri);
    }

    public static void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail) {
        sDb.updateThumbnailForUrl(cr, uri, thumbnail);
    }

    @RobocopTarget
    public static byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        return sDb.getThumbnailForUrl(cr, uri);
    }

    public static Cursor getThumbnailsForUrls(ContentResolver cr, List<String> urls) {
        return sDb.getThumbnailsForUrls(cr, urls);
    }

    @RobocopTarget
    public static void removeThumbnails(ContentResolver cr) {
        sDb.removeThumbnails(cr);
    }

    public static void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
        sDb.registerBookmarkObserver(cr, observer);
    }

    public static int getCount(ContentResolver cr, String database) {
        return sDb.getCount(cr, database);
    }

    public static void pinSite(ContentResolver cr, String url, String title, int position) {
        sDb.pinSite(cr, url, title, position);
    }

    public static void unpinSite(ContentResolver cr, int position) {
        sDb.unpinSite(cr, position);
    }

    @RobocopTarget
    public static Cursor getBookmarkForUrl(ContentResolver cr, String url) {
        return sDb.getBookmarkForUrl(cr, url);
    }

    public static void setEnableContentProviders(boolean enableContentProviders) {
        sAreContentProvidersEnabled = enableContentProviders;
    }
}
