package org.mozilla.gecko.tests;

import android.content.ContentValues;
import android.content.ContentUris;
import android.content.ContentProviderResult;
import android.content.ContentProviderOperation;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import java.util.ArrayList;
import java.util.Random;

/*
 * This test is meant to exercise all operations exposed by Fennec's
 * history and bookmarks content provider. It does so in an isolated
 * environment (see ContentProviderTest) without affecting any UI-related
 * code.
 */
public class testBrowserProvider extends ContentProviderTest {
    private String PLACES_FOLDER_GUID;
    private String MOBILE_FOLDER_GUID;
    private String MENU_FOLDER_GUID;
    private String TAGS_FOLDER_GUID;
    private String TOOLBAR_FOLDER_GUID;
    private String UNFILED_FOLDER_GUID;
    private String READING_LIST_FOLDER_GUID;

    private Uri mBookmarksUri;
    private Uri mBookmarksPositionUri;
    private Uri mHistoryUri;
    private Uri mHistoryOldUri;
    private Uri mCombinedUri;
    private Uri mFaviconsUri;
    private Uri mThumbnailsUri;

    private String mBookmarksIdCol;
    private String mBookmarksTitleCol;
    private String mBookmarksUrlCol;
    private String mBookmarksFaviconCol;
    private String mBookmarksParentCol;
    private String mBookmarksTypeCol;
    private String mBookmarksPositionCol;
    private String mBookmarksTagsCol;
    private String mBookmarksDescriptionCol;
    private String mBookmarksKeywordCol;
    private String mBookmarksGuidCol;
    private String mBookmarksIsDeletedCol;
    private String mBookmarksDateCreatedCol;
    private String mBookmarksDateModifiedCol;
    private int mBookmarksTypeFolder;
    private int mBookmarksTypeBookmark;
    private long mMobileFolderId;

    private String mHistoryIdCol;
    private String mHistoryTitleCol;
    private String mHistoryUrlCol;
    private String mHistoryVisitsCol;
    private String mHistoryFaviconCol;
    private String mHistoryLastVisitedCol;
    private String mHistoryGuidCol;
    private String mHistoryIsDeletedCol;
    private String mHistoryDateCreatedCol;
    private String mHistoryDateModifiedCol;

    private String mCombinedIdCol;
    private String mCombinedBookmarkIdCol;
    private String mCombinedHistoryIdCol;
    private String mCombinedDisplayCol;
    private String mCombinedUrlCol;
    private String mCombinedTitleCol;
    private String mCombinedVisitsCol;
    private String mCombinedLastVisitedCol;
    private String mCombinedFaviconCol;
    private int mCombinedDisplayNormal;
    private int mCombinedDisplayReader;

    private String mFaviconsUrlCol;
    private String mFaviconsPageUrlCol;
    private String mFaviconsDataCol;

    private String mThumbnailsUrlCol;
    private String mThumbnailsDataCol;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    private void loadContractInfo() throws Exception {
        mBookmarksUri = getContentUri("Bookmarks");
        mHistoryUri = getContentUri("History");
        mHistoryOldUri = getUriColumn("History", "CONTENT_OLD_URI");
        mCombinedUri = getContentUri("Combined");
        mFaviconsUri = getContentUri("Favicons");
        mThumbnailsUri = getContentUri("Thumbnails");

        mBookmarksPositionUri = getUriColumn("Bookmarks", "POSITIONS_CONTENT_URI");

        PLACES_FOLDER_GUID = getStringColumn("Bookmarks", "PLACES_FOLDER_GUID");
        MOBILE_FOLDER_GUID = getStringColumn("Bookmarks", "MOBILE_FOLDER_GUID");
        MENU_FOLDER_GUID = getStringColumn("Bookmarks", "MENU_FOLDER_GUID");
        TAGS_FOLDER_GUID = getStringColumn("Bookmarks", "TAGS_FOLDER_GUID");
        TOOLBAR_FOLDER_GUID = getStringColumn("Bookmarks", "TOOLBAR_FOLDER_GUID");
        UNFILED_FOLDER_GUID = getStringColumn("Bookmarks", "UNFILED_FOLDER_GUID");
        READING_LIST_FOLDER_GUID = getStringColumn("Bookmarks", "READING_LIST_FOLDER_GUID");

        mBookmarksIdCol = getStringColumn("Bookmarks", "_ID");
        mBookmarksTitleCol = getStringColumn("Bookmarks", "TITLE");
        mBookmarksUrlCol = getStringColumn("Bookmarks", "URL");
        mBookmarksFaviconCol = getStringColumn("Bookmarks", "FAVICON");
        mBookmarksParentCol = getStringColumn("Bookmarks", "PARENT");
        mBookmarksTypeCol = getStringColumn("Bookmarks", "TYPE");
        mBookmarksPositionCol = getStringColumn("Bookmarks", "POSITION");
        mBookmarksTagsCol = getStringColumn("Bookmarks", "TAGS");
        mBookmarksDescriptionCol = getStringColumn("Bookmarks", "DESCRIPTION");
        mBookmarksKeywordCol= getStringColumn("Bookmarks", "KEYWORD");
        mBookmarksGuidCol = getStringColumn("Bookmarks", "GUID");
        mBookmarksIsDeletedCol = getStringColumn("Bookmarks", "IS_DELETED");
        mBookmarksDateCreatedCol = getStringColumn("Bookmarks", "DATE_CREATED");
        mBookmarksDateModifiedCol = getStringColumn("Bookmarks", "DATE_MODIFIED");

        mBookmarksTypeFolder = getIntColumn("Bookmarks", "TYPE_FOLDER");
        mBookmarksTypeBookmark = getIntColumn("Bookmarks", "TYPE_BOOKMARK");

        mHistoryIdCol = getStringColumn("History", "_ID");
        mHistoryTitleCol = getStringColumn("History", "TITLE");
        mHistoryUrlCol = getStringColumn("History", "URL");
        mHistoryVisitsCol = getStringColumn("History", "VISITS");
        mHistoryLastVisitedCol = getStringColumn("History", "DATE_LAST_VISITED");
        mHistoryFaviconCol = getStringColumn("History", "FAVICON");
        mHistoryGuidCol = getStringColumn("History", "GUID");
        mHistoryIsDeletedCol = getStringColumn("History", "IS_DELETED");
        mHistoryDateCreatedCol = getStringColumn("History", "DATE_CREATED");
        mHistoryDateModifiedCol = getStringColumn("History", "DATE_MODIFIED");

        mCombinedIdCol = getStringColumn("Combined", "_ID");
        mCombinedBookmarkIdCol = getStringColumn("Combined", "BOOKMARK_ID");
        mCombinedHistoryIdCol = getStringColumn("Combined", "HISTORY_ID");
        mCombinedDisplayCol = getStringColumn("Combined", "DISPLAY");
        mCombinedUrlCol = getStringColumn("Combined", "URL");
        mCombinedTitleCol = getStringColumn("Combined", "TITLE");
        mCombinedVisitsCol = getStringColumn("Combined", "VISITS");
        mCombinedLastVisitedCol = getStringColumn("Combined", "DATE_LAST_VISITED");
        mCombinedFaviconCol = getStringColumn("Combined", "FAVICON");

        mCombinedDisplayNormal = getIntColumn("Combined", "DISPLAY_NORMAL");
        mCombinedDisplayReader = getIntColumn("Combined", "DISPLAY_READER");

        mFaviconsUrlCol = getStringColumn("Favicons", "URL");
        mFaviconsPageUrlCol = getStringColumn("Favicons", "PAGE_URL");
        mFaviconsDataCol = getStringColumn("Favicons", "DATA");

        mThumbnailsUrlCol = getStringColumn("Thumbnails", "URL");
        mThumbnailsDataCol = getStringColumn("Thumbnails", "DATA");
    }

    private void loadMobileFolderId() throws Exception {
        Cursor c = getBookmarkByGuid(MOBILE_FOLDER_GUID);
        mAsserter.is(c.moveToFirst(), true, "Mobile bookmarks folder is present");

        mMobileFolderId = c.getLong(c.getColumnIndex(mBookmarksIdCol));
    }

