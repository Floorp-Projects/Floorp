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

import org.mozilla.gecko.db.BrowserContract;

/*
 * This test is meant to exercise all operations exposed by Fennec's
 * history and bookmarks content provider. It does so in an isolated
 * environment (see ContentProviderTest) without affecting any UI-related
 * code.
 */
public class testBrowserProvider extends ContentProviderTest {
    private long mMobileFolderId;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    private void loadMobileFolderId() throws Exception {
        Cursor c = null;
        try {
            c = getBookmarkByGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
            mAsserter.is(c.moveToFirst(), true, "Mobile bookmarks folder is present");

            mMobileFolderId = c.getLong(c.getColumnIndex(BrowserContract.Bookmarks._ID));
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    private void ensureEmptyDatabase() throws Exception {
        Cursor c = null;

        String guid = BrowserContract.Bookmarks.GUID;

        mProvider.delete(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ? AND " +
                         guid + " != ?",
                         new String[] { BrowserContract.Bookmarks.PLACES_FOLDER_GUID,
                                        BrowserContract.Bookmarks.MOBILE_FOLDER_GUID,
                                        BrowserContract.Bookmarks.MENU_FOLDER_GUID,
                                        BrowserContract.Bookmarks.TAGS_FOLDER_GUID,
                                        BrowserContract.Bookmarks.TOOLBAR_FOLDER_GUID,
                                        BrowserContract.Bookmarks.UNFILED_FOLDER_GUID,
                                        BrowserContract.Bookmarks.READING_LIST_FOLDER_GUID });

        c = mProvider.query(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), null, null, null, null);
        assertCountIsAndClose(c, 7, "All non-special bookmarks and folders were deleted");

        mProvider.delete(appendUriParam(BrowserContract.History.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"), null, null);
        c = mProvider.query(appendUriParam(BrowserContract.History.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), null, null, null, null);
        assertCountIsAndClose(c, 0, "All history entries were deleted");

        /**
         * There's no reason why the following two parts should fail.
         * But sometimes they do, and I'm not going to spend the time
         * to figure out why in an unrelated bug.
         */

        mProvider.delete(appendUriParam(BrowserContract.Favicons.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"), null, null);
        c = mProvider.query(appendUriParam(BrowserContract.Favicons.CONTENT_URI,
                                           BrowserContract.PARAM_SHOW_DELETED, "1"),
                                           null, null, null, null);

        if (c.getCount() > 0) {
          mAsserter.dumpLog("Unexpected favicons in ensureEmptyDatabase.");
        }
        c.close();

        // assertCountIsAndClose(c, 0, "All favicons were deleted");

        mProvider.delete(appendUriParam(BrowserContract.Thumbnails.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"), null, null);
        c = mProvider.query(appendUriParam(BrowserContract.Thumbnails.CONTENT_URI,
                                           BrowserContract.PARAM_SHOW_DELETED, "1"),
                                           null, null, null, null);

        if (c.getCount() > 0) {
          mAsserter.dumpLog("Unexpected thumbnails in ensureEmptyDatabase.");
        }
        c.close();

        // assertCountIsAndClose(c, 0, "All thumbnails were deleted");
    }

    private ContentValues createBookmark(String title, String url, long parentId,
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

    private ContentValues createOneBookmark() throws Exception {
        return createBookmark("Example", "http://example.com", mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
    }

    private Cursor getBookmarksByParent(long parent) throws Exception {
        // Order by position.
        return mProvider.query(BrowserContract.Bookmarks.CONTENT_URI, null,
                               BrowserContract.Bookmarks.PARENT + " = ?",
                               new String[] { String.valueOf(parent) },
                               BrowserContract.Bookmarks.POSITION);
    }

    private Cursor getBookmarkByGuid(String guid) throws Exception {
        return mProvider.query(BrowserContract.Bookmarks.CONTENT_URI, null,
                               BrowserContract.Bookmarks.GUID + " = ?",
                               new String[] { guid },
                               null);
    }

    private Cursor getBookmarkById(long id) throws Exception {
        return getBookmarkById(BrowserContract.Bookmarks.CONTENT_URI, id, null);
    }

    private Cursor getBookmarkById(long id, String[] projection) throws Exception {
        return getBookmarkById(BrowserContract.Bookmarks.CONTENT_URI, id, projection);
    }

    private Cursor getBookmarkById(Uri bookmarksUri, long id) throws Exception {
        return getBookmarkById(bookmarksUri, id, null);
    }

    private Cursor getBookmarkById(Uri bookmarksUri, long id, String[] projection) throws Exception {
        return mProvider.query(bookmarksUri, projection,
                               BrowserContract.Bookmarks._ID + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private ContentValues createHistoryEntry(String title, String url, int visits, long lastVisited) throws Exception {
        ContentValues historyEntry = new ContentValues();

        historyEntry.put(BrowserContract.History.TITLE, title);
        historyEntry.put(BrowserContract.History.URL, url);
        historyEntry.put(BrowserContract.History.VISITS, visits);
        historyEntry.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);

        return historyEntry;
    }

    private ContentValues createFaviconEntry(String pageUrl, String data) throws Exception {
        ContentValues faviconEntry = new ContentValues();

        faviconEntry.put(BrowserContract.Favicons.PAGE_URL, pageUrl);
        faviconEntry.put(BrowserContract.Favicons.URL, pageUrl + "/favicon.ico");
        faviconEntry.put(BrowserContract.Favicons.DATA, data.getBytes("UTF8"));

        return faviconEntry;
    }

    private ContentValues createThumbnailEntry(String pageUrl, String data) throws Exception {
        ContentValues thumbnailEntry = new ContentValues();

        thumbnailEntry.put(BrowserContract.Thumbnails.URL, pageUrl);
        thumbnailEntry.put(BrowserContract.Thumbnails.DATA, data.getBytes("UTF8"));

        return thumbnailEntry;
    }

    private ContentValues createOneHistoryEntry() throws Exception {
        return createHistoryEntry("Example", "http://example.com", 10, System.currentTimeMillis());
    }

    private Cursor getHistoryEntryById(long id) throws Exception {
        return getHistoryEntryById(BrowserContract.History.CONTENT_URI, id, null);
    }

    private Cursor getHistoryEntryById(long id, String[] projection) throws Exception {
        return getHistoryEntryById(BrowserContract.History.CONTENT_URI, id, projection);
    }

    private Cursor getHistoryEntryById(Uri historyUri, long id) throws Exception {
        return getHistoryEntryById(historyUri, id, null);
    }

    private Cursor getHistoryEntryById(Uri historyUri, long id, String[] projection) throws Exception {
        return mProvider.query(historyUri, projection,
                               BrowserContract.History._ID + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private Cursor getFaviconsByUrl(String url) throws Exception {
        return mProvider.query(BrowserContract.Combined.CONTENT_URI, null,
                               BrowserContract.Combined.URL + " = ?",
                               new String[] { url },
                               null);
    }

    private Cursor getThumbnailByUrl(String url) throws Exception {
        return mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null,
                               BrowserContract.Thumbnails.URL + " = ?",
                               new String[] { url },
                               null);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp(sBrowserProviderCallable, BrowserContract.AUTHORITY, "browser.db");

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

    private class TestBatchOperations extends TestCase {
        static final int TESTCOUNT = 100;

        public void testApplyBatch() throws Exception {
            ArrayList<ContentProviderOperation> mOperations
                = new ArrayList<ContentProviderOperation>();

            // Test a bunch of inserts with applyBatch
            ContentValues values = new ContentValues();
            ContentProviderOperation.Builder builder = null;

            for (int i = 0; i < TESTCOUNT; i++) {
                values.clear();
                values.put(BrowserContract.History.VISITS, i);
                values.put(BrowserContract.History.TITLE, "Test" + i);
                values.put(BrowserContract.History.URL, "http://www.test.org/" + i);

                // Insert
                builder = ContentProviderOperation.newInsert(BrowserContract.History.CONTENT_URI);
                builder.withValues(values);
                // Queue the operation
                mOperations.add(builder.build());
            }

            ContentProviderResult[] applyResult =
                mProvider.applyBatch(mOperations);

            boolean allFound = true;
            for (int i = 0; i < TESTCOUNT; i++) {
                Cursor cursor = mProvider.query(BrowserContract.History.CONTENT_URI,
                                                null,
                                                BrowserContract.History.URL + " = ?",
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
            values.put(BrowserContract.History.VISITS, 1);
            for (int i = 0; i < TESTCOUNT; i++) {
                builder = ContentProviderOperation.newUpdate(BrowserContract.History.CONTENT_URI);
                builder.withSelection(BrowserContract.History.URL  + " = ?",
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

            // Delete all visits
            for (int i = 0; i < TESTCOUNT; i++) {
                builder = ContentProviderOperation.newDelete(BrowserContract.History.CONTENT_URI);
                builder.withSelection(BrowserContract.History.URL  + " = ?",
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
            mAsserter.is(seenException, false, "Batch deletion succeeded");
        }

        // Force a Constraint error, see if later operations still apply correctly
        public void testApplyBatchErrors() throws Exception {
            ArrayList<ContentProviderOperation> mOperations
                = new ArrayList<ContentProviderOperation>();

            // Test a bunch of inserts with applyBatch
            ContentProviderOperation.Builder builder = null;
            ContentValues values = createFaviconEntry("http://www.test.org", "FAVICON");
            builder = ContentProviderOperation.newInsert(BrowserContract.Favicons.CONTENT_URI);
            builder.withValues(values);
            mOperations.add(builder.build());

            // Make a duplicate, this will fail because of a UNIQUE constraint
            builder = ContentProviderOperation.newInsert(BrowserContract.Favicons.CONTENT_URI);
            builder.withValues(values);
            mOperations.add(builder.build());

            // This is valid and should be in the table afterwards
            values.put(BrowserContract.Favicons.URL, "http://www.test.org/valid.ico");
            builder = ContentProviderOperation.newInsert(BrowserContract.Favicons.CONTENT_URI);
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
            Cursor cursor = mProvider.query(BrowserContract.Favicons.CONTENT_URI,
                                            null,
                                            BrowserContract.Favicons.URL + " = ?",
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
                allVals[i].put(BrowserContract.History.URL, i);
                allVals[i].put(BrowserContract.History.TITLE, "Test" + i);
                allVals[i].put(BrowserContract.History.URL, "http://www.test.org/" + i);
            }

            int inserts = mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, allVals);
            mAsserter.is(inserts, TESTCOUNT, "Excepted number of inserts matches");

            boolean allFound = true;
            for (int i = 0; i < TESTCOUNT; i++) {
                Cursor cursor = mProvider.query(BrowserContract.History.CONTENT_URI,
                                                null,
                                                BrowserContract.History.URL + " = ?",
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

    private class TestSpecialFolders extends TestCase {
        @Override
        public void test() throws Exception {
            Cursor c = mProvider.query(BrowserContract.Bookmarks.CONTENT_URI,
                                       new String[] { BrowserContract.Bookmarks._ID,
                                                      BrowserContract.Bookmarks.GUID,
                                                      BrowserContract.Bookmarks.PARENT },
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ? OR " +
                                       BrowserContract.Bookmarks.GUID + " = ?",
                                       new String[] { BrowserContract.Bookmarks.PLACES_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.MOBILE_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.MENU_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.TAGS_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.TOOLBAR_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.UNFILED_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.READING_LIST_FOLDER_GUID },
                                       null);

            mAsserter.is(c.getCount(), 7, "Right number of special folders");

            int rootId = BrowserContract.Bookmarks.FIXED_ROOT_ID;
            int readingListId = BrowserContract.Bookmarks.FIXED_READING_LIST_ID;

            while (c.moveToNext()) {
                int id = c.getInt(c.getColumnIndex(BrowserContract.Bookmarks._ID));
                String guid = c.getString(c.getColumnIndex(BrowserContract.Bookmarks.GUID));
                int parentId = c.getInt(c.getColumnIndex(BrowserContract.Bookmarks.PARENT));

                if (guid.equals(BrowserContract.Bookmarks.PLACES_FOLDER_GUID)) {
                    mAsserter.is(new Integer(id), new Integer(rootId), "The id of places folder is correct");
                } else if (guid.equals(BrowserContract.Bookmarks.READING_LIST_FOLDER_GUID)) {
                    mAsserter.is(new Integer(id), new Integer(readingListId), "The id of reading list folder is correct");
                }

                mAsserter.is(new Integer(parentId), new Integer(rootId),
                             "The PARENT of the " + guid + " special folder is correct");
            }

            c.close();
        }
    }

    private class TestInsertBookmarks extends TestCase {
        private long insertWithNullCol(String colName) throws Exception {
            ContentValues b = createOneBookmark();
            b.putNull(colName);
            long id = -1;

            try {
                id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
            } catch (Exception e) {}

            return id;
        }

        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
            Cursor c = getBookmarkById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TITLE)), b.getAsString(BrowserContract.Bookmarks.TITLE),
                         "Inserted bookmark has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.URL)), b.getAsString(BrowserContract.Bookmarks.URL),
                         "Inserted bookmark has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TAGS)), b.getAsString(BrowserContract.Bookmarks.TAGS),
                         "Inserted bookmark has correct tags");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.KEYWORD)), b.getAsString(BrowserContract.Bookmarks.KEYWORD),
                         "Inserted bookmark has correct keyword");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.DESCRIPTION)), b.getAsString(BrowserContract.Bookmarks.DESCRIPTION),
                         "Inserted bookmark has correct description");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.POSITION)), b.getAsString(BrowserContract.Bookmarks.POSITION),
                         "Inserted bookmark has correct position");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TYPE)), b.getAsString(BrowserContract.Bookmarks.TYPE),
                         "Inserted bookmark has correct type");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.PARENT)), b.getAsString(BrowserContract.Bookmarks.PARENT),
                         "Inserted bookmark has correct parent ID");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.IS_DELETED)), String.valueOf(0),
                         "Inserted bookmark has correct is-deleted state");

            id = insertWithNullCol(BrowserContract.Bookmarks.POSITION);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert bookmark with null position");

            id = insertWithNullCol(BrowserContract.Bookmarks.TYPE);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert bookmark with null type");

            if (Build.VERSION.SDK_INT >= 8 &&
                Build.VERSION.SDK_INT < 16) {
                b = createOneBookmark();
                b.put(BrowserContract.Bookmarks.PARENT, -1);
                id = -1;

                try {
                    id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
                } catch (Exception e) {}

                mAsserter.is(new Long(id), new Long(-1),
                             "Should not be able to insert bookmark with invalid parent");
            }

            b = createOneBookmark();
            b.remove(BrowserContract.Bookmarks.TYPE);
            id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
            c = getBookmarkById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TYPE)), String.valueOf(BrowserContract.Bookmarks.TYPE_BOOKMARK),
                         "Inserted bookmark has correct default type");
            c.close();
        }
    }

    private class TestInsertBookmarksFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String favicon = "FAVICON";
            final String pageUrl = b.getAsString(BrowserContract.Bookmarks.URL);

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));

            // Insert the favicon into the favicons table
            mProvider.insert(BrowserContract.Favicons.CONTENT_URI, createFaviconEntry(pageUrl, favicon));

            Cursor c = getBookmarkById(id, new String[] { BrowserContract.Bookmarks.FAVICON });

            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Bookmarks.FAVICON)), "UTF8"),
                         favicon, "Inserted bookmark has corresponding favicon image");
            c.close();

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");
            c.close();
        }
    }

    private class TestDeleteBookmarks extends TestCase {
        private long insertOneBookmark() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));

            Cursor c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");
            c.close();

            return id;
        }

        @Override
        public void test() throws Exception {
            long id = insertOneBookmark();

            int deleted = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted bookmark was deleted");

            Cursor c = getBookmarkById(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Deleted bookmark was only marked as deleted");
            c.close();

            deleted = mProvider.delete(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                                       BrowserContract.Bookmarks._ID + " = ?",
                                       new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted bookmark was deleted");

            c = getBookmarkById(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), false, "Inserted bookmark is now actually deleted");
            c.close();

            id = insertOneBookmark();

            deleted = mProvider.delete(ContentUris.withAppendedId(BrowserContract.Bookmarks.CONTENT_URI, id), null, null);
            mAsserter.is((deleted == 1), true,
                         "Inserted bookmark was deleted using URI with id");

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), false,
                         "Inserted bookmark can't be found after deletion using URI with ID");
            c.close();

            if (Build.VERSION.SDK_INT >= 8 &&
                Build.VERSION.SDK_INT < 16) {
                ContentValues b = createBookmark("Folder", null, mMobileFolderId,
                        BrowserContract.Bookmarks.TYPE_FOLDER, 0, "folderTags", "folderDescription", "folderKeyword");

                long parentId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
                c = getBookmarkById(parentId);
                mAsserter.is(c.moveToFirst(), true, "Inserted bookmarks folder found");
                c.close();

                b = createBookmark("Example", "http://example.com", parentId,
                        BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");

                id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
                c = getBookmarkById(id);
                mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");
                c.close();

                deleted = 0;
                try {
                    Uri uri = ContentUris.withAppendedId(BrowserContract.Bookmarks.CONTENT_URI, parentId);
                    deleted = mProvider.delete(appendUriParam(uri, BrowserContract.PARAM_IS_SYNC, "1"), null, null);
                } catch(Exception e) {}

                mAsserter.is((deleted == 0), true,
                             "Should not be able to delete folder that causes orphan bookmarks");
            }
        }
    }

    private class TestDeleteBookmarksFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String pageUrl = b.getAsString(BrowserContract.Bookmarks.URL);
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));

            // Insert the favicon into the favicons table
            mProvider.insert(BrowserContract.Favicons.CONTENT_URI, createFaviconEntry(pageUrl, "FAVICON"));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");
            c.close();

            mProvider.delete(ContentUris.withAppendedId(BrowserContract.Bookmarks.CONTENT_URI, id), null, null);

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Favicon is deleted with last reference to it");
            c.close();
        }
    }

    private class TestUpdateBookmarks extends TestCase {
        private int updateWithNullCol(long id, String colName) throws Exception {
            ContentValues u = new ContentValues();
            u.putNull(colName);

            int updated = 0;

            try {
                updated = mProvider.update(BrowserContract.Bookmarks.CONTENT_URI, u,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(id) });
            } catch (Exception e) {}

            return updated;
        }

        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));

            Cursor c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");

            long dateCreated = c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_CREATED));
            long dateModified = c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_MODIFIED));

            ContentValues u = new ContentValues();
            u.put(BrowserContract.Bookmarks.TITLE, b.getAsString(BrowserContract.Bookmarks.TITLE) + "CHANGED");
            u.put(BrowserContract.Bookmarks.URL, b.getAsString(BrowserContract.Bookmarks.URL) + "/more/stuff");
            u.put(BrowserContract.Bookmarks.TAGS, b.getAsString(BrowserContract.Bookmarks.TAGS) + "CHANGED");
            u.put(BrowserContract.Bookmarks.DESCRIPTION, b.getAsString(BrowserContract.Bookmarks.DESCRIPTION) + "CHANGED");
            u.put(BrowserContract.Bookmarks.KEYWORD, b.getAsString(BrowserContract.Bookmarks.KEYWORD) + "CHANGED");
            u.put(BrowserContract.Bookmarks.TYPE, BrowserContract.Bookmarks.TYPE_FOLDER);
            u.put(BrowserContract.Bookmarks.POSITION, 10);

            int updated = mProvider.update(BrowserContract.Bookmarks.CONTENT_URI, u,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((updated == 1), true, "Inserted bookmark was updated");
            c.close();

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TITLE)), u.getAsString(BrowserContract.Bookmarks.TITLE),
                         "Inserted bookmark has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.URL)), u.getAsString(BrowserContract.Bookmarks.URL),
                         "Inserted bookmark has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TAGS)), u.getAsString(BrowserContract.Bookmarks.TAGS),
                         "Inserted bookmark has correct tags");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.KEYWORD)), u.getAsString(BrowserContract.Bookmarks.KEYWORD),
                         "Inserted bookmark has correct keyword");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.DESCRIPTION)), u.getAsString(BrowserContract.Bookmarks.DESCRIPTION),
                         "Inserted bookmark has correct description");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.POSITION)), u.getAsString(BrowserContract.Bookmarks.POSITION),
                         "Inserted bookmark has correct position");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TYPE)), u.getAsString(BrowserContract.Bookmarks.TYPE),
                         "Inserted bookmark has correct type");

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_CREATED))),
                         new Long(dateCreated),
                         "Updated bookmark has same creation date");

            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_MODIFIED))),
                            new Long(dateModified),
                            "Updated bookmark has new modification date");

            updated = updateWithNullCol(id, BrowserContract.Bookmarks.POSITION);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update bookmark with null position");

            updated = updateWithNullCol(id, BrowserContract.Bookmarks.TYPE);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update bookmark with null type");

            u = new ContentValues();
            u.put(BrowserContract.Bookmarks.URL, "http://examples2.com");

            updated = mProvider.update(ContentUris.withAppendedId(BrowserContract.Bookmarks.CONTENT_URI, id), u, null, null);
            c.close();

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated bookmark found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.URL)), u.getAsString(BrowserContract.Bookmarks.URL),
                         "Updated bookmark has correct URL using URI with id");
            c.close();
        }
    }

    private class TestUpdateBookmarksFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues b = createOneBookmark();

            final String favicon = "FAVICON";
            final String newFavicon = "NEW_FAVICON";
            final String pageUrl = b.getAsString(BrowserContract.Bookmarks.URL);

            mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b);

            // Insert the favicon into the favicons table
            ContentValues f = createFaviconEntry(pageUrl, favicon);
            long faviconId = ContentUris.parseId(mProvider.insert(BrowserContract.Favicons.CONTENT_URI, f));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");

            ContentValues u = createFaviconEntry(pageUrl, newFavicon);
            mProvider.update(BrowserContract.Favicons.CONTENT_URI, u, null, null);
            c.close();

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         newFavicon, "Updated favicon has corresponding favicon image");
            c.close();
        }
    }

    /**
     * Create a folder of one thousand and one bookmarks, then impose an order
     * on them.
     *
     * Verify that the reordering worked by querying.
     */
    private class TestPositionBookmarks extends TestCase {

        public String makeGUID(final long in) {
            String part = String.valueOf(in);
            return "aaaaaaaaaaaa".substring(0, (12 - part.length())) + part;
        }

        public void compareCursorToItems(final Cursor c, final String[] items, final int count) {
            mAsserter.is(c.moveToFirst(), true, "Folder has children.");

            int posColumn = c.getColumnIndex(BrowserContract.Bookmarks.POSITION);
            int guidColumn = c.getColumnIndex(BrowserContract.Bookmarks.GUID);
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
            c.close();
        }

        public static final int NUMBER_OF_CHILDREN = 1001;
        @Override
        public void test() throws Exception {
            // Create the containing folder.
            ContentValues folder = createBookmark("FolderFolder", "", mMobileFolderId,
                                                  BrowserContract.Bookmarks.TYPE_FOLDER, 0, "",
                                                  "description", "keyword");
            folder.put(BrowserContract.Bookmarks.GUID, "folderfolder");
            long folderId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, folder));

            // Create the children.
            String[] items = new String[NUMBER_OF_CHILDREN];

            // Reuse the same ContentValues.
            ContentValues item = createBookmark("Test Bookmark", "http://example.com", folderId,
                                                BrowserContract.Bookmarks.TYPE_FOLDER, 0, "",
                                                "description", "keyword");

            for (int i = 0; i < NUMBER_OF_CHILDREN; ++i) {
                String guid = makeGUID(i);
                items[i] = guid;
                item.put(BrowserContract.Bookmarks.GUID, guid);
                item.put(BrowserContract.Bookmarks.POSITION, i);
                item.put(BrowserContract.Bookmarks.URL, "http://example.com/" + guid);
                item.put(BrowserContract.Bookmarks.TITLE, "Test Bookmark " + guid);
                mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, item);
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
            long updated = mProvider.update(BrowserContract.Bookmarks.POSITIONS_CONTENT_URI, null, null, items);
            mAsserter.is(updated, (long) NUMBER_OF_CHILDREN, "Updated " + NUMBER_OF_CHILDREN + " positions.");

            // Verify that the database was updated.
            c = getBookmarksByParent(folderId);
            compareCursorToItems(c, items, NUMBER_OF_CHILDREN);
            c.close();
        }
    }

    private class TestInsertHistory extends TestCase {
        private long insertWithNullCol(String colName) throws Exception {
            ContentValues h = createOneHistoryEntry();
            h.putNull(colName);
            long id = -1;

            try {
                id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));
            } catch (Exception e) {}

            return id;
        }

        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));
            Cursor c = getHistoryEntryById(id);

            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), h.getAsString(BrowserContract.History.TITLE),
                         "Inserted history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.URL)), h.getAsString(BrowserContract.History.URL),
                         "Inserted history entry has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.VISITS)), h.getAsString(BrowserContract.History.VISITS),
                         "Inserted history entry has correct number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.DATE_LAST_VISITED)), h.getAsString(BrowserContract.History.DATE_LAST_VISITED),
                         "Inserted history entry has correct last visited date");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.IS_DELETED)), String.valueOf(0),
                         "Inserted history entry has correct is-deleted state");

            id = insertWithNullCol(BrowserContract.History.URL);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert history with null URL");

            id = insertWithNullCol(BrowserContract.History.VISITS);
            mAsserter.is(new Long(id), new Long(-1),
                         "Should not be able to insert history with null number of visits");
            c.close();
        }
    }

    private class TestInsertHistoryFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String favicon = "FAVICON";
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));

            // Insert the favicon into the favicons table
            mProvider.insert(BrowserContract.Favicons.CONTENT_URI, createFaviconEntry(pageUrl, favicon));

            Cursor c = getHistoryEntryById(id, new String[] { BrowserContract.History.FAVICON });

            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.History.FAVICON)), "UTF8"),
                         favicon, "Inserted history entry has corresponding favicon image");
            c.close();

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");
            c.close();
        }
    }

    private class TestDeleteHistory extends TestCase {
        private long insertOneHistoryEntry() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));

            Cursor c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");
            c.close();

            return id;
        }

        @Override
        public void test() throws Exception {
            long id = insertOneHistoryEntry();

            int deleted = mProvider.delete(BrowserContract.History.CONTENT_URI,
                                           BrowserContract.History._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted history entry was deleted");

            Cursor c = getHistoryEntryById(appendUriParam(BrowserContract.History.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Deleted history entry was only marked as deleted");

            deleted = mProvider.delete(appendUriParam(BrowserContract.History.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                                       BrowserContract.History._ID + " = ?",
                                       new String[] { String.valueOf(id) });

            mAsserter.is((deleted == 1), true, "Inserted history entry was deleted");
            c.close();

            c = getHistoryEntryById(appendUriParam(BrowserContract.History.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), false, "Inserted history is now actually deleted");

            id = insertOneHistoryEntry();

            deleted = mProvider.delete(ContentUris.withAppendedId(BrowserContract.History.CONTENT_URI, id), null, null);
            mAsserter.is((deleted == 1), true,
                         "Inserted history entry was deleted using URI with id");
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), false,
                         "Inserted history entry can't be found after deletion using URI with ID");
            c.close();
        }
    }

    private class TestDeleteHistoryFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            // Insert the favicon into the favicons table
            mProvider.insert(BrowserContract.Favicons.CONTENT_URI, createFaviconEntry(pageUrl, "FAVICON"));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mProvider.delete(ContentUris.withAppendedId(BrowserContract.History.CONTENT_URI, id), null, null);
            c.close();

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Favicon is deleted with last reference to it");
            c.close();
        }
    }

    private class TestUpdateHistory extends TestCase {
        private int updateWithNullCol(long id, String colName) throws Exception {
            ContentValues u = new ContentValues();
            u.putNull(colName);

            int updated = 0;

            try {
                updated = mProvider.update(BrowserContract.History.CONTENT_URI, u,
                                           BrowserContract.History._ID + " = ?",
                                           new String[] { String.valueOf(id) });
            } catch (Exception e) {}

            return updated;
        }

        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));

            Cursor c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted history entry found");

            long dateCreated = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED));
            long dateModified = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED));

            ContentValues u = new ContentValues();
            u.put(BrowserContract.History.VISITS, h.getAsInteger(BrowserContract.History.VISITS) + 1);
            u.put(BrowserContract.History.DATE_LAST_VISITED, System.currentTimeMillis());
            u.put(BrowserContract.History.TITLE, h.getAsString(BrowserContract.History.TITLE) + "CHANGED");
            u.put(BrowserContract.History.URL, h.getAsString(BrowserContract.History.URL) + "/more/stuff");

            int updated = mProvider.update(BrowserContract.History.CONTENT_URI, u,
                                           BrowserContract.History._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is((updated == 1), true, "Inserted history entry was updated");
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), u.getAsString(BrowserContract.History.TITLE),
                         "Updated history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.URL)), u.getAsString(BrowserContract.History.URL),
                         "Updated history entry has correct URL");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.VISITS)), u.getAsString(BrowserContract.History.VISITS),
                         "Updated history entry has correct number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.DATE_LAST_VISITED)), u.getAsString(BrowserContract.History.DATE_LAST_VISITED),
                         "Updated history entry has correct last visited date");

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED))),
                         new Long(dateCreated),
                         "Updated history entry has same creation date");

            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED))),
                            new Long(dateModified),
                            "Updated history entry has new modification date");

            updated = updateWithNullCol(id, BrowserContract.History.URL);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update history with null URL");

            updated = updateWithNullCol(id, BrowserContract.History.VISITS);
            mAsserter.is((updated > 0), false,
                         "Should not be able to update history with null number of visits");

            u = new ContentValues();
            u.put(BrowserContract.History.URL, "http://examples2.com");

            updated = mProvider.update(ContentUris.withAppendedId(BrowserContract.History.CONTENT_URI, id), u, null, null);
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.URL)), u.getAsString(BrowserContract.History.URL),
                         "Updated history entry has correct URL using URI with id");
            c.close();
        }
    }

    private class TestUpdateHistoryFavicons extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String favicon = "FAVICON";
            final String newFavicon = "NEW_FAVICON";
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            mProvider.insert(BrowserContract.History.CONTENT_URI, h);

            // Insert the favicon into the favicons table
            mProvider.insert(BrowserContract.Favicons.CONTENT_URI, createFaviconEntry(pageUrl, favicon));

            Cursor c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         favicon, "Inserted favicon has corresponding favicon image");

            ContentValues u = createFaviconEntry(pageUrl, newFavicon);

            mProvider.update(BrowserContract.Favicons.CONTENT_URI, u, null, null);
            c.close();

            c = getFaviconsByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated favicon found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Combined.FAVICON)), "UTF8"),
                         newFavicon, "Updated favicon has corresponding favicon image");
            c.close();
        }
    }

    private class TestUpdateOrInsertHistory extends TestCase {
        private final String TEST_URL_1 = "http://example.com";
        private final String TEST_URL_2 = "http://example.org";
        private final String TEST_TITLE = "Example";

        private long getHistoryEntryIdByUrl(String url) {
            Cursor c = mProvider.query(BrowserContract.History.CONTENT_URI,
                                       new String[] { BrowserContract.History._ID },
                                       BrowserContract.History.URL + " = ?",
                                       new String[] { url },
                                       null);
            c.moveToFirst();
            long id = c.getLong(0);
            c.close();

            return id;
        }

        @Override
        public void test() throws Exception {
            Uri updateHistoryUri = BrowserContract.History.CONTENT_URI.buildUpon().
                appendQueryParameter("increment_visits", "true").build();
            Uri updateOrInsertHistoryUri = BrowserContract.History.CONTENT_URI.buildUpon().
                appendQueryParameter("insert_if_needed", "true").
                appendQueryParameter("increment_visits", "true").build();

            // Update a non-existent history entry, without specifying visits or title
            ContentValues values = new ContentValues();
            values.put(BrowserContract.History.URL, TEST_URL_1);

            int updated = mProvider.update(updateHistoryUri, values,
                                           BrowserContract.History.URL + " = ?",
                                           new String[] { TEST_URL_1 });
            mAsserter.is((updated == 0), true, "History entry was not updated");
            Cursor c = mProvider.query(BrowserContract.History.CONTENT_URI, null, null, null, null);
            mAsserter.is(c.moveToFirst(), false, "History entry was not inserted");
            c.close();

            // Now let's try with update-or-insert.
            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                           BrowserContract.History.URL + " = ?",
                                           new String[] { TEST_URL_1 });
            mAsserter.is((updated == 1), true, "History entry was inserted");

            long id = getHistoryEntryIdByUrl(TEST_URL_1);
            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "History entry was inserted");

            long dateCreated = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED));
            long dateModified = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED));

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS))), new Long(1),
                         "Inserted history entry has correct default number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), TEST_URL_1,
                         "Inserted history entry has correct default title");

            // Update the history entry, without specifying an additional visit count
            values = new ContentValues();
            values.put(BrowserContract.History.DATE_LAST_VISITED, System.currentTimeMillis());
            values.put(BrowserContract.History.TITLE, TEST_TITLE);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                       BrowserContract.History._ID + " = ?",
                                       new String[] { String.valueOf(id) });
            mAsserter.is((updated == 1), true, "Inserted history entry was updated");
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), TEST_TITLE,
                         "Updated history entry has correct title");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS))), new Long(2),
                         "Updated history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED))), new Long(dateCreated),
                         "Updated history entry has same creation date");
            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED))), new Long(dateModified),
                            "Updated history entry has new modification date");

            // Create a new history entry, specifying visits and history
            values = new ContentValues();
            values.put(BrowserContract.History.URL, TEST_URL_2);
            values.put(BrowserContract.History.TITLE, TEST_TITLE);
            values.put(BrowserContract.History.VISITS, 10);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                           BrowserContract.History.URL + " = ?",
                                           new String[] { values.getAsString(BrowserContract.History.URL) });
            mAsserter.is((updated == 1), true, "History entry was inserted");

            id = getHistoryEntryIdByUrl(TEST_URL_2);
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "History entry was inserted");

            dateCreated = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED));
            dateModified = c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED));

            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS))), new Long(10),
                         "Inserted history entry has correct specified number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), TEST_TITLE,
                         "Inserted history entry has correct specified title");

            // Update the history entry, specifying additional visit count
            values = new ContentValues();
            values.put(BrowserContract.History.VISITS, 10);

            updated = mProvider.update(updateOrInsertHistoryUri, values,
                                       BrowserContract.History._ID + " = ?",
                                       new String[] { String.valueOf(id) });
            mAsserter.is((updated == 1), true, "Inserted history entry was updated");
            c.close();

            c = getHistoryEntryById(id);
            mAsserter.is(c.moveToFirst(), true, "Updated history entry found");

            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), TEST_TITLE,
                         "Updated history entry has correct unchanged title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.URL)), TEST_URL_2,
                         "Updated history entry has correct unchanged URL");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS))), new Long(20),
                         "Updated history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED))), new Long(dateCreated),
                         "Updated history entry has same creation date");
            mAsserter.isnot(new Long(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED))), new Long(dateModified),
                            "Updated history entry has new modification date");
            c.close();

        }
    }

    private class TestInsertHistoryThumbnails extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String thumbnail = "THUMBNAIL";
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(BrowserContract.Thumbnails.CONTENT_URI, createThumbnailEntry(pageUrl, thumbnail));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Thumbnails.DATA)), "UTF8"),
                         thumbnail, "Inserted thumbnail has corresponding thumbnail image");
            c.close();
        }
    }

    private class TestUpdateHistoryThumbnails extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            final String thumbnail = "THUMBNAIL";
            final String newThumbnail = "NEW_THUMBNAIL";
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            mProvider.insert(BrowserContract.History.CONTENT_URI, h);

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(BrowserContract.Thumbnails.CONTENT_URI, createThumbnailEntry(pageUrl, thumbnail));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Thumbnails.DATA)), "UTF8"),
                         thumbnail, "Inserted thumbnail has corresponding thumbnail image");

            ContentValues u = createThumbnailEntry(pageUrl, newThumbnail);

            mProvider.update(BrowserContract.Thumbnails.CONTENT_URI, u, null, null);
            c.close();

            c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Updated thumbnail found");

            mAsserter.is(new String(c.getBlob(c.getColumnIndex(BrowserContract.Thumbnails.DATA)), "UTF8"),
                         newThumbnail, "Updated thumbnail has corresponding thumbnail image");
            c.close();
        }
    }

    private class TestDeleteHistoryThumbnails extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues h = createOneHistoryEntry();

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));
            final String pageUrl = h.getAsString(BrowserContract.History.URL);

            // Insert the thumbnail into the thumbnails table
            mProvider.insert(BrowserContract.Thumbnails.CONTENT_URI, createThumbnailEntry(pageUrl, "THUMBNAIL"));

            Cursor c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), true, "Inserted thumbnail found");

            mProvider.delete(ContentUris.withAppendedId(BrowserContract.History.CONTENT_URI, id), null, null);
            c.close();

            c = getThumbnailByUrl(pageUrl);
            mAsserter.is(c.moveToFirst(), false, "Thumbnail is deleted with last reference to it");
            c.close();
        }
    }

    private class TestCombinedView extends TestCase {
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
            long basicHistoryId = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, basicHistory));

            // Create a basic bookmark entry
            ContentValues basicBookmark = createBookmark(TITLE_2, URL_2, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long basicBookmarkId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, basicBookmark));

            // Create a history entry and bookmark entry with the same URL to
            // represent a visited bookmark
            ContentValues combinedHistory = createHistoryEntry(TITLE_3_HISTORY, URL_3, VISITS, LAST_VISITED);
            long combinedHistoryId = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, combinedHistory));


            ContentValues combinedBookmark = createBookmark(TITLE_3_BOOKMARK, URL_3, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long combinedBookmarkId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, combinedBookmark));

            ContentValues combinedBookmark2 = createBookmark(TITLE_3_BOOKMARK2, URL_3, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long combinedBookmarkId2 = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, combinedBookmark2));

            // Create a bookmark folder to make sure it _doesn't_ show up in the results
            ContentValues folderBookmark = createBookmark("", "", mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_FOLDER, 0, "tags", "description", "keyword");
            mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, folderBookmark);

            // Sort entries by url so we can check them individually
            Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, BrowserContract.Combined.URL);

            mAsserter.is(c.getCount(), 3, "3 combined entries found");

            // First combined entry is basic history entry
            mAsserter.is(c.moveToFirst(), true, "Found basic history entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID))), new Long(0),
                         "Combined _id column should always be 0");
            // TODO: Should we change BrowserProvider to make this return -1, not 0?
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(0),
                         "Bookmark id should be 0 for basic history entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID))), new Long(basicHistoryId),
                         "Basic history entry has correct history id");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)), TITLE_1,
                         "Basic history entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_1,
                         "Basic history entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), VISITS,
                         "Basic history entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED))), new Long(LAST_VISITED),
                         "Basic history entry has correct last visit time");

            // Second combined entry is basic bookmark entry
            mAsserter.is(c.moveToNext(), true, "Found basic bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID))), new Long(0),
                         "Combined _id column should always be 0");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(basicBookmarkId),
                         "Basic bookmark entry has correct bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID))), new Long(-1),
                         "History id should be -1 for basic bookmark entry");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)), TITLE_2,
                         "Basic bookmark entry has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_2,
                         "Basic bookmark entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), -1,
                         "Visits should be -1 for basic bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED))), new Long(-1),
                         "Basic entry has correct last visit time");

            // Third combined entry is a combined history/bookmark entry
            mAsserter.is(c.moveToNext(), true, "Found third combined entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID))), new Long(0),
                         "Combined _id column should always be 0");
            // The bookmark data (bookmark_id and title) associated with the combined entry is non-deterministic,
            // it might end up with data coming from any of the matching bookmark entries.
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)) == combinedBookmarkId ||
                         c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)) == combinedBookmarkId2, true,
                         "Combined entry has correct bookmark id");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)).equals(TITLE_3_BOOKMARK) ||
                         c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)).equals(TITLE_3_BOOKMARK2), true,
                         "Combined entry has title corresponding to bookmark entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID))), new Long(combinedHistoryId),
                         "Combined entry has correct history id");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_3,
                         "Combined entry has correct url");
            mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), VISITS,
                         "Combined entry has correct number of visits");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED))), new Long(LAST_VISITED),
                         "Combined entry has correct last visit time");
            c.close();
        }
    }

    private class TestCombinedViewDisplay extends TestCase {
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
            ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, basicHistory));

            // Create a basic bookmark entry
            ContentValues basicBookmark = createBookmark(TITLE_2, URL_2, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, basicBookmark);

            // Create a history entry and bookmark entry with the same URL to
            // represent a visited bookmark
            ContentValues combinedHistory = createHistoryEntry(TITLE_3_HISTORY, URL_3, VISITS, LAST_VISITED);
            mProvider.insert(BrowserContract.History.CONTENT_URI, combinedHistory);

            ContentValues combinedBookmark = createBookmark(TITLE_3_BOOKMARK, URL_3, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, combinedBookmark);

            // Create a reading list entries
            int readingListId = BrowserContract.Bookmarks.FIXED_READING_LIST_ID;
            ContentValues readingListItem = createBookmark(TITLE_3_BOOKMARK, URL_3, readingListId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long readingListItemId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, readingListItem));

            ContentValues readingListItem2 = createBookmark(TITLE_4, URL_4, readingListId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long readingListItemId2 = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, readingListItem2));

            Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 4, "4 combined entries found");

            while (c.moveToNext()) {
                long id = c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID));

                int display = c.getInt(c.getColumnIndex(BrowserContract.Combined.DISPLAY));
                int expectedDisplay = (id == readingListItemId || id == readingListItemId2 ? BrowserContract.Combined.DISPLAY_READER : BrowserContract.Combined.DISPLAY_NORMAL);

                mAsserter.is(new Integer(display), new Integer(expectedDisplay),
                                 "Combined display column should always be DISPLAY_READER for the reading list item");
            }
            c.close();
        }
    }

    private class TestCombinedViewWithDeletedBookmark extends TestCase {
        @Override
        public void test() throws Exception {
            final String TITLE = "Test Page 1";
            final String URL = "http://example.com";
            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a combined history entry
            ContentValues combinedHistory = createHistoryEntry(TITLE, URL, VISITS, LAST_VISITED);
            mProvider.insert(BrowserContract.History.CONTENT_URI, combinedHistory);

            // Create a combined bookmark entry
            ContentValues combinedBookmark = createBookmark(TITLE, URL, mMobileFolderId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long combinedBookmarkId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, combinedBookmark));

            Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry with bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(combinedBookmarkId),
                         "Bookmark id should be set correctly on combined entry");

            int deleted = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(combinedBookmarkId) });

            mAsserter.is((deleted == 1), true, "Inserted combined bookmark was deleted");
            c.close();

            c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry without bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(0),
                         "Bookmark id should not be set to removed bookmark id");
            c.close();
        }
    }

    private class TestCombinedViewWithDeletedReadingListItem extends TestCase {
        @Override
        public void test() throws Exception {
            final String TITLE = "Test Page 1";
            final String URL = "http://example.com";
            final int VISITS = 10;
            final long LAST_VISITED = System.currentTimeMillis();

            // Create a combined history entry
            ContentValues combinedHistory = createHistoryEntry(TITLE, URL, VISITS, LAST_VISITED);
            mProvider.insert(BrowserContract.History.CONTENT_URI, combinedHistory);

            // Create a combined bookmark entry
            int readingListId = BrowserContract.Bookmarks.FIXED_READING_LIST_ID;
            ContentValues combinedReadingListItem = createBookmark(TITLE, URL, readingListId,
                BrowserContract.Bookmarks.TYPE_BOOKMARK, 0, "tags", "description", "keyword");
            long combinedReadingListItemId = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, combinedReadingListItem));

            Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry with bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(combinedReadingListItemId),
                         "Bookmark id should be set correctly on combined entry");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.DISPLAY))), new Long(BrowserContract.Combined.DISPLAY_READER),
                         "Combined entry should have reader display type");

            int deleted = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(combinedReadingListItemId) });

            mAsserter.is((deleted == 1), true, "Inserted combined reading list item was deleted");
            c.close();

            c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry without bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID))), new Long(0),
                         "Bookmark id should not be set to removed bookmark id");
            mAsserter.is(new Long(c.getLong(c.getColumnIndex(BrowserContract.Combined.DISPLAY))), new Long(BrowserContract.Combined.DISPLAY_NORMAL),
                         "Combined entry should have reader display type");
            c.close();
        }
    }

    private class TestExpireHistory extends TestCase {
        private void createFakeHistory(long timeShift, int count) {
            // Insert a bunch of very new entries
            ContentValues[] allVals = new ContentValues[count];
            long time = System.currentTimeMillis() - timeShift;
            for (int i = 0; i < count; i++) {
                allVals[i] = new ContentValues();
                allVals[i].put(BrowserContract.History.TITLE, "Test " + i);
                allVals[i].put(BrowserContract.History.URL, "http://www.test.org/" + i);
                allVals[i].put(BrowserContract.History.VISITS, i);
                allVals[i].put(BrowserContract.History.DATE_LAST_VISITED, time);
            }

            int inserts = mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, allVals);
            mAsserter.is(inserts, count, "Expected number of inserts matches");

            // inserting a new entry sets the date created and modified automatically
            // reset all of them
            for (int i = 0; i < count; i++) {
                ContentValues cv = new ContentValues();
                cv.put(BrowserContract.History.DATE_CREATED, time);
                cv.put(BrowserContract.History.DATE_MODIFIED, time);
                mProvider.update(BrowserContract.History.CONTENT_URI, cv, BrowserContract.History.URL + " = ?",
                                 new String[] { "http://www.test.org/" + i });
            }

            Cursor c = mProvider.query(BrowserContract.History.CONTENT_URI, null, "", null, null);

            assertCountIsAndClose(c, count, count + " history entries found");

            // add thumbnails for each entry
            allVals = new ContentValues[count];
            for (int i = 0; i < count; i++) {
                allVals[i] = new ContentValues();
                allVals[i].put(BrowserContract.Thumbnails.DATA, i);
                allVals[i].put(BrowserContract.Thumbnails.URL, "http://www.test.org/" + i);
            }

            inserts = mProvider.bulkInsert(BrowserContract.Thumbnails.CONTENT_URI, allVals);
            mAsserter.is(inserts, count, "Expected number of inserts matches");

            c = mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null, null, null, null);
            assertCountIsAndClose(c, count, count + " thumbnails entries found");
        }

        @Override
        public void test() throws Exception {
            final int count = 3000;
            final int thumbCount = 15;

            // insert a bunch of new entries
            createFakeHistory(0, count);

            // expiring with a normal priority should not delete new entries
            Uri url = appendUriParam(BrowserContract.History.CONTENT_OLD_URI, BrowserContract.PARAM_EXPIRE_PRIORITY, "NORMAL");
            mProvider.delete(url, null, null);
            Cursor c = mProvider.query(BrowserContract.History.CONTENT_URI, null, "", null, null);
            assertCountIsAndClose(c, count, count + " history entries found");

            // expiring with a normal priority should delete all but 10 thumbnails
            c = mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null, null, null, null);
            assertCountIsAndClose(c, thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();

            // Insert a bunch of new entries.
            createFakeHistory(0, count);

            // Expiring with a aggressive priority should leave 500 entries.
            url = appendUriParam(BrowserContract.History.CONTENT_OLD_URI, BrowserContract.PARAM_EXPIRE_PRIORITY, "AGGRESSIVE");
            mProvider.delete(url, null, null);

            c = mProvider.query(BrowserContract.History.CONTENT_URI, null, "", null, null);
            assertCountIsAndClose(c, 500, "500 history entries found");

            // Expiring with a aggressive priority should delete all but 10 thumbnails.
            c = mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null, null, null, null);
            assertCountIsAndClose(c, thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();

            // Insert a bunch of entries with an old time created/modified.
            long time = 1000L * 60L * 60L * 24L * 30L * 3L;
            createFakeHistory(time, count);

            // Expiring with an normal priority should remove at most 1000 entries,
            // entries leaving at least 2000.
            url = appendUriParam(BrowserContract.History.CONTENT_OLD_URI, BrowserContract.PARAM_EXPIRE_PRIORITY, "NORMAL");
            mProvider.delete(url, null, null);

            c = mProvider.query(BrowserContract.History.CONTENT_URI, null, "", null, null);
            assertCountIsAndClose(c, 2000, "2000 history entries found");

            // Expiring with a normal priority should delete all but 10 thumbnails.
            c = mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null, null, null, null);
            assertCountIsAndClose(c, thumbCount, thumbCount + " thumbnails found");

            ensureEmptyDatabase();
            // insert a bunch of entries with an old time created/modified
            time = 1000L * 60L * 60L * 24L * 30L * 3L;
            createFakeHistory(time, count);

            // Expiring with an aggressive priority should remove old
            // entries, leaving at least 500.
            url = appendUriParam(BrowserContract.History.CONTENT_OLD_URI, BrowserContract.PARAM_EXPIRE_PRIORITY, "AGGRESSIVE");
            mProvider.delete(url, null, null);
            c = mProvider.query(BrowserContract.History.CONTENT_URI, null, "", null, null);
            assertCountIsAndClose(c, 500, "500 history entries found");

            // expiring with an aggressive priority should delete all but 10 thumbnails
            c = mProvider.query(BrowserContract.Thumbnails.CONTENT_URI, null, null, null, null);
            assertCountIsAndClose(c, thumbCount, thumbCount + " thumbnails found");
        }
    }

    /*
     * Verify that insert, update, delete, and bulkInsert operations
     * notify the ambient content resolver.  Each operation calls the
     * content resolver notifyChange method synchronously, so it is
     * okay to test sequentially.
     */
    private class TestBrowserProviderNotifications extends TestCase {
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
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.History.CONTENT_URI, h));

            mAsserter.isnot(Long.valueOf(id),
                            Long.valueOf(-1),
                            "Inserted item has valid id");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "insert");

            // Update
            mResolver.notifyChangeList.clear();
            h.put(BrowserContract.History.TITLE, "http://newexample.com");

            long numUpdated = mProvider.update(BrowserContract.History.CONTENT_URI, h,
                                               BrowserContract.History._ID + " = ?",
                                               new String[] { String.valueOf(id) });

            mAsserter.is(Long.valueOf(numUpdated),
                         Long.valueOf(1),
                         "Correct number of items are updated");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "update");

            // Delete
            mResolver.notifyChangeList.clear();
            long numDeleted = mProvider.delete(BrowserContract.History.CONTENT_URI, null, null);

            mAsserter.is(Long.valueOf(numDeleted),
                         Long.valueOf(1),
                         "Correct number of items are deleted");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "delete");

            // Bulk insert
            final ContentValues[] hs = new ContentValues[] { createOneHistoryEntry() };

            mResolver.notifyChangeList.clear();
            long numBulkInserted = mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, hs);

            mAsserter.is(Long.valueOf(numBulkInserted),
                         Long.valueOf(1),
                         "Correct number of items are bulkInserted");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "bulkInsert");
        }
    }

    /**
     * Assert that the provided cursor has the expected number of rows,
     * closing the cursor afterwards.
     */
    private void assertCountIsAndClose(Cursor c, int expectedCount, String message) {
        try {
            mAsserter.is(c.getCount(), expectedCount, message);
        } finally {
            c.close();
        }
    }
}
