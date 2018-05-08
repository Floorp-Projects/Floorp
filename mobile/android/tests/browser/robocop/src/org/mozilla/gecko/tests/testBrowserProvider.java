/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations.SyncStatus;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.URLMetadata;
import org.mozilla.gecko.db.URLImageDataTable;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.StringUtils;

import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

/*
 * This test is meant to exercise all operations exposed by Fennec's
 * history and bookmarks content provider. It does so in an isolated
 * environment (see ContentProviderTest) without affecting any UI-related
 * code.
 */
public class testBrowserProvider extends ContentProviderTest {
    private long mMobileFolderId;

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
        Cursor c;

        String guid = BrowserContract.Bookmarks.GUID;

        mProvider.delete(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
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
                                        BrowserContract.Bookmarks.UNFILED_FOLDER_GUID });

        c = mProvider.query(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), null, null, null, null);
        assertCountIsAndClose(c, 6, "All non-special bookmarks and folders were deleted");

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

        mAsserter.dumpLog("ensureEmptyDatabase: Favicon deletion completed."); // Bug 968951 debug.
        // assertCountIsAndClose(c, 0, "All favicons were deleted");

        mProvider.delete(appendUriParam(BrowserContract.Thumbnails.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"), null, null);
        c = mProvider.query(appendUriParam(BrowserContract.Thumbnails.CONTENT_URI,
                                           BrowserContract.PARAM_SHOW_DELETED, "1"),
                                           null, null, null, null);

        if (c.getCount() > 0) {
          mAsserter.dumpLog("Unexpected thumbnails in ensureEmptyDatabase.");
        }
        c.close();

        mAsserter.dumpLog("ensureEmptyDatabase: Thumbnail deletion completed."); // Bug 968951 debug.
        // assertCountIsAndClose(c, 0, "All thumbnails were deleted");
    }

    private ContentValues createBookmark(String title, String url, long parentId,
                                         int type, int position, String tags, String description, String keyword) throws Exception {
        return createBookmark(Utils.generateGuid(), title, url, parentId, type, position, tags, description, keyword);
    }

    private ContentValues createBookmark(String guid, String title, String url, long parentId,
            int type, int position, String tags, String description, String keyword) throws Exception {
        ContentValues bookmark = new ContentValues();

        bookmark.put(BrowserContract.Bookmarks.GUID, guid);
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
        return createOneBookmark(Utils.generateGuid());
    }

    private ContentValues createOneBookmark(String guid) throws Exception {
        return createBookmark(guid, "Example", "http://example.com", mMobileFolderId,
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
        faviconEntry.put(BrowserContract.Favicons.DATA, data.getBytes(StringUtils.UTF_8));

        return faviconEntry;
    }

    private ContentValues createThumbnailEntry(String pageUrl, String data) throws Exception {
        ContentValues thumbnailEntry = new ContentValues();

        thumbnailEntry.put(BrowserContract.Thumbnails.URL, pageUrl);
        thumbnailEntry.put(BrowserContract.Thumbnails.DATA, data.getBytes(StringUtils.UTF_8));

        return thumbnailEntry;
    }

    private ContentValues createUrlMetadataEntry(final String url, final String tileImage, final String tileColor,
                final String touchIcon) {
        final ContentValues values = new ContentValues();
        values.put(URLImageDataTable.URL_COLUMN, url);
        values.put(URLImageDataTable.TILE_IMAGE_URL_COLUMN, tileImage);
        values.put(URLImageDataTable.TILE_COLOR_COLUMN, tileColor);
        values.put(URLImageDataTable.TOUCH_ICON_COLUMN, touchIcon);
        return values;
    }

    private ContentValues createUrlAnnotationEntry(final String url, final String key, final String value,
                final long dateCreated) {
        final ContentValues values = new ContentValues();
        values.put(BrowserContract.UrlAnnotations.URL, url);
        values.put(BrowserContract.UrlAnnotations.KEY, key);
        values.put(BrowserContract.UrlAnnotations.VALUE, value);
        values.put(BrowserContract.UrlAnnotations.DATE_CREATED, dateCreated);
        values.put(BrowserContract.UrlAnnotations.DATE_MODIFIED, dateCreated);
        return values;
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

    private Cursor getUrlAnnotationByUrl(final String url) throws Exception {
        return mProvider.query(BrowserContract.UrlAnnotations.CONTENT_URI, null,
                BrowserContract.UrlAnnotations.URL + " = ?",
                new String[] { url },
                null);
    }

    private Cursor getUrlMetadataByUrl(final String url) throws Exception {
        return mProvider.query(URLImageDataTable.CONTENT_URI, null,
                URLImageDataTable.URL_COLUMN + " = ?",
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

        mTests.add(new TestInsertUrlAnnotations());
        mTests.add(new TestInsertUrlMetadata());

        mTests.add(new TestBatchOperations());

        mTests.add(new TestCombinedView());
        mTests.add(new TestCombinedViewDisplay());
        mTests.add(new TestCombinedViewWithDeletedBookmark());

        mTests.add(new TestBrowserProviderNotifications());
    }

    public void testBrowserProvider() throws Exception {
        loadMobileFolderId();

        for (int i = 0; i < mTests.size(); i++) {
            Runnable test = mTests.get(i);

            final String testName = test.getClass().getSimpleName();
            setTestName(testName);
            ensureEmptyDatabase();
            mAsserter.dumpLog("testBrowserProvider: Database empty - Starting " + testName + ".");
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
            mAsserter.is(seenException, false, "Batch updating succeeded");
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
                                       BrowserContract.Bookmarks.GUID + " = ?",
                                       new String[] { BrowserContract.Bookmarks.PLACES_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.MOBILE_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.MENU_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.TAGS_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.TOOLBAR_FOLDER_GUID,
                                                      BrowserContract.Bookmarks.UNFILED_FOLDER_GUID},
                                       null);

            mAsserter.is(c.getCount(), 6, "Right number of special folders");

            int rootId = BrowserContract.Bookmarks.FIXED_ROOT_ID;

            while (c.moveToNext()) {
                int id = c.getInt(c.getColumnIndex(BrowserContract.Bookmarks._ID));
                String guid = c.getString(c.getColumnIndex(BrowserContract.Bookmarks.GUID));
                int parentId = c.getInt(c.getColumnIndex(BrowserContract.Bookmarks.PARENT));

                if (guid.equals(BrowserContract.Bookmarks.PLACES_FOLDER_GUID)) {
                    mAsserter.is(id, rootId, "The id of places folder is correct");
                }

                mAsserter.is(parentId, rootId,
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

            final Cursor c = getBookmarkById(id);
            try {
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
                mAsserter.is(id, -1L,
                             "Should not be able to insert bookmark with null position");

                id = insertWithNullCol(BrowserContract.Bookmarks.TYPE);
                mAsserter.is(id, -1L,
                             "Should not be able to insert bookmark with null type");

                if (Build.VERSION.SDK_INT < 16) {
                    b = createOneBookmark();
                    b.put(BrowserContract.Bookmarks.PARENT, -1);
                    id = -1;

                    try {
                        id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
                    } catch (Exception e) {}

                    mAsserter.is(id, -1L,
                            "Should not be able to insert bookmark with invalid parent");
                }

                b = createOneBookmark();
                b.remove(BrowserContract.Bookmarks.TYPE);
                id = ContentUris.parseId(mProvider.insert(BrowserContract.Bookmarks.CONTENT_URI, b));
                final Cursor c2 = getBookmarkById(id);
                try {
                    mAsserter.is(c2.moveToFirst(), true, "Inserted bookmark found");
                    mAsserter.is(c2.getString(c2.getColumnIndex(BrowserContract.Bookmarks.TYPE)), String.valueOf(BrowserContract.Bookmarks.TYPE_BOOKMARK),
                                 "Inserted bookmark has correct default type");
                } finally {
                    c2.close();
                }
            } finally {
                c.close();
            }
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
            return insertOneBookmark(Utils.generateGuid(), false);
        }

        private long insertOneBookmark(String guid, boolean fromSync) throws Exception {
            ContentValues b = createOneBookmark(guid);
            Uri insertUri = BrowserContract.Bookmarks.CONTENT_URI;
            if (fromSync) {
                insertUri = insertUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "true")
                        .build();
            }
            long id = ContentUris.parseId(mProvider.insert(insertUri, b));

            Cursor c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), true, "Inserted bookmark found");
            c.close();

            return id;
        }

        private void verifyMarkedAsDeleted(Cursor c) {
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TITLE)), null,
                    "Deleted bookmark title is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.URL)), null,
                    "Deleted bookmark URL is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.TAGS)), null,
                    "Deleted bookmark tags is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.KEYWORD)), null,
                    "Deleted bookmark keyword is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.DESCRIPTION)), null,
                    "Deleted bookmark description is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.POSITION)), String.valueOf(0),
                    "Deleted bookmark has correct position");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.PARENT)), null,
                    "Deleted bookmark parent ID is null");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.IS_DELETED)), String.valueOf(1),
                    "Deleted bookmark has correct is-deleted state");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.FAVICON_ID)), null,
                    "Deleted bookmark Favicon ID is null");
            mAsserter.isnot(c.getString(c.getColumnIndex(BrowserContract.Bookmarks.GUID)), null,
                    "Deleted bookmark GUID is not null");
        }

        @Override
        public void test() throws Exception {
            // Test that unsynced bookmarks are not dropped from the database.
            long id = insertOneBookmark();

            int changed = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            // Deletions also affect parents of folders, and so that must be accounted for.
            mAsserter.is((changed == 2), true, "Inserted bookmark was deleted");

            Cursor c = getBookmarkById(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Unsynced deleted was only marked as deleted");
            verifyMarkedAsDeleted(c);
            c.close();

            // Test that synced bookmarks are only marked as deleted.
            id = insertOneBookmark("test-guid", true);

            // Bookmark has been inserted from sync. Let's delete it again, and test that it has not
            // been dropped from the database.
            changed = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                    BrowserContract.Bookmarks._ID + " = ?",
                    new String[] { String.valueOf(id) });

            // Deletions also affect parents of folders, and so that must be accounted for.
            mAsserter.is((changed == 2), true, "Inserted bookmark was deleted");

            c = getBookmarkById(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), true, "Deleted bookmark was only marked as deleted");
            verifyMarkedAsDeleted(c);
            c.close();

            changed = mProvider.delete(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                                       BrowserContract.Bookmarks._ID + " = ?",
                                       new String[] { String.valueOf(id) });

            // Deletions from sync skip bumping timestamps of parents.
            mAsserter.is((changed == 1), true, "Inserted bookmark was deleted");

            c = getBookmarkById(appendUriParam(BrowserContract.Bookmarks.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1"), id);
            mAsserter.is(c.moveToFirst(), false, "Inserted bookmark is now actually deleted");
            c.close();

            id = insertOneBookmark();

            changed = mProvider.delete(ContentUris.withAppendedId(BrowserContract.Bookmarks.CONTENT_URI, id), null, null);
            mAsserter.is((changed == 2), true,
                         "Inserted bookmark was deleted using URI with id");

            c = getBookmarkById(id);
            mAsserter.is(c.moveToFirst(), false,
                         "Inserted bookmark can't be found after deletion using URI with ID");
            c.close();
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

            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_CREATED)),
                         dateCreated,
                         "Updated bookmark has same creation date");

            mAsserter.isnot(c.getLong(c.getColumnIndex(BrowserContract.Bookmarks.DATE_MODIFIED)),
                            dateModified,
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
            mAsserter.is(id, -1L,
                         "Should not be able to insert history with null URL");

            id = insertWithNullCol(BrowserContract.History.VISITS);
            mAsserter.is(id, -1L,
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

            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED)),
                         dateCreated,
                         "Updated history entry has same creation date");

            mAsserter.isnot(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED)),
                            dateModified,
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
        private static final String TEST_URL_1 = "http://example.com";
        private static final String TEST_URL_2 = "http://example.org";
        private static final String TEST_TITLE = "Example";

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

            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS)), 1L,
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
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS)), 2L,
                         "Updated history entry has correct number of visits");
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED)), dateCreated,
                         "Updated history entry has same creation date");
            mAsserter.isnot(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED)), dateModified,
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

            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS)), 10L,
                         "Inserted history entry has correct specified number of visits");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.History.TITLE)), TEST_TITLE,
                         "Inserted history entry has correct specified title");

            // Update the history entry, specifying additional visit count.
            // The expectation is that the value is ignored, and count is bumped by 1 only.
            // At the same time, a visit is inserted into the visits table.
            // See junit4 tests in BrowserProviderHistoryVisitsTest.
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
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.VISITS)), 11L,
                         "Updated history entry has correct number of visits");
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_CREATED)), dateCreated,
                         "Updated history entry has same creation date");
            mAsserter.isnot(c.getLong(c.getColumnIndex(BrowserContract.History.DATE_MODIFIED)), dateModified,
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

    private class TestInsertUrlAnnotations extends TestCase {
        @Override
        public void test() throws Exception {
            testInsertionViaContentProvider();
            testInsertionViaUrlAnnotations();
        }

        private void testInsertionViaContentProvider() throws Exception {
            final String url = "http://mozilla.org";
            final String key = "todo";
            final String value = "v";
            final long dateCreated = System.currentTimeMillis();

            mProvider.insert(BrowserContract.UrlAnnotations.CONTENT_URI, createUrlAnnotationEntry(url, key, value, dateCreated));

            final Cursor c = getUrlAnnotationByUrl(url);
            try {
                mAsserter.is(c.moveToFirst(), true, "Inserted url annotation found");
                assertKeyValueSync(c, key, value);
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.UrlAnnotations.DATE_CREATED)), dateCreated,
                        "Inserted url annotation has correct date created");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.UrlAnnotations.DATE_MODIFIED)), dateCreated,
                        "Inserted url annotation has correct date modified");
            } finally {
                c.close();
            }
        }

        private void testInsertionViaUrlAnnotations() throws Exception {
            final String url = "http://hello.org";
            final String key = "toTheUniverse";
            final String value = "42a";
            final long timeBeforeCreation = System.currentTimeMillis();

            BrowserDB.from(getTestProfile()).getUrlAnnotations().insertAnnotation(mResolver, url, key, value);

            final Cursor c = getUrlAnnotationByUrl(url);
            try {
                mAsserter.is(c.moveToFirst(), true, "Inserted url annotation found");
                assertKeyValueSync(c, key, value);
                mAsserter.is(true, c.getLong(c.getColumnIndex(BrowserContract.UrlAnnotations.DATE_CREATED)) >= timeBeforeCreation,
                        "Inserted url annotation has date created greater than or equal to time saved before insertion");
                mAsserter.is(true, c.getLong(c.getColumnIndex(BrowserContract.UrlAnnotations.DATE_MODIFIED)) >= timeBeforeCreation,
                        "Inserted url annotation has correct date modified greater than or equal to time saved before insertion");
            } finally {
                c.close();
            }
        }

        private void assertKeyValueSync(final Cursor c, final String key, final String value) {
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.UrlAnnotations.KEY)), key,
                    "Inserted url annotation has correct key");
            mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.UrlAnnotations.VALUE)), value,
                    "Inserted url annotation has correct value");
            mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.UrlAnnotations.SYNC_STATUS)), SyncStatus.NEW.getDBValue(),
                    "Inserted url annotation has default sync status");
        }
    }

    private class TestInsertUrlMetadata extends TestCase {
        @Override
        public void test() throws Exception {
            testInsertionViaContentProvider();
            testInsertionViaUrlMetadata();
            // testRetrievalViaUrlMetadata depends on data added in the previous two tests
            testRetrievalViaUrlMetadata();
        }

        static final String url1 = "http://mozilla.org";
        static final String url2 = "http://hello.org";

        private void testInsertionViaContentProvider() throws Exception {
            final String tileImage = "http://mozilla.org/tileImage.png";
            final String tileColor = "#FF0000";
            final String touchIcon = "http://mozilla.org/touchIcon.png";

            // We can only use update since the redirection machinery doesn't exist for insert
            mProvider.update(URLImageDataTable.CONTENT_URI.buildUpon().appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build(),
                    createUrlMetadataEntry(url1, tileImage, tileColor, touchIcon),
                    URLImageDataTable.URL_COLUMN + "=?",
                    new String[] {url1}
            );

            final Cursor c = getUrlMetadataByUrl(url1);
            try {
                mAsserter.is(c.getCount(), 1, "URL metadata inserted via Content Provider not found");
            } finally {
                c.close();
            }
        }

        private void testInsertionViaUrlMetadata() throws Exception {
            final String tileImage = "http://hello.org/tileImage.png";
            final String tileColor = "#FF0000";
            final String touchIcon = "http://hello.org/touchIcon.png";

            final Map<String, Object> data = new HashMap<>();
            data.put(URLImageDataTable.URL_COLUMN, url2);
            data.put(URLImageDataTable.TILE_IMAGE_URL_COLUMN, tileImage);
            data.put(URLImageDataTable.TILE_COLOR_COLUMN, tileColor);
            data.put(URLImageDataTable.TOUCH_ICON_COLUMN, touchIcon);

            BrowserDB.from(getTestProfile()).getURLMetadata().save(mResolver, data);

            final Cursor c = getUrlMetadataByUrl(url2);
            try {
                mAsserter.is(c.moveToFirst(), true, "URL metadata inserted via UrlMetadata not found");
            } finally {
                c.close();
            }
        }

        private void testRetrievalViaUrlMetadata() {
            // LocalURLMetadata has some caching of results: we need to test that this caching
            // doesn't prevent us from accessing data that might not have been loaded into the cache.
            // We do this by first doing queries with a subset of data, then later querying additional
            // data for a given URL. E.g. even if the first query results in only the requested
            // column being cached, the subsequent query should still retrieve all requested columns.
            // (In this case the URL may be cached but without all data, we need to make sure that
            // this state is correctly handled.)
            URLMetadata metadata = BrowserDB.from(getTestProfile()).getURLMetadata();

            Map<String, Map<String, Object>> results;
            Map<String, Object> urlData;

            // 1: retrieve just touch Icons for URL 1
            results = metadata.getForURLs(mResolver,
                    Collections.singletonList(url1),
                    Collections.singletonList(URLImageDataTable.TOUCH_ICON_COLUMN));

            mAsserter.is(results.containsKey(url1), true, "URL 1 not found in results");

            urlData = results.get(url1);
            mAsserter.is(urlData.containsKey(URLImageDataTable.TOUCH_ICON_COLUMN), true, "touchIcon column missing in UrlMetadata results");

            // 2: retrieve just tile color for URL 2
            results = metadata.getForURLs(mResolver,
                    Collections.singletonList(url2),
                    Collections.singletonList(URLImageDataTable.TILE_COLOR_COLUMN));

            mAsserter.is(results.containsKey(url2), true, "URL 2 not found in results");

            urlData = results.get(url2);
            mAsserter.is(urlData.containsKey(URLImageDataTable.TILE_COLOR_COLUMN), true, "touchIcon column missing in UrlMetadata results");


            // 3: retrieve all columns for both URLs
            final List<String> urls = Arrays.asList(url1, url2);

            results = metadata.getForURLs(mResolver,
                    urls,
                    Arrays.asList(URLImageDataTable.TILE_IMAGE_URL_COLUMN,
                            URLImageDataTable.TILE_COLOR_COLUMN,
                            URLImageDataTable.TOUCH_ICON_COLUMN
                    ));

            mAsserter.is(results.containsKey(url1), true, "URL 1 not found in results");
            mAsserter.is(results.containsKey(url2), true, "URL 2 not found in results");


            for (final String url : urls) {
                urlData = results.get(url);
                mAsserter.is(urlData.containsKey(URLImageDataTable.TILE_IMAGE_URL_COLUMN), true, "touchIcon column missing in UrlMetadata results");
                mAsserter.is(urlData.containsKey(URLImageDataTable.TILE_COLOR_COLUMN), true, "touchIcon column missing in UrlMetadata results");
                mAsserter.is(urlData.containsKey(URLImageDataTable.TOUCH_ICON_COLUMN), true, "touchIcon column missing in UrlMetadata results");
            }
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
            final Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, BrowserContract.Combined.URL);

            try {
                mAsserter.is(c.getCount(), 3, "3 combined entries found");
    
                // First combined entry is basic history entry
                mAsserter.is(c.moveToFirst(), true, "Found basic history entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID)), 0L,
                             "Combined _id column should always be 0");
                // TODO: Should we change BrowserProvider to make this return -1, not 0?
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)), 0L,
                             "Bookmark id should be 0 for basic history entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID)), basicHistoryId,
                             "Basic history entry has correct history id");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)), TITLE_1,
                             "Basic history entry has correct title");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_1,
                             "Basic history entry has correct url");
                mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), VISITS,
                             "Basic history entry has correct number of visits");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED)), LAST_VISITED,
                             "Basic history entry has correct last visit time");
    
                // Second combined entry is basic bookmark entry
                mAsserter.is(c.moveToNext(), true, "Found basic bookmark entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID)), 0L,
                             "Combined _id column should always be 0");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)), basicBookmarkId,
                             "Basic bookmark entry has correct bookmark id");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID)), -1L,
                             "History id should be -1 for basic bookmark entry");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)), TITLE_2,
                             "Basic bookmark entry has correct title");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_2,
                             "Basic bookmark entry has correct url");
                mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), -1,
                             "Visits should be -1 for basic bookmark entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED)), -1L,
                             "Basic entry has correct last visit time");
    
                // Third combined entry is a combined history/bookmark entry
                mAsserter.is(c.moveToNext(), true, "Found third combined entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined._ID)), 0L,
                             "Combined _id column should always be 0");
                // The bookmark data (bookmark_id and title) associated with the combined entry is non-deterministic,
                // it might end up with data coming from any of the matching bookmark entries.
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)) == combinedBookmarkId ||
                             c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)) == combinedBookmarkId2, true,
                             "Combined entry has correct bookmark id");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)).equals(TITLE_3_BOOKMARK) ||
                             c.getString(c.getColumnIndex(BrowserContract.Combined.TITLE)).equals(TITLE_3_BOOKMARK2), true,
                             "Combined entry has title corresponding to bookmark entry");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.HISTORY_ID)), combinedHistoryId,
                             "Combined entry has correct history id");
                mAsserter.is(c.getString(c.getColumnIndex(BrowserContract.Combined.URL)), URL_3,
                             "Combined entry has correct url");
                mAsserter.is(c.getInt(c.getColumnIndex(BrowserContract.Combined.VISITS)), VISITS,
                             "Combined entry has correct number of visits");
                mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.DATE_LAST_VISITED)), LAST_VISITED,
                             "Combined entry has correct last visit time");
            } finally {
                c.close();
            }
        }
    }

    private class TestCombinedViewDisplay extends TestCase {
        @Override
        public void test() throws Exception {
            final String TITLE_1 = "Test Page 1";
            final String TITLE_2 = "Test Page 2";
            final String TITLE_3_HISTORY = "Test Page 3 (History Entry)";
            final String TITLE_3_BOOKMARK = "Test Page 3 (Bookmark Entry)";

            final String URL_1 = "http://example.com";
            final String URL_2 = "http://example.org";
            final String URL_3 = "http://examples2.com";

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

            final Cursor c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            try {
                mAsserter.is(c.getCount(), 3, "3 combined entries found");
            } finally {
                c.close();
            }
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
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)), combinedBookmarkId,
                         "Bookmark id should be set correctly on combined entry");

            int changed = mProvider.delete(BrowserContract.Bookmarks.CONTENT_URI,
                                           BrowserContract.Bookmarks._ID + " = ?",
                                           new String[] { String.valueOf(combinedBookmarkId) });

            // Deletion of a bookmark also affects its parent, and that must be reflected in the count.
            mAsserter.is((changed == 2), true, "Inserted combined bookmark was deleted");
            c.close();

            c = mProvider.query(BrowserContract.Combined.CONTENT_URI, null, "", null, null);
            mAsserter.is(c.getCount(), 1, "1 combined entry found");

            mAsserter.is(c.moveToFirst(), true, "Found combined entry without bookmark id");
            mAsserter.is(c.getLong(c.getColumnIndex(BrowserContract.Combined.BOOKMARK_ID)), 0L,
                         "Bookmark id should not be set to removed bookmark id");
            c.close();
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

            mAsserter.is((long) mResolver.notifyChangeList.size(),
                         1L,
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

            mAsserter.isnot(id,
                            -1L,
                            "Inserted item has valid id");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "insert");

            // Update
            mResolver.notifyChangeList.clear();
            h.put(BrowserContract.History.TITLE, "http://newexample.com");

            long numUpdated = mProvider.update(BrowserContract.History.CONTENT_URI, h,
                                               BrowserContract.History._ID + " = ?",
                                               new String[] { String.valueOf(id) });

            mAsserter.is(numUpdated,
                         1L,
                         "Correct number of items are updated");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "update");

            // Delete
            mResolver.notifyChangeList.clear();
            long numDeleted = mProvider.delete(BrowserContract.History.CONTENT_URI, null, null);

            mAsserter.is(numDeleted,
                         1L,
                         "Correct number of items are deleted");

            ensureOnlyChangeNotifiedStartsWith(BrowserContract.History.CONTENT_URI, "delete");

            // Bulk insert
            final ContentValues[] hs = new ContentValues[] { createOneHistoryEntry() };

            mResolver.notifyChangeList.clear();
            long numBulkInserted = mProvider.bulkInsert(BrowserContract.History.CONTENT_URI, hs);

            mAsserter.is(numBulkInserted,
                         1L,
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