    private void ensureEmptyDatabase() throws Exception {
        Cursor c = null;

        String guid = getStringColumn("Bookmarks", "GUID");

        mProvider.delete(appendUriParam(mBookmarksUri, "PARAM_IS_SYNC", "1"),
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ?",
                         new String[] { PLACES_FOLDER_GUID,
                                        MOBILE_FOLDER_GUID,
                                        MENU_FOLDER_GUID,
                                        TAGS_FOLDER_GUID,
                                        TOOLBAR_FOLDER_GUID,
                                        UNFILED_FOLDER_GUID,
                                        READING_LIST_FOLDER_GUID });

        c = mProvider.query(appendUriParam(mBookmarksUri, "PARAM_SHOW_DELETED", "1"), null, null, null, null);
        mAsserter.is(c.getCount(), 7, "All non-special bookmarks and folders were deleted");

        mProvider.delete(appendUriParam(mHistoryUri, "PARAM_IS_SYNC", "1"), null, null);
        c = mProvider.query(appendUriParam(mHistoryUri, "PARAM_SHOW_DELETED", "1"), null, null, null, null);
        mAsserter.is(c.getCount(), 0, "All history entries were deleted");

        mProvider.delete(mFaviconsUri, null, null);
        mAsserter.is(c.getCount(), 0, "All favicons were deleted");

        mProvider.delete(mThumbnailsUri, null, null);
        mAsserter.is(c.getCount(), 0, "All thumbnails were deleted");
    }

