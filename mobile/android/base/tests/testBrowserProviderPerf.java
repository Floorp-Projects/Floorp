package org.mozilla.gecko.tests;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.SystemClock;
import java.util.UUID;
import java.util.Random;

import org.mozilla.gecko.db.BrowserDB;

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

    private Uri mBookmarksUri;
    private Uri mHistoryUri;
    private Uri mFaviconsUri;

    private String mBookmarksIdCol;
    private String mBookmarksTitleCol;
    private String mBookmarksUrlCol;
    private String mBookmarksParentCol;
    private String mBookmarksTypeCol;
    private String mBookmarksPositionCol;
    private String mBookmarksTagsCol;
    private String mBookmarksDescriptionCol;
    private String mBookmarksKeywordCol;
    private String mBookmarksGuidCol;
    private int mBookmarksTypeBookmark;

    private String mHistoryTitleCol;
    private String mHistoryUrlCol;
    private String mHistoryVisitsCol;
    private String mHistoryLastVisitedCol;

    private String mFaviconsUrlCol;
    private String mFaviconsPageUrlCol;
    private String mFaviconsDataCol;

    @Override
    protected int getTestType() {
        return TEST_TALOS;
    }

    private void loadContractInfo() throws Exception {
        mBookmarksUri = getContentUri("Bookmarks");
        mHistoryUri = getContentUri("History");
        mFaviconsUri = getContentUri("Favicons");

        mBookmarksIdCol = getStringColumn("Bookmarks", "_ID");
        mBookmarksTitleCol = getStringColumn("Bookmarks", "TITLE");
        mBookmarksUrlCol = getStringColumn("Bookmarks", "URL");
        mBookmarksParentCol = getStringColumn("Bookmarks", "PARENT");
        mBookmarksTypeCol = getStringColumn("Bookmarks", "TYPE");
        mBookmarksPositionCol = getStringColumn("Bookmarks", "POSITION");
        mBookmarksTagsCol = getStringColumn("Bookmarks", "TAGS");
        mBookmarksDescriptionCol = getStringColumn("Bookmarks", "DESCRIPTION");
        mBookmarksKeywordCol= getStringColumn("Bookmarks", "KEYWORD");
        mBookmarksGuidCol= getStringColumn("Bookmarks", "GUID");
        mBookmarksTypeBookmark = getIntColumn("Bookmarks", "TYPE_BOOKMARK");

        mHistoryTitleCol = getStringColumn("History", "TITLE");
        mHistoryUrlCol = getStringColumn("History", "URL");
        mHistoryVisitsCol = getStringColumn("History", "VISITS");
        mHistoryLastVisitedCol = getStringColumn("History", "DATE_LAST_VISITED");

        mFaviconsUrlCol = getStringColumn("Favicons", "URL");
        mFaviconsPageUrlCol = getStringColumn("Favicons", "PAGE_URL");
        mFaviconsDataCol = getStringColumn("Favicons", "DATA");
    }

    private void loadMobileFolderId() throws Exception {
        Cursor c = mProvider.query(mBookmarksUri, null,
                                   mBookmarksGuidCol + " = ?",
                                   new String[] { MOBILE_FOLDER_GUID },
                                   null);
        c.moveToFirst();
        mMobileFolderId = c.getLong(c.getColumnIndex(mBookmarksIdCol));

        c.close();
    }

    private ContentValues createBookmarkEntry(String title, String url, long parentId,
            int type, int position, String tags, String description, String keyword) throws Exception {
        ContentValues bookmark = new ContentValues();

        bookmark.put(mBookmarksTitleCol, title);
        bookmark.put(mBookmarksUrlCol, url);
        bookmark.put(mBookmarksParentCol, parentId);
        bookmark.put(mBookmarksTypeCol, type);
        bookmark.put(mBookmarksPositionCol, position);
        bookmark.put(mBookmarksTagsCol, tags);
        bookmark.put(mBookmarksDescriptionCol, description);
        bookmark.put(mBookmarksKeywordCol, keyword);

        return bookmark;
    }

    private ContentValues createBookmarkEntryWithUrl(String url) throws Exception {
        return createBookmarkEntry(url, url, mMobileFolderId,
            mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
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

        historyEntry.put(mHistoryTitleCol, title);
        historyEntry.put(mHistoryUrlCol, url);
        historyEntry.put(mHistoryVisitsCol, visits);
        historyEntry.put(mHistoryLastVisitedCol, lastVisited);

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

        faviconEntry.put(mFaviconsUrlCol, url + "/favicon.ico");
        faviconEntry.put(mFaviconsPageUrlCol, url);
        faviconEntry.put(mFaviconsDataCol, url.getBytes("UTF8"));

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

            mProvider.bulkInsert(mBookmarksUri, bookmarkEntries);
        }

        // Create some random history entries
        ContentValues[] historyEntries = new ContentValues[BATCH_SIZE];
        ContentValues[] faviconEntries = new ContentValues[BATCH_SIZE];

        for (int i = 0; i < NUMBER_OF_BASIC_HISTORY_URLS / BATCH_SIZE; i++) {
            historyEntries = new ContentValues[BATCH_SIZE];
            faviconEntries = new ContentValues[BATCH_SIZE];

            for (int j = 0; j < BATCH_SIZE; j++) {
                historyEntries[j] = createRandomHistoryEntry();
                faviconEntries[j] = createFaviconEntryWithUrl(historyEntries[j].getAsString(mHistoryUrlCol));
            }

            mProvider.bulkInsert(mHistoryUri, historyEntries);
            mProvider.bulkInsert(mFaviconsUri, faviconEntries);
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

            mProvider.bulkInsert(mBookmarksUri, bookmarkEntries);
            mProvider.bulkInsert(mHistoryUri, historyEntries);
            mProvider.bulkInsert(mFaviconsUri, faviconEntries);
        }

        // Create some history entries with a known prefix
        historyEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        faviconEntries = new ContentValues[NUMBER_OF_KNOWN_URLS];
        for (int i = 0; i < NUMBER_OF_KNOWN_URLS; i++) {
            historyEntries[i] = createRandomHistoryEntry(KNOWN_PREFIX);
            faviconEntries[i] = createFaviconEntryWithUrl(historyEntries[i].getAsString(mHistoryUrlCol));
        }

        mProvider.bulkInsert(mHistoryUri, historyEntries);
        mProvider.bulkInsert(mFaviconsUri, faviconEntries);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp("org.mozilla.gecko.db.BrowserProvider", "AUTHORITY");

        mGenerator = new Random(19580427);

        loadContractInfo();
    }

    public void testBrowserProviderPerf() throws Exception {
        BrowserDB.initialize("default");

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
