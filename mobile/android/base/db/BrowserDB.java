/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.File;
import java.util.Collection;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserContract.ExpirePriority;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;

/**
 * Interface for interactions with all databases. If you want an instance
 * that implements this, you should go through GeckoProfile. E.g.,
 * <code>GeckoProfile.get(context).getDB()</code>.
 *
 * GeckoProfile itself will construct an appropriate subclass using
 * a factory that the containing application can set with
 * {@link GeckoProfile#setBrowserDBFactory(BrowserDB.Factory)}.
 */
public interface BrowserDB {
    public interface Factory {
        public BrowserDB get(String profileName, File profileDir);
    }

    public static enum FilterFlags {
        EXCLUDE_PINNED_SITES
    }

    public abstract Searches getSearches();
    public abstract TabsAccessor getTabsAccessor();
    public abstract URLMetadata getURLMetadata();
    public abstract ReadingListAccessor getReadingListAccessor();

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
     */
    public abstract Cursor getTopSites(ContentResolver cr, int limit);

    /**
     * @return a cursor over top sites (high-ranking bookmarks and history).
     * Can return <code>null</code>.
     * Returns no more than <code>maxLimit</code> results.
     */
    public abstract Cursor getTopSites(ContentResolver cr, int minLimit, int maxLimit);

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

    public abstract void expireHistory(ContentResolver cr, ExpirePriority priority);

    public abstract void removeHistoryEntry(ContentResolver cr, String url);

    public abstract void clearHistory(ContentResolver cr);


    public abstract String getUrlForKeyword(ContentResolver cr, String keyword);

    public abstract boolean isBookmark(ContentResolver cr, String uri);
    public abstract void addBookmark(ContentResolver cr, String title, String uri);
    public abstract Cursor getBookmarkForUrl(ContentResolver cr, String url);
    public abstract void removeBookmarksWithURL(ContentResolver cr, String uri);
    public abstract void registerBookmarkObserver(ContentResolver cr, ContentObserver observer);
    public abstract void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword);

    /**
     * Can return <code>null</code>.
     */
    public abstract Cursor getBookmarksInFolder(ContentResolver cr, long folderId);

    /**
     * Get the favicon from the database, if any, associated with the given favicon URL. (That is,
     * the URL of the actual favicon image, not the URL of the page with which the favicon is associated.)
     * @param cr The ContentResolver to use.
     * @param faviconURL The URL of the favicon to fetch from the database.
     * @return The decoded Bitmap from the database, if any. null if none is stored.
     */
    public abstract LoadFaviconResult getFaviconForUrl(ContentResolver cr, String faviconURL);

    /**
     * Try to find a usable favicon URL in the history or bookmarks table.
     */
    public abstract String getFaviconURLFromPageURL(ContentResolver cr, String uri);

    public abstract void updateFaviconForUrl(ContentResolver cr, String pageUri, byte[] encodedFavicon, String faviconUri);

    public abstract byte[] getThumbnailForUrl(ContentResolver cr, String uri);
    public abstract void updateThumbnailForUrl(ContentResolver cr, String uri, BitmapDrawable thumbnail);

    /**
     * Query for non-null thumbnails matching the provided <code>urls</code>.
     * The returned cursor will have no more than, but possibly fewer than,
     * the requested number of thumbnails.
     *
     * Returns null if the provided list of URLs is empty or null.
     */
    public abstract Cursor getThumbnailsForUrls(ContentResolver cr,
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

    public abstract void updateFaviconInBatch(ContentResolver cr,
            Collection<ContentProviderOperation> operations, String url,
            String faviconUrl, String faviconGuid, byte[] data);


    public abstract Cursor getPinnedSites(ContentResolver cr, int limit);
    public abstract void pinSite(ContentResolver cr, String url, String title, int position);
    public abstract void unpinSite(ContentResolver cr, int position);

    public abstract boolean hideSuggestedSite(String url);
    public abstract void setSuggestedSites(SuggestedSites suggestedSites);
    public abstract boolean hasSuggestedImageUrl(String url);
    public abstract String getSuggestedImageUrlForUrl(String url);
    public abstract int getSuggestedBackgroundColorForUrl(String url);
    public abstract int getTrackingIdForUrl(String url);
}
