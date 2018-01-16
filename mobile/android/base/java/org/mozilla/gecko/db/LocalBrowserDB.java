/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.IllegalAccessException;
import java.lang.NoSuchFieldException;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.ActivityStreamBlocklist;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.ExpirePriority;
import org.mozilla.gecko.db.BrowserContract.Favicons;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.SyncColumns;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserContract.TopSites;
import org.mozilla.gecko.db.BrowserContract.PageMetadata;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.BitmapUtils;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.StringUtils;

import android.content.ContentProviderClient;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MergeCursor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.RemoteException;
import android.os.SystemClock;
import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.content.CursorLoader;
import android.text.TextUtils;
import android.util.Log;
import org.mozilla.gecko.util.IOUtils;

import static org.mozilla.gecko.util.IOUtils.ConsumedInputStream;

public class LocalBrowserDB extends BrowserDB {
    // The default size of the buffer to use for downloading Favicons in the event no size is given
    // by the server.
    public static final int DEFAULT_FAVICON_BUFFER_SIZE_BYTES = 25000;

    private static final String LOGTAG = "GeckoLocalBrowserDB";

    // Calculate this once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static final boolean logDebug = Log.isLoggable(LOGTAG, Log.DEBUG);
    protected static void debug(String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    // Sentinel value used to indicate a failure to locate an ID for a default favicon.
    private static final int FAVICON_ID_NOT_FOUND = Integer.MIN_VALUE;

    // Constant used to indicate that no folder was found for particular GUID.
    private static final long FOLDER_NOT_FOUND = -1L;

    private final String mProfile;

    // Map of folder GUIDs to IDs. Used for caching.
    private final HashMap<String, Long> mFolderIdMap;

    // Use wrapped Boolean so that we can have a null state
    private volatile Boolean mDesktopBookmarksExist;

    private volatile SuggestedSites mSuggestedSites;

    // Constants used when importing history data from legacy browser.
    public static final String HISTORY_VISITS_DATE = "date";
    public static final String HISTORY_VISITS_COUNT = "visits";
    public static final String HISTORY_VISITS_URL = "url";

    private static final String TELEMETRY_HISTOGRAM_ACTIVITY_STREAM_TOPSITES = "FENNEC_ACTIVITY_STREAM_TOPSITES_LOADER_TIME_MS";

    private final Uri mBookmarksUriWithProfile;
    private final Uri mParentsUriWithProfile;
    private final Uri mHistoryUriWithProfile;
    private final Uri mHistoryExpireUriWithProfile;
    private final Uri mCombinedUriWithProfile;
    private final Uri mUpdateHistoryUriWithProfile;
    private final Uri mFaviconsUriWithProfile;
    private final Uri mThumbnailsUriWithProfile;
    private final Uri mTopSitesUriWithProfile;
    private final Uri mHighlightCandidatesUriWithProfile;
    private final Uri mSearchHistoryUri;
    private final Uri mActivityStreamBlockedUriWithProfile;
    private final Uri mPageMetadataWithProfile;

    private LocalSearches searches;
    private LocalTabsAccessor tabsAccessor;
    private LocalURLMetadata urlMetadata;
    private LocalUrlAnnotations urlAnnotations;

    private static final String[] DEFAULT_BOOKMARK_COLUMNS =
            new String[] { Bookmarks._ID,
                           Bookmarks.GUID,
                           Bookmarks.URL,
                           Bookmarks.TITLE,
                           Bookmarks.TYPE,
                           Bookmarks.PARENT };

    public LocalBrowserDB(String profile) {
        mProfile = profile;
        mFolderIdMap = new HashMap<String, Long>();

        mBookmarksUriWithProfile = DBUtils.appendProfile(profile, Bookmarks.CONTENT_URI);
        mParentsUriWithProfile = DBUtils.appendProfile(profile, Bookmarks.PARENTS_CONTENT_URI);
        mHistoryUriWithProfile = DBUtils.appendProfile(profile, History.CONTENT_URI);
        mHistoryExpireUriWithProfile = DBUtils.appendProfile(profile, History.CONTENT_OLD_URI);
        mCombinedUriWithProfile = DBUtils.appendProfile(profile, Combined.CONTENT_URI);
        mFaviconsUriWithProfile = DBUtils.appendProfile(profile, Favicons.CONTENT_URI);
        mTopSitesUriWithProfile = DBUtils.appendProfile(profile, TopSites.CONTENT_URI);
        mHighlightCandidatesUriWithProfile = DBUtils.appendProfile(profile, BrowserContract.HighlightCandidates.CONTENT_URI);
        mThumbnailsUriWithProfile = DBUtils.appendProfile(profile, Thumbnails.CONTENT_URI);
        mActivityStreamBlockedUriWithProfile = DBUtils.appendProfile(profile, ActivityStreamBlocklist.CONTENT_URI);

        mPageMetadataWithProfile = DBUtils.appendProfile(profile, PageMetadata.CONTENT_URI);

        mSearchHistoryUri = BrowserContract.SearchHistory.CONTENT_URI;

        mUpdateHistoryUriWithProfile =
                mHistoryUriWithProfile.buildUpon()
                                      .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                                      .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                                      .build();

        searches = new LocalSearches(mProfile);
        tabsAccessor = new LocalTabsAccessor(mProfile);
        urlMetadata = new LocalURLMetadata(mProfile);
        urlAnnotations = new LocalUrlAnnotations(mProfile);
    }

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

    @RobocopTarget
    @Override
    public UrlAnnotations getUrlAnnotations() {
        return urlAnnotations;
    }

    /**
     * Not thread safe. A helper to allocate new IDs for arbitrary strings.
     */
    private static class NameCounter {
        private final HashMap<String, Integer> names = new HashMap<String, Integer>();
        private int counter;
        private final int increment;

        public NameCounter(int start, int increment) {
            this.counter = start;
            this.increment = increment;
        }

        public int get(final String name) {
            Integer mapping = names.get(name);
            if (mapping == null) {
                int ours = counter;
                counter += increment;
                names.put(name, ours);
                return ours;
            }

            return mapping;
        }

        public boolean has(final String name) {
            return names.containsKey(name);
        }
    }

    /**
     * Add default bookmarks to the database.
     * Takes an offset; returns a new offset.
     */
    @Override
    public int addDefaultBookmarks(Context context, ContentResolver cr, final int offset) {
        final long folderID = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        if (folderID == FOLDER_NOT_FOUND) {
            Log.e(LOGTAG, "No mobile folder: cannot add default bookmarks.");
            return offset;
        }

        // Use reflection to walk the set of bookmark defaults.
        // This is horrible.
        final Class<?> stringsClass = R.string.class;
        final Field[] fields = stringsClass.getFields();
        final Pattern p = Pattern.compile("^bookmarkdefaults_title_");

        int pos = offset;
        final long now = System.currentTimeMillis();

        final ArrayList<ContentValues> bookmarkValues = new ArrayList<ContentValues>();
        final ArrayList<ContentValues> faviconValues = new ArrayList<ContentValues>();

        // Count down from -offset into negative values to get new favicon IDs.
        final NameCounter faviconIDs = new NameCounter((-1 - offset), -1);

        for (int i = 0; i < fields.length; i++) {
            final String name = fields[i].getName();
            final Matcher m = p.matcher(name);
            if (!m.find()) {
                continue;
            }

            try {
                if (Restrictions.isRestrictedProfile(context)) {
                    // matching on variable name from strings.xml.in
                    final String addons = "bookmarkdefaults_title_addons";
                    final String regularSumo = "bookmarkdefaults_title_support";
                    if (name.equals(addons) || name.equals(regularSumo)) {
                        continue;
                    }
                }
                if (!Restrictions.isRestrictedProfile(context)) {
                    // if we're not in kidfox, skip the kidfox specific bookmark(s)
                    if (name.startsWith("bookmarkdefaults_title_restricted")) {
                        continue;
                    }
                }
                final int titleID = fields[i].getInt(null);
                final String title = context.getString(titleID);

                final Field urlField = stringsClass.getField(name.replace("_title_", "_url_"));
                final int urlID = urlField.getInt(null);
                final String url = context.getString(urlID);

                final ContentValues bookmarkValue = createBookmark(now, title, url, pos++, folderID);
                bookmarkValues.add(bookmarkValue);

                ConsumedInputStream faviconStream = getDefaultFaviconFromDrawable(context, name);
                if (faviconStream == null) {
                    faviconStream = getDefaultFaviconFromPath(context, name);
                }

                if (faviconStream == null) {
                    continue;
                }

                // In the event that truncating the buffer fails, give up and move on.
                byte[] icon;
                try {
                    icon = faviconStream.getTruncatedData();
                } catch (OutOfMemoryError e) {
                    continue;
                }

                final ContentValues iconValue = createFavicon(url, icon);

                // Assign a reserved negative _id to each new favicon.
                // For now, each name is expected to be unique, and duplicate
                // icons will be duplicated in the DB. See Bug 1040806 Comment 8.
                if (iconValue != null) {
                    final int faviconID = faviconIDs.get(name);
                    iconValue.put("_id", faviconID);
                    bookmarkValue.put(Bookmarks.FAVICON_ID, faviconID);
                    faviconValues.add(iconValue);
                }
            } catch (IllegalAccessException e) {
                Log.wtf(LOGTAG, "Reflection failure.", e);
            } catch (IllegalArgumentException e) {
                Log.wtf(LOGTAG, "Reflection failure.", e);
            } catch (NoSuchFieldException e) {
                Log.wtf(LOGTAG, "Reflection failure.", e);
            }
        }

        if (!faviconValues.isEmpty()) {
            try {
                cr.bulkInsert(mFaviconsUriWithProfile, faviconValues.toArray(new ContentValues[faviconValues.size()]));
            } catch (Exception e) {
                Log.e(LOGTAG, "Error bulk-inserting default favicons.", e);
            }
        }

        if (!bookmarkValues.isEmpty()) {
            try {
                final int inserted = cr.bulkInsert(mBookmarksUriWithProfile, bookmarkValues.toArray(new ContentValues[bookmarkValues.size()]));
                return offset + inserted;
            } catch (Exception e) {
                Log.e(LOGTAG, "Error bulk-inserting default bookmarks.", e);
            }
        }

        return offset;
    }

    /**
     * Add bookmarks from the provided distribution.
     * Takes an offset; returns a new offset.
     */
    @Override
    public int addDistributionBookmarks(ContentResolver cr, Distribution distribution, int offset) {
        if (!distribution.exists()) {
            Log.d(LOGTAG, "No distribution from which to add bookmarks.");
            return offset;
        }

        final JSONArray bookmarks = distribution.getBookmarks();
        if (bookmarks == null) {
            Log.d(LOGTAG, "No distribution bookmarks.");
            return offset;
        }

        final long folderID = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        if (folderID == FOLDER_NOT_FOUND) {
            Log.e(LOGTAG, "No mobile folder: cannot add distribution bookmarks.");
            return offset;
        }

        final Locale locale = Locale.getDefault();
        final long now = System.currentTimeMillis();
        int mobilePos = offset;
        int pinnedPos = 0;        // Assume nobody has pinned anything yet.

        final ArrayList<ContentValues> bookmarkValues = new ArrayList<ContentValues>();
        final ArrayList<ContentValues> faviconValues = new ArrayList<ContentValues>();

        // Count down from -offset into negative values to get new favicon IDs.
        final NameCounter faviconIDs = new NameCounter((-1 - offset), -1);

        for (int i = 0; i < bookmarks.length(); i++) {
            try {
                final JSONObject bookmark = bookmarks.getJSONObject(i);

                final String title = getLocalizedProperty(bookmark, "title", locale);
                final String url = getLocalizedProperty(bookmark, "url", locale);
                final long parent;
                final int pos;
                if (bookmark.has("pinned")) {
                    parent = Bookmarks.FIXED_PINNED_LIST_ID;
                    pos = pinnedPos++;
                } else {
                    parent = folderID;
                    pos = mobilePos++;
                }

                final ContentValues bookmarkValue = createBookmark(now, title, url, pos, parent);
                bookmarkValues.add(bookmarkValue);

                // Return early if there is no icon for this bookmark.
                if (!bookmark.has("icon")) {
                    continue;
                }

                try {
                    final String iconData = bookmark.getString("icon");

                    byte[] icon = BitmapUtils.getBytesFromDataURI(iconData);
                    if (icon == null) {
                        continue;
                    }

                    final ContentValues iconValue = createFavicon(url, icon);
                    if (iconValue == null) {
                        continue;
                    }

                    /*
                     * Find out if this icon is a duplicate. If it is, don't try
                     * to insert it again, but reuse the shared ID.
                     * Otherwise, assign a new reserved negative _id.
                     * Duplicates won't be detected in default bookmarks, or
                     * those already in the database.
                     */
                    final boolean seen = faviconIDs.has(iconData);
                    final int faviconID = faviconIDs.get(iconData);

                    iconValue.put("_id", faviconID);
                    bookmarkValue.put(Bookmarks.FAVICON_ID, faviconID);

                    if (!seen) {
                        faviconValues.add(iconValue);
                    }
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error creating distribution bookmark icon.", e);
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error creating distribution bookmark.", e);
            }
        }

        if (!faviconValues.isEmpty()) {
            try {
                cr.bulkInsert(mFaviconsUriWithProfile, faviconValues.toArray(new ContentValues[faviconValues.size()]));
            } catch (Exception e) {
                Log.e(LOGTAG, "Error bulk-inserting distribution favicons.", e);
            }
        }

        if (!bookmarkValues.isEmpty()) {
            try {
                final int inserted = cr.bulkInsert(mBookmarksUriWithProfile, bookmarkValues.toArray(new ContentValues[bookmarkValues.size()]));
                return offset + inserted;
            } catch (Exception e) {
                Log.e(LOGTAG, "Error bulk-inserting distribution bookmarks.", e);
            }
        }

        return offset;
    }

    private static ContentValues createBookmark(final long timestamp, final String title, final String url, final int pos, final long parent) {
        final ContentValues v = new ContentValues();

        v.put(Bookmarks.DATE_CREATED, timestamp);
        v.put(Bookmarks.DATE_MODIFIED, timestamp);
        v.put(Bookmarks.GUID, Utils.generateGuid());

        v.put(Bookmarks.PARENT, parent);
        v.put(Bookmarks.POSITION, pos);
        v.put(Bookmarks.TITLE, title);
        v.put(Bookmarks.URL, url);
        return v;
    }

    private static ContentValues createFavicon(final String url, final byte[] icon) {
        ContentValues iconValues = new ContentValues();
        iconValues.put(Favicons.PAGE_URL, url);
        iconValues.put(Favicons.DATA, icon);

        return iconValues;
    }

    private static String getLocalizedProperty(final JSONObject bookmark, final String property, final Locale locale) throws JSONException {
        // Try the full locale.
        final String fullLocale = property + "." + locale.toString();
        if (bookmark.has(fullLocale)) {
            return bookmark.getString(fullLocale);
        }

        // Try without a variant.
        if (!TextUtils.isEmpty(locale.getVariant())) {
            String noVariant = fullLocale.substring(0, fullLocale.lastIndexOf("_"));
            if (bookmark.has(noVariant)) {
                return bookmark.getString(noVariant);
            }
        }

        // Try just the language.
        String lang = property + "." + locale.getLanguage();
        if (bookmark.has(lang)) {
            return bookmark.getString(lang);
        }

        // Default to the non-localized property name.
        return bookmark.getString(property);
    }

    private static int getFaviconId(String name) {
        try {
            Class<?> drawablesClass = R.raw.class;

            // Look for a favicon with the id R.raw.bookmarkdefaults_favicon_*.
            Field faviconField = drawablesClass.getField(name.replace("_title_", "_favicon_"));
            faviconField.setAccessible(true);

            return faviconField.getInt(null);
        } catch (IllegalAccessException e) {
            // We'll end up here for any default bookmark that doesn't have a favicon in
            // resources/raw/ (i.e., about:firefox). When this happens, the Favicons service will
            // fall back to the default branding icon for about pages. Non-about pages should always
            // specify an icon; otherwise, the placeholder globe favicon will be used.
            Log.d(LOGTAG, "No raw favicon resource found for " + name);
        } catch (NoSuchFieldException e) {
            // We'll end up here for any default bookmark that doesn't have a favicon in
            // resources/raw/ (i.e., about:firefox). When this happens, the Favicons service will
            // fall back to the default branding icon for about pages. Non-about pages should always
            // specify an icon; otherwise, the placeholder globe favicon will be used.
            Log.d(LOGTAG, "No raw favicon resource found for " + name);
        }

        Log.e(LOGTAG, "Failed to find favicon resource ID for " + name);
        return FAVICON_ID_NOT_FOUND;
    }

    @Override
    public boolean insertPageMetadata(ContentProviderClient contentProviderClient, String pageUrl, boolean hasImage, String metadataJSON) {
        final String historyGUID = lookupHistoryGUIDByPageUri(contentProviderClient, pageUrl);

        if (historyGUID == null) {
            return false;
        }

        // We have the GUID, insert the metadata.
        final ContentValues cv = new ContentValues();
        cv.put(PageMetadata.HISTORY_GUID, historyGUID);
        cv.put(PageMetadata.HAS_IMAGE, hasImage);
        cv.put(PageMetadata.JSON, metadataJSON);

        try {
            contentProviderClient.insert(mPageMetadataWithProfile, cv);
        } catch (RemoteException e) {
            throw new IllegalStateException("Unexpected RemoteException", e);
        }

        return true;
    }

    @Override
    public int deletePageMetadata(ContentProviderClient contentProviderClient, String pageUrl) {
        final String historyGUID = lookupHistoryGUIDByPageUri(contentProviderClient, pageUrl);

        if (historyGUID == null) {
            return 0;
        }

        try {
            return contentProviderClient.delete(mPageMetadataWithProfile, PageMetadata.HISTORY_GUID + " = ?", new String[]{historyGUID});
        } catch (RemoteException e) {
            throw new IllegalStateException("Unexpected RemoteException", e);
        }
    }

    @Nullable
    private String lookupHistoryGUIDByPageUri(ContentProviderClient contentProviderClient, String uri) {
        // Unfortunately we might have duplicate history records for the same URL.
        final Cursor cursor;
        try {
            cursor = contentProviderClient.query(
                    mHistoryUriWithProfile
                            .buildUpon()
                            .appendQueryParameter(BrowserContract.PARAM_LIMIT, "1")
                            .build(),
                    new String[]{
                            History.GUID,
                    },
                    History.URL + "= ?",
                    new String[]{uri}, History.DATE_LAST_VISITED + " DESC"
            );
        } catch (RemoteException e) {
            // Won't happen, we control the implementation.
            throw new IllegalStateException("Unexpected RemoteException", e);
        }

        if (cursor == null) {
            return null;
        }

        try {
            if (!cursor.moveToFirst()) {
                return null;
            }

            final int historyGUIDCol = cursor.getColumnIndexOrThrow(History.GUID);
            return cursor.getString(historyGUIDCol);
        } finally {
            cursor.close();
        }
    }

    /**
     * Load a favicon from the omnijar.
     * @return A ConsumedInputStream containing the bytes loaded from omnijar. This must be a format
     *         compatible with the favicon decoder (most probably a PNG or ICO file).
     */
    private static ConsumedInputStream getDefaultFaviconFromPath(Context context, String name) {
        final int faviconId = getFaviconId(name);
        if (faviconId == FAVICON_ID_NOT_FOUND) {
            return null;
        }

        final String bitmapPath = GeckoJarReader.getJarURL(context, context.getString(faviconId));
        final InputStream iStream = GeckoJarReader.getStream(context, bitmapPath);

        return IOUtils.readFully(iStream, DEFAULT_FAVICON_BUFFER_SIZE_BYTES);
    }

    private static ConsumedInputStream getDefaultFaviconFromDrawable(Context context, String name) {
        int faviconId = getFaviconId(name);
        if (faviconId == FAVICON_ID_NOT_FOUND) {
            return null;
        }

        InputStream iStream = context.getResources().openRawResource(faviconId);
        return IOUtils.readFully(iStream, DEFAULT_FAVICON_BUFFER_SIZE_BYTES);
    }

    // Invalidate cached data
    @Override
    public void invalidate() {
        mDesktopBookmarksExist = null;
    }

    private Uri bookmarksUriWithLimit(int limit) {
        return mBookmarksUriWithProfile.buildUpon()
                                       .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                             String.valueOf(limit))
                                       .build();
    }

    private Uri combinedUriWithLimit(int limit) {
        return mCombinedUriWithProfile.buildUpon()
                                      .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                                                            String.valueOf(limit))
                                      .build();
    }

