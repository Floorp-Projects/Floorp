/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.File;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.favicons.decoders.LoadFaviconResult;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;

class StubSearches implements Searches {
    public StubSearches() {
    }

    public void insert(ContentResolver cr, String query) {
    }
}

class StubURLMetadata implements URLMetadata {
    public StubURLMetadata() {
    }

    public Map<String, Object> fromJSON(JSONObject obj) {
        return new HashMap<String, Object>();
    }

    public Map<String, Map<String, Object>> getForURLs(final ContentResolver cr,
                                                       final List<String> urls,
                                                       final List<String> columns) {
        return new HashMap<String, Map<String, Object>>();
    }

    public void save(final ContentResolver cr, final Map<String, Object> data) {
    }
}

class StubTabsAccessor implements TabsAccessor {
    public StubTabsAccessor() {
    }

    @Override
    public List<RemoteClient> getClientsWithoutTabsByRecencyFromCursor(Cursor cursor) {
        return new ArrayList<>();
    }

    @Override
    public List<RemoteClient> getClientsFromCursor(final Cursor cursor) {
        return new ArrayList<RemoteClient>();
    }

    @Override
    public Cursor getRemoteClientsByRecencyCursor(Context context) {
        return null;
    }

    public Cursor getRemoteTabsCursor(Context context) {
        return null;
    }

    public Cursor getRemoteTabsCursor(Context context, int limit) {
        return null;
    }

    public void getTabs(final Context context, final OnQueryTabsCompleteListener listener) {
        listener.onQueryTabsComplete(new ArrayList<RemoteClient>());
    }
    public void getTabs(final Context context, final int limit, final OnQueryTabsCompleteListener listener) {
        listener.onQueryTabsComplete(new ArrayList<RemoteClient>());
    }

    public synchronized void persistLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs) { }
}

class StubUrlAnnotations implements UrlAnnotations {
    @Override
    public void insertAnnotation(ContentResolver cr, String url, String key, String value) {}

    @Override
    public Cursor getScreenshots(ContentResolver cr) { return null; }

    @Override
    public void insertScreenshot(ContentResolver cr, String pageUrl, final String screenshotLocation) {}

    @Override
    public Cursor getFeedSubscriptions(ContentResolver cr) { return null; }

    @Override
    public Cursor getWebsitesWithFeedUrl(ContentResolver cr) { return null; }

    @Override
    public void deleteFeedUrl(ContentResolver cr, String websiteUrl) {}

    @Override
    public boolean hasWebsiteForFeedUrl(ContentResolver cr, String feedUrl) { return false; }

    @Override
    public void deleteFeedSubscription(ContentResolver cr, FeedSubscription subscription) {}

    @Override
    public void updateFeedSubscription(ContentResolver cr, FeedSubscription subscription) {}

    @Override
    public boolean hasFeedSubscription(ContentResolver cr, String feedUrl) { return false; }

    @Override
    public void insertFeedSubscription(ContentResolver cr, FeedSubscription subscription) {}

    @Override
    public boolean hasFeedUrlForWebsite(ContentResolver cr, String websiteUrl) { return false; }

    @Override
    public void insertFeedUrl(ContentResolver cr, String originUrl, String feedUrl) {}

    @Override
    public void insertReaderViewUrl(ContentResolver cr, String pageURL) {}

    @Override
    public void deleteReaderViewUrl(ContentResolver cr, String pageURL) {}

    @Override
    public boolean hasAcceptedOrDeclinedHomeScreenShortcut(ContentResolver cr, String url) { return false; }

    @Override
    public void insertHomeScreenShortcut(ContentResolver cr, String url, boolean hasCreatedShortCut) {}

    @Override
    public int getAnnotationCount(ContentResolver cr, BrowserContract.UrlAnnotations.Key key) {
        return 0;
    }
}

/*
 * This base implementation just stubs all methods. For the
 * real implementations, see LocalBrowserDB.java.
 */
public class StubBrowserDB implements BrowserDB {
    private final StubSearches searches = new StubSearches();
    private final StubTabsAccessor tabsAccessor = new StubTabsAccessor();
    private final StubURLMetadata urlMetadata = new StubURLMetadata();
    private final StubUrlAnnotations urlAnnotations = new StubUrlAnnotations();
    private SuggestedSites suggestedSites = null;

    @Override
    public Searches getSearches() {
        return searches;
    }

    @Override
    public TabsAccessor getTabsAccessor() {
        return tabsAccessor;
    }

    @Override
    public URLMetadata getURLMetadata() {
        return urlMetadata;
    }

    @Override
    public UrlAnnotations getUrlAnnotations() {
        return urlAnnotations;
    }

    protected static final Integer FAVICON_ID_NOT_FOUND = Integer.MIN_VALUE;

    public StubBrowserDB(String profile) {
    }

    public void invalidate() { }

