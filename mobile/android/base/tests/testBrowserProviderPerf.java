/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.util.Random;
import java.util.UUID;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.util.FileUtils;

import android.app.Activity;
import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;
import android.os.SystemClock;
import android.util.Log;

/**
 * This test is meant to exercise the performance of Fennec's history and
 * bookmarks content provider.
 *
 * It does not extend ContentProviderTest because that class is unable to
 * accurately assess the performance of the ContentProvider -- it's a second
 * instance of a class that's only supposed to exist once, wrapped in a bunch of
 * junk.
 *
 * Instead, we directly use the existing ContentProvider, accessing a new
 * profile directory that we initialize via BrowserDB.
 */
@SuppressWarnings("unchecked")
public class testBrowserProviderPerf extends BaseRobocopTest {
    private static final String LAUNCH_ACTIVITY_FULL_CLASSNAME = TestConstants.ANDROID_PACKAGE_NAME + ".App";

    private static Class<Activity> mLauncherActivityClass;

    private final int NUMBER_OF_BASIC_HISTORY_URLS = 10000;
    private final int NUMBER_OF_BASIC_BOOKMARK_URLS = 500;
    private final int NUMBER_OF_COMBINED_URLS = 500;

    private final int NUMBER_OF_KNOWN_URLS = 200;
    private final int BATCH_SIZE = 500;

    // Include spaces in prefix to test performance querying with
    // multiple constraint words.
    private final String KNOWN_PREFIX = "my mozilla test ";

    private Random mGenerator;

    private final String MOBILE_FOLDER_GUID = "mobile";
    private long mMobileFolderId;
    private ContentResolver mResolver;
    private String mProfile;
    private Uri mHistoryURI;
    private Uri mBookmarksURI;
    private Uri mFaviconsURI;

    static {
        try {
            mLauncherActivityClass = (Class<Activity>) Class.forName(LAUNCH_ACTIVITY_FULL_CLASSNAME);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public testBrowserProviderPerf() {
        super(TARGET_PACKAGE_ID, mLauncherActivityClass);
    }

    @Override
    protected int getTestType() {
        return TEST_TALOS;
    }

    private void loadMobileFolderId() throws Exception {
        Cursor c = mResolver.query(mBookmarksURI, null,
                                   BrowserContract.Bookmarks.GUID + " = ?",
                                   new String[] { MOBILE_FOLDER_GUID },
                                   null);
        c.moveToFirst();
        mMobileFolderId = c.getLong(c.getColumnIndex(BrowserContract.Bookmarks._ID));

        c.close();
    }

    private ContentValues createBookmarkEntry(String title, String url, long parentId,
            int type, int position, String tags, String description, String keyword) throws Exception {
        ContentValues bookmark = new ContentValues();

        bookmark.put(BrowserContract.Bookmarks.TITLE, title);
        bookmark.put(BrowserContract.Bookmarks.URL, url);
        bookmark.put(BrowserContract.Bookmarks.PARENT, parentId);
        bookmark.put(BrowserContract.Bookmarks.TYPE, type);
        bookmark.put(BrowserContract.Bookmarks.POSITION, position);
        bookmark.put(BrowserContract.Bookmarks.TAGS, tags);
        bookmark.put(BrowserContract.Bookmarks.DESCRIPTION, description);
        bookmark.put(BrowserContract.Bookmarks.KEYWORD, keyword);

        return bookmark;
    }

    private ContentValues createBookmarkEntryWithUrl(String url) throws Exception {
        return createBookmarkEntry(url, url, mMobileFolderId,
            BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
    }

    private ContentValues createRandomBookmarkEntry() throws Exception {
        return createRandomBookmarkEntry("");
    }

    private ContentValues createRandomBookmarkEntry(String knownPrefix) throws Exception {
        String randomStr = createRandomUrl(knownPrefix);
        return createBookmarkEntryWithUrl(randomStr);
    }

    private ContentValues createHistoryEntry(String title, String url, int visits,
            long lastVisited) throws Exception {
        ContentValues historyEntry = new ContentValues();

        historyEntry.put(BrowserContract.History.TITLE, title);
        historyEntry.put(BrowserContract.History.URL, url);
        historyEntry.put(BrowserContract.History.VISITS, visits);
        historyEntry.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);

        return historyEntry;
    }

    private ContentValues createHistoryEntryWithUrl(String url) throws Exception {
        int visits = mGenerator.nextInt(500);
        return createHistoryEntry(url, url, visits,
            System.currentTimeMillis());
    }

    private ContentValues createRandomHistoryEntry() throws Exception {
        return createRandomHistoryEntry("");
    }

    private ContentValues createRandomHistoryEntry(String knownPrefix) throws Exception {
        String randomStr = createRandomUrl(knownPrefix);
        return createHistoryEntryWithUrl(randomStr);
    }

    private ContentValues createFaviconEntryWithUrl(String url) throws Exception {
        ContentValues faviconEntry = new ContentValues();

        faviconEntry.put(BrowserContract.Favicons.URL, url + "/favicon.ico");
        faviconEntry.put(BrowserContract.Favicons.PAGE_URL, url);
        faviconEntry.put(BrowserContract.Favicons.DATA, url.getBytes("UTF8"));

        return faviconEntry;
    }

    private String createRandomUrl(String knownPrefix) throws Exception {
        return knownPrefix + UUID.randomUUID().toString();
    }

    private void addTonsOfUrls() throws Exception {
        // Create some random bookmark entries.
        ContentValues[] bookmarkEntries = new ContentValues[BATCH_SIZE];

        for (int i = 0; i < NUMBER_OF_BASIC_BOOKMARK_URLS / BATCH_SIZE; i++) {
            bookmarkEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                bookmarkEntries[j] = createRandomBookmarkEntry();
            }

            mResolver.bulkInsert(mBookmarksURI, bookmarkEntries);
        }

        // Create some random history entries.
        ContentValues[] historyEntries = new ContentValues[BATCH_SIZE];
        ContentValues[] faviconEntries = new ContentValues[BATCH_SIZE];

        for (int i = 0; i < NUMBER_OF_BASIC_HISTORY_URLS / BATCH_SIZE; i++) {
            historyEntries = new ContentValues[BATCH_SIZE];
            faviconEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                historyEntries[j] = createRandomHistoryEntry();
                faviconEntries[j] = createFaviconEntryWithUrl(historyEntries[j].getAsString(BrowserContract.History.URL));
            }

            mResolver.bulkInsert(mHistoryURI, historyEntries);
            mResolver.bulkInsert(mFaviconsURI, faviconEntries);
        }


        // Create random bookmark/history entries with the same URL.
        for (int i = 0; i < NUMBER_OF_COMBINED_URLS / BATCH_SIZE; i++) {
            bookmarkEntries = new ContentValues[BATCH_SIZE];
            historyEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                String url = createRandomUrl("");
                bookmarkEntries[j] = createBookmarkEntryWithUrl(url);
                historyEntries[j] = createHistoryEntryWithUrl(url);
                faviconEntries[j] = createFaviconEntryWithUrl(url);
            }

            mResolver.bulkInsert(mBookmarksURI, bookmarkEntries);
            mResolver.bulkInsert(mHistoryURI, historyEntries);
            mResolver.bulkInsert(mFaviconsURI, faviconEntries);
        }