    private static Uri withDeleted(final Uri uri) {
        return uri.buildUpon()
                  .appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1")
                  .build();
    }

    private Cursor filterAllSites(ContentResolver cr, String[] projection, CharSequence constraint,
                                  int limit, CharSequence urlFilter, String selection, String[] selectionArgs) {
        // The combined history/bookmarks selection queries for sites with a URL or title containing
        // the constraint string(s), treating space-separated words as separate constraints
        if (!TextUtils.isEmpty(constraint)) {
            final String[] constraintWords = constraint.toString().split(" ");

            // Only create a filter query with a maximum of 10 constraint words.
            final int constraintCount = Math.min(constraintWords.length, 10);
            for (int i = 0; i < constraintCount; i++) {
                selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " LIKE ? OR " +
                                                                      Combined.TITLE + " LIKE ?)");
                String constraintWord =  "%" + constraintWords[i] + "%";
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                                                            new String[] { constraintWord, constraintWord });
            }
        }

        if (urlFilter != null) {
            selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " NOT LIKE ?)");
            selectionArgs = DBUtils.appendSelectionArgs(selectionArgs, new String[] { urlFilter.toString() });
        }

        // Order by combined remote+local frecency score.
        // Local visits are preferred, so they will by far outweigh remote visits.
        // Bookmarked history items get extra frecency points.
        final String sortOrder = BrowserContract.getCombinedFrecencySortOrder(true, false);

        return cr.query(combinedUriWithLimit(limit),
                        projection,
                        selection,
                        selectionArgs,
                        sortOrder);
    }

    @Override
    public int getCount(ContentResolver cr, String database) {
        int count = 0;
        String[] columns = null;
        String constraint = null;
        Uri uri = null;

        if ("history".equals(database)) {
            uri = mHistoryUriWithProfile;
            columns = new String[] { History._ID };
            constraint = Combined.VISITS + " > 0";
        } else if ("bookmarks".equals(database)) {
            uri = mBookmarksUriWithProfile;
            columns = new String[] { Bookmarks._ID };
            // ignore folders, tags, keywords, separators, etc.
            constraint = Bookmarks.TYPE + " = " + Bookmarks.TYPE_BOOKMARK;
        } else if ("thumbnails".equals(database)) {
            uri = mThumbnailsUriWithProfile;
            columns = new String[] { Thumbnails._ID };
        } else if ("favicons".equals(database)) {
            uri = mFaviconsUriWithProfile;
            columns = new String[] { Favicons._ID };
        }

        if (uri != null) {
            final Cursor cursor = cr.query(uri, columns, constraint, null, null);

            try {
                count = cursor.getCount();
            } finally {
                cursor.close();
            }
        }

        debug("Got count " + count + " for " + database);
        return count;
    }

    @Override
    @RobocopTarget
    public Cursor filter(ContentResolver cr, CharSequence constraint, int limit,
                         EnumSet<FilterFlags> flags) {
        String selection = "";
        String[] selectionArgs = null;

        if (flags.contains(FilterFlags.EXCLUDE_PINNED_SITES)) {
            selection = Combined.URL + " NOT IN (SELECT " +
                                                 Bookmarks.URL + " FROM bookmarks WHERE " +
                                                 DBUtils.qualifyColumn("bookmarks", Bookmarks.PARENT) + " = ? AND " +
                                                 DBUtils.qualifyColumn("bookmarks", Bookmarks.IS_DELETED) + " == 0)";
            selectionArgs = new String[] { String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID) };
        }

        return filterAllSites(cr,
                              new String[] { Combined._ID,
                                             Combined.URL,
                                             Combined.TITLE,
                                             Combined.BOOKMARK_ID,
                                             Combined.HISTORY_ID },
                              constraint,
                              limit,
                              null,
                              selection, selectionArgs);
    }

    @Override
    public void updateVisitedHistory(ContentResolver cr, String uri) {
        ContentValues values = new ContentValues();

        values.put(History.URL, uri);
        values.put(History.DATE_LAST_VISITED, System.currentTimeMillis());
        values.put(History.IS_DELETED, 0);

        // This will insert a new history entry if one for this URL
        // doesn't already exist
        cr.update(mUpdateHistoryUriWithProfile,
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    @Override
    public void updateHistoryTitle(ContentResolver cr, String uri, String title) {
        ContentValues values = new ContentValues();
        values.put(History.TITLE, title);

        cr.update(mHistoryUriWithProfile,
                  values,
                  History.URL + " = ?",
                  new String[] { uri });
    }

    @Override
    @RobocopTarget
    public Cursor getAllVisitedHistory(ContentResolver cr) {
        return cr.query(mHistoryUriWithProfile,
                        new String[] { History.URL },
                        History.VISITS + " > 0",
                        null,
                        null);
    }

    @Override
    public Cursor getRecentHistory(ContentResolver cr, int limit) {
        return cr.query(combinedUriWithLimit(limit),
                        new String[] { Combined._ID,
                                       Combined.BOOKMARK_ID,
                                       Combined.HISTORY_ID,
                                       Combined.URL,
                                       Combined.TITLE,
                                       Combined.DATE_LAST_VISITED,
                                       Combined.VISITS },
                        History.DATE_LAST_VISITED + " > 0",
                        null,
                        History.DATE_LAST_VISITED + " DESC");
    }

    @Nullable
    public Cursor getHistoryForURL(ContentResolver cr, String uri) {
        return cr.query(mHistoryUriWithProfile,
                new String[] {
                    History.VISITS,
                    History.DATE_LAST_VISITED
                },
                History.URL + "= ?",
                new String[] { uri },
                History.DATE_LAST_VISITED + " DESC"
        );
    }

    @Override
    public long getPrePathLastVisitedTimeMilliseconds(ContentResolver cr, String prePath) {
        if (prePath == null) {
            return 0;
        }
        // If we don't end with a trailing slash, then both https://foo.com and https://foo.company.biz will match.
        if (!prePath.endsWith("/")) {
            prePath = prePath + "/";
        }
        final Cursor cursor = cr.query(BrowserContract.History.CONTENT_URI,
                new String[] { "MAX(" + BrowserContract.HistoryColumns.DATE_LAST_VISITED + ") AS date" },
                BrowserContract.URLColumns.URL + " BETWEEN ? AND ?", new String[] { prePath, prePath + "\u007f" }, null);
        try {
            cursor.moveToFirst();
            if (cursor.isAfterLast()) {
                return 0;
            }
            return cursor.getLong(0);
        } finally {
            cursor.close();
        }
    }

    @Override
    public void expireHistory(ContentResolver cr, ExpirePriority priority) {
        Uri url = mHistoryExpireUriWithProfile;
        url = url.buildUpon().appendQueryParameter(BrowserContract.PARAM_EXPIRE_PRIORITY, priority.toString()).build();
        cr.delete(url, null, null);
    }

    @Override
    @RobocopTarget
    public void removeHistoryEntry(ContentResolver cr, String url) {
        cr.delete(mHistoryUriWithProfile,
                  History.URL + " = ?",
                  new String[] { url });
    }

    @Override
    public void clearHistory(ContentResolver cr, boolean clearSearchHistory) {
        if (clearSearchHistory) {
            cr.delete(mSearchHistoryUri, null, null);
        } else {
            cr.delete(mHistoryUriWithProfile, null, null);
        }
    }

    private void assertDefaultBookmarkColumnOrdering() {
        // We need to insert MatrixCursor values in a specific order - in order to protect against changes
        // in DEFAULT_BOOKMARK_COLUMNS we can just assert that we're using the correct ordering.
        // Alternatively we could use RowBuilder.add(columnName, value) but that needs api >= 19,
        // or we could iterate over DEFAULT_BOOKMARK_COLUMNS, but that gets messy once we need
        // to add more than one artificial folder.
        if (!((DEFAULT_BOOKMARK_COLUMNS[0].equals(Bookmarks._ID)) &&
                (DEFAULT_BOOKMARK_COLUMNS[1].equals(Bookmarks.GUID)) &&
                (DEFAULT_BOOKMARK_COLUMNS[2].equals(Bookmarks.URL)) &&
                (DEFAULT_BOOKMARK_COLUMNS[3].equals(Bookmarks.TITLE)) &&
                (DEFAULT_BOOKMARK_COLUMNS[4].equals(Bookmarks.TYPE)) &&
                (DEFAULT_BOOKMARK_COLUMNS[5].equals(Bookmarks.PARENT)) &&
                (DEFAULT_BOOKMARK_COLUMNS.length == 6))) {
            // If DEFAULT_BOOKMARK_COLUMNS changes we need to update all the MatrixCursor rows
            // to contain appropriate data.
            throw new IllegalStateException("Fake folder MatrixCursor creation code must be updated to match DEFAULT_BOOKMARK_COLUMNS");
        }
    }

    /**
     * Retrieve the list of reader-view bookmarks, i.e. the equivalent of the former reading-list.
     * This is the result of a join of bookmarks with reader-view annotations (as stored in
     * UrlAnnotations).
     */
    private Cursor getReadingListBookmarks(ContentResolver cr) {
        // group by URL to avoid having duplicate bookmarks listed. It's possible to have multiple
        // bookmarks pointing to the same URL (this would most commonly happen by manually
        // copying bookmarks on desktop, followed by syncing with mobile), and we don't want
        // to show the same URL multiple times in the reading list folder.
        final Uri bookmarksGroupedByUri = mBookmarksUriWithProfile.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_GROUP_BY, Bookmarks.URL)
                .build();

        return cr.query(bookmarksGroupedByUri,
                DEFAULT_BOOKMARK_COLUMNS,
                Bookmarks.ANNOTATION_KEY + " == ? AND " +
                Bookmarks.ANNOTATION_VALUE + " == ? AND " +
                "(" + Bookmarks.TYPE + " = ? AND " + Bookmarks.URL + " IS NOT NULL)",
                new String[] {
                        BrowserContract.UrlAnnotations.Key.READER_VIEW.getDbValue(),
                        BrowserContract.UrlAnnotations.READER_VIEW_SAVED_VALUE,
                        String.valueOf(Bookmarks.TYPE_BOOKMARK) },
                null);
    }

    @Override
    @RobocopTarget
    public Cursor getBookmarksInFolder(ContentResolver cr, long folderId) {
        final boolean addDesktopFolder;
        final boolean addScreenshotsFolder;
        final boolean addReadingListFolder;

        // We always want to show mobile bookmarks in the root view.
        if (folderId == Bookmarks.FIXED_ROOT_ID) {
            folderId = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);

            // We'll add a fake "Desktop Bookmarks" folder to the root view if desktop
            // bookmarks exist, so that the user can still access non-mobile bookmarks.
            addDesktopFolder = desktopBookmarksExist(cr);
            addScreenshotsFolder = AppConstants.SCREENSHOTS_IN_BOOKMARKS_ENABLED;

            final int readingListItemCount = getBookmarkCountForFolder(cr, Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID);
            addReadingListFolder = (readingListItemCount > 0);
        } else {
            addDesktopFolder = false;
            addScreenshotsFolder = false;
            addReadingListFolder = false;
        }

        final Cursor c;

        // (You can't switch on a long in Java, hence the if statements)
        if (folderId == Bookmarks.FAKE_DESKTOP_FOLDER_ID) {
            // Since the "Desktop Bookmarks" folder doesn't actually exist, we
            // just fake it by querying specifically certain known desktop folders.
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ? OR " +
                         Bookmarks.GUID + " = ?",
                         new String[] { Bookmarks.TOOLBAR_FOLDER_GUID,
                                        Bookmarks.MENU_FOLDER_GUID,
                                        Bookmarks.UNFILED_FOLDER_GUID },
                         null);
        } else if (folderId == Bookmarks.FIXED_SCREENSHOT_FOLDER_ID) {
            c = getUrlAnnotations().getScreenshots(cr);
        } else if (folderId == Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID) {
            c = getReadingListBookmarks(cr);
        } else {
            // Right now, we only support showing folder and bookmark type of
            // entries. We should add support for other types though (bug 737024)
            c = cr.query(mBookmarksUriWithProfile,
                         DEFAULT_BOOKMARK_COLUMNS,
                         Bookmarks.PARENT + " = ? AND " +
                         "(" + Bookmarks.TYPE + " = ? OR " +
                            "(" + Bookmarks.TYPE + " = ? AND " + Bookmarks.URL + " IS NOT NULL))",
                         new String[] { String.valueOf(folderId),
                                        String.valueOf(Bookmarks.TYPE_FOLDER),
                                        String.valueOf(Bookmarks.TYPE_BOOKMARK) },
                         null);
        }

        final List<Cursor> cursorsToMerge = getSpecialFoldersCursorList(addDesktopFolder, addScreenshotsFolder, addReadingListFolder);
        if (cursorsToMerge.size() >= 1) {
            cursorsToMerge.add(c);
            final Cursor[] arr = (Cursor[]) Array.newInstance(Cursor.class, cursorsToMerge.size());
            return new MergeCursor(cursorsToMerge.toArray(arr));
        } else {
            return c;
        }
    }

    @Override
    public int getBookmarkCountForFolder(ContentResolver cr, long folderID) {
        if (folderID == Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID) {
            return getUrlAnnotations().getAnnotationCount(cr, BrowserContract.UrlAnnotations.Key.READER_VIEW);
        } else {
            throw new IllegalArgumentException("Retrieving bookmark count for folder with ID=" + folderID + " not supported yet");
        }
    }

    @CheckResult
    private ArrayList<Cursor> getSpecialFoldersCursorList(final boolean addDesktopFolder,
            final boolean addScreenshotsFolder, final boolean addReadingListFolder) {
        if (addDesktopFolder || addScreenshotsFolder || addReadingListFolder) {
            // Avoid calling this twice.
            assertDefaultBookmarkColumnOrdering();
        }

        // Capacity is number of cursors added below plus one for non-special data.
        final ArrayList<Cursor> out = new ArrayList<>(4);
        if (addDesktopFolder) {
            out.add(getSpecialFolderCursor(Bookmarks.FAKE_DESKTOP_FOLDER_ID, Bookmarks.FAKE_DESKTOP_FOLDER_GUID));
        }

        if (addScreenshotsFolder) {
            out.add(getSpecialFolderCursor(Bookmarks.FIXED_SCREENSHOT_FOLDER_ID, Bookmarks.SCREENSHOT_FOLDER_GUID));
        }

        if (addReadingListFolder) {
            out.add(getSpecialFolderCursor(Bookmarks.FAKE_READINGLIST_SMARTFOLDER_ID, Bookmarks.FAKE_READINGLIST_SMARTFOLDER_GUID));
        }

        return out;
    }

    @CheckResult
    private MatrixCursor getSpecialFolderCursor(final int folderId, final String folderGuid) {
        final MatrixCursor out = new MatrixCursor(DEFAULT_BOOKMARK_COLUMNS);
        out.addRow(new Object[] {
                folderId,
                folderGuid,
                "",
                "", // Title localisation is done later, in the UI layer (BookmarksListAdapter)
                Bookmarks.TYPE_FOLDER,
                Bookmarks.FIXED_ROOT_ID
        });
        return out;
    }

    // Returns true if any desktop bookmarks exist, which will be true if the user
    // has set up sync at one point, or done a profile migration from XUL fennec.
    private boolean desktopBookmarksExist(ContentResolver cr) {
        if (mDesktopBookmarksExist != null) {
            return mDesktopBookmarksExist;
        }

        // Check to see if there are any bookmarks in one of our three
        // fixed "Desktop Bookmarks" folders.
        final Cursor c = cr.query(bookmarksUriWithLimit(1),
                                  new String[] { Bookmarks._ID },
                                  Bookmarks.PARENT + " = ? OR " +
                                  Bookmarks.PARENT + " = ? OR " +
                                  Bookmarks.PARENT + " = ?",
                                  new String[] { String.valueOf(getFolderIdFromGuid(cr, Bookmarks.TOOLBAR_FOLDER_GUID)),
                                                 String.valueOf(getFolderIdFromGuid(cr, Bookmarks.MENU_FOLDER_GUID)),
                                                 String.valueOf(getFolderIdFromGuid(cr, Bookmarks.UNFILED_FOLDER_GUID)) },
                                  null);

        try {
            // Don't read back out of the cache to avoid races with invalidation.
            final boolean e = c.getCount() > 0;
            mDesktopBookmarksExist = e;
            return e;
        } finally {
            c.close();
        }
    }

    @Override
    @RobocopTarget
    public boolean isBookmark(ContentResolver cr, String uri) {
        final Cursor c = cr.query(bookmarksUriWithLimit(1),
                                  new String[] { Bookmarks._ID },
                                  Bookmarks.URL + " = ? AND " + Bookmarks.PARENT + " != ?",
                                  new String[] { uri, String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID) },
                                  Bookmarks.URL);

        if (c == null) {
            Log.e(LOGTAG, "Null cursor in isBookmark");
            return false;
        }

        try {
            return c.getCount() > 0;
        } finally {
            c.close();
        }
    }

    @Override
    public String getUrlForKeyword(ContentResolver cr, String keyword) {
        final Cursor c = cr.query(mBookmarksUriWithProfile,
                                  new String[] { Bookmarks.URL },
                                  Bookmarks.KEYWORD + " = ?",
                                  new String[] { keyword },
                                  null);
        try {
            if (!c.moveToFirst()) {
                return null;
            }

            return c.getString(c.getColumnIndexOrThrow(Bookmarks.URL));
        } finally {
            c.close();
        }
    }

    private synchronized long getFolderIdFromGuid(final ContentResolver cr, final String guid) {
        if (mFolderIdMap.containsKey(guid)) {
            return mFolderIdMap.get(guid);
        }

        final Cursor c = cr.query(mBookmarksUriWithProfile,
                                  new String[] { Bookmarks._ID },
                                  Bookmarks.GUID + " = ?",
                                  new String[] { guid },
                                  null);
        try {
            final int col = c.getColumnIndexOrThrow(Bookmarks._ID);
            if (!c.moveToFirst() || c.isNull(col)) {
                return FOLDER_NOT_FOUND;
            }

            final long id = c.getLong(col);
            mFolderIdMap.put(guid, id);
            return id;
        } finally {
            c.close();
        }
    }

    private void addBookmarkItem(ContentResolver cr, String title, String uri, long folderId) {
        final long now = System.currentTimeMillis();
        ContentValues values = new ContentValues();
        if (title != null) {
            values.put(Bookmarks.TITLE, title);
        }

        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.PARENT, folderId);
        values.put(Bookmarks.DATE_MODIFIED, now);

        // Get the page's favicon ID from the history table
        final Cursor c = cr.query(mHistoryUriWithProfile,
                                  new String[] { History.FAVICON_ID },
                                  History.URL + " = ?",
                                  new String[] { uri },
                                  null);
        try {
            if (c.moveToFirst()) {
                int columnIndex = c.getColumnIndexOrThrow(History.FAVICON_ID);
                if (!c.isNull(columnIndex)) {
                    values.put(Bookmarks.FAVICON_ID, c.getLong(columnIndex));
                }
            }
        } finally {
            c.close();
        }

        // Restore deleted record if possible
        values.put(Bookmarks.IS_DELETED, 0);

        final Uri bookmarksWithInsert = mBookmarksUriWithProfile.buildUpon()
                                          .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                                          .build();
        cr.update(bookmarksWithInsert,
                  values,
                  Bookmarks.URL + " = ? AND " +
                  Bookmarks.PARENT + " = " + folderId,
                  new String[] { uri });

        // BrowserProvider will handle updating parent's lastModified timestamp, nothing else to do.
    }

    @Override
    @RobocopTarget
    public boolean addBookmark(ContentResolver cr, String title, String uri) {
        long folderId = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        if (isBookmarkForUrlInFolder(cr, uri, folderId)) {
            // Bookmark added already.
            return false;
        }

        // Add a new bookmark.
        addBookmarkItem(cr, title, uri, folderId);
        return true;
    }
    private boolean isBookmarkForUrlInFolder(ContentResolver cr, String uri, long folderId) {
        final Cursor c = cr.query(bookmarksUriWithLimit(1),
                                  new String[] { Bookmarks._ID },
                                  Bookmarks.URL + " = ? AND " + Bookmarks.PARENT + " = ? AND " + Bookmarks.IS_DELETED + " == 0",
                                  new String[] { uri, String.valueOf(folderId) },
                                  Bookmarks.URL);

        if (c == null) {
            return false;
        }

        try {
            return c.getCount() > 0;
        } finally {
            c.close();
        }
    }

    @Override
    public Uri addBookmarkFolder(ContentResolver cr, String title, long parentId) {
        final ContentValues values = new ContentValues();
        final long now = System.currentTimeMillis();
        values.put(Bookmarks.DATE_CREATED, now);
        values.put(Bookmarks.DATE_MODIFIED, now);
        values.put(Bookmarks.GUID, Utils.generateGuid());
        values.put(Bookmarks.PARENT, parentId);
        values.put(Bookmarks.TITLE, title);
        values.put(Bookmarks.TYPE, Bookmarks.TYPE_FOLDER);

        // BrowserProvider will bump parent's lastModified timestamp after successful insertion.
        return cr.insert(mBookmarksUriWithProfile, values);
    }

    @Override
    @RobocopTarget
    public void removeBookmarksWithURL(ContentResolver cr, String uri) {
        // BrowserProvider will bump parent's lastModified timestamp after successful deletion.
        cr.delete(mBookmarksUriWithProfile,
                  Bookmarks.URL + " = ? AND " + Bookmarks.PARENT + " != ? ",
                  new String[] { uri, String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID) });
    }

    @Override
    public void removeBookmarkWithId(ContentResolver cr, long id) {
        // BrowserProvider will bump parent's lastModified timestamp after successful deletion.
        cr.delete(mBookmarksUriWithProfile,
                  Bookmarks._ID + " = ? AND " + Bookmarks.PARENT + " != ? ",
                  new String[] { String.valueOf(id), String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID) });
    }

    @Override
    public void registerBookmarkObserver(ContentResolver cr, ContentObserver observer) {
        cr.registerContentObserver(mBookmarksUriWithProfile, false, observer);
    }

    @Override
    @RobocopTarget
    public void updateBookmark(ContentResolver cr, long id, String uri, String title, String keyword) {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.KEYWORD, keyword);
        values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

        cr.update(mBookmarksUriWithProfile,
                  values,
                  Bookmarks._ID + " = ?",
                  new String[] { String.valueOf(id) });
    }

    @Override
    public void updateBookmark(ContentResolver cr, long id, String uri, String title, String keyword,
                               long newParentId, long oldParentId) {
        final ContentValues values = new ContentValues();
        values.put(Bookmarks.TITLE, title);
        values.put(Bookmarks.URL, uri);
        values.put(Bookmarks.KEYWORD, keyword);
        values.put(Bookmarks.PARENT, newParentId);
        values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

        final Uri contentUri = mBookmarksUriWithProfile.buildUpon()
                                .appendQueryParameter(BrowserContract.PARAM_OLD_BOOKMARK_PARENT,
                                                      String.valueOf(oldParentId))
                                .build();
        cr.update(contentUri,
                  values,
                  Bookmarks._ID + " = ?",
                  new String[] { String.valueOf(id) });
    }

    @Override
    public boolean hasBookmarkWithGuid(ContentResolver cr, String guid) {
        Cursor c = cr.query(bookmarksUriWithLimit(1),
                new String[] { Bookmarks.GUID },
                Bookmarks.GUID + " = ?",
                new String[] { guid },
                null);

        try {
            return c != null && c.getCount() > 0;
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    /**
     * Get the favicon from the database, if any, associated with the given favicon URL. (That is,
     * the URL of the actual favicon image, not the URL of the page with which the favicon is associated.)
     * @param cr The ContentResolver to use.
     * @param faviconURL The URL of the favicon to fetch from the database.
     * @return The decoded Bitmap from the database, if any. null if none is stored.
     */
    @Override
    public LoadFaviconResult getFaviconForUrl(Context context, ContentResolver cr, String faviconURL) {
        final Cursor c = cr.query(mFaviconsUriWithProfile,
                                  new String[] { Favicons.DATA },
                                  Favicons.URL + " = ? AND " + Favicons.DATA + " IS NOT NULL",
                                  new String[] { faviconURL },
                                  null);

        boolean shouldDelete = false;
        byte[] b = null;
        try {
            if (!c.moveToFirst()) {
                return null;
            }

            final int faviconIndex = c.getColumnIndexOrThrow(Favicons.DATA);
            try {
                b = c.getBlob(faviconIndex);
            } catch (IllegalStateException e) {
                // This happens when the blob is more than 1MB: Bug 1106347.
                // Delete that row.
                shouldDelete = true;
            }
        } finally {
            c.close();
        }

        if (shouldDelete) {
            try {
                Log.d(LOGTAG, "Deleting invalid favicon.");
                cr.delete(mFaviconsUriWithProfile,
                          Favicons.URL + " = ?",
                          new String[] { faviconURL });
            } catch (Exception e) {
                // Do nothing.
            }
        }

        if (b == null) {
            return null;
        }

        return FaviconDecoder.decodeFavicon(context, b);
    }

    /**
     * Try to find a usable favicon URL in the history or bookmarks table.
     */
    @Override
    public String getFaviconURLFromPageURL(ContentResolver cr, String uri) {
        // Check first in the history table.
        Cursor c = cr.query(mHistoryUriWithProfile,
                            new String[] { History.FAVICON_URL },
                            Combined.URL + " = ?",
                            new String[] { uri },
                            null);

        try {
            if (c.moveToFirst()) {
                // Interrupted page loads can leave History items without a valid favicon_id.
                final int columnIndex = c.getColumnIndexOrThrow(History.FAVICON_URL);
                if (!c.isNull(columnIndex)) {
                    final String faviconURL = c.getString(columnIndex);
                    if (faviconURL != null) {
                        return faviconURL;
                    }
                }
            }
        } finally {
            c.close();
        }

        // If that fails, check in the bookmarks table.
        c = cr.query(mBookmarksUriWithProfile,
                     new String[] { Bookmarks.FAVICON_URL },
                     Bookmarks.URL + " = ?",
                     new String[] { uri },
                     null);

        try {
            if (c.moveToFirst()) {
                return c.getString(c.getColumnIndexOrThrow(Bookmarks.FAVICON_URL));
            }

            return null;
        } finally {
            c.close();
        }
    }

    @Override
    public boolean hideSuggestedSite(String url) {
        if (mSuggestedSites == null) {
            return false;
        }

        return mSuggestedSites.hideSite(url);
    }

    @Override
    public void updateThumbnailForUrl(ContentResolver cr, String uri,
            BitmapDrawable thumbnail) {
        // If a null thumbnail was passed in, delete the stored thumbnail for this url.
        if (thumbnail == null) {
            cr.delete(mThumbnailsUriWithProfile, Thumbnails.URL + " == ?", new String[] { uri });
            return;
        }

        Bitmap bitmap = thumbnail.getBitmap();

        byte[] data = null;
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        if (bitmap.compress(Bitmap.CompressFormat.PNG, 0, stream)) {
            data = stream.toByteArray();
        } else {
            Log.w(LOGTAG, "Favicon compression failed.");
        }

        ContentValues values = new ContentValues();
        values.put(Thumbnails.URL, uri);
        values.put(Thumbnails.DATA, data);

        Uri thumbnailsUri = mThumbnailsUriWithProfile.buildUpon().
                appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();
        cr.update(thumbnailsUri,
                  values,
                  Thumbnails.URL + " = ?",
                  new String[] { uri });
    }

    @Override
    @RobocopTarget
    public byte[] getThumbnailForUrl(ContentResolver cr, String uri) {
        final Cursor c = cr.query(mThumbnailsUriWithProfile,
                                  new String[]{ Thumbnails.DATA },
                                  Thumbnails.URL + " = ? AND " + Thumbnails.DATA + " IS NOT NULL",
                                  new String[]{ uri },
                                  null);
        try {
            if (!c.moveToFirst()) {
                return null;
            }

            int thumbnailIndex = c.getColumnIndexOrThrow(Thumbnails.DATA);

            return c.getBlob(thumbnailIndex);
        } finally {
            c.close();
        }

    }

    /**
     * Query for non-null thumbnails matching the provided <code>urls</code>.
     * The returned cursor will have no more than, but possibly fewer than,
     * the requested number of thumbnails.
     *
     * Returns null if the provided list of URLs is empty or null.
     */
    @Override
    @Nullable
    public Cursor getThumbnailsForUrls(ContentResolver cr, List<String> urls) {
        final int urlCount = urls.size();
        if (urlCount == 0) {
            return null;
        }

        // Don't match against null thumbnails.
        final String selection = Thumbnails.DATA + " IS NOT NULL AND " +
                           DBUtils.computeSQLInClause(urlCount, Thumbnails.URL);
        final String[] selectionArgs = urls.toArray(new String[urlCount]);

        return cr.query(mThumbnailsUriWithProfile,
                        new String[] { Thumbnails.URL, Thumbnails.DATA },
                        selection,
                        selectionArgs,
                        null);
    }

    @Override
    @RobocopTarget
    public void removeThumbnails(ContentResolver cr) {
        cr.delete(mThumbnailsUriWithProfile, null, null);
    }

    /**
     * Utility method used by AndroidImport for updating existing history record using batch operations.
     *
     * @param cr <code>ContentResolver</code> used for querying information about existing history records.
     * @param operations Collection of operations for queueing record updates.
     * @param url URL used for querying history records to update.
     * @param title Optional new title.
     * @param date  New last visited date. Will be used if newer than current last visited date.
     * @param visits Will increment existing visit counts by this number.
     */
    @Override
    public void updateHistoryInBatch(@NonNull ContentResolver cr,
                                     @NonNull Collection<ContentProviderOperation> operations,
                                     @NonNull String url, @Nullable String title,
                                     long date, int visits) {
        final String[] projection = {
            History._ID,
            History.VISITS,
            History.LOCAL_VISITS,
            History.DATE_LAST_VISITED,
            History.LOCAL_DATE_LAST_VISITED
        };

        // We need to get the old visit and date aggregates.
        final Cursor cursor = cr.query(withDeleted(mHistoryUriWithProfile),
                                       projection,
                                       History.URL + " = ?",
                                       new String[] { url },
                                       null);
        if (cursor == null) {
            Log.w(LOGTAG, "Null cursor while querying for old visit and date aggregates");
            return;
        }

        try {
            final ContentValues values = new ContentValues();

            // Restore deleted record if possible
            values.put(History.IS_DELETED, 0);

            if (cursor.moveToFirst()) {
                final int visitsCol = cursor.getColumnIndexOrThrow(History.VISITS);
                final int localVisitsCol = cursor.getColumnIndexOrThrow(History.LOCAL_VISITS);
                final int dateCol = cursor.getColumnIndexOrThrow(History.DATE_LAST_VISITED);
                final int localDateCol = cursor.getColumnIndexOrThrow(History.LOCAL_DATE_LAST_VISITED);

                final int oldVisits = cursor.getInt(visitsCol);
                final int oldLocalVisits = cursor.getInt(localVisitsCol);
                final long oldDate = cursor.getLong(dateCol);
                final long oldLocalDate = cursor.getLong(localDateCol);

                // NB: This will increment visit counts even if subsequent "insert visits" operations
                // insert no new visits (see insertVisitsFromImportHistoryInBatch).
                // So, we're doing a wrong thing here if user imports history more than once.
                // See Bug 1277330.
                values.put(History.VISITS, oldVisits + visits);
                values.put(History.LOCAL_VISITS, oldLocalVisits + visits);
                // Only update last visited if newer.
                if (date > oldDate) {
                    values.put(History.DATE_LAST_VISITED, date);
                }
                if (date > oldLocalDate) {
                    values.put(History.LOCAL_DATE_LAST_VISITED, date);
                }
            } else {
                values.put(History.VISITS, visits);
                values.put(History.LOCAL_VISITS, visits);
                values.put(History.DATE_LAST_VISITED, date);
                values.put(History.LOCAL_DATE_LAST_VISITED, date);
            }
            if (title != null) {
                values.put(History.TITLE, title);
            }
            values.put(History.URL, url);

            final Uri historyUri = withDeleted(mHistoryUriWithProfile).buildUpon().
                appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();

            // Update or insert
            final ContentProviderOperation.Builder builder =
                ContentProviderOperation.newUpdate(historyUri);
            builder.withSelection(History.URL + " = ?", new String[] { url });
            builder.withValues(values);

            // Queue the operation
            operations.add(builder.build());
        } finally {
            cursor.close();
        }
    }

    /**
     * Utility method used by AndroidImport to insert visit data for history records that were just imported.
     * Uses batch operations.
     *
     * @param cr <code>ContentResolver</code> used to query history table and bulkInsert visit records
     * @param operations Collection of operations for queueing inserts
     * @param visitsToSynthesize List of ContentValues describing visit information for each history record:
     *                                  (History URL, LAST DATE VISITED, VISIT COUNT)
     */
    public void insertVisitsFromImportHistoryInBatch(ContentResolver cr,
                                                     Collection<ContentProviderOperation> operations,
                                                     ArrayList<ContentValues> visitsToSynthesize) {
        // If for any reason we fail to obtain history GUID for a tuple we're processing,
        // let's just ignore it. It's possible that the "best-effort" history import
        // did not fully succeed, so we could be missing some of the records.
        int historyGUIDCol = -1;
        for (ContentValues visitsInformation : visitsToSynthesize) {
            final Cursor cursor = cr.query(mHistoryUriWithProfile,
                    new String[] {History.GUID},
                    History.URL + " = ?",
                    new String[] {visitsInformation.getAsString(HISTORY_VISITS_URL)},
                    null);
            if (cursor == null) {
                continue;
            }

            final String historyGUID;

            try {
                if (!cursor.moveToFirst()) {
                    continue;
                }
                if (historyGUIDCol == -1) {
                    historyGUIDCol = cursor.getColumnIndexOrThrow(History.GUID);
                }

                historyGUID = cursor.getString(historyGUIDCol);
            } finally {
                // We "continue" on a null cursor above, so it's safe to act upon it without checking.
                cursor.close();
            }
            if (historyGUID == null) {
                continue;
            }

            // This fakes the individual visit records, using last visited date as the starting point.
            for (int i = 0; i < visitsInformation.getAsInteger(HISTORY_VISITS_COUNT); i++) {
                // We rely on database defaults for IS_LOCAL and VISIT_TYPE.
                final ContentValues visitToInsert = new ContentValues();
                visitToInsert.put(BrowserContract.Visits.HISTORY_GUID, historyGUID);

                // Visit timestamps are stored in microseconds, while Android Browser visit timestmaps
                // are in milliseconds. This is the conversion point for imports.
                visitToInsert.put(BrowserContract.Visits.DATE_VISITED,
                        (visitsInformation.getAsLong(HISTORY_VISITS_DATE) - i) * 1000);

                final ContentProviderOperation.Builder builder =
                        ContentProviderOperation.newInsert(BrowserContract.Visits.CONTENT_URI);
                builder.withValues(visitToInsert);

                // Queue the insert operation
                operations.add(builder.build());
            }
        }
    }

    @Override
    public void updateBookmarkInBatch(ContentResolver cr,
                                      Collection<ContentProviderOperation> operations,
                                      String url, String title, String guid,
                                      long parent, long added,
                                      long modified, long position,
                                      String keyword, int type) {
        ContentValues values = new ContentValues();
        if (title == null && url != null) {
            title = url;
        }
        if (title != null) {
            values.put(Bookmarks.TITLE, title);
        }
        if (url != null) {
            values.put(Bookmarks.URL, url);
        }
        if (guid != null) {
            values.put(SyncColumns.GUID, guid);
        }
        if (keyword != null) {
            values.put(Bookmarks.KEYWORD, keyword);
        }
        if (added > 0) {
            values.put(SyncColumns.DATE_CREATED, added);
        }
        if (modified > 0) {
            values.put(SyncColumns.DATE_MODIFIED, modified);
        }
        values.put(Bookmarks.POSITION, position);
        // Restore deleted record if possible
        values.put(Bookmarks.IS_DELETED, 0);

        // This assumes no "real" folder has a negative ID. Only
        // things like the reading list folder do.
        if (parent < 0) {
            parent = getFolderIdFromGuid(cr, Bookmarks.MOBILE_FOLDER_GUID);
        }
        values.put(Bookmarks.PARENT, parent);
        values.put(Bookmarks.TYPE, type);

        Uri bookmarkUri = withDeleted(mBookmarksUriWithProfile).buildUpon().
            appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();
        // Update or insert
        ContentProviderOperation.Builder builder =
            ContentProviderOperation.newUpdate(bookmarkUri);
        if (url != null) {
            // Bookmarks are defined by their URL and Folder.
            builder.withSelection(Bookmarks.URL + " = ? AND "
                                  + Bookmarks.PARENT + " = ?",
                                  new String[] { url,
                                                 Long.toString(parent)
                                  });
        } else if (title != null) {
            // Or their title and parent folder. (Folders!)
            builder.withSelection(Bookmarks.TITLE + " = ? AND "
                                  + Bookmarks.PARENT + " = ?",
                                  new String[]{ title,
                                                Long.toString(parent)
                                  });
        } else if (type == Bookmarks.TYPE_SEPARATOR) {
            // Or their their position (separators)
            builder.withSelection(Bookmarks.POSITION + " = ? AND "
                                  + Bookmarks.PARENT + " = ?",
                                  new String[] { Long.toString(position),
                                                 Long.toString(parent)
                                  });
        } else {
            Log.e(LOGTAG, "Bookmark entry without url or title and not a separator, not added.");
        }
        builder.withValues(values);

        // Queue the operation
        operations.add(builder.build());
    }

    @Override
    public void pinSite(ContentResolver cr, String url, String title, int position) {
        ContentValues values = new ContentValues();
        final long now = System.currentTimeMillis();
        values.put(Bookmarks.TITLE, title);
        values.put(Bookmarks.URL, url);
        values.put(Bookmarks.PARENT, Bookmarks.FIXED_PINNED_LIST_ID);
        values.put(Bookmarks.DATE_MODIFIED, now);
        values.put(Bookmarks.POSITION, position);
        values.put(Bookmarks.IS_DELETED, 0);

        // We do an update-and-replace here without deleting any existing pins for the given URL.
        // That means if the user pins a URL, then edits another thumbnail to use the same URL,
        // we'll end up with two pins for that site. This is the intended behavior, which
        // incidentally saves us a delete query.
        Uri uri = mBookmarksUriWithProfile.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();
        cr.update(uri,
                  values,
                  Bookmarks.POSITION + " = ? AND " +
                  Bookmarks.PARENT + " = ?",
                  new String[] { Integer.toString(position),
                                 String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID) });
    }

    @Override
    public void unpinSite(ContentResolver cr, int position) {
        cr.delete(mBookmarksUriWithProfile,
                  Bookmarks.PARENT + " == ? AND " + Bookmarks.POSITION + " = ?",
                  new String[] {
                      String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID),
                      Integer.toString(position)
                  });
    }

    @Override
    public void pinSiteForAS(ContentResolver cr, String url, String title) {
        ContentValues values = new ContentValues();
        final long now = System.currentTimeMillis();
        values.put(Bookmarks.TITLE, title);
        values.put(Bookmarks.URL, url);
        values.put(Bookmarks.PARENT, Bookmarks.FIXED_PINNED_LIST_ID);
        values.put(Bookmarks.DATE_MODIFIED, now);
        values.put(Bookmarks.POSITION, Bookmarks.FIXED_AS_PIN_POSITION);
        values.put(Bookmarks.IS_DELETED, 0);

        cr.insert(mBookmarksUriWithProfile, values);
    }

    @Override
    public void unpinSiteForAS(ContentResolver cr, String url) {
        cr.delete(mBookmarksUriWithProfile,
                Bookmarks.PARENT + " == ? AND " +
                Bookmarks.URL + " = ?",
                new String[] {
                        String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID),
                        url
                });
    }

    @Override
    public boolean isPinnedForAS(ContentResolver cr, String url) {
        final Cursor c = cr.query(bookmarksUriWithLimit(1),
                new String[] { Bookmarks._ID },
                Bookmarks.URL + " = ? AND " + Bookmarks.PARENT + " = ?",
                new String[] {
                        url,
                        String.valueOf(Bookmarks.FIXED_PINNED_LIST_ID),
                }, null);

        if (c == null) {
            throw new IllegalStateException("Null cursor in isPinnedByUrl");
        }

        try {
            return c.getCount() > 0;
        } finally {
            c.close();
        }
    }

    @Override
    @RobocopTarget
    @Nullable
    public Cursor getBookmarkForUrl(ContentResolver cr, String url) {
        Cursor c = cr.query(bookmarksUriWithLimit(1),
                            new String[] { Bookmarks._ID,
                                           Bookmarks.URL,
                                           Bookmarks.TITLE,
                                           Bookmarks.TYPE,
                                           Bookmarks.PARENT,
                                           Bookmarks.GUID,
                                           Bookmarks.KEYWORD },
                            Bookmarks.URL + " = ?",
                            new String[] { url },
                            null);

        if (c != null && c.getCount() == 0) {
            c.close();
            c = null;
        }

        return c;
    }

    @Override
    @Nullable
    public Cursor getBookmarkById(ContentResolver cr, long id) {
        final Cursor c = cr.query(mBookmarksUriWithProfile,
                                  new String[] { Bookmarks._ID,
                                                 Bookmarks.URL,
                                                 Bookmarks.TITLE,
                                                 Bookmarks.TYPE,
                                                 Bookmarks.PARENT,
                                                 Bookmarks.GUID,
                                                 Bookmarks.KEYWORD },
                                  Bookmarks._ID + " = ?",
                                  new String[] { String.valueOf(id) },
                                  null);

        if (c != null && c.getCount() == 0) {
            c.close();
            return null;
        }

        return c;
    }

    @Override
    @Nullable
    public Cursor getBookmarkByGuid(ContentResolver cr, String guid) {
        final Cursor c = cr.query(mBookmarksUriWithProfile,
                                  DEFAULT_BOOKMARK_COLUMNS,
                                  Bookmarks.GUID + " = ?",
                                  new String[] { guid },
                                  null);

        if (c != null && c.getCount() == 0) {
            c.close();
            return null;
        }

        return c;
    }

    @Override
    @Nullable
    public Cursor getAllBookmarkFolders(ContentResolver cr) {
        final Cursor cursor = cr.query(mBookmarksUriWithProfile,
                                       DEFAULT_BOOKMARK_COLUMNS,
                                       Bookmarks.TYPE + " = ? AND " +
                                       Bookmarks.GUID + " NOT IN (?, ?, ?, ?, ?) AND " +
                                       Bookmarks.IS_DELETED + " = 0",
                                       new String[] { String.valueOf(Bookmarks.TYPE_FOLDER),
                                                      Bookmarks.SCREENSHOT_FOLDER_GUID,
                                                      Bookmarks.FAKE_READINGLIST_SMARTFOLDER_GUID,
                                                      Bookmarks.TAGS_FOLDER_GUID,
                                                      Bookmarks.PLACES_FOLDER_GUID,
                                                      Bookmarks.PINNED_FOLDER_GUID },
                                       null);
        if (desktopBookmarksExist(cr)) {
            final Cursor desktopCursor = getSpecialFolderCursor(Bookmarks.FAKE_DESKTOP_FOLDER_ID,
                                                                Bookmarks.FAKE_DESKTOP_FOLDER_GUID);
            return new MergeCursor(new Cursor[] { cursor, desktopCursor });
        }
        return cursor;
    }

    @Override
    @Nullable
    public Cursor getBookmarksForPartialUrl(ContentResolver cr, String partialUrl) {
        Cursor c = cr.query(mBookmarksUriWithProfile,
                new String[] { Bookmarks.GUID, Bookmarks._ID, Bookmarks.URL },
                Bookmarks.URL + " LIKE '%" + partialUrl + "%'", // TODO: Escaping!
                null,
                null);

        if (c != null && c.getCount() == 0) {
            c.close();
            c = null;
        }

        return c;
    }

    @Override
    public void setSuggestedSites(SuggestedSites suggestedSites) {
        mSuggestedSites = suggestedSites;
    }

    @Override
    public SuggestedSites getSuggestedSites() {
        return mSuggestedSites;
    }

    @Override
    public boolean hasSuggestedImageUrl(String url) {
        if (mSuggestedSites == null) {
            return false;
        }
        return mSuggestedSites.contains(url);
    }

    @Override
    public String getSuggestedImageUrlForUrl(String url) {
        if (mSuggestedSites == null) {
            return null;
        }
        return mSuggestedSites.getImageUrlForUrl(url);
    }

    @Override
    public int getSuggestedBackgroundColorForUrl(String url) {
        if (mSuggestedSites == null) {
            return 0;
        }
        final String bgColor = mSuggestedSites.getBackgroundColorForUrl(url);
        if (bgColor != null) {
            return Color.parseColor(bgColor);
        }

        return 0;
    }

    private static void appendUrlsFromCursor(List<String> urls, Cursor c) {
        if (!c.moveToFirst()) {
            return;
        }

        do {
            String url = c.getString(c.getColumnIndex(History.URL));

            // Do a simpler check before decoding to avoid parsing
            // all URLs unnecessarily.
            if (StringUtils.isUserEnteredUrl(url)) {
                url = StringUtils.decodeUserEnteredUrl(url);
            }

            urls.add(url);
        } while (c.moveToNext());
    }


    /**
     * Internal CursorLoader that extends the framework CursorLoader in order to measure
     * performance for telemetry purposes.
     */
    private static final class TelemetrisedCursorLoader extends CursorLoader {
        final String mHistogramName;

        public TelemetrisedCursorLoader(Context context, Uri uri, String[] projection, String selection,
                                        String[] selectionArgs, String sortOrder,
                                        final String histogramName) {
            super(context, uri, projection, selection, selectionArgs, sortOrder);
            mHistogramName = histogramName;
        }

        @Override
        public Cursor loadInBackground() {
            final long start = SystemClock.uptimeMillis();

            final Cursor cursor = super.loadInBackground();

            final long end = SystemClock.uptimeMillis();
            final long took = end - start;

            Telemetry.addToHistogram(mHistogramName, (int) Math.min(took, Integer.MAX_VALUE));
            return cursor;
        }
    }

    public CursorLoader getActivityStreamTopSites(Context context, int suggestedRangeLimit, int limit) {
        final Uri uri = mTopSitesUriWithProfile.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                        String.valueOf(limit))
                .appendQueryParameter(BrowserContract.PARAM_SUGGESTEDSITES_LIMIT,
                        String.valueOf(suggestedRangeLimit))
                .appendQueryParameter(BrowserContract.PARAM_NON_POSITIONED_PINS,
                        String.valueOf(true))
                .appendQueryParameter(BrowserContract.PARAM_TOPSITES_EXCLUDE_REMOTE_ONLY,
                        String.valueOf(true))
                .build();

        return new TelemetrisedCursorLoader(context,
                uri,
                new String[]{ Combined._ID,
                        Combined.URL,
                        Combined.TITLE,
                        Combined.BOOKMARK_ID,
                        Combined.HISTORY_ID },
                null,
                null,
                null,
                TELEMETRY_HISTOGRAM_ACTIVITY_STREAM_TOPSITES);
    }

    @Override
    public Cursor getTopSites(ContentResolver cr, int suggestedRangeLimit, int limit) {
        final Uri uri = mTopSitesUriWithProfile.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_LIMIT,
                        String.valueOf(limit))
                .appendQueryParameter(BrowserContract.PARAM_SUGGESTEDSITES_LIMIT,
                        String.valueOf(suggestedRangeLimit))
                .build();

        Cursor topSitesCursor = cr.query(uri,
                                         new String[] { Combined._ID,
                                                 Combined.URL,
                                                 Combined.TITLE,
                                                 Combined.BOOKMARK_ID,
                                                 Combined.HISTORY_ID },
                                         null,
                                         null,
                                         null);

        // It's possible that we will retrieve fewer sites than are required to fill the top-sites panel - in this case
        // we need to add "blank" tiles. It's much easier to add these here (as opposed to SQL), since we don't care
        // about their ordering (they go after all the other sites), but we do care about their number (and calculating
        // that inside out topsites SQL query would be difficult given the other processing we're already doing there).
        final int blanksRequired = suggestedRangeLimit - topSitesCursor.getCount();

        if (blanksRequired <= 0) {
            return topSitesCursor;
        }

        MatrixCursor blanksCursor = new MatrixCursor(new String[] {
                TopSites._ID,
                TopSites.BOOKMARK_ID,
                TopSites.HISTORY_ID,
                TopSites.URL,
                TopSites.TITLE,
                TopSites.TYPE});

        final MatrixCursor.RowBuilder rb = blanksCursor.newRow();
        rb.add(-1);
        rb.add(-1);
        rb.add(-1);
        rb.add("");
        rb.add("");
        rb.add(TopSites.TYPE_BLANK);

        return new MergeCursor(new Cursor[] {topSitesCursor, blanksCursor});
    }

    @Override
    @Nullable
    public Cursor getHighlightCandidates(ContentResolver contentResolver, int limit) {
        final Uri uri = mHighlightCandidatesUriWithProfile.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(limit))
                .build();

        return contentResolver.query(uri, null, null, null, null);
    }

    @Override
    public void blockActivityStreamSite(ContentResolver cr, String url) {
        final ContentValues values = new ContentValues();
        values.put(ActivityStreamBlocklist.URL, url);
        cr.insert(mActivityStreamBlockedUriWithProfile, values);
    }

}
