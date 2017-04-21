/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.Collection;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract.ExpirePriority;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;

import android.content.ContentProviderClient;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.support.annotation.Nullable;
import android.support.v4.content.CursorLoader;

/**
 * Interface for interactions with all databases. If you want an instance
 * that implements this, you should go through GeckoProfile. E.g.,
 * <code>BrowserDB.from(context)</code>.
 */
public abstract class BrowserDB {
    public static enum FilterFlags {
        EXCLUDE_PINNED_SITES
    }

    public abstract Searches getSearches();
    public abstract TabsAccessor getTabsAccessor();
    public abstract URLMetadata getURLMetadata();
    @RobocopTarget public abstract UrlAnnotations getUrlAnnotations();

    /**
     * Add default bookmarks to the database.
     * Takes an offset; returns a new offset.
     */
    public abstract int addDefaultBookmarks(Context context, ContentResolver cr, int offset);

    /**
     * Add bookmarks from the provided distribution.
     * Takes an offset; returns a new offset.
     */
    public abstract int addDistributionBookmarks(ContentResolver cr, Distribution distribution, int offset);

    /**
     * Invalidate cached data.
     */
    public abstract void invalidate();

    public abstract int getCount(ContentResolver cr, String database);

    /**
     * @return a cursor representing the contents of the DB filtered according to the arguments.
     * Can return <code>null</code>. <code>CursorLoader</code> will handle this correctly.
     */
    public abstract Cursor filter(ContentResolver cr, CharSequence constraint,
                                  int limit, EnumSet<BrowserDB.FilterFlags> flags);

    /**
     * @return a cursor over top sites (high-ranking bookmarks and history).
     * Can return <code>null</code>.
     * Returns no more than <code>limit</code> results.
     * Suggested sites will be limited to being within the first <code>suggestedRangeLimit</code> results.
     */
    public abstract Cursor getTopSites(ContentResolver cr, int suggestedRangeLimit, int limit);

    public abstract CursorLoader getActivityStreamTopSites(Context context, int suggestedRangeLimit, int limit);

    public abstract void updateVisitedHistory(ContentResolver cr, String uri);

    public abstract void updateHistoryTitle(ContentResolver cr, String uri, String title);

    /**
     * Can return <code>null</code>.
     */
    public abstract Cursor getAllVisitedHistory(ContentResolver cr);

    /**
     * Can return <code>null</code>.
     */
    public abstract Cursor getRecentHistory(ContentResolver cr, int limit);

    public abstract Cursor getHistoryForURL(ContentResolver cr, String uri);

    public abstract Cursor getRecentHistoryBetweenTime(ContentResolver cr, int historyLimit, long start, long end);

    public abstract long getPrePathLastVisitedTimeMilliseconds(ContentResolver cr, String prePath);

    public abstract void expireHistory(ContentResolver cr, ExpirePriority priority);

    public abstract void removeHistoryEntry(ContentResolver cr, String url);

    public abstract void clearHistory(ContentResolver cr, boolean clearSearchHistory);


    public abstract String getUrlForKeyword(ContentResolver cr, String keyword);

    public abstract boolean isBookmark(ContentResolver cr, String uri);
    public abstract boolean addBookmark(ContentResolver cr, String title, String uri);
    public abstract Uri addBookmarkFolder(ContentResolver cr, String title, long parentId);
    @Nullable public abstract Cursor getBookmarkForUrl(ContentResolver cr, String url);
    @Nullable public abstract Cursor getBookmarksForPartialUrl(ContentResolver cr, String partialUrl);
    @Nullable public abstract Cursor getBookmarkById(ContentResolver cr, long id);
    @Nullable public abstract Cursor getAllBookmarkFolders(ContentResolver cr);
    public abstract void removeBookmarksWithURL(ContentResolver cr, String uri);
    public abstract void removeBookmarkWithId(ContentResolver cr, long id);
    public abstract void registerBookmarkObserver(ContentResolver cr, ContentObserver observer);
    public abstract void updateBookmark(ContentResolver cr, long id, String uri, String title, String keyword);
    public abstract void updateBookmark(ContentResolver cr, long id, String uri, String title, String keyword, long newParentId, long oldParentId);
    public abstract boolean hasBookmarkWithGuid(ContentResolver cr, String guid);

