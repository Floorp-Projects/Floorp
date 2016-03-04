/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.DeletedLogins;
import org.mozilla.gecko.db.BrowserContract.Logins;
import org.mozilla.gecko.db.BrowserContract.LoginsDisabledHosts;
import org.mozilla.gecko.db.LoginsProvider;

import java.util.concurrent.Callable;

import static org.mozilla.gecko.db.BrowserContract.CommonColumns._ID;
import static org.mozilla.gecko.db.BrowserContract.DeletedLogins.TABLE_DELETED_LOGINS;
import static org.mozilla.gecko.db.BrowserContract.Logins.TABLE_LOGINS;
import static org.mozilla.gecko.db.BrowserContract.LoginsDisabledHosts.TABLE_DISABLED_HOSTS;

public class testLoginsProvider extends ContentProviderTest {

    private static final String DB_NAME = "browser.db";

    private final TestCase[] TESTS_TO_RUN = {
            new InsertLoginsTest(),
            new UpdateLoginsTest(),
            new DeleteLoginsTest(),
            new InsertDeletedLoginsTest(),
            new InsertDeletedLoginsFailureTest(),
            new DisabledHostsInsertTest(),
            new DisabledHostsInsertFailureTest(),
            new InsertLoginsWithDefaultValuesTest(),
            new InsertLoginsWithDuplicateGuidFailureTest(),
            new DeleteLoginsByNonExistentGuidTest(),
    };

    /**
     * Factory function that makes new LoginsProvider instances.
     * <p>
     * We want a fresh provider each test, so this should be invoked in
     * <code>setUp</code> before each individual test.
     */
    private static final Callable<ContentProvider> sProviderFactory = new Callable<ContentProvider>() {
        @Override
        public ContentProvider call() {
            return new LoginsProvider();
        }
    };

    @Override
    public void setUp() throws Exception {
        super.setUp(sProviderFactory, BrowserContract.LOGINS_AUTHORITY, DB_NAME);
        for (TestCase test: TESTS_TO_RUN) {
            mTests.add(test);
        }
    }

    public void testLoginProviderTests() throws Exception {
        for (Runnable test : mTests) {
            final String testName = test.getClass().getSimpleName();
            setTestName(testName);
            ensureEmptyDatabase();
            mAsserter.dumpLog("testLoginsProvider: Database empty - Starting " + testName + ".");
            test.run();
        }
    }

    /**
     * Wipe DB.
     */
    private void ensureEmptyDatabase() {
        getWritableDatabase(Logins.CONTENT_URI).delete(TABLE_LOGINS, null, null);
        getWritableDatabase(DeletedLogins.CONTENT_URI).delete(TABLE_DELETED_LOGINS, null, null);
        getWritableDatabase(LoginsDisabledHosts.CONTENT_URI).delete(TABLE_DISABLED_HOSTS, null, null);
    }

    private SQLiteDatabase getWritableDatabase(Uri uri) {
        Uri testUri = appendUriParam(uri, BrowserContract.PARAM_IS_TEST, "1");
        DelegatingTestContentProvider delegateProvider = (DelegatingTestContentProvider) mProvider;
        LoginsProvider loginsProvider = (LoginsProvider) delegateProvider.getTargetProvider();
        return loginsProvider.getWritableDatabaseForTesting(testUri);
    }

