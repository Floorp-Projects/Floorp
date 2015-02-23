/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.HashSet;
import java.util.Random;
import java.util.concurrent.Callable;

import android.app.Activity;
import android.content.ContentResolver;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.ReadingListAccessor;
import org.mozilla.gecko.db.ReadingListProvider;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.*;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.ADDED_ON;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.CLIENT_LAST_MODIFIED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.EXCERPT;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.GUID;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.IS_UNREAD;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.RESOLVED_TITLE;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.RESOLVED_URL;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SERVER_LAST_MODIFIED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SERVER_STORED_ON;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_FLAGS;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_NONE;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_RESOLVED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_CHANGE_UNREAD_CHANGED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS_MODIFIED;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS_NEW;
import static org.mozilla.gecko.db.BrowserContract.ReadingListItems.SYNC_STATUS_SYNCED;
import static org.mozilla.gecko.db.BrowserContract.URLColumns.TITLE;
import static org.mozilla.gecko.db.BrowserContract.URLColumns.URL;

public class testReadingListProvider extends ContentProviderTest {

    private static final String DB_NAME = "browser.db";

    // List of tests to be run sorted by dependency.
    private final TestCase[] TESTS_TO_RUN = {
            new TestInsertItems(),
            new TestDeleteItems(),
            new TestUpdateItems(),
            new TestBatchOperations(),
            new TestBrowserProviderNotifications(),
            new TestStateSequencing(),
    };

    // Columns used to test for item equivalence.
    final String[] TEST_COLUMNS = { TITLE,
                                    URL,
                                    EXCERPT,
                                    ADDED_ON };

    // Indicates that insertions have been tested. ContentProvider.insert
    // has been proven to work.
    private boolean mContentProviderInsertTested = false;

    // Indicates that updates have been tested. ContentProvider.update
    // has been proven to work.
    private boolean mContentProviderUpdateTested = false;

    /**
     * Factory function that makes new ReadingListProvider instances.
     * <p>
     * We want a fresh provider each test, so this should be invoked in
     * <code>setUp</code> before each individual test.
     */
    private static final Callable<ContentProvider> sProviderFactory = new Callable<ContentProvider>() {
        @Override
        public ContentProvider call() {
            return new ReadingListProvider();
        }
    };

    @Override
    public void setUp() throws Exception {
        super.setUp(sProviderFactory, BrowserContract.READING_LIST_AUTHORITY, DB_NAME);
        for (TestCase test: TESTS_TO_RUN) {
            mTests.add(test);
        }

        // Disable background fetches of content, because it causes the test harness
        // to kill us for network fetches.
        Activity a = getActivity();
        if (a instanceof BrowserApp) {
            ((BrowserApp) a).getReadingListHelper().disableBackgroundFetches();
        }
    }

    public void testReadingListProviderTests() throws Exception {
        for (Runnable test : mTests) {
            setTestName(test.getClass().getSimpleName());
            ensureEmptyDatabase();
            test.run();
        }

        // Ensure browser initialization is complete before completing test,
        // so that the minidumps directory is consistently created.
        blockForGeckoReady();
    }

    /**
     * Verify that we can insert a reading list item into the DB.
     */
    private class TestInsertItems extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues b = createFillerReadingListItem();
            long id = ContentUris.parseId(mProvider.insert(CONTENT_URI, b));
            Cursor c = getItemById(id);

            try {
                mAsserter.ok(c.moveToFirst(), "Inserted item found", "");
                assertRowEqualsContentValues(c, b);

                mAsserter.is(c.getInt(c.getColumnIndex(CONTENT_STATUS)),
                             STATUS_UNFETCHED,
                             "Inserted item has correct default content status");
            } finally {
                c.close();
            }

