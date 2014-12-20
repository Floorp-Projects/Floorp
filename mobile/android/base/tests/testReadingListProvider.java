/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.HashSet;
import java.util.Random;
import java.util.concurrent.Callable;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.ReadingListProvider;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

public class testReadingListProvider extends ContentProviderTest {

    private static final String DB_NAME = "browser.db";

    // List of tests to be run sorted by dependency.
    private final TestCase[] TESTS_TO_RUN = { new TestInsertItems(),
                                              new TestDeleteItems(),
                                              new TestUpdateItems(),
                                              new TestBatchOperations(),
                                              new TestBrowserProviderNotifications() };

    // Columns used to test for item equivalence.
    final String[] TEST_COLUMNS = { ReadingListItems.TITLE,
                                    ReadingListItems.URL,
                                    ReadingListItems.EXCERPT,
                                    ReadingListItems.LENGTH,
                                    ReadingListItems.DATE_CREATED };

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
            long id = ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, b));
            Cursor c = getItemById(id);

            try {
                mAsserter.ok(c.moveToFirst(), "Inserted item found", "");
                assertRowEqualsContentValues(c, b);
            } finally {
                c.close();
            }

            testInsertWithNullCol(ReadingListItems.GUID);
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
                ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, b));
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
            long id = insertAnItemWithAssertion();
            // Test that the item is only marked as deleted and
            // not removed from the database.
            testNonFirefoxSyncDelete(id);

            // Test that the item is removed from the database.
            testFirefoxSyncDelete(id);

            id = insertAnItemWithAssertion();
            // Test that deleting works with only a URI.
            testDeleteWithItemURI(id);
        }

        /**
         * Delete an item with PARAM_IS_SYNC unset and verify that item was only marked
         * as deleted and not actually removed from the database. Also verify that the item
         * marked as deleted doesn't show up in a query.
         *
         * @param id of the item to be deleted
         */
        private void testNonFirefoxSyncDelete(long id) {
            final int deleted = mProvider.delete(ReadingListItems.CONTENT_URI,
                    ReadingListItems._ID + " = ?",
                    new String[] { String.valueOf(id) });

            mAsserter.is(deleted, 1, "Inserted item was deleted");

            // PARAM_SHOW_DELETED in the URI allows items marked as deleted to be
            // included in the query.
            Uri uri = appendUriParam(ReadingListItems.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1");
            assertItemExistsByID(uri, id, "Deleted item was only marked as deleted");

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
        private void testFirefoxSyncDelete(long id) {
            final int deleted = mProvider.delete(appendUriParam(ReadingListItems.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1"),
                    ReadingListItems._ID + " = ?",
                    new String[] { String.valueOf(id) });

            mAsserter.is(deleted, 1, "Inserted item was deleted");

            Uri uri = appendUriParam(ReadingListItems.CONTENT_URI, BrowserContract.PARAM_SHOW_DELETED, "1");
            assertItemDoesNotExistByID(uri, id, "Inserted item is now actually deleted");
        }

        /**
         * Delete an item with its URI and verify that the item
         * was actually removed from the database.
         *
         * @param id of the item to be deleted
         */
        private void testDeleteWithItemURI(long id) {
            final int deleted = mProvider.delete(ContentUris.withAppendedId(ReadingListItems.CONTENT_URI, id), null, null);
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
            long id = ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, original));
            int updated = 0;
            Long originalDateCreated = null;
            Long originalDateModified = null;
            ContentValues updates = new ContentValues();
            Cursor c = getItemById(id);
            try {
                mAsserter.ok(c.moveToFirst(), "Inserted item found", "");

                originalDateCreated = c.getLong(c.getColumnIndex(ReadingListItems.DATE_CREATED));
                originalDateModified = c.getLong(c.getColumnIndex(ReadingListItems.DATE_MODIFIED));

                updates.put(ReadingListItems.TITLE, original.getAsString(ReadingListItems.TITLE) + "CHANGED");
                updates.put(ReadingListItems.URL, original.getAsString(ReadingListItems.URL) + "/more/stuff");
                updates.put(ReadingListItems.EXCERPT, original.getAsString(ReadingListItems.EXCERPT) + "CHANGED");

                updated = mProvider.update(ReadingListItems.CONTENT_URI, updates,
                                               ReadingListItems._ID + " = ?",
                                               new String[] { String.valueOf(id) });

                mAsserter.is(updated, 1, "Inserted item was updated");
            } finally {
                c.close();
            }

            // Name change for clarity. These values will be compared with the
            // current cursor row.
            ContentValues expectedValues = updates;
            c = getItemById(id);
            try {
                mAsserter.ok(c.moveToFirst(), "Updated item found", "");
                mAsserter.isnot(c.getLong(c.getColumnIndex(ReadingListItems.DATE_MODIFIED)),
                originalDateModified,
                "Date modified should have changed");

                // DATE_CREATED and LENGTH should equal old values since they weren't updated.
                expectedValues.put(ReadingListItems.DATE_CREATED, originalDateCreated);
                expectedValues.put(ReadingListItems.LENGTH, original.getAsString(ReadingListItems.LENGTH));
                assertRowEqualsContentValues(c, expectedValues, /* compareDateModified */ false);
            } finally {
                c.close();
            }

            // Test that updates on an item that doesn't exist does not modify any rows.
            testUpdateWithInvalidID();

            // Test that update fails when a GUID is null.
            testUpdateWithNullCol(id, ReadingListItems.GUID);

            mContentProviderUpdateTested = true;
        }

        /**
         * Test that updates on an item that doesn't exist does
         * not modify any rows.
         *
         * @param id of the item to be deleted
         */
        private void testUpdateWithInvalidID() {
            ensureEmptyDatabase();
            final ContentValues b = createFillerReadingListItem();
            final long id = ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, b));
            final long INVALID_ID = id + 1;
            final ContentValues updates = new ContentValues();
            updates.put(ReadingListItems.TITLE, b.getAsString(ReadingListItems.TITLE) + "CHANGED");
            final int updated = mProvider.update(ReadingListItems.CONTENT_URI, updates,
                                               ReadingListItems._ID + " = ?",
                                               new String[] { String.valueOf(INVALID_ID) });
            mAsserter.is(updated, 0, "Should not be able to update item with an invalid GUID");
        }

        /**
         * Test that update fails when a required column is null.
         */
        private int testUpdateWithNullCol(long id, String colName) {
            ContentValues updates = new ContentValues();
            updates.putNull(colName);

            int updated = mProvider.update(ReadingListItems.CONTENT_URI, updates,
                                           ReadingListItems._ID + " = ?",
                                           new String[] { String.valueOf(id) });

            mAsserter.is(updated, 0, "Should not be able to update item with " + colName + " == null ");
            return updated;
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
                allVals[i].put(ReadingListItems.TITLE, "Test" + i);
                allVals[i].put(ReadingListItems.URL, url);
                allVals[i].put(ReadingListItems.EXCERPT, "EXCERPT" + i);
                allVals[i].put(ReadingListItems.LENGTH, i);
                urls.add(url);
            }

            int inserts = mProvider.bulkInsert(ReadingListItems.CONTENT_URI, allVals);
            mAsserter.is(inserts, ITEM_COUNT, "Excepted number of inserts matches");

            Cursor c = mProvider.query(ReadingListItems.CONTENT_URI, null,
                               null,
                               null,
                               null);
            try {
                while (c.moveToNext()) {
                    final String url = c.getString(c.getColumnIndex(ReadingListItems.URL));
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
            h.put(ReadingListItems.TITLE, "http://newexample.com");

            long numUpdated = mProvider.update(ReadingListItems.CONTENT_URI, h,
                                               ReadingListItems._ID + " = ?",
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

    /**
     * Removes all items from the DB.
     */
    private void ensureEmptyDatabase() {
        Uri uri = appendUriParam(ReadingListItems.CONTENT_URI, BrowserContract.PARAM_IS_SYNC, "1");
        getWritableDatabase(uri).delete(ReadingListItems.TABLE_NAME, null, null);
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
     * @param cursor over the row to be checked
     * @param values to be checked
     */
    private void assertRowEqualsContentValues(Cursor cursorWithActual, ContentValues expectedValues, boolean compareDateModified) {
        for (String column: TEST_COLUMNS) {
            String expected = expectedValues.getAsString(column);
            String actual = cursorWithActual.getString(cursorWithActual.getColumnIndex(column));
            mAsserter.is(actual, expected, "Item has correct " + column);
        }

        if (compareDateModified) {
            String expected = expectedValues.getAsString(ReadingListItems.DATE_MODIFIED);
            String actual = cursorWithActual.getString(cursorWithActual.getColumnIndex(ReadingListItems.DATE_MODIFIED));
            mAsserter.is(actual, expected, "Item has correct " + ReadingListItems.DATE_MODIFIED);
        }
    }

    private void assertRowEqualsContentValues(Cursor cursorWithActual, ContentValues expectedValues) {
        assertRowEqualsContentValues(cursorWithActual, expectedValues, true);
    }

    private ContentValues fillContentValues(String title, String url, String excerpt) {
        ContentValues values = new ContentValues();

        values.put(ReadingListItems.TITLE, title);
        values.put(ReadingListItems.URL, url);
        values.put(ReadingListItems.EXCERPT, excerpt);
        values.put(ReadingListItems.LENGTH, excerpt.length());

        return values;
    }

    private ContentValues createFillerReadingListItem() {
        Random rand = new Random();
        return fillContentValues("Example", "http://example.com/?num=" + rand.nextInt(), "foo bar");
    }

    private Cursor getItemById(Uri uri, long id, String[] projection) {
        return mProvider.query(uri, projection,
                               ReadingListItems._ID + " = ?",
                               new String[] { String.valueOf(id) },
                               null);
    }

    private Cursor getItemById(long id) {
        return getItemById(ReadingListItems.CONTENT_URI, id, null);
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
        long id = ContentUris.parseId(mProvider.insert(ReadingListItems.CONTENT_URI, v));

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