    /**
     * LoginsProvider insert logins test.
     */
    private class InsertLoginsTest extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", "guid1");
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Logins.CONTENT_URI, contentValues));
            verifyLoginExists(contentValues, id);
            Cursor cursor = mProvider.query(Logins.CONTENT_URI, null, Logins.GUID + " = ?", new String[] { "guid1" }, null);
            verifyRowMatches(contentValues, cursor, "logins found");

            // Empty ("") encrypted username and password are valid.
            contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "", "", "guid2");
            id = ContentUris.parseId(mProvider.insert(BrowserContract.Logins.CONTENT_URI, contentValues));
            verifyLoginExists(contentValues, id);
            cursor = mProvider.query(Logins.CONTENT_URI, null, Logins.GUID + " = ?", new String[] { "guid2" }, null);
            verifyRowMatches(contentValues, cursor, "logins found");
        }
    }

    /**
     * LoginsProvider updates logins test.
     */
    private class UpdateLoginsTest extends TestCase {
        @Override
        public void test() throws Exception {
            final String guid1 = "guid1";
            ContentValues contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", guid1);
            long timeBeforeCreated = System.currentTimeMillis();
            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Logins.CONTENT_URI, contentValues));
            long timeAfterCreated = System.currentTimeMillis();
            verifyLoginExists(contentValues, id);

            Cursor cursor = getLoginById(id);
            try {
                mAsserter.ok(cursor.moveToFirst(), "cursor is not empty", "");
                verifyBounded(timeBeforeCreated, cursor.getLong(cursor.getColumnIndexOrThrow(Logins.TIME_CREATED)), timeAfterCreated);
            } finally {
                cursor.close();
            }

            contentValues.put(BrowserContract.Logins.ENCRYPTED_USERNAME, "username2");
            contentValues.put(Logins.ENCRYPTED_PASSWORD, "password2");

            Uri updateUri = Logins.CONTENT_URI.buildUpon().appendPath(String.valueOf(id)).build();
            int numUpdated = mProvider.update(updateUri, contentValues, null, null);
            mAsserter.is(1, numUpdated, "Correct number updated");
            verifyLoginExists(contentValues, id);

            contentValues.put(BrowserContract.Logins.ENCRYPTED_USERNAME, "username1");
            contentValues.put(Logins.ENCRYPTED_PASSWORD, "password1");

            updateUri = Logins.CONTENT_URI;
            numUpdated = mProvider.update(updateUri, contentValues, Logins.GUID + " = ?", new String[]{guid1});
            mAsserter.is(1, numUpdated, "Correct number updated");
            verifyLoginExists(contentValues, id);
        }
    }

    /**
     * LoginsProvider deletion logins test.
     * - inserts a new logins
     * - deletes the logins and verify deleted-logins table has entry for deleted guid.
     */
    private class DeleteLoginsTest extends TestCase {
        @Override
        public void test() throws Exception {
            final String guid1 = "guid1";
            ContentValues contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", guid1);
            long id = ContentUris.parseId(mProvider.insert(Logins.CONTENT_URI, contentValues));
            verifyLoginExists(contentValues, id);

            Uri deletedUri = Logins.CONTENT_URI.buildUpon().appendPath(String.valueOf(id)).build();
            int numDeleted = mProvider.delete(deletedUri, null, null);
            mAsserter.is(1, numDeleted, "Correct number deleted");
            verifyNoRowExists(Logins.CONTENT_URI, "No login entry found");

            contentValues = new ContentValues();
            contentValues.put(DeletedLogins.GUID, guid1);
            Cursor cursor = mProvider.query(DeletedLogins.CONTENT_URI, null, null, null, null);
            verifyRowMatches(contentValues, cursor, "deleted-login found");
            cursor = mProvider.query(DeletedLogins.CONTENT_URI, null, DeletedLogins.GUID + " = ?", new String[] { guid1 }, null);
            verifyRowMatches(contentValues, cursor, "deleted-login found");
        }
    }

    /**
     * LoginsProvider re-insert logins test.
     * - inserts a row into deleted-logins
     * - insert the same login (matching guid) and verify deleted-logins table is empty.
     */
    private class InsertDeletedLoginsTest extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues contentValues = new ContentValues();
            contentValues.put(DeletedLogins.GUID, "guid1");
            long id = ContentUris.parseId(mProvider.insert(DeletedLogins.CONTENT_URI, contentValues));
            final Uri insertedUri = DeletedLogins.CONTENT_URI.buildUpon().appendPath(String.valueOf(id)).build();
            Cursor cursor = mProvider.query(insertedUri, null, null, null, null);
            verifyRowMatches(contentValues, cursor, "deleted-login found");
            verifyNoRowExists(BrowserContract.Logins.CONTENT_URI, "No login entry found");

            contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", "guid1");
            id = ContentUris.parseId(mProvider.insert(Logins.CONTENT_URI, contentValues));
            verifyLoginExists(contentValues, id);
            verifyNoRowExists(DeletedLogins.CONTENT_URI, "No deleted-login entry found");
        }
    }

    /**
     * LoginsProvider insert Deleted logins test.
     * - inserts a row into deleted-login without GUID.
     */
    private class InsertDeletedLoginsFailureTest extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues contentValues = new ContentValues();
            try {
                mProvider.insert(DeletedLogins.CONTENT_URI, contentValues);
                fail("Failed to throw IllegalArgumentException while missing GUID");
            } catch (Exception e) {
                mAsserter.is(e.getClass(), IllegalArgumentException.class, "IllegalArgumentException thrown for invalid GUID");
            }
        }
    }

    /**
     * LoginsProvider disabled host test.
     * - inserts a disabled-host
     * - delete the inserted disabled-host and verify disabled-hosts table is empty.
     */
    private class DisabledHostsInsertTest extends TestCase {
        @Override
        public void test() throws Exception {
            final String hostname = "localhost";
            final ContentValues contentValues = new ContentValues();
            contentValues.put(LoginsDisabledHosts.HOSTNAME, hostname);
            mProvider.insert(LoginsDisabledHosts.CONTENT_URI, contentValues);
            final Uri insertedUri = LoginsDisabledHosts.CONTENT_URI.buildUpon().appendPath("hostname").appendPath(hostname).build();
            final Cursor cursor = mProvider.query(insertedUri, null, null, null, null);
            verifyRowMatches(contentValues, cursor, "disabled-hosts found");

            final Uri deletedUri = LoginsDisabledHosts.CONTENT_URI.buildUpon().appendPath("hostname").appendPath(hostname).build();
            final int numDeleted = mProvider.delete(deletedUri, null, null);
            mAsserter.is(1, numDeleted, "Correct number deleted");
            verifyNoRowExists(LoginsDisabledHosts.CONTENT_URI, "No disabled-hosts entry found");
        }
    }

    /**
     * LoginsProvider disabled host insert failure testcase.
     * - inserts a disabled-host without providing hostname
     */
    private class DisabledHostsInsertFailureTest extends TestCase {
        @Override
        public void test() throws Exception {
            final String hostname = "localhost";
            final ContentValues contentValues = new ContentValues();
            try {
                mProvider.insert(LoginsDisabledHosts.CONTENT_URI, contentValues);
                fail("Failed to throw IllegalArgumentException while missing hostname");
            } catch (Exception e) {
                mAsserter.is(e.getClass(), IllegalArgumentException.class, "IllegalArgumentException thrown for invalid hostname");
            }
        }
    }

    /**
     * LoginsProvider login insertion with default values test.
     * - insert a login missing GUID, FORM_SUBMIT_URL, HTTP_REALM and verify default values are set.
     */
    private class InsertLoginsWithDefaultValuesTest extends TestCase {
        @Override
        protected void test() throws Exception {
            ContentValues contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", null);
            // Remove GUID, HTTP_REALM, FORM_SUBMIT_URL from content values
            contentValues.remove(Logins.GUID);
            contentValues.remove(Logins.FORM_SUBMIT_URL);
            contentValues.remove(Logins.HTTP_REALM);

            long id = ContentUris.parseId(mProvider.insert(BrowserContract.Logins.CONTENT_URI, contentValues));
            Cursor cursor = getLoginById(id);
            assertNotNull(cursor);
            cursor.moveToFirst();

            mAsserter.isnot(cursor.getString(cursor.getColumnIndex(Logins.GUID)), null, "GUID is not null");
            mAsserter.is(cursor.getString(cursor.getColumnIndex(Logins.HTTP_REALM)), null, "HTTP_REALM is not null");
            mAsserter.is(cursor.getString(cursor.getColumnIndex(Logins.FORM_SUBMIT_URL)), null, "FORM_SUBMIT_URL is not null");
            mAsserter.isnot(cursor.getString(cursor.getColumnIndex(Logins.TIME_LAST_USED)), null, "TIME_LAST_USED is not null");
            mAsserter.isnot(cursor.getString(cursor.getColumnIndex(Logins.TIME_CREATED)), null, "TIME_CREATED is not null");
            mAsserter.isnot(cursor.getString(cursor.getColumnIndex(Logins.TIME_PASSWORD_CHANGED)), null, "TIME_PASSWORD_CHANGED is not null");
            mAsserter.is(cursor.getString(cursor.getColumnIndex(Logins.ENC_TYPE)), "0", "ENC_TYPE is 0");
            mAsserter.is(cursor.getString(cursor.getColumnIndex(Logins.TIMES_USED)), "0", "TIMES_USED is 0");

            // Verify other values.
            verifyRowMatches(contentValues, cursor, "Updated login found");
        }
    }

    /**
     * LoginsProvider login insertion with duplicate GUID test.
     * - insert two different logins with same GUID and verify that only one login exists.
     */
    private class InsertLoginsWithDuplicateGuidFailureTest extends TestCase {
        @Override
        protected void test() throws Exception {
            final String guid = "guid1";
            ContentValues contentValues = createLogin("http://www.example.com", "http://www.example.com",
                    "http://www.example.com", "username1", "password1", "username1", "password1", guid);
            long id1 = ContentUris.parseId(mProvider.insert(BrowserContract.Logins.CONTENT_URI, contentValues));
            verifyLoginExists(contentValues, id1);

            // Insert another login with duplicate GUID.
            contentValues = createLogin("http://www.example2.com", "http://www.example2.com",
                    "http://www.example2.com", "username2", "password2", "username2", "password2", guid);
            Uri insertUri = mProvider.insert(Logins.CONTENT_URI, contentValues);
            mAsserter.is(insertUri, null, "Duplicate Guid insertion id1");

            // Verify login with id1 still exists.
            verifyLoginExists(contentValues, id1);
        }
    }

    /**
     * LoginsProvider deletion by non-existent GUID test.
     * - delete a login with random GUID and verify that no entry was deleted.
     */
    private class DeleteLoginsByNonExistentGuidTest extends TestCase {
        @Override
        protected void test() throws Exception {
            Uri deletedUri = Logins.CONTENT_URI;
            int numDeleted = mProvider.delete(deletedUri, Logins.GUID + "= ?", new String[] { "guid1" });
            mAsserter.is(0, numDeleted, "Correct number deleted");
        }
    }

    private void verifyBounded(long left, long middle, long right) {
        mAsserter.ok(left <= middle, "Left <= middle", left + " <= " + middle);
        mAsserter.ok(middle <= right, "Middle <= right", middle + " <= " + right);
    }

    private Cursor getById(Uri uri, long id, String[] projection) {
        return mProvider.query(uri, projection,
                _ID + " = ?",
                new String[] { String.valueOf(id) },
                null);
    }

    private Cursor getLoginById(long id) {
        return getById(Logins.CONTENT_URI, id, null);
    }

    private void verifyLoginExists(ContentValues contentValues, long id) {
        Cursor cursor = getLoginById(id);
        verifyRowMatches(contentValues, cursor, "Updated login found");
    }

    private void verifyRowMatches(ContentValues contentValues, Cursor cursor, String name) {
        try {
            mAsserter.ok(cursor.moveToFirst(), name, "cursor is not empty");
            CursorMatches(cursor, contentValues);
        } finally {
            cursor.close();
        }
    }

    private void verifyNoRowExists(Uri contentUri, String name) {
        Cursor cursor = mProvider.query(contentUri, null, null, null, null);
        try {
            mAsserter.is(0, cursor.getCount(), name);
        } finally {
            cursor.close();
        }
    }

    private ContentValues createLogin(String hostname, String httpRealm, String formSubmitUrl,
                                      String usernameField, String passwordField, String encryptedUsername,
                                      String encryptedPassword, String guid) {
        final ContentValues values = new ContentValues();
        values.put(Logins.HOSTNAME, hostname);
        values.put(Logins.HTTP_REALM, httpRealm);
        values.put(Logins.FORM_SUBMIT_URL, formSubmitUrl);
        values.put(Logins.USERNAME_FIELD, usernameField);
        values.put(Logins.PASSWORD_FIELD, passwordField);
        values.put(Logins.ENCRYPTED_USERNAME, encryptedUsername);
        values.put(Logins.ENCRYPTED_PASSWORD, encryptedPassword);
        values.put(Logins.GUID, guid);
        return values;
    }
}
