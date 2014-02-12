package org.mozilla.gecko.tests;

import android.content.ContentValues;
import android.database.Cursor;
import android.os.SystemClock;

import java.util.UUID;
import java.util.Random;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserProvider;

/*
 * This test is meant to exercise the performance of Fennec's
 * history and bookmarks content provider.
 */
public class testBrowserProviderPerf extends ContentProviderTest {
    private final int NUMBER_OF_BASIC_HISTORY_URLS = 10000;
    private final int NUMBER_OF_BASIC_BOOKMARK_URLS = 500;
    private final int NUMBER_OF_COMBINED_URLS = 500;

    private final int NUMBER_OF_KNOWN_URLS = 200;
    private final int BATCH_SIZE = 500;

    // Include spaces in prefix to test performance querying with
    // multiple constraint words
    private final String KNOWN_PREFIX = "my mozilla test ";

    private Random mGenerator;

    private final String MOBILE_FOLDER_GUID = "mobile";
    private long mMobileFolderId;

    @Override
    protected int getTestType() {
        return TEST_TALOS;
    }

    private void loadMobileFolderId() throws Exception {
        Cursor c = mProvider.query(BrowserContract.Bookmarks.CONTENT_URI, null,
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
        // Create some random bookmark entries
        ContentValues[] bookmarkEntries = new ContentValues[BATCH_SIZE];

        for (int i = 0; i < NUMBER_OF_BASIC_BOOKMARK_URLS / BATCH_SIZE; i++) {
            bookmarkEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                bookmarkEntries[j] = createRandomBookmarkEntry();
            }

            mProvider.bulkInsert(BrowserContract.Bookmarks.CONTENT_URI, bookmarkEntries);
        }

        // Create some random history entries
        ContentValues[] historyEntries = new ContentValues[BATCH_SIZE];
        ContentValues[] faviconEntries = new ContentValues[BATCH_SIZE];

        for (int i = 0; i < NUMBER_OF_BASIC_HISTORY_URLS / BATCH_SIZE; i++) {
            historyEntries = new ContentValues[BATCH_SIZE];
            faviconEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                historyEntries[j] = createRandomHistoryEntry();
                faviconEntries[j] = createFaviconEntryWithUrl(historyEntries[j].getAsString(BrowserContract.History.URL));
            }

            mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, historyEntries);
            mProvider.bulkInsert(BrowserContract.Favicons.CONTENT_URI, faviconEntries);
        }


        // Create random bookmark/history entries with the same url
        for (int i = 0; i < NUMBER_OF_COMBINED_URLS / BATCH_SIZE; i++) {
            bookmarkEntries = new ContentValues[BATCH_SIZE];
            historyEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                String url = createRandomUrl("");
                bookmarkEntries[j] = createBookmarkEntryWithUrl(url);
                historyEntries[j] = createHistoryEntryWithUrl(url);
                faviconEntries[j] = createFaviconEntryWithUrl(url);
            }

            mProvider.bulkInsert(BrowserContract.Bookmarks.CONTENT_URI, bookmarkEntries);
            mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, historyEntries);
            mProvider.bulkInsert(BrowserContract.Favicons.CONTENT_URI, faviconEntries);
        }

        // Create some history entries with a known prefix
        historyEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        faviconEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        for (int i = 0; i < NUMBER_OF_KNOWN_URLS; i++) {
            historyEntries[i] = createRandomHistoryEntry(KNOWN_PREFIX);
            faviconEntries[i] = createFaviconEntryWithUrl(historyEntries[i].getAsString(BrowserContract.History.URL));
        }

        mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, historyEntries);
        mProvider.bulkInsert(BrowserContract.Favicons.CONTENT_URI, faviconEntries);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp(sBrowserProviderCallable, BrowserContract.AUTHORITY, "browser.db");

        mGenerator = new Random(19580427);
    }

    public void testBrowserProviderPerf() throws Exception {
        BrowserDB.initialize(GeckoProfile.DEFAULT_PROFILE);

        loadMobileFolderId();
        addTonsOfUrls();

        long start = SystemClock.uptimeMillis();

        final Cursor c = BrowserDB.filter(mResolver, KNOWN_PREFIX, 100);
        c.getCount(); // ensure query is not lazy loaded

        long end = SystemClock.uptimeMillis();

        mAsserter.dumpLog("__start_report" + Long.toString(end - start) + "__end_report");
        mAsserter.dumpLog("__startTimestamp" + Long.toString(end - start) + "__endTimestamp");

        c.close();
    }

}