    public abstract boolean insertPageMetadata(ContentProviderClient contentProviderClient, String pageUrl, boolean hasImage, String metadataJSON);
    public abstract int deletePageMetadata(ContentProviderClient contentProviderClient, String pageUrl);
    /**
     * Can return <code>null</code>.
     */
    @Nullable public abstract Cursor getBookmarksInFolder(ContentResolver cr, long folderId);

    public abstract int getBookmarkCountForFolder(ContentResolver cr, long folderId);

    /**
     * Get the favicon from the database, if any, associated with the given favicon URL. (That is,
     * the URL of the actual favicon image, not the URL of the page with which the favicon is associated.)
     * @param cr The ContentResolver to use.
     * @param faviconURL The URL of the favicon to fetch from the database.
     * @return The decoded Bitmap from the database, if any. null if none is stored.
     */
    public abstract LoadFaviconResult getFaviconForUrl(Context context, ContentResolver cr, String faviconURL);

    /**
     * Try to find a usable favicon URL in the history or bookmarks table.
     */
    public abstract String getFaviconURLFromPageURL(ContentResolver cr, String uri);

    public abstract byte[] getThumbnailForUrl(ContentResolver cr, String uri);
    public abstract void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail);

    /**
     * Query for non-null thumbnails matching the provided <code>urls</code>.
     * The returned cursor will have no more than, but possibly fewer than,
     * the requested number of thumbnails.
     *
     * Returns null if the provided list of URLs is empty or null.
     */
    @Nullable public abstract Cursor getThumbnailsForUrls(ContentResolver cr,
            List<String> urls);

    public abstract void removeThumbnails(ContentResolver cr);

    // Utility function for updating existing history using batch operations
    public abstract void updateHistoryInBatch(ContentResolver cr,
            Collection<ContentProviderOperation> operations, String url,
            String title, long date, int visits);

    public abstract void updateBookmarkInBatch(ContentResolver cr,
            Collection<ContentProviderOperation> operations, String url,
            String title, String guid, long parent, long added, long modified,
            long position, String keyword, int type);

    // Used by regular top sites, which observe pinning position.
    public abstract void pinSite(ContentResolver cr, String url, String title, int position);
    public abstract void unpinSite(ContentResolver cr, int position);

    // Used by activity stream top sites, which ignore position - it's always 0.
    // Pins show up in front of other top sites.
    public abstract void pinSiteForAS(ContentResolver cr, String url, String title);
    public abstract void unpinSiteForAS(ContentResolver cr, String url);

    public abstract boolean isPinnedForAS(ContentResolver cr, String url);

    public abstract boolean hideSuggestedSite(String url);
    public abstract void setSuggestedSites(SuggestedSites suggestedSites);
    public abstract SuggestedSites getSuggestedSites();
    public abstract boolean hasSuggestedImageUrl(String url);
    public abstract String getSuggestedImageUrlForUrl(String url);
    public abstract int getSuggestedBackgroundColorForUrl(String url);

    /**
     * Obtain a set of recently visited links to rank.
     *
     * @param contentResolver to load the cursor.
     * @param limit Maximum number of results to return.
     */
    @Nullable public abstract Cursor getHighlightCandidates(ContentResolver contentResolver, int limit);

    /**
     * Block a page from the highlights list.
     *
     * @param url The page URL. Only pages exactly matching this URL will be blocked.
     */
    public abstract void blockActivityStreamSite(ContentResolver cr, String url);

    public static BrowserDB from(final Context context) {
        return from(GeckoProfile.get(context));
    }

    public static BrowserDB from(final GeckoProfile profile) {
        synchronized (profile.getLock()) {
            BrowserDB db = (BrowserDB) profile.getData();
            if (db != null) {
                return db;
            }

            db = new LocalBrowserDB(profile.getName());
            profile.setData(db);
            return db;
        }
    }
}