    public int addDefaultBookmarks(Context context, ContentResolver cr, final int offset) {
        return 0;
    }

    public int addDistributionBookmarks(ContentResolver cr, Distribution distribution, int offset) {
        return 0;
    }

    public int getCount(ContentResolver cr, String database) {
        return 0;
    }

    @RobocopTarget
    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit,
                         EnumSet<BrowserDB.FilterFlags> flags) {
        return null;
    }

    public void updateVisitedHistory(ContentResolver cr, String uri) {
    }

    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
    }

    @RobocopTarget
    public Cursor getAllVisitedHistory(ContentResolver cr) {
        return null;
    }

    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        return null;
    }

    @Override
    public Cursor getHistoryForURL(ContentResolver cr, String uri) {
        return null;
    }

    @Override
    public Cursor getRecentHistoryBetweenTime(ContentResolver cr, int limit, long time, long end) {
        return null;
    }

    @Override
    public long getPrePathLastVisitedTimeMilliseconds(ContentResolver cr, String prePath) { return 0; }

    public void expireHistory(ContentResolver cr, BrowserContract.ExpirePriority priority) {
    }

    @RobocopTarget
    public void removeHistoryEntry(ContentResolver cr, String url) {
    }

    public void clearHistory(ContentResolver cr, boolean clearSearchHistory) {
    }

    @RobocopTarget
    public Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        return null;
    }

    public int getBookmarkCountForFolder(ContentResolver cr, long folderId) { return 0; }

    @RobocopTarget
    public boolean isBookmark(ContentResolver cr, String uri) {
        return false;
    }

    public String getUrlForKeyword(ContentResolver cr, String keyword) {
        return null;
    }

    protected void bumpParents(ContentResolver cr, String param, String value) {
    }

    @RobocopTarget
    public boolean addBookmark(ContentResolver cr, String title, String uri) {
        return false;
    }

    @RobocopTarget
    public void removeBookmarksWithURL(ContentResolver cr, String uri) {
    }

    public void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
    }

    @RobocopTarget
    public void updateBookmark(ContentResolver cr, int id, String uri, String title, String keyword) {
    }

    public LoadFaviconResult getFaviconForUrl(ContentResolver cr, String faviconURL) {
        return null;
    }

    public String getFaviconURLFromPageURL(ContentResolver cr, String uri) {
        return null;
    }

    public void updateFaviconForUrl(ContentResolver cr, String pageUri,
                                    byte[] encodedFavicon, String faviconUri) {
    }

    public boolean hideSuggestedSite(String url) {
        return false;
    }

    public void updateThumbnailForUrl(ContentResolver cr, String uri,
                                      BitmapDrawable thumbnail) {
    }

    @RobocopTarget
    public byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        return null;
    }

    public Cursor getThumbnailsForUrls(ContentResolver cr, List<String> urls) {
        return null;
    }

    @RobocopTarget
    public void removeThumbnails(ContentResolver cr) {
    }

    public void updateHistoryInBatch(ContentResolver cr,
                                     Collection<ContentProviderOperation> operations,
                                     String url, String title,
                                     long date, int visits) {
    }

    public void updateBookmarkInBatch(ContentResolver cr,
                                      Collection<ContentProviderOperation> operations,
                                      String url, String title, String guid,
                                      long parent, long added,
                                      long modified, long position,
                                      String keyword, int type) {
    }

    public void updateFaviconInBatch(ContentResolver cr,
                                     Collection<ContentProviderOperation> operations,
                                     String url, String faviconUrl,
                                     String faviconGuid, byte[] data) {
    }

    public void pinSite(ContentResolver cr, String url, String title, int position) {
    }

    public void unpinSite(ContentResolver cr, int position) {
    }

    @RobocopTarget
    public Cursor getBookmarkForUrl(ContentResolver cr, String url) {
        return null;
    }

    @Override
    public Cursor getBookmarksForPartialUrl(ContentResolver cr, String partialUrl) {
        return null;
    }

    @Override
    public boolean hasBookmarkWithGuid(ContentResolver cr, String guid) {
        return false;
    }

    public void setSuggestedSites(SuggestedSites suggestedSites) {
        this.suggestedSites = suggestedSites;
    }

    public SuggestedSites getSuggestedSites() {
        return suggestedSites;
    }

    public boolean hasSuggestedImageUrl(String url) {
        return false;
    }

    public String getSuggestedImageUrlForUrl(String url) {
        return null;
    }

    public int getSuggestedBackgroundColorForUrl(String url) {
        return 0;
    }

    public Cursor getTopSites(ContentResolver cr, int suggestedRangeLimit, int limit) {
        return null;
    }

    public static Factory getFactory() {
        return new Factory() {
            @Override
            public BrowserDB get(String profileName, File profileDir) {
                return new StubBrowserDB(profileName);
            }
        };
    }
}