            testInsertWithNullCol(URL);
            mContentProviderInsertTested = true;
        }

        /**
         * Test that insertion fails when a required column
         * is null.
         */
        private void testInsertWithNullCol(String colName) {
            ContentValues b = createFillerReadingListItem();
            b.putNull(colName);

            try {
                ContentUris.parseId(mProvider.insert(CONTENT_URI, b));
                // If we get to here, the flawed insertion succeeded. Fail the test.
                mAsserter.ok(false, "Insertion did not succeed with " + colName + " == null", "");
            } catch (NullPointerException e) {
                // Indicates test was successful.
            }
        }
    }

    /**
     * Verify that we can remove a reading list item from the DB.
     */
    private class TestDeleteItems extends TestCase {

        @Override
        public void test() throws Exception {
            final long one = insertAnItemWithAssertion();
            final long two = insertAnItemWithAssertion();

            assignGUID(one);

            // Test that the item with a GUID is only marked as deleted and
            // not removed from the database.
            testNonSyncDelete(one, true);

            // The item without a GUID is just deleted immediately.
            testNonSyncDelete(two, false);

            // Test that the item with a GUID is removed from the database when deleted by Sync.
            testSyncDelete(one);

            final long three = insertAnItemWithAssertion();

            // Test that deleting works with only a URI.
            testDeleteWithItemURI(three);
        }

        private void assignGUID(final long id) {
            final ContentValues values = new ContentValues();
            values.put(GUID, "abcdefghi");
            mProvider.update(CONTENT_URI, values, _ID + " = " + id, null);
        }

        /**
         * Delete an item with PARAM_IS_SYNC unset and verify that item was only marked
         * as deleted and not actually removed from the database. Also verify that the item
         * marked as deleted doesn't show up in a query.
         *
         * Note that items are deleted immediately if they don't have a GUID.
         *
         * @param id of the item to be deleted
         * @param hasGUID if true, we expect the item to stick around and be marked.
         */
        private void testNonSyncDelete(long id, boolean hasGUID) {
            final int deleted = mProvider.delete(CONTENT_URI,
                                                 _ID + " = " + id,
                                                 null);

            mAsserter.is(deleted, 1, "Inserted item was deleted");

            // PARAM_SHOW_DELETED in the URI allows items marked as deleted to be
            // included in the query.
            Uri uri = appendUriParam(CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1");
            if (hasGUID) {
                assertItemExistsByID(uri, id, "Deleted item was only marked as deleted");
            } else {
                assertItemDoesNotExistByID(uri, id, "Deleted item had no GUID, so was really deleted.");
            }

            // Test that the 'deleted' item does not show up in a query when PARAM_SHOW_DELETED
            // is not specified in the URI.
            assertItemDoesNotExistByID(id, "Inserted item can't be found after deletion");
        }

        /**
         * Delete an item with PARAM_IS_SYNC=1 and verify that item
         * was actually removed from the database.
         *
         * @param id of the item to be deleted
         */
        private void testSyncDelete(long id) {
            final int deleted = mProvider.delete(appendUriParam(CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                    _ID + " = " + id,
                    null);

            mAsserter.is(deleted, 1, "Inserted item was deleted");

            Uri uri = appendUriParam(CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1");
            assertItemDoesNotExistByID(uri, id, "Inserted item is now actually deleted");
        }

        /**
         * Delete an item with its URI and verify that the item
         * was actually removed from the database.
         *
         * @param id of the item to be deleted
         */
        private void testDeleteWithItemURI(long id) {
            final int deleted = mProvider.delete(ContentUris.withAppendedId(CONTENT_URI, id), null, null);
            mAsserter.is(deleted, 1, "Inserted item was deleted using URI with id");
        }
    }

    /**
     * Verify that we can update reading list items.
     */
    private class TestUpdateItems extends TestCase {

        @Override
        public void test() throws Exception {
            // We should be able to insert into the DB.
            ensureCanInsert();

            ContentValues original = createFillerReadingListItem();
            long id = ContentUris.parseId(mProvider.insert(CONTENT_URI, original));
            int updated = 0;
            Long originalDateCreated = null;
            Long originalDateModified = null;
            ContentValues updates = new ContentValues();
            Cursor c = getItemById(id);
            try {
                mAsserter.ok(c.moveToFirst(), "Inserted item found", "");

                originalDateCreated = c.getLong(c.getColumnIndex(ADDED_ON));
                originalDateModified = c.getLong(c.getColumnIndex(CLIENT_LAST_MODIFIED));

                updates.put(TITLE, original.getAsString(TITLE) + "CHANGED");
                updates.put(URL, original.getAsString(URL) + "/more/stuff");
                updates.put(EXCERPT, original.getAsString(EXCERPT) + "CHANGED");

                updated = mProvider.update(CONTENT_URI, updates,
                                           _ID + " = ?",
                                           new String[] { String.valueOf(id) });

                mAsserter.is(updated, 1, "Inserted item was updated");
            } finally {
                c.close();
            }

            // Name change for clarity. These values will be compared with the
            // current cursor row.
            final ContentValues expectedValues = updates;
            c = getItemById(id);
            try {
                mAsserter.ok(c.moveToFirst(), "Updated item found", "");
                mAsserter.isnot(c.getLong(c.getColumnIndex(CLIENT_LAST_MODIFIED)),
                originalDateModified,
                "Date modified should have changed");

                // ADDED_ON shouldn't have changed.
                expectedValues.put(ADDED_ON, originalDateCreated);
                assertRowEqualsContentValues(c, expectedValues, /* compareDateModified */ false, TEST_COLUMNS);
            } finally {
                c.close();
            }

            // Test that updates on an item that doesn't exist does not modify any rows.
            testUpdateWithInvalidID();

            mContentProviderUpdateTested = true;
        }

        /**
         * Test that updates on an item that doesn't exist does
         * not modify any rows.
         */
        private void testUpdateWithInvalidID() {
            ensureEmptyDatabase();
            final ContentValues b = createFillerReadingListItem();
            final long id = ContentUris.parseId(mProvider.insert(CONTENT_URI, b));
            final long INVALID_ID = id + 1;
            final ContentValues updates = new ContentValues();
            updates.put(TITLE, b.getAsString(TITLE) + "CHANGED");
            final int updated = mProvider.update(CONTENT_URI, updates,
                                                 _ID + " = ?",
                                                 new String[] { String.valueOf(INVALID_ID) });
            mAsserter.is(updated, 0, "Should not be able to update item with an invalid ID");
        }
    }

    private class TestBatchOperations extends TestCase {
        private static final int ITEM_COUNT = 10;

        /**
         * Insert a bunch of items into the DB with the bulkInsert
         * method and verify that they are there.
         */
        private void testBulkInsert() {
            ensureEmptyDatabase();
            final ContentValues allVals[] = new ContentValues[ITEM_COUNT];
            final HashSet<String> urls = new HashSet<String>();
            for (int i = 0; i < ITEM_COUNT; i++) {
                final String url =  "http://www.test.org/" + i;
                allVals[i] = new ContentValues();
                allVals[i].put(TITLE, "Test" + i);
                allVals[i].put(URL, url);
                allVals[i].put(EXCERPT, "EXCERPT" + i);
                urls.add(url);
            }

            int inserts = mProvider.bulkInsert(CONTENT_URI, allVals);
            mAsserter.is(inserts, ITEM_COUNT, "Excepted number of inserts matches");

            final Cursor c = mProvider.query(CONTENT_URI, null,
                                             null,
                                             null,
                                             null);
            try {
                while (c.moveToNext()) {
                    final String url = c.getString(c.getColumnIndex(URL));
                    mAsserter.ok(urls.contains(url), "Bulk inserted item with url == " + url + " was found in the DB", "");
                    // We should only be seeing each item once. Remove from set to prevent dups.
                    urls.remove(url);
                }
            } finally {
                c.close();
            }
        }

        @Override
        public void test() {
            testBulkInsert();
        }
    }

    /*
     * Verify that insert, update, delete, and bulkInsert operations
     * notify the ambient content resolver. Each operation calls the
     * content resolver notifyChange method synchronously, so it is
     * okay to test sequentially.
     */
    private class TestBrowserProviderNotifications extends TestCase {

        @Override
        public void test() {
            // We should be able to insert into the DB.
            ensureCanInsert();
            // We should be able to update the DB.
            ensureCanUpdate();

            final String CONTENT_URI = ReadingListItems.CONTENT_URI.toString();

            mResolver.notifyChangeList.clear();

            // Insert
            final ContentValues h = createFillerReadingListItem();
            long id = ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, h));

            mAsserter.isnot(id,
                            -1L,
                            "Inserted item has valid id");

            ensureOnlyChangeNotifiedStartsWith(CONTENT_URI, "insert");

            // Update
            mResolver.notifyChangeList.clear();
            h.put(TITLE, "http://newexample.com");

            long numUpdated = mProvider.update(ReadingListItems.CONTENT_URI, h,
                                               _ID + " = ?",
                                               new String[] { String.valueOf(id) });

            mAsserter.is(numUpdated,
                         1L,
                         "Correct number of items are updated");

            ensureOnlyChangeNotifiedStartsWith(CONTENT_URI, "update");

            // Delete
            mResolver.notifyChangeList.clear();
            long numDeleted = mProvider.delete(ReadingListItems.CONTENT_URI, null, null);

            mAsserter.is(numDeleted,
                         1L,
                         "Correct number of items are deleted");

            ensureOnlyChangeNotifiedStartsWith(CONTENT_URI, "delete");

            // Bulk insert
            mResolver.notifyChangeList.clear();
            final ContentValues[] hs = { createFillerReadingListItem(),
                                         createFillerReadingListItem(),
                                         createFillerReadingListItem() };

            long numBulkInserted = mProvider.bulkInsert(ReadingListItems.CONTENT_URI, hs);

            mAsserter.is(numBulkInserted,
                         3L,
                         "Correct number of items are bulkInserted");

            ensureOnlyChangeNotifiedStartsWith(CONTENT_URI, "bulkInsert");
        }

        protected void ensureOnlyChangeNotifiedStartsWith(String expectedUri, String operation) {
            mAsserter.is(Long.valueOf(mResolver.notifyChangeList.size()),
                         1L,
                         "Content observer was notified exactly once by " + operation);

            final Uri uri = mResolver.notifyChangeList.poll();

            mAsserter.isnot(uri,
                            null,
                            "Notification from " + operation + " was valid");

            mAsserter.ok(uri.toString().startsWith(expectedUri),
                         "Content observer was notified exactly once by " + operation,
                         "");
        }
    }

    private class TestStateSequencing extends TestCase {
        @Override
        protected void test() throws Exception {
            final ReadingListAccessor accessor = getTestProfile().getDB().getReadingListAccessor();
            final ContentResolver cr = getActivity().getContentResolver();
            final Uri syncURI = CONTENT_URI.buildUpon()
                                           .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1")
                                           .appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1")
                                           .build();

            mAsserter.ok(accessor != null, "We have an accessor.", null);

            // Verify that the accessor thinks we're empty.
            mAsserter.ok(0 == accessor.getCount(cr), "We have no items.", null);

            // Insert an item via the accessor.
            final long addedItem = accessor.addBasicReadingListItem(cr, "http://example.org/", "Example A");

            mAsserter.ok(1 == accessor.getCount(cr), "We have one item.", null);
            final Cursor cursor = accessor.getReadingList(cr);
            try {
                mAsserter.ok(cursor.moveToNext(), "The cursor isn't empty.", null);
                mAsserter.ok(1 == cursor.getCount(), "The cursor agrees.", null);
            } finally {
                cursor.close();
            }

            // Verify that it has no GUID, that its state is NEW, etc.
            // This requires fetching more fields than the accessor uses.
            final Cursor all = getEverything(syncURI);
            try {
                mAsserter.ok(all.moveToNext(), "The cursor isn't empty.", null);
                mAsserter.ok(1 == all.getCount(), "The cursor agrees.", null);

                ContentValues expected = new ContentValues();
                expected.putNull(GUID);
                expected.put(SYNC_STATUS, SYNC_STATUS_NEW);
                expected.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
                expected.put(URL, "http://example.org/");
                expected.put(TITLE, "Example A");
                expected.putNull(RESOLVED_URL);
                expected.putNull(RESOLVED_TITLE);

                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, URL, TITLE, RESOLVED_URL, RESOLVED_TITLE, URL, TITLE};
                assertRowEqualsContentValues(all, expected, false, testColumns);
            } finally {
                all.close();
            }

            // Pretend that it was just synced.
            final long serverTime = System.currentTimeMillis();
            final ContentValues wasSynced = new ContentValues();
            wasSynced.put(GUID, "eeeeeeeeeeeeee");
            wasSynced.put(SYNC_STATUS, SYNC_STATUS_SYNCED);
            wasSynced.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
            wasSynced.put(SERVER_STORED_ON, serverTime);
            wasSynced.put(SERVER_LAST_MODIFIED, serverTime);

            mAsserter.ok(1 == mProvider.update(syncURI, wasSynced, _ID + " = " + addedItem, null), "Updated one item.", null);
            final Cursor afterSync = getEverything(syncURI);
            try {
                mAsserter.ok(afterSync.moveToNext(), "The cursor isn't empty.", null);
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, SERVER_STORED_ON, SERVER_LAST_MODIFIED};
                assertRowEqualsContentValues(afterSync, wasSynced, false, testColumns);
            } finally {
                afterSync.close();
            }

            // Make changes to the record that exercise the various change flags, verifying that
            // the correct flags are set.
            final long beforeMarkedRead = System.currentTimeMillis();
            accessor.markAsRead(cr, addedItem);
            final ContentValues markedAsRead = new ContentValues();
            markedAsRead.put(GUID, "eeeeeeeeeeeeee");
            markedAsRead.put(SYNC_STATUS, SYNC_STATUS_MODIFIED);
            markedAsRead.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_UNREAD_CHANGED);
            markedAsRead.put(IS_UNREAD, 0);

            final Cursor afterMarkedRead = getEverything(syncURI);
            try {
                mAsserter.ok(afterMarkedRead.moveToNext(), "The cursor isn't empty.", null);
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, IS_UNREAD};
                assertRowEqualsContentValues(afterMarkedRead, markedAsRead, false, testColumns);
                assertModifiedInRange(afterMarkedRead, beforeMarkedRead);
            } finally {
                afterMarkedRead.close();
            }

            // Now our content is here!
            final long beforeContentUpdated = System.currentTimeMillis();
            accessor.updateContent(cr, addedItem, "New title", "http://www.example.com/article", "The excerpt is long.");

            // After this the content status should have changed, and we should be flagged to sync.
            final ContentValues contentUpdated = new ContentValues();
            contentUpdated.put(GUID, "eeeeeeeeeeeeee");
            contentUpdated.put(SYNC_STATUS, SYNC_STATUS_MODIFIED);
            contentUpdated.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_UNREAD_CHANGED | SYNC_CHANGE_RESOLVED);
            contentUpdated.put(IS_UNREAD, 0);
            contentUpdated.put(CONTENT_STATUS, STATUS_FETCHED_ARTICLE);

            final Cursor afterContentUpdated = getEverything(syncURI);
            try {
                mAsserter.ok(afterContentUpdated.moveToNext(), "The cursor isn't empty.", null);
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, IS_UNREAD, CONTENT_STATUS};
                assertRowEqualsContentValues(afterContentUpdated, contentUpdated, false, testColumns);
                assertModifiedInRange(afterContentUpdated, beforeContentUpdated);
            } finally {
                afterContentUpdated.close();
            }

            // Delete the record, and verify that its Sync state is DELETED.
            final long beforeDeletion = System.currentTimeMillis();
            accessor.deleteItem(cr, addedItem);
            final ContentValues itemDeleted = new ContentValues();
            itemDeleted.put(GUID, "eeeeeeeeeeeeee");
            itemDeleted.put(SYNC_STATUS, SYNC_STATUS_DELETED);
            itemDeleted.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
            // TODO: CONTENT_STATUS on deletion?

            final Cursor afterDeletion = getEverything(syncURI);
            try {
                mAsserter.ok(afterDeletion.moveToNext(), "The cursor isn't empty.", null);
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS};
                assertRowEqualsContentValues(afterDeletion, itemDeleted, false, testColumns);
                assertModifiedInRange(afterDeletion, beforeDeletion);
            } finally {
                afterDeletion.close();
            }

            // The accessor will no longer return the record.
            mAsserter.ok(0 == accessor.getCount(cr), "No items found.", null);

            // Add a new record as Sync -- it should start in state SYNCED.
            final ContentValues newRecord = new ContentValues();
            final long newServerTime = System.currentTimeMillis() - 50000;
            newRecord.put(GUID, "ffeeeeeeeeeeee");
            newRecord.put(SYNC_STATUS, SYNC_STATUS_SYNCED);
            newRecord.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_NONE);
            newRecord.put(SERVER_STORED_ON, newServerTime);
            newRecord.put(SERVER_LAST_MODIFIED, newServerTime);
            newRecord.put(CLIENT_LAST_MODIFIED, System.currentTimeMillis());
            newRecord.put(URL, "http://www.mozilla.org/");
            newRecord.put(TITLE, "Mozilla");

            final long newID = ContentUris.parseId(cr.insert(syncURI, newRecord));
            mAsserter.ok(newID > 0, "New ID is greater than 0.", null);
            mAsserter.ok(newID != addedItem, "New ID differs from last ID.", null);

            final Cursor afterNewInsert = getEverything(syncURI);
            try {
                mAsserter.ok(afterNewInsert.moveToNext(), "The cursor isn't empty.", null);
                mAsserter.ok(2 == afterNewInsert.getCount(), "The cursor has two rows.", null);

                // Default sort order means newest first.
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, SERVER_STORED_ON, SERVER_LAST_MODIFIED, CLIENT_LAST_MODIFIED, URL, TITLE};
                assertRowEqualsContentValues(afterNewInsert, newRecord, false, testColumns);
            } finally {
                afterNewInsert.close();
            }

            // Make a change to it. Verify that it's now changed with the right flags.
            final long beforeNewRead = System.currentTimeMillis();
            accessor.markAsRead(cr, newID);
            newRecord.put(SYNC_STATUS, SYNC_STATUS_MODIFIED);
            newRecord.put(SYNC_CHANGE_FLAGS, SYNC_CHANGE_UNREAD_CHANGED);

            final Cursor afterNewRead = getEverything(syncURI);
            try {
                mAsserter.ok(afterNewRead.moveToNext(), "The cursor isn't empty.", null);
                mAsserter.ok(2 == afterNewRead.getCount(), "The cursor has two rows.", null);

                // Default sort order means newest first.
                final String[] testColumns = {GUID, SYNC_STATUS, SYNC_CHANGE_FLAGS, SERVER_STORED_ON, SERVER_LAST_MODIFIED, URL, TITLE};
                assertRowEqualsContentValues(afterNewRead, newRecord, false, testColumns);
                assertModifiedInRange(afterNewRead, beforeNewRead);
            } finally {
                afterNewRead.close();
            }
        }

        private void assertModifiedInRange(Cursor cursor, long earliest) {
            final long dbModified = cursor.getLong(cursor.getColumnIndexOrThrow(CLIENT_LAST_MODIFIED));
            mAsserter.ok(dbModified >= earliest, "DB timestamp is at least as late as earliest.", null);
            mAsserter.ok(dbModified <= System.currentTimeMillis(), "DB timestamp is earlier than now.", null);
        }
    }

    /**
     * Removes all items from the DB.
     */
    private void ensureEmptyDatabase() {
        getWritableDatabase(CONTENT_URI).delete(TABLE_NAME, null, null);
    }


    private SQLiteDatabase getWritableDatabase(Uri uri) {
        Uri testUri = appendUriParam(uri, BrowserContract.PARAM_IS_TEST, "1");
        DelegatingTestContentProvider delegateProvider = (DelegatingTestContentProvider) mProvider;
        ReadingListProvider readingListProvider = (ReadingListProvider) delegateProvider.getTargetProvider();
        return readingListProvider.getWritableDatabaseForTesting(testUri);
    }

    /**
     * Checks that the values in the cursor's current row match those
     * in the ContentValues object.
     *
     * @param testColumns
     * @param cursorWithActual over the row to be checked
     * @param expectedValues to be checked
     */
    private void assertRowEqualsContentValues(Cursor cursorWithActual, ContentValues expectedValues, boolean compareDateModified, String[] testColumns) {
        for (String column: testColumns) {
            String expected = expectedValues.getAsString(column);
            String actual = cursorWithActual.getString(cursorWithActual.getColumnIndex(column));
            mAsserter.is(actual, expected, "Item has correct " + column);
        }

        if (compareDateModified) {
            String expected = expectedValues.getAsString(CLIENT_LAST_MODIFIED);
            String actual = cursorWithActual.getString(cursorWithActual.getColumnIndex(CLIENT_LAST_MODIFIED));
            mAsserter.is(actual, expected, "Item has correct " + CLIENT_LAST_MODIFIED);
        }
    }

    private void assertRowEqualsContentValues(Cursor cursorWithActual, ContentValues expectedValues) {
        assertRowEqualsContentValues(cursorWithActual, expectedValues, true, TEST_COLUMNS);
    }

    private ContentValues fillContentValues(String title, String url, String excerpt) {
        ContentValues values = new ContentValues();

        values.put(TITLE, title);
        values.put(URL, url);
        values.put(EXCERPT, excerpt);
        values.put(ADDED_ON, System.currentTimeMillis());

        return values;
    }

    private ContentValues createFillerReadingListItem() {
        Random rand = new Random();
        return fillContentValues("Example", "http://example.com/?num=" + rand.nextInt(), "foo bar");
    }

    private Cursor getEverything(Uri uri) {
        return mProvider.query(uri,
                               null,
                               null,
                               null,
                               null);
    }

    private Cursor getItemById(Uri uri, long id, String[] projection) {
        return mProvider.query(uri, projection,
                               _ID + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private Cursor getItemById(long id) {
        return getItemById(CONTENT_URI, id, null);
    }

    private Cursor getItemById(Uri uri, long id) {
        return getItemById(uri, id, null);
    }

    /**
     * Verifies that ContentProvider insertions have been tested.
     */
    private void ensureCanInsert() {
        if (!mContentProviderInsertTested) {
            mAsserter.ok(false, "ContentProvider insertions have not been tested yet.", "");
        }
    }

    /**
     * Verifies that ContentProvider updates have been tested.
     */
    private void ensureCanUpdate() {
        if (!mContentProviderUpdateTested) {
            mAsserter.ok(false, "ContentProvider updates have not been tested yet.", "");
        }
    }

    private long insertAnItemWithAssertion() {
        // We should be able to insert into the DB.
        ensureCanInsert();

        ContentValues v = createFillerReadingListItem();
        long id = ContentUris.parseId(mProvider.insert(CONTENT_URI, v));

        assertItemExistsByID(id, "Inserted item found");
        return id;
    }

    private void assertItemExistsByID(Uri uri, long id, String msg) {
        Cursor c = getItemById(uri, id);
        try {
            mAsserter.ok(c.moveToFirst(), msg, "");
        } finally {
            c.close();
        }
    }

    private void assertItemExistsByID(long id, String msg) {
        Cursor c = getItemById(id);
        try {
            mAsserter.ok(c.moveToFirst(), msg, "");
        } finally {
            c.close();
        }
    }

    private void assertItemDoesNotExistByID(long id, String msg) {
        Cursor c = getItemById(id);
        try {
            mAsserter.ok(!c.moveToFirst(), msg, "");
        } finally {
            c.close();
        }
    }

    private void assertItemDoesNotExistByID(Uri uri, long id, String msg) {
        Cursor c = getItemById(uri, id);
        try {
            mAsserter.ok(!c.moveToFirst(), msg, "");
        } finally {
            c.close();
        }
    }
}