    private ContentValues createBookmark(String title, String url, long parentId,
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

    private ContentValues createOneBookmark() throws Exception {
        return createBookmark("Example", "http://example.com", mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
    }

    private Cursor getBookmarksByParent(long parent) throws Exception {
        // Order by position.
        return mProvider.query(mBookmarksUri, null,
                               mBookmarksParentCol + " = ?",
                               new String[] { String.valueOf(parent) },
                               mBookmarksPositionCol);
    }

    private Cursor getBookmarkByGuid(String guid) throws Exception {
        return mProvider.query(mBookmarksUri, null,
                               mBookmarksGuidCol + " = ?",
                               new String[] { guid },
                               null);
    }

    private Cursor getBookmarkById(long id) throws Exception {
        return getBookmarkById(mBookmarksUri, id, null);
    }

    private Cursor getBookmarkById(long id, String[] projection) throws Exception {
        return getBookmarkById(mBookmarksUri, id, projection);
    }

    private Cursor getBookmarkById(Uri bookmarksUri, long id) throws Exception {
        return getBookmarkById(bookmarksUri, id, null);
    }

    private Cursor getBookmarkById(Uri bookmarksUri, long id, String[] projection) throws Exception {
        return mProvider.query(bookmarksUri, projection,
                               mBookmarksIdCol + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private ContentValues createHistoryEntry(String title, String url, int visits, long lastVisited) throws Exception {
        ContentValues historyEntry = new ContentValues();

        historyEntry.put(mHistoryTitleCol, title);
        historyEntry.put(mHistoryUrlCol, url);
        historyEntry.put(mHistoryVisitsCol, visits);
        historyEntry.put(mHistoryLastVisitedCol, lastVisited);

        return historyEntry;
    }

    private ContentValues createFaviconEntry(String pageUrl, String data) throws Exception {
        ContentValues faviconEntry = new ContentValues();

        faviconEntry.put(mFaviconsPageUrlCol, pageUrl);
        faviconEntry.put(mFaviconsUrlCol, pageUrl + "/favicon.ico");
        faviconEntry.put(mFaviconsDataCol, data.getBytes("UTF8"));

        return faviconEntry;
    }

    private ContentValues createThumbnailEntry(String pageUrl, String data) throws Exception {
        ContentValues thumbnailEntry = new ContentValues();

        thumbnailEntry.put(mThumbnailsUrlCol, pageUrl);
        thumbnailEntry.put(mThumbnailsDataCol, data.getBytes("UTF8"));

        return thumbnailEntry;
    }

    private ContentValues createOneHistoryEntry() throws Exception {
        return createHistoryEntry("Example", "http://example.com", 10, System.currentTimeMillis());
    }

    private Cursor getHistoryEntryById(long id) throws Exception {
        return getHistoryEntryById(mHistoryUri, id, null);
    }

    private Cursor getHistoryEntryById(long id, String[] projection) throws Exception {
        return getHistoryEntryById(mHistoryUri, id, projection);
    }

    private Cursor getHistoryEntryById(Uri historyUri, long id) throws Exception {
        return getHistoryEntryById(historyUri, id, null);
    }

    private Cursor getHistoryEntryById(Uri historyUri, long id, String[] projection) throws Exception {
        return mProvider.query(historyUri, projection,
                               mHistoryIdCol + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private Cursor getFaviconsByUrl(String url) throws Exception {
        return mProvider.query(mCombinedUri, null,
                               mCombinedUrlCol + " = ?",
                               new String[] { url },
                               null);
    }

    private Cursor getThumbnailByUrl(String url) throws Exception {
        return mProvider.query(mThumbnailsUri, null,
                               mThumbnailsUrlCol + " = ?",
                               new String[] { url },
                               null);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp("org.mozilla.gecko.db.BrowserProvider", "AUTHORITY");
        loadContractInfo();

        mTests.add(new TestSpecialFolders());

        mTests.add(new TestInsertBookmarks());
        mTests.add(new TestInsertBookmarksFavicons());
        mTests.add(new TestDeleteBookmarks());
        mTests.add(new TestDeleteBookmarksFavicons());
        mTests.add(new TestUpdateBookmarks());
        mTests.add(new TestUpdateBookmarksFavicons());
        mTests.add(new TestPositionBookmarks());

        mTests.add(new TestInsertHistory());
        mTests.add(new TestInsertHistoryFavicons());
        mTests.add(new TestDeleteHistory());
        mTests.add(new TestDeleteHistoryFavicons());
        mTests.add(new TestUpdateHistory());
        mTests.add(new TestUpdateHistoryFavicons());
        mTests.add(new TestUpdateOrInsertHistory());
        mTests.add(new TestInsertHistoryThumbnails());
        mTests.add(new TestUpdateHistoryThumbnails());
        mTests.add(new TestDeleteHistoryThumbnails());

        mTests.add(new TestBatchOperations());

        mTests.add(new TestCombinedView());
        mTests.add(new TestCombinedViewDisplay());
        mTests.add(new TestCombinedViewWithDeletedBookmark());
        mTests.add(new TestCombinedViewWithDeletedReadingListItem());
        mTests.add(new TestExpireHistory());

        mTests.add(new TestBrowserProviderNotifications());
    }

    public void testBrowserProvider() throws Exception {
        loadMobileFolderId();

        for (int i = 0; i < mTests.size(); i++) {
            Runnable test = mTests.get(i);

            setTestName(test.getClass().getSimpleName());
            ensureEmptyDatabase();
            test.run();
        }
    }

    abstract class Test implements Runnable {
        @Override
        public void run() {
            try {
                test();
            } catch (Exception e) {
                mAsserter.is(true, false, "Test " + this.getClass().getName() +
                        " threw exception: " + e);
            }
        }

        public abstract void test() throws Exception;
    }

    class TestBatchOperations extends Test {
        static final int TESTCOUNT = 100;

        public void testApplyBatch() throws Exception {
            ArrayList<ContentProviderOperation> mOperations
                = new ArrayList<ContentProviderOperation>();

            // Test a bunch of inserts with applyBatch
            ContentValues values = new ContentValues();
            ContentProviderOperation.Builder builder = null;

            for (int i = 0; i < TESTCOUNT; i++) {
                values.clear();
                values.put(mHistoryVisitsCol, i);
                values.put(mHistoryTitleCol, "Test" + i);
                values.put(mHistoryUrlCol, "http://www.test.org/" + i);

                // Insert
                builder = ContentProviderOperation.newInsert(mHistoryUri);
                builder.withValues(values);
                // Queue the operation
                mOperations.add(builder.build());
            }

            ContentProviderResult[] applyResult =
                mProvider.applyBatch(mOperations);

            boolean allFound = true;
            for (int i = 0; i < TESTCOUNT; i++) {
                Cursor cursor = mProvider.query(mHistoryUri,
                                                null,
                                                mHistoryUrlCol + " = ?",
                                                new String[] { "http://www.test.org/" + i },
                                                null);

                if (!cursor.moveToFirst())
                    allFound = false;
                cursor.close();
            }
            mAsserter.is(allFound, true, "Found all batchApply entries");
            mOperations.clear();

            // Update all visits to 1
            values.clear();
            values.put(mHistoryVisitsCol, 1);
            for (int i = 0; i < TESTCOUNT; i++) {
                builder = ContentProviderOperation.newUpdate(mHistoryUri);
                builder.withSelection(mHistoryUrlCol  + " = ?",
                                      new String[] {"http://www.test.org/" + i});
                builder.withValues(values);
                builder.withExpectedCount(1);
                // Queue the operation
                mOperations.add(builder.build());
            }

            boolean seenException = false;
            try {
                applyResult = mProvider.applyBatch(mOperations);
            } catch (OperationApplicationException ex) {
                seenException = true;
            }
            mAsserter.is(seenException, false, "Batch updating succeded");
            mOperations.clear();

            // Delte all visits
            for (int i = 0; i < TESTCOUNT; i++) {
                builder = ContentProviderOperation.newDelete(mHistoryUri);
                builder.withSelection(mHistoryUrlCol  + " = ?",
                                      new String[] {"http://www.test.org/" + i});
                builder.withExpectedCount(1);
                // Queue the operation
                mOperations.add(builder.build());
            }
            try {
                applyResult = mProvider.applyBatch(mOperations);
            } catch (OperationApplicationException ex) {
                seenException = true;
            }
            mAsserter.is(seenException, false, "Batch deletion succeded");
        }

        // Force a Constraint error, see if later operations still apply correctly
        public void testApplyBatchErrors() throws Exception {
            ArrayList<ContentProviderOperation> mOperations
                = new ArrayList<ContentProviderOperation>();

            // Test a bunch of inserts with applyBatch
            ContentProviderOperation.Builder builder = null;
            ContentValues values = createFaviconEntry("http://www.test.org", "FAVICON");
            builder = ContentProviderOperation.newInsert(mFaviconsUri);
            builder.withValues(values);
            mOperations.add(builder.build());

            // Make a duplicate, this will fail because of a UNIQUE constraint
            builder = ContentProviderOperation.newInsert(mFaviconsUri);
            builder.withValues(values);
            mOperations.add(builder.build());

            // This is valid and should be in the table afterwards
            values.put(mFaviconsUrlCol, "http://www.test.org/valid.ico");
            builder = ContentProviderOperation.newInsert(mFaviconsUri);
            builder.withValues(values);
            mOperations.add(builder.build());

            boolean seenException = false;

            try {
                ContentProviderResult[] applyResult =
                    mProvider.applyBatch(mOperations);
            } catch (OperationApplicationException ex) {
                seenException = true;
            }

            // This test may need to go away if Bug 717428 is fixed.
            mAsserter.is(seenException, true, "Expected failure in favicons table");

            boolean allFound = true;
            Cursor cursor = mProvider.query(mFaviconsUri,
                                            null,
                                            mFaviconsUrlCol + " = ?",
                                            new String[] { "http://www.test.org/valid.ico" },
                                            null);

            if (!cursor.moveToFirst())
                allFound = false;
            cursor.close();

            mAsserter.is(allFound, true, "Found all applyBatch (with error) entries");
        }

        public void testBulkInsert() throws Exception {
            // Test a bunch of inserts with bulkInsert
            ContentValues allVals[] = new ContentValues[TESTCOUNT];
            for (int i = 0; i < TESTCOUNT; i++) {
                allVals[i] = new ContentValues();
                allVals[i].put(mHistoryUrlCol, i);
                allVals[i].put(mHistoryTitleCol, "Test" + i);
                allVals[i].put(mHistoryUrlCol, "http://www.test.org/" + i);
            }

            int inserts = mProvider.bulkInsert(mHistoryUri, allVals);
            mAsserter.is(inserts, TESTCOUNT, "Excepted number of inserts matches");

            boolean allFound = true;
            for (int i = 0; i < TESTCOUNT; i++) {
                Cursor cursor = mProvider.query(mHistoryUri,
                                                null,
                                                mHistoryUrlCol + " = ?",
                                                new String[] { "http://www.test.org/" + i },
                                                null);

                if (!cursor.moveToFirst())
                    allFound = false;
                cursor.close();
            }
            mAsserter.is(allFound, true, "Found all bulkInsert entries");
        }

        @Override
        public void test() throws Exception {
            testApplyBatch();
            // Clean up
            ensureEmptyDatabase();

            testBulkInsert();
            ensureEmptyDatabase();

            testApplyBatchErrors();
        }
    }

    class TestSpecialFolders extends Test {
        @Override
        public void test() throws Exception {
            Cursor c = mProvider.query(mBookmarksUri,
                                       new String[] { mBookmarksIdCol,
                                                      mBookmarksGuidCol,
                                                      mBookmarksParentCol },
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ? OR " +
                                       mBookmarksGuidCol + " = ?",
                                       new String[] { PLACES_FOLDER_GUID,
                                                      MOBILE_FOLDER_GUID,
                                                      MENU_FOLDER_GUID,
                                                      TAGS_FOLDER_GUID,
                                                      TOOLBAR_FOLDER_GUID,
                                                      UNFILED_FOLDER_GUID,
                                                      READING_LIST_FOLDER_GUID },
                                       null);

            mAsserter.is(c.getCount(), 7, "Right number of special folders");

            int rootId = getIntColumn("Bookmarks", "FIXED_ROOT_ID");
            int readingListId = getIntColumn("Bookmarks", "FIXED_READING_LIST_ID");

            while (c.moveToNext()) {
                int id = c.getInt(c.getColumnIndex(mBookmarksIdCol));
                String guid = c.getString(c.getColumnIndex(mBookmarksGuidCol));
                int parentId = c.getInt(c.getColumnIndex(mBookmarksParentCol));

                if (guid.equals(PLACES_FOLDER_GUID)) {
                    mAsserter.is(new Integer(id), new Integer(rootId), "The id of places folder is correct");
                } else if (guid.equals(READING_LIST_FOLDER_GUID)) {
                    mAsserter.is(new Integer(id), new Integer(readingListId), "The id of reading list folder is correct");
                }

                mAsserter.is(new Integer(parentId), new Integer(rootId),
                             "The PARENT of the " + guid + " special folder is correct");
            }
        }
    }

    class TestInsertBookmarks extends Test {
        private long insertWithNullCol(String colName) throws Exception {
            ContentValues b = createOneBookmark();
            b.putNull(colName);
            long id = -1;

            try {
                id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
            } catch (Exception e) {}

            return id;
        }

        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
            Cursor c = getBookmarkById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTitleCol)), b.getAsString(mBookmarksTitleCol),
                         "Inserted bookmark has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksUrlCol)), b.getAsString(mBookmarksUrlCol),
                         "Inserted bookmark has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTagsCol)), b.getAsString(mBookmarksTagsCol),
                         "Inserted bookmark has correct tags");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksKeywordCol)), b.getAsString(mBookmarksKeywordCol),
                         "Inserted bookmark has correct keyword");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksDescriptionCol)), b.getAsString(mBookmarksDescriptionCol),
                         "Inserted bookmark has correct description");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksPositionCol)), b.getAsString(mBookmarksPositionCol),
                         "Inserted bookmark has correct position");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTypeCol)), b.getAsString(mBookmarksTypeCol),
                         "Inserted bookmark has correct type");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksParentCol)), b.getAsString(mBookmarksParentCol),
                         "Inserted bookmark has correct parent ID");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksIsDeletedCol)), String.valueOf(0),
                         "Inserted bookmark has correct is-deleted state");

            id = insertWithNullCol(mBookmarksPositionCol);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert bookmark with null position");

            id = insertWithNullCol(mBookmarksTypeCol);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert bookmark with null type");

            if (Build.VERSION.SDK_INT >= 8 &&
                Build.VERSION.SDK_INT < 16) {
                b = createOneBookmark();
                b.put(mBookmarksParentCol, -1);
                id = -1;

                try {
                    id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
                } catch (Exception e) {}

                mAsserter.is(new Long(id), new Long(-1),
                             "Should not be able to insert bookmark with invalid parent");
            }

            b = createOneBookmark();
            b.remove(mBookmarksTypeCol);
            id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
            c = getBookmarkById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTypeCol)), String.valueOf(mBookmarksTypeBookmark),
                         "Inserted bookmark has correct default type");
        }
    }

    class TestInsertBookmarksFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String favicon = "FAVICON";
            final String pageUrl = b.getAsString(mBookmarksUrlCol);

            long id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));

            // Insert the favicon into the favicons table
            mProvider.insert(mFaviconsUri, createFaviconEntry(pageUrl, favicon));

            Cursor c = getBookmarkById(id, new String[] { mBookmarksFaviconCol });

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mBookmarksFaviconCol)), "UTF8"),
                         favicon, "Inserted bookmark has corresponding favicon image");

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");
        }
    }

    class TestDeleteBookmarks extends Test {
        private long insertOneBookmark() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));

            Cursor c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            return id;
        }

        @Override
        public void test() throws Exception {
            long id = insertOneBookmark();

            int deleted = mProvider.delete(mBookmarksUri,
                                           mBookmarksIdCol + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted bookmark was deleted");

            Cursor c = getBookmarkById(appendUriParam(mBookmarksUri, "PARAM_SHOW_DELETED", "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Deleted bookmark was only marked as deleted");

            deleted = mProvider.delete(appendUriParam(mBookmarksUri, "PARAM_IS_SYNC", "1"),
                                       mBookmarksIdCol + " = ?",
                                       new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted bookmark was deleted");

            c = getBookmarkById(appendUriParam(mBookmarksUri, "PARAM_SHOW_DELETED", "1"), id);
            mAsserter.is(c.moveToFirst(), false, "Inserted bookmark is now actually deleted");

            id = insertOneBookmark();

            deleted = mProvider.delete(ContentUris.withAppendedId(mBookmarksUri, id), null, null);
            mAsserter.is((deleted == 1), true,
                         "Inserted bookmark was deleted using URI with id");

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), false,
                         "Inserted bookmark can't be found after deletion using URI with ID");

            if (Build.VERSION.SDK_INT >= 8 &&
                Build.VERSION.SDK_INT < 16) {
                ContentValues b = createBookmark("Folder", null, mMobileFolderId,
                        mBookmarksTypeFolder, 0, "folderTags", "folderDescription", "folderKeyword");

                long parentId = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
                c = getBookmarkById(parentId);
                mAsserter.is(c.moveToFirst(), true, "Inserted bookmarks folder found");

                b = createBookmark("Example", "http://example.com", parentId,
                        mBookmarksTypeBookmark, 0, "tags", "description", "keyword");

                id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));
                c = getBookmarkById(id);
                mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

                deleted = 0;
                try {
                    Uri uri = ContentUris.withAppendedId(mBookmarksUri, parentId);
                    deleted = mProvider.delete(appendUriParam(uri, "PARAM_IS_SYNC", "1"), null, null);
                } catch(Exception e) {}

                mAsserter.is((deleted == 0), true,
                             "Should not be able to delete folder that causes orphan bookmarks");
            }
        }
    }

    class TestDeleteBookmarksFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String pageUrl = b.getAsString(mBookmarksUrlCol);
            long id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));

            // Insert the favicon into the favicons table
            mProvider.insert(mFaviconsUri, createFaviconEntry(pageUrl, "FAVICON"));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mProvider.delete(ContentUris.withAppendedId(mBookmarksUri, id), null, null);

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Favicon is deleted with last reference to it");
        }
    }

    class TestUpdateBookmarks extends Test {
        private int updateWithNullCol(long id, String colName) throws Exception {
            ContentValues u = new ContentValues();
            u.putNull(colName);

            int updated = 0;

            try {
                updated = mProvider.update(mBookmarksUri, u,
                                           mBookmarksIdCol + " = ?",
                                           new String[] { String.valueOf(id) });
            } catch (Exception e) {}

            return updated;
        }

        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(mBookmarksUri, b));

            Cursor c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            long dateCreated = c.getLong(c.getColumnIndex(mBookmarksDateCreatedCol));
            long dateModified = c.getLong(c.getColumnIndex(mBookmarksDateModifiedCol));

            ContentValues u = new ContentValues();
            u.put(mBookmarksTitleCol, b.getAsString(mBookmarksTitleCol) + "CHANGED");
            u.put(mBookmarksUrlCol, b.getAsString(mBookmarksUrlCol) + "/more/stuff");
            u.put(mBookmarksTagsCol, b.getAsString(mBookmarksTagsCol) + "CHANGED");
            u.put(mBookmarksDescriptionCol, b.getAsString(mBookmarksDescriptionCol) + "CHANGED");
            u.put(mBookmarksKeywordCol, b.getAsString(mBookmarksKeywordCol) + "CHANGED");
            u.put(mBookmarksTypeCol, mBookmarksTypeFolder);
            u.put(mBookmarksPositionCol, 10);

            int updated = mProvider.update(mBookmarksUri, u,
                                           mBookmarksIdCol + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((updated == 1), true, "Inserted bookmark was updated");

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTitleCol)), u.getAsString(mBookmarksTitleCol),
                         "Inserted bookmark has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksUrlCol)), u.getAsString(mBookmarksUrlCol),
                         "Inserted bookmark has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTagsCol)), u.getAsString(mBookmarksTagsCol),
                         "Inserted bookmark has correct tags");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksKeywordCol)), u.getAsString(mBookmarksKeywordCol),
                         "Inserted bookmark has correct keyword");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksDescriptionCol)), u.getAsString(mBookmarksDescriptionCol),
                         "Inserted bookmark has correct description");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksPositionCol)), u.getAsString(mBookmarksPositionCol),
                         "Inserted bookmark has correct position");
            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksTypeCol)), u.getAsString(mBookmarksTypeCol),
                         "Inserted bookmark has correct type");

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mBookmarksDateCreatedCol))),
                         new Long(dateCreated),
                         "Updated bookmark has same creation date");

            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(mBookmarksDateModifiedCol))),
                            new Long(dateModified),
                            "Updated bookmark has new modification date");

            updated = updateWithNullCol(id, mBookmarksPositionCol);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update bookmark with null position");

            updated = updateWithNullCol(id, mBookmarksTypeCol);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update bookmark with null type");

            u = new ContentValues();
            u.put(mBookmarksUrlCol, "http://examples2.com");

            updated = mProvider.update(ContentUris.withAppendedId(mBookmarksUri, id), u, null, null);

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(mBookmarksUrlCol)), u.getAsString(mBookmarksUrlCol),
                         "Updated bookmark has correct URL using URI with id");
        }
    }

    class TestUpdateBookmarksFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String favicon = "FAVICON";
            final String newFavicon = "NEW_FAVICON";
            final String pageUrl = b.getAsString(mBookmarksUrlCol);

            mProvider.insert(mBookmarksUri, b);

            // Insert the favicon into the favicons table
            ContentValues f = createFaviconEntry(pageUrl, favicon);
            long faviconId = ContentUris.parseId(mProvider.insert(mFaviconsUri, f));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");

            ContentValues u = createFaviconEntry(pageUrl, newFavicon);
            mProvider.update(mFaviconsUri, u, null, null);

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         newFavicon, "Updated favicon has corresponding favicon image");
        }
    }

    /**
     * Create a folder of one thousand and one bookmarks, then impose an order
     * on them.
     *
     * Verify that the reordering worked by querying.
     */
    class TestPositionBookmarks extends Test {

        public String makeGUID(final long in) {
            String part = String.valueOf(in);
            return "aaaaaaaaaaaa".substring(0, (12 - part.length())) + part;
        }

        public void compareCursorToItems(final Cursor c, final String[] items, final int count) {
            mAsserter.is(c.moveToFirst(), true, "Folder has children.");

            int posColumn = c.getColumnIndex(mBookmarksPositionCol);
            int guidColumn = c.getColumnIndex(mBookmarksGuidCol);
            int i = 0;

            while (!c.isAfterLast()) {
                String guid = c.getString(guidColumn);
                long pos = c.getLong(posColumn);
                if ((pos != i) || (guid == null) || (!guid.equals(items[i]))) {
                    mAsserter.is(pos, (long) i, "Position matches sequence.");
                    mAsserter.is(guid, items[i], "GUID matches sequence.");
                }
                ++i;
                c.moveToNext();
            }

            mAsserter.is(i, count, "Folder has the right number of children.");
        }

        public static final int NUMBER_OF_CHILDREN = 1001;
        @Override
        public void test() throws Exception {
            // Create the containing folder.
            ContentValues folder = createBookmark("FolderFolder", "", mMobileFolderId,
                                                  mBookmarksTypeFolder, 0, "",
                                                  "description", "keyword");
            folder.put(mBookmarksGuidCol, "folderfolder");
            long folderId = ContentUris.parseId(mProvider.insert(mBookmarksUri, folder));

            // Create the children.
            String[] items = new String[NUMBER_OF_CHILDREN];

            // Reuse the same ContentValues.
            ContentValues item = createBookmark("Test Bookmark", "http://example.com", folderId,
                                                mBookmarksTypeFolder, 0, "",
                                                "description", "keyword");

            for (int i = 0; i < NUMBER_OF_CHILDREN; ++i) {
                String guid = makeGUID(i);
                items[i] = guid;
                item.put(mBookmarksGuidCol, guid);
                item.put(mBookmarksPositionCol, i);
                item.put(mBookmarksUrlCol, "http://example.com/" + guid);
                item.put(mBookmarksTitleCol, "Test Bookmark " + guid);
                mProvider.insert(mBookmarksUri, item);
            }

            Cursor c;

            // Verify insertion.
            c = getBookmarksByParent(folderId);
            compareCursorToItems(c, items, NUMBER_OF_CHILDREN);
            c.close();

            // Now permute the items array.
            Random rand = new Random();
            for (int i = 0; i < NUMBER_OF_CHILDREN; ++i) {
                final int newPosition = rand.nextInt(NUMBER_OF_CHILDREN);
                final String switched = items[newPosition];
                items[newPosition] = items[i];
                items[i] = switched;
            }

            // Impose the positions.
            long updated = mProvider.update(mBookmarksPositionUri, null, null, items);
            mAsserter.is(updated, (long) NUMBER_OF_CHILDREN, "Updated " + NUMBER_OF_CHILDREN + " positions.");

            // Verify that the database was updated.
            c = getBookmarksByParent(folderId);
            compareCursorToItems(c, items, NUMBER_OF_CHILDREN);
            c.close();
        }
    }

    class TestInsertHistory extends Test {
        private long insertWithNullCol(String colName) throws Exception {
            ContentValues h = createOneHistoryEntry();
            h.putNull(colName);
            long id = -1;

            try {
                id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));
            } catch (Exception e) {}

            return id;
        }

        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));
            Cursor c = getHistoryEntryById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), h.getAsString(mHistoryTitleCol),
                         "Inserted history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryUrlCol)), h.getAsString(mHistoryUrlCol),
                         "Inserted history entry has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryVisitsCol)), h.getAsString(mHistoryVisitsCol),
                         "Inserted history entry has correct number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryLastVisitedCol)), h.getAsString(mHistoryLastVisitedCol),
                         "Inserted history entry has correct last visited date");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryIsDeletedCol)), String.valueOf(0),
                         "Inserted history entry has correct is-deleted state");

            id = insertWithNullCol(mHistoryUrlCol);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert history with null URL");

            id = insertWithNullCol(mHistoryVisitsCol);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert history with null number of visits");
        }
    }

    class TestInsertHistoryFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String favicon = "FAVICON";
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));

            // Insert the favicon into the favicons table
            mProvider.insert(mFaviconsUri, createFaviconEntry(pageUrl, favicon));

            Cursor c = getHistoryEntryById(id, new String[] { mHistoryFaviconCol });

            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mHistoryFaviconCol)), "UTF8"),
                         favicon, "Inserted history entry has corresponding favicon image");

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");
        }
    }

    class TestDeleteHistory extends Test {
        private long insertOneHistoryEntry() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));

            Cursor c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            return id;
        }

        @Override
        public void test() throws Exception {
            long id = insertOneHistoryEntry();

            int deleted = mProvider.delete(mHistoryUri,
                                           mHistoryIdCol + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted history entry was deleted");

            Cursor c = getHistoryEntryById(appendUriParam(mHistoryUri, "PARAM_SHOW_DELETED", "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Deleted history entry was only marked as deleted");

            deleted = mProvider.delete(appendUriParam(mHistoryUri, "PARAM_IS_SYNC", "1"),
                                       mHistoryIdCol + " = ?",
                                       new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted history entry was deleted");

            c = getHistoryEntryById(appendUriParam(mHistoryUri, "PARAM_SHOW_DELETED", "1"), id);
            mAsserter.is(c.moveToFirst(), false, "Inserted history is now actually deleted");

            id = insertOneHistoryEntry();

            deleted = mProvider.delete(ContentUris.withAppendedId(mHistoryUri, id), null, null);
            mAsserter.is((deleted == 1), true,
                         "Inserted history entry was deleted using URI with id");

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), false,
                         "Inserted history entry can't be found after deletion using URI with ID");
        }
    }

    class TestDeleteHistoryFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            // Insert the favicon into the favicons table
            mProvider.insert(mFaviconsUri, createFaviconEntry(pageUrl, "FAVICON"));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mProvider.delete(ContentUris.withAppendedId(mHistoryUri, id), null, null);

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Favicon is deleted with last reference to it");
        }
    }

    class TestUpdateHistory extends Test {
        private int updateWithNullCol(long id, String colName) throws Exception {
            ContentValues u = new ContentValues();
            u.putNull(colName);

            int updated = 0;

            try {
                updated = mProvider.update(mHistoryUri, u,
                                           mHistoryIdCol + " = ?",
                                           new String[] { String.valueOf(id) });
            } catch (Exception e) {}

            return updated;
        }

        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));

            Cursor c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            long dateCreated = c.getLong(c.getColumnIndex(mHistoryDateCreatedCol));
            long dateModified = c.getLong(c.getColumnIndex(mHistoryDateModifiedCol));

            ContentValues u = new ContentValues();
            u.put(mHistoryVisitsCol, h.getAsInteger(mHistoryVisitsCol) + 1);
            u.put(mHistoryLastVisitedCol, System.currentTimeMillis());
            u.put(mHistoryTitleCol, h.getAsString(mHistoryTitleCol) + "CHANGED");
            u.put(mHistoryUrlCol, h.getAsString(mHistoryUrlCol) + "/more/stuff");

            int updated = mProvider.update(mHistoryUri, u,
                                           mHistoryIdCol + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((updated == 1), true, "Inserted history entry was updated");

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), u.getAsString(mHistoryTitleCol),
                         "Updated history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryUrlCol)), u.getAsString(mHistoryUrlCol),
                         "Updated history entry has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryVisitsCol)), u.getAsString(mHistoryVisitsCol),
                         "Updated history entry has correct number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryLastVisitedCol)), u.getAsString(mHistoryLastVisitedCol),
                         "Updated history entry has correct last visited date");

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryDateCreatedCol))),
                         new Long(dateCreated),
                         "Updated history entry has same creation date");

            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(mHistoryDateModifiedCol))),
                            new Long(dateModified),
                            "Updated history entry has new modification date");

            updated = updateWithNullCol(id, mHistoryUrlCol);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update history with null URL");

            updated = updateWithNullCol(id, mHistoryVisitsCol);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update history with null number of visits");

            u = new ContentValues();
            u.put(mHistoryUrlCol, "http://examples2.com");

            updated = mProvider.update(ContentUris.withAppendedId(mHistoryUri, id), u, null, null);

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(mHistoryUrlCol)), u.getAsString(mHistoryUrlCol),
                         "Updated history entry has correct URL using URI with id");
        }
    }

    class TestUpdateHistoryFavicons extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String favicon = "FAVICON";
            final String newFavicon = "NEW_FAVICON";
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            mProvider.insert(mHistoryUri, h);

            // Insert the favicon into the favicons table
            mProvider.insert(mFaviconsUri, createFaviconEntry(pageUrl, favicon));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");

            ContentValues u = createFaviconEntry(pageUrl, newFavicon);

            mProvider.update(mFaviconsUri, u, null, null);

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mCombinedFaviconCol)), "UTF8"),
                         newFavicon, "Updated favicon has corresponding favicon image");
        }
    }

    class TestUpdateOrInsertHistory extends Test {
        private final String TEST_URL_1 = "http://example.com";
        private final String TEST_URL_2 = "http://example.org";
        private final String TEST_TITLE = "Example";

        private long getHistoryEntryIdByUrl(String url) {
            Cursor c = mProvider.query(mHistoryUri,
                                       new String[] { mHistoryIdCol },
                                       mHistoryUrlCol + " = ?",
                                       new String[] { url },
                                       null);
            c.moveToFirst();
            long id = c.getLong(0);
            c.close();

            return id;
        }

        @Override
        public void test() throws Exception {
            Uri updateHistoryUri = mHistoryUri.buildUpon().
                appendQueryParameter("increment_visits", "true").build();
            Uri updateOrInsertHistoryUri = mHistoryUri.buildUpon().
                appendQueryParameter("insert_if_needed", "true").
                appendQueryParameter("increment_visits", "true").build();

            // Update a non-existent history entry, without specifying visits or title
            ContentValues values = new ContentValues();
            values.put(mHistoryUrlCol, TEST_URL_1);

            int updated = mProvider.update(updateHistoryUri, values,
                                           mHistoryUrlCol + " = ?",
                                           new String[] { TEST_URL_1 });
            mAsserter.is((updated == 0), true, "History entry was not updated");
            Cursor c = mProvider.query(mHistoryUri, null, null, null, null);
            mAsserter.is(c.moveToFirst(), false, "History entry was not inserted");
            c.close();

            // Now let's try with update-or-insert.
            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                           mHistoryUrlCol + " = ?",
                                           new String[] { TEST_URL_1 });
            mAsserter.is((updated == 1), true, "History entry was inserted");

            long id = getHistoryEntryIdByUrl(TEST_URL_1);
            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "History entry was inserted");

            long dateCreated = c.getLong(c.getColumnIndex(mHistoryDateCreatedCol));
            long dateModified = c.getLong(c.getColumnIndex(mHistoryDateModifiedCol));

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryVisitsCol))), new Long(1),
                         "Inserted history entry has correct default number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), TEST_URL_1,
                         "Inserted history entry has correct default title");

            // Update the history entry, without specifying an additional visit count
            values = new ContentValues();
            values.put(mHistoryLastVisitedCol, System.currentTimeMillis());
            values.put(mHistoryTitleCol, TEST_TITLE);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                       mHistoryIdCol + " = ?",
                                       new String[] { String.valueOf(id) });
            mAsserter.is((updated == 1), true, "Inserted history entry was updated");

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), TEST_TITLE,
                         "Updated history entry has correct title");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryVisitsCol))), new Long(2),
                         "Updated history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryDateCreatedCol))), new Long(dateCreated),
                         "Updated history entry has same creation date");
            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(mHistoryDateModifiedCol))), new Long(dateModified),
                            "Updated history entry has new modification date");

            // Create a new history entry, specifying visits and history
            values = new ContentValues();
            values.put(mHistoryUrlCol, TEST_URL_2);
            values.put(mHistoryTitleCol, TEST_TITLE);
            values.put(mHistoryVisitsCol, 10);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                           mHistoryUrlCol + " = ?",
                                           new String[] { values.getAsString(mHistoryUrlCol) });
            mAsserter.is((updated == 1), true, "History entry was inserted");

            id = getHistoryEntryIdByUrl(TEST_URL_2);
            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "History entry was inserted");

            dateCreated = c.getLong(c.getColumnIndex(mHistoryDateCreatedCol));
            dateModified = c.getLong(c.getColumnIndex(mHistoryDateModifiedCol));

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryVisitsCol))), new Long(10),
                         "Inserted history entry has correct specified number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), TEST_TITLE,
                         "Inserted history entry has correct specified title");

            // Update the history entry, specifying additional visit count
            values = new ContentValues();
            values.put(mHistoryVisitsCol, 10);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                       mHistoryIdCol + " = ?",
                                       new String[] { String.valueOf(id) });
            mAsserter.is((updated == 1), true, "Inserted history entry was updated");

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(mHistoryTitleCol)), TEST_TITLE,
                         "Updated history entry has correct unchanged title");
            mAsserter.is(c.getString(c.getColumnIndex(mHistoryUrlCol)), TEST_URL_2,
                         "Updated history entry has correct unchanged URL");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryVisitsCol))), new Long(20),
                         "Updated history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mHistoryDateCreatedCol))), new Long(dateCreated),
                         "Updated history entry has same creation date");
            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(mHistoryDateModifiedCol))), new Long(dateModified),
                            "Updated history entry has new modification date");

        }
    }

    class TestInsertHistoryThumbnails extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String thumbnail = "THUMBNAIL";
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(mThumbnailsUri, createThumbnailEntry(pageUrl, thumbnail));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mThumbnailsDataCol)), "UTF8"),
                         thumbnail, "Inserted thumbnail has corresponding thumbnail image");
        }
    }

    class TestUpdateHistoryThumbnails extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String thumbnail = "THUMBNAIL";
            final String newThumbnail = "NEW_THUMBNAIL";
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            mProvider.insert(mHistoryUri, h);

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(mThumbnailsUri, createThumbnailEntry(pageUrl, thumbnail));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mThumbnailsDataCol)), "UTF8"),
                         thumbnail, "Inserted thumbnail has corresponding thumbnail image");

            ContentValues u = createThumbnailEntry(pageUrl, newThumbnail);

            mProvider.update(mThumbnailsUri, u, null, null);

            c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(mThumbnailsDataCol)), "UTF8"),
                         newThumbnail, "Updated thumbnail has corresponding thumbnail image");
        }
    }

    class TestDeleteHistoryThumbnails extends Test {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));
            final String pageUrl = h.getAsString(mHistoryUrlCol);

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(mThumbnailsUri, createThumbnailEntry(pageUrl, "THUMBNAIL"));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mProvider.delete(ContentUris.withAppendedId(mHistoryUri, id), null, null);

            c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Thumbnail is deleted with last reference to it");
        }
    }

    class TestCombinedView extends Test {
        @Override
        public void test() throws Exception {
            final String TITLE_1 = "Test Page 1";
            final String TITLE_2 = "Test Page 2";
            final String TITLE_3_HISTORY = "Test Page 3 (History Entry)";
            final String TITLE_3_BOOKMARK = "Test Page 3 (Bookmark Entry)";
            final String TITLE_3_BOOKMARK2 = "Test Page 3 (Bookmark Entry 2)";

            final String URL_1 = "http://example1.com";
            final String URL_2 = "http://example2.com";
            final String URL_3 = "http://example3.com";

            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a basic history entry
            ContentValues basicHistory = createHistoryEntry(TITLE_1, URL_1, VISITS, LAST_VISITED);
            long basicHistoryId = ContentUris.parseId(mProvider.insert(mHistoryUri, basicHistory));

            // Create a basic bookmark entry
            ContentValues basicBookmark = createBookmark(TITLE_2, URL_2, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long basicBookmarkId = ContentUris.parseId(mProvider.insert(mBookmarksUri, basicBookmark));

            // Create a history entry and bookmark entry with the same URL to
            // represent a visited bookmark
            ContentValues combinedHistory = createHistoryEntry(TITLE_3_HISTORY, URL_3, VISITS, LAST_VISITED);
            long combinedHistoryId = ContentUris.parseId(mProvider.insert(mHistoryUri, combinedHistory));


            ContentValues combinedBookmark = createBookmark(TITLE_3_BOOKMARK, URL_3, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long combinedBookmarkId = ContentUris.parseId(mProvider.insert(mBookmarksUri, combinedBookmark));

            ContentValues combinedBookmark2 = createBookmark(TITLE_3_BOOKMARK2, URL_3, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long combinedBookmarkId2 = ContentUris.parseId(mProvider.insert(mBookmarksUri, combinedBookmark2));

            // Create a bookmark folder to make sure it _doesn't_ show up in the results
            ContentValues folderBookmark = createBookmark("", "", mMobileFolderId,
                mBookmarksTypeFolder, 0, "tags", "description", "keyword");
            mProvider.insert(mBookmarksUri, folderBookmark);

            // Sort entries by url so we can check them individually
            Cursor c = mProvider.query(mCombinedUri, null, "", null, mCombinedUrlCol);

            mAsserter.is(c.getCount(), 3, "3 combined entries found");

            // First combined entry is basic history entry
            mAsserter.is(c.moveToFirst(), true, "Found basic history entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedIdCol))), new Long(0),
                         "Combined _id column should always be 0");
            // TODO: Should we change BrowserProvider to make this return -1, not 0?
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(0),
                         "Bookmark id should be 0 for basic history entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedHistoryIdCol))), new Long(basicHistoryId),
                         "Basic history entry has correct history id");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedTitleCol)), TITLE_1,
                         "Basic history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedUrlCol)), URL_1,
                         "Basic history entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(mCombinedVisitsCol)), VISITS,
                         "Basic history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedLastVisitedCol))), new Long(LAST_VISITED),
                         "Basic history entry has correct last visit time");

            // Second combined entry is basic bookmark entry
            mAsserter.is(c.moveToNext(), true, "Found basic bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedIdCol))), new Long(0),
                         "Combined _id column should always be 0");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(basicBookmarkId),
                         "Basic bookmark entry has correct bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedHistoryIdCol))), new Long(-1),
                         "History id should be -1 for basic bookmark entry");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedTitleCol)), TITLE_2,
                         "Basic bookmark entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedUrlCol)), URL_2,
                         "Basic bookmark entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(mCombinedVisitsCol)), -1,
                         "Visits should be -1 for basic bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedLastVisitedCol))), new Long(-1),
                         "Basic entry has correct last visit time");

            // Third combined entry is a combined history/bookmark entry
            mAsserter.is(c.moveToNext(), true, "Found third combined entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedIdCol))), new Long(0),
                         "Combined _id column should always be 0");
            // The bookmark data (bookmark_id and title) associated with the combined entry is non-deterministic,
            // it might end up with data coming from any of the matching bookmark entries.
            mAsserter.is(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol)) == combinedBookmarkId ||
                         c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol)) == combinedBookmarkId2, true,
                         "Combined entry has correct bookmark id");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedTitleCol)).equals(TITLE_3_BOOKMARK) ||
                         c.getString(c.getColumnIndex(mCombinedTitleCol)).equals(TITLE_3_BOOKMARK2), true,
                         "Combined entry has title corresponding to bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedHistoryIdCol))), new Long(combinedHistoryId),
                         "Combined entry has correct history id");
            mAsserter.is(c.getString(c.getColumnIndex(mCombinedUrlCol)), URL_3,
                         "Combined entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(mCombinedVisitsCol)), VISITS,
                         "Combined entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedLastVisitedCol))), new Long(LAST_VISITED),
                         "Combined entry has correct last visit time");
        }
    }

    class TestCombinedViewDisplay extends Test {
        @Override
        public void test() throws Exception {
            final String TITLE_1 = "Test Page 1";
            final String TITLE_2 = "Test Page 2";
            final String TITLE_3_HISTORY = "Test Page 3 (History Entry)";
            final String TITLE_3_BOOKMARK = "Test Page 3 (Bookmark Entry)";
            final String TITLE_4 = "Test Page 4";

            final String URL_1 = "http://example.com";
            final String URL_2 = "http://example.org";
            final String URL_3 = "http://examples2.com";
            final String URL_4 = "http://readinglist.com";

            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a basic history entry
            ContentValues basicHistory = createHistoryEntry(TITLE_1, URL_1, VISITS, LAST_VISITED);
            ContentUris.parseId(mProvider.insert(mHistoryUri, basicHistory));

            // Create a basic bookmark entry
            ContentValues basicBookmark = createBookmark(TITLE_2, URL_2, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            mProvider.insert(mBookmarksUri, basicBookmark);

            // Create a history entry and bookmark entry with the same URL to
            // represent a visited bookmark
            ContentValues combinedHistory = createHistoryEntry(TITLE_3_HISTORY, URL_3, VISITS, LAST_VISITED);
            mProvider.insert(mHistoryUri, combinedHistory);

            ContentValues combinedBookmark = createBookmark(TITLE_3_BOOKMARK, URL_3, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            mProvider.insert(mBookmarksUri, combinedBookmark);

            // Create a reading list entries
            int readingListId = getIntColumn("Bookmarks", "FIXED_READING_LIST_ID");
            ContentValues readingListItem = createBookmark(TITLE_3_BOOKMARK, URL_3, readingListId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long readingListItemId = ContentUris.parseId(mProvider.insert(mBookmarksUri, readingListItem));

            ContentValues readingListItem2 = createBookmark(TITLE_4, URL_4, readingListId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long readingListItemId2 = ContentUris.parseId(mProvider.insert(mBookmarksUri, readingListItem2));

            Cursor c = mProvider.query(mCombinedUri, null, "", null, null);
            mAsserter.is(c.getCount(), 4, "4 combined entries found");

            while (c.moveToNext()) {
                long id = c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol));

                int display = c.getInt(c.getColumnIndex(mCombinedDisplayCol));
                int expectedDisplay = (id == readingListItemId || id == readingListItemId2 ? mCombinedDisplayReader : mCombinedDisplayNormal);

                mAsserter.is(new Integer(display), new Integer(expectedDisplay),
                                 "Combined display column should always be DISPLAY_READER for the reading list item");
            }
        }
    }

    class TestCombinedViewWithDeletedBookmark extends Test {
        @Override
        public void test() throws Exception {
            final String TITLE = "Test Page 1";
            final String URL = "http://example.com";
            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a combined history entry
            ContentValues combinedHistory = createHistoryEntry(TITLE, URL, VISITS, LAST_VISITED);
            mProvider.insert(mHistoryUri, combinedHistory);

            // Create a combined bookmark entry
            ContentValues combinedBookmark = createBookmark(TITLE, URL, mMobileFolderId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long combinedBookmarkId = ContentUris.parseId(mProvider.insert(mBookmarksUri, combinedBookmark));

            Cursor c = mProvider.query(mCombinedUri, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry with bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(combinedBookmarkId),
                         "Bookmark id should be set correctly on combined entry");

            int deleted = mProvider.delete(mBookmarksUri,
                                           mBookmarksIdCol + " = ?",
                                           new String[] { String.valueOf(combinedBookmarkId) });

            mAsserter.is((deleted == 1), true, "Inserted combined bookmark was deleted");

            c = mProvider.query(mCombinedUri, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry without bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(0),
                         "Bookmark id should not be set to removed bookmark id");
        }
    }

    class TestCombinedViewWithDeletedReadingListItem extends Test {
        @Override
        public void test() throws Exception {
            final String TITLE = "Test Page 1";
            final String URL = "http://example.com";
            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a combined history entry
            ContentValues combinedHistory = createHistoryEntry(TITLE, URL, VISITS, LAST_VISITED);
            mProvider.insert(mHistoryUri, combinedHistory);

            // Create a combined bookmark entry
            int readingListId = getIntColumn("Bookmarks", "FIXED_READING_LIST_ID");
            ContentValues combinedReadingListItem = createBookmark(TITLE, URL, readingListId,
                mBookmarksTypeBookmark, 0, "tags", "description", "keyword");
            long combinedReadingListItemId = ContentUris.parseId(mProvider.insert(mBookmarksUri, combinedReadingListItem));

            Cursor c = mProvider.query(mCombinedUri, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry with bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(combinedReadingListItemId),
                         "Bookmark id should be set correctly on combined entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedDisplayCol))), new Long(mCombinedDisplayReader),
                         "Combined entry should have reader display type");

            int deleted = mProvider.delete(mBookmarksUri,
                                           mBookmarksIdCol + " = ?",
                                           new String[] { String.valueOf(combinedReadingListItemId) });

            mAsserter.is((deleted == 1), true, "Inserted combined reading list item was deleted");

            c = mProvider.query(mCombinedUri, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry without bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedBookmarkIdCol))), new Long(0),
                         "Bookmark id should not be set to removed bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(mCombinedDisplayCol))), new Long(mCombinedDisplayNormal),
                         "Combined entry should have reader display type");
        }
    }

    class TestExpireHistory extends Test {
        private void createFakeHistory(long timeShift, int count) {
            // Insert a bunch of very new entries
            ContentValues[] allVals = new ContentValues[count];
            long time = System.currentTimeMillis() - timeShift;
            for (int i = 0; i < count; i++) {
                allVals[i] = new ContentValues();
                allVals[i].put(mHistoryTitleCol, "Test " + i);
                allVals[i].put(mHistoryUrlCol, "http://www.test.org/" + i);
                allVals[i].put(mHistoryVisitsCol, i);
                allVals[i].put(mHistoryLastVisitedCol, time);
            }

            int inserts = mProvider.bulkInsert(mHistoryUri, allVals);
            mAsserter.is(inserts, count, "Expected number of inserts matches");

            // inserting a new entry sets the date created and modified automatically
            // reset all of them
            for (int i = 0; i < count; i++) {
                ContentValues cv = new ContentValues();
                cv.put(mHistoryDateCreatedCol, time);
                cv.put(mHistoryDateModifiedCol, time);
                mProvider.update(mHistoryUri, cv, mHistoryUrlCol + " = ?",
                                 new String[] { "http://www.test.org/" + i });
            }

            Cursor c = mProvider.query(mHistoryUri, null, "", null, null);
            mAsserter.is(c.getCount(), count, count + " history entries found");

            // add thumbnails for each entry
            allVals = new ContentValues[count];
            for (int i = 0; i < count; i++) {
                allVals[i] = new ContentValues();
                allVals[i].put(mThumbnailsDataCol, i);
                allVals[i].put(mThumbnailsUrlCol, "http://www.test.org/" + i);
            }

            inserts = mProvider.bulkInsert(mThumbnailsUri, allVals);
            mAsserter.is(inserts, count, "Expected number of inserts matches");

            c = mProvider.query(mThumbnailsUri, null, null, null, null);
            mAsserter.is(c.getCount(), count, count + " thumbnails entries found");
        }

        @Override
        public void test() throws Exception {
            final int count = 3000;
            final int thumbCount = 15;

            // insert a bunch of new entries
            createFakeHistory(0, count);

            // expiring with a normal priority should not delete new entries
            Uri url = appendUriParam(mHistoryOldUri, "PARAM_EXPIRE_PRIORITY", "NORMAL");
            mProvider.delete(url, null, null);
            Cursor c = mProvider.query(mHistoryUri, null, "", null, null);
            mAsserter.is(c.getCount(), count, count + " history entries found");

            // expiring with a normal priority should delete all but 10 thumbnails
            c = mProvider.query(mThumbnailsUri, null, null, null, null);
            mAsserter.is(c.getCount(), thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();
            // insert a bunch of new entries
            createFakeHistory(0, count);

            // expiring with a aggressive priority should leave 500 entries
            url = appendUriParam(mHistoryOldUri, "PARAM_EXPIRE_PRIORITY", "AGGRESSIVE");
            mProvider.delete(url, null, null);
            c = mProvider.query(mHistoryUri, null, "", null, null);
            mAsserter.is(c.getCount(), 500, "500 history entries found");

            // expiring with a aggressive priority should delete all but 10 thumbnails
            c = mProvider.query(mThumbnailsUri, null, null, null, null);
            mAsserter.is(c.getCount(), thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();
            // insert a bunch of entries with an old time created/modified
            long time = 1000L * 60L * 60L * 24L * 30L * 3L;
            createFakeHistory(time, count);

            // expiring with an normal priority should remove at most 1000 entries
            // entries leaving at least 2000
            url = appendUriParam(mHistoryOldUri, "PARAM_EXPIRE_PRIORITY", "NORMAL");
            mProvider.delete(url, null, null);
            c = mProvider.query(mHistoryUri, null, "", null, null);
            mAsserter.is(c.getCount(), 2000, "2000 history entries found");

            // expiring with a normal priority should delete all but 10 thumbnails
            c = mProvider.query(mThumbnailsUri, null, null, null, null);
            mAsserter.is(c.getCount(), thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();
            // insert a bunch of entries with an old time created/modified
            time = 1000L * 60L * 60L * 24L * 30L * 3L;
            createFakeHistory(time, count);

            // expiring with an agressive priority should remove old
            // entries leaving at least 500
            url = appendUriParam(mHistoryOldUri, "PARAM_EXPIRE_PRIORITY", "AGGRESSIVE");
            mProvider.delete(url, null, null);
            c = mProvider.query(mHistoryUri, null, "", null, null);
            mAsserter.is(c.getCount(), 500, "500 history entries found");

            // expiring with an aggressive priority should delete all but 10 thumbnails
            c = mProvider.query(mThumbnailsUri, null, null, null, null);
            mAsserter.is(c.getCount(), thumbCount, thumbCount + " thumbnails found");
        }
    }

    /*
     * Verify that insert, update, delete, and bulkInsert operations
     * notify the ambient content resolver.  Each operation calls the
     * content resolver notifyChange method synchronously, so it is
     * okay to test sequentially.
     */
    class TestBrowserProviderNotifications extends Test {
        public static final String LOGTAG = "TestBPNotifications";

        protected void ensureOnlyChangeNotifiedStartsWith(Uri expectedUri, String operation) {
            if (expectedUri == null) {
                throw new IllegalArgumentException("expectedUri must not be null");
            }

            if (mResolver.notifyChangeList.size() != 1) {
                // Log to help post-mortem debugging
                Log.w(LOGTAG, "after operation, notifyChangeList = " + mResolver.notifyChangeList);
            }

            mAsserter.is(Long.valueOf(mResolver.notifyChangeList.size()),
                         Long.valueOf(1),
                         "Content observer was notified exactly once by " + operation);

            Uri uri = mResolver.notifyChangeList.poll();

            mAsserter.isnot(uri,
                            null,
                            "Notification from " + operation + " was valid");

            mAsserter.ok(uri.toString().startsWith(expectedUri.toString()),
                         "Content observer was notified exactly once by " + operation,
                         uri.toString() + " starts with expected prefix " + expectedUri);
        }

        @Override
        public void test() throws Exception {
            // Insert
            final ContentValues h = createOneHistoryEntry();

            mResolver.notifyChangeList.clear();
            long id = ContentUris.parseId(mProvider.insert(mHistoryUri, h));

            mAsserter.isnot(Long.valueOf(id),
                            Long.valueOf(-1),
                            "Inserted item has valid id");

            ensureOnlyChangeNotifiedStartsWith(mHistoryUri, "insert");

            // Update
            mResolver.notifyChangeList.clear();
            h.put(mHistoryTitleCol, "http://newexample.com");

            long numUpdated = mProvider.update(mHistoryUri, h,
                                               mHistoryIdCol + " = ?",
                                               new String[] { String.valueOf(id) });

            mAsserter.is(Long.valueOf(numUpdated),
                         Long.valueOf(1),
                         "Correct number of items are updated");

            ensureOnlyChangeNotifiedStartsWith(mHistoryUri, "update");

            // Delete
            mResolver.notifyChangeList.clear();
            long numDeleted = mProvider.delete(mHistoryUri, null, null);

            mAsserter.is(Long.valueOf(numDeleted),
                         Long.valueOf(1),
                         "Correct number of items are deleted");

            ensureOnlyChangeNotifiedStartsWith(mHistoryUri, "delete");

            // Bulk insert
            final ContentValues[] hs = new ContentValues[] { createOneHistoryEntry() };

            mResolver.notifyChangeList.clear();
            long numBulkInserted = mProvider.bulkInsert(mHistoryUri, hs);

            mAsserter.is(Long.valueOf(numBulkInserted),
                         Long.valueOf(1),
                         "Correct number of items are bulkInserted");

            ensureOnlyChangeNotifiedStartsWith(mHistoryUri, "bulkInsert");
        }
    }
}
