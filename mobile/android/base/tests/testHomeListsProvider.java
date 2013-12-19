package org.mozilla.gecko.tests;

import android.content.ContentValues;
import android.content.ContentUris;
import android.database.Cursor;
import android.net.Uri;

public class testHomeListsProvider extends ContentProviderTest {

    private Uri mItemsFakeUri;
    private Uri mItemsUri;

    private String mItemsIdCol;
    private String mItemsProviderIdCol;
    private String mItemsTitleCol;
    private String mItemsUrlCol;

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    private void loadContractInfo() throws Exception {
        mItemsFakeUri = getUriColumn("HomeListItems", "CONTENT_FAKE_URI");
        mItemsUri = getContentUri("HomeListItems");

        mItemsIdCol = getStringColumn("HomeListItems", "_ID");
        mItemsProviderIdCol = getStringColumn("HomeListItems", "PROVIDER_ID");
        mItemsTitleCol = getStringColumn("HomeListItems", "TITLE");
        mItemsUrlCol = getStringColumn("HomeListItems", "URL");
    }

    private void ensureEmptyDatabase() throws Exception {
        // Delete all the list entries.
        mProvider.delete(mItemsUri, null, null);

        final Cursor c = mProvider.query(mItemsUri, null, null, null, null);
        mAsserter.is(c.getCount(), 0, "All list entries were deleted");
        c.close();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp("org.mozilla.gecko.db.HomeListsProvider", "AUTHORITY", "homelists.db");
        loadContractInfo();

        mTests.add(new TestFakeItems());

        // Disabled until database support lands
        //mTests.add(new TestInsertItem());
    }

    public void testListsProvider() throws Exception {
        for (int i = 0; i < mTests.size(); i++) {
            Runnable test = mTests.get(i);

            setTestName(test.getClass().getSimpleName());
            // Disabled until database support lands
            //ensureEmptyDatabase();
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

    class TestFakeItems extends Test {
        @Override
        public void test() throws Exception {
            final long id = 1;
            final String providerId = "fake-provider";
            final String title = "Example";
            final String url = "http://example.com";

            final Cursor c = mProvider.query(mItemsFakeUri, null, null, null, null);
            mAsserter.is(c.moveToFirst(), true, "Fake list item found");

            mAsserter.is(c.getLong(c.getColumnIndex(mItemsIdCol)), id, "Fake list item has correct ID");
            mAsserter.is(c.getString(c.getColumnIndex(mItemsProviderIdCol)), providerId, "Fake list item has correct provider ID");
            mAsserter.is(c.getString(c.getColumnIndex(mItemsTitleCol)), title, "Fake list item has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mItemsUrlCol)), url, "Fake list item has correct URL");

            c.close();
        }
    }

    class TestInsertItem extends Test {
        @Override
        public void test() throws Exception {
            final String providerId = "{c77da387-4c80-0c45-9f22-70276c29b3ed}";
            final String title = "Mozilla";
            final String url = "https://mozilla.org";

            // Insert a new list item with test values.
            final ContentValues cv = new ContentValues();
            cv.put(mItemsProviderIdCol, providerId);
            cv.put(mItemsTitleCol, title);
            cv.put(mItemsUrlCol, url);

            final long id = ContentUris.parseId(mProvider.insert(mItemsUri, cv));

            // Check that the item was inserted correctly.
            final Cursor c = mProvider.query(mItemsUri, null, mItemsIdCol + " = ?", new String[] { String.valueOf(id) }, null);
            mAsserter.is(c.moveToFirst(), true, "Inserted list item found");

            mAsserter.is(c.getString(c.getColumnIndex(mItemsProviderIdCol)), providerId, "Inserted list item has correct provider ID");
            mAsserter.is(c.getString(c.getColumnIndex(mItemsTitleCol)), title, "Inserted list item has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(mItemsUrlCol)), url, "Inserted list item has correct URL");

            c.close();
        }
    }
}
