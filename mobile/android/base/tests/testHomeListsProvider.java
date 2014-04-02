package org.mozilla.gecko.tests;

import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

public class testHomeListsProvider extends ContentProviderTest {
    // This test does not run, so it just needs to compile. The test was
    // disabled at the time the real Contract was removed; to leave a skeleton
    // for a future re-implementor, we include this dummy Contract class.
    private static class Contract {
        public static final Uri CONTENT_URI = null;
        public static final Uri CONTENT_FAKE_URI = null;

        public static final String _ID = null;
        public static final String PROVIDER_ID = null;
        public static final String TITLE = null;
        public static final String URL = null;
    }

    @SuppressWarnings("unused")
  private void ensureEmptyDatabase() throws Exception {
        // Delete all the list entries.
        mProvider.delete(Contract.CONTENT_URI, null, null);

        final Cursor c = mProvider.query(Contract.CONTENT_URI, null, null, null, null);
        mAsserter.is(c.getCount(), 0, "All list entries were deleted");
        c.close();
    }

    @Override
    public void setUp() throws Exception {
        // This test is disabled, so this just needs to compile.
        super.setUp(null, null, "homelists.db");

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

            final Cursor c = mProvider.query(Contract.CONTENT_FAKE_URI, null, null, null, null);
            mAsserter.is(c.moveToFirst(), true, "Fake list item found");

            mAsserter.is(c.getLong(c.getColumnIndex(Contract._ID)), id, "Fake list item has correct ID");
            mAsserter.is(c.getString(c.getColumnIndex(Contract.PROVIDER_ID)), providerId, "Fake list item has correct provider ID");
            mAsserter.is(c.getString(c.getColumnIndex(Contract.TITLE)), title, "Fake list item has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(Contract.URL)), url, "Fake list item has correct URL");

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
            cv.put(Contract.PROVIDER_ID, providerId);
            cv.put(Contract.TITLE, title);
            cv.put(Contract.URL, url);

            final long id = ContentUris.parseId(mProvider.insert(Contract.CONTENT_URI, cv));

            // Check that the item was inserted correctly.
            final Cursor c = mProvider.query(Contract.CONTENT_URI, null, Contract._ID + " = ?", new String[] { String.valueOf(id) }, null);
            mAsserter.is(c.moveToFirst(), true, "Inserted list item found");

            mAsserter.is(c.getString(c.getColumnIndex(Contract.PROVIDER_ID)), providerId, "Inserted list item has correct provider ID");
            mAsserter.is(c.getString(c.getColumnIndex(Contract.TITLE)), title, "Inserted list item has correct title");
            mAsserter.is(c.getString(c.getColumnIndex(Contract.URL)), url, "Inserted list item has correct URL");

            c.close();
        }
    }
}