        // Create some history entries with a known prefix.
        historyEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        faviconEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        for (int i = 0; i < NUMBER_OF_KNOWN_URLS; i++) {
            historyEntries[i] = createRandomHistoryEntry(KNOWN_PREFIX);
            faviconEntries[i] = createFaviconEntryWithUrl(historyEntries[i].getAsString(BrowserContract.History.URL));
        }

        mResolver.bulkInsert(mHistoryURI, historyEntries);
        mResolver.bulkInsert(mFaviconsURI, faviconEntries);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mProfile = "prof" + System.currentTimeMillis();

        mHistoryURI = prepUri(BrowserContract.History.CONTENT_URI);
        mBookmarksURI = prepUri(BrowserContract.Bookmarks.CONTENT_URI);
        mFaviconsURI = prepUri(BrowserContract.Favicons.CONTENT_URI);

        mResolver = getActivity().getApplicationContext().getContentResolver();

        mGenerator = new Random(19580427);
    }

    @Override
    public void tearDown() {
        final ContentProviderClient client = mResolver.acquireContentProviderClient(mBookmarksURI);
        try {
            final ContentProvider cp = client.getLocalContentProvider();
            final BrowserProvider bp = ((BrowserProvider) cp);

            // This will be the DB we were just testing.
            final SQLiteDatabase db = bp.getWritableDatabaseForTesting(mBookmarksURI);
            try {
                db.close();
            } catch (Throwable e) {
                // Nothing we can do.
            }
        } finally {
            try {
                client.release();
            } catch (Throwable e) {
                // Still go ahead and try to delete the profile.
            }

            try {
                FileUtils.delTree(new File(mProfile), null, true);
            } catch (Exception e) {
                Log.w("GeckoTest", "Unable to delete profile " + mProfile, e);
            }
        }
    }

    public Uri prepUri(Uri uri) {
        return uri.buildUpon()
                  .appendQueryParameter(BrowserContract.PARAM_PROFILE, mProfile)
                  .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1")       // So we don't trigger a sync.
                  .build();
    }

    /**
     * This method:
     *
     * * Adds a bunch of test data via the ContentProvider API.
     * * Runs a single query against that test data via BrowserDB.
     * * Reports timing for Talos.
     */
    public void testBrowserProviderQueryPerf() throws Exception {
        // We add at least this many results.
        final int limit = 100;

        // Make sure we're querying the right profile.
        final LocalBrowserDB db = new LocalBrowserDB(mProfile);

        final Cursor before = db.filter(mResolver, KNOWN_PREFIX, limit);
        try {
            mAsserter.is(before.getCount(), 0, "Starts empty");
        } finally {
            before.close();
        }

        // Add data.
        loadMobileFolderId();
        addTonsOfUrls();

        // Wait for a little while after inserting data. We do this because
        // this test launches about:home, and Top Sites watches for DB changes.
        // We don't have a good way for it to only watch changes related to
        // its current profile, nor is it convenient for us to launch a different
        // activity that doesn't depend on the DB.
        // We can fix this by:
        // * Adjusting the provider interface to allow a "don't notify" param.
        // * Adjusting the interface schema to include the profile in the path,
        //   and only observe the correct path.
        // * Launching a different activity.
        Thread.sleep(5000);

        // Time the query.
        final long start = SystemClock.uptimeMillis();
        final Cursor c = db.filter(mResolver, KNOWN_PREFIX, limit);

        try {
            final int count = c.getCount();
            final long end = SystemClock.uptimeMillis();

            mAsserter.is(count, limit, "Retrieved results");
            mAsserter.dumpLog("Results: " + count);
            mAsserter.dumpLog("__start_report" + Long.toString(end - start) + "__end_report");
            mAsserter.dumpLog("__startTimestamp" + Long.toString(end - start) + "__endTimestamp");
        } finally {
            c.close();
        }
    }
}
