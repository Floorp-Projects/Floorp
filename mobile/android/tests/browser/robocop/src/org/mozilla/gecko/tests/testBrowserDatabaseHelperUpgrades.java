/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import android.database.sqlite.SQLiteDatabase;
import android.util.Log;

import org.mozilla.gecko.db.BrowserDatabaseHelper;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

// TODO: Move to junit 3 tests, once those run in automation. There is no ui testing to do so it's a better fit.
/**
 * This test runs upgrades for the databases in (robocop-assets)/browser_db_upgrade. Currently,
 * (robocop-assets)=mobile/android/tests/browser/robocop/assets/.
 *
 * It copies the old database from the robocop assets directory into a temporary file and opens a SQLiteHelper
 * on the database to verify it gets upgraded to the correct version. If there is an issue with the upgrade,
 * generally a SQLiteException will be thrown and the test will fail. For example:
 * android.database.sqlite.SQLiteException: duplicate column name: calculated_pages_times_rating (code 1): , while compiling: ALTER TABLE book_information ADD COLUMN calculated_pages_times_rating INTEGER;
 *
 * Alternative upgrade tests:
 *   * Robolectric 2.3+ uses a real SQLite database implementation so we could run our upgrades there. However, the
 * SQLite implementation may not be the same as we run on Android. Benefits: speed & we don't need the application to
 * run (and thus a valid DB of the latest version) to run these tests.
 *   * We could copy the current database creation code into a new test, create the database, and then try to upgrade
 * it. However, the tables are empty and thus not a realistic migration plan (e.g. foreign key constraints).
 *
 * TO EDIT THIS TEST:
 *   * Copy the current version of the database into (robocop-assets)/browser_db_upgrade/v##.db database. You can do
 * this via Margaret's copy profile addon - take browser.db from the profile directory. This db copy should contain a
 * used profile - e.g. history items, bookmarks. A good way to get a used profile is to sign into sync.
 *   * MAKE SURE YOU COPY YOUR DB FIRST. Then make your changes to the DB schema code.
 *   * Test!
 *   * Note: when the application starts for testing, it may need to upgrade the database from your existing version. If
 * this fails, the application will crash and the test may fail to start.
 *
 * IMPORTANT:
 * Test DBs must be created on the oldest version of Android that is currently supported. SQLite
 * is not forwards compatible. E.g. uploading a DB created on a 6.0 device will cause failures
 * when robocop tests running on 4.3 are unable to load it.
 *
 * Implementation inspired by:
 *   http://riggaroo.co.za/automated-testing-sqlite-database-upgrades-android/
 */
public class testBrowserDatabaseHelperUpgrades extends UITest {
    private static final int TEST_FROM_VERSION = 27; // We only started testing on this version.

    private ArrayList<File> temporaryDbFiles;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        // TODO: We already install & remove the profile directory each run so it'd be safer for clean-up to store
        // this there. That being said, temporary files are still stored in the application directory so these temporary
        // files will get cleaned up when the application is uninstalled or when data is cleared.
        temporaryDbFiles = new ArrayList<>();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        for (final File dbFile : temporaryDbFiles) {
            dbFile.delete();
        }
    }

    /**
     * @throws IOException if the database cannot be copied.
     */
    public void test() throws IOException {
        for (int i = TEST_FROM_VERSION; i < BrowserDatabaseHelper.DATABASE_VERSION; ++i) {
            Log.d(LOGTAG, "Testing upgrade from version: " + i);
            final String tempDbPath = copyDatabase(i);

            final SQLiteDatabase db = SQLiteDatabase.openDatabase(tempDbPath, null, 0);
            try {
                fAssertEquals("Input DB isn't the expected version",
                              i, db.getVersion());
            } finally {
                db.close();
            }

            final BrowserDatabaseHelper dbHelperToUpgrade = new BrowserDatabaseHelper(getActivity(), tempDbPath);
            // Ideally, we'd test upgrading version i to version i + 1 but this method does not permit that. Alas!
            final SQLiteDatabase upgradedDb = dbHelperToUpgrade.getWritableDatabase();
            try {
                fAssertEquals("DB helper should upgrade to latest version",
                              BrowserDatabaseHelper.DATABASE_VERSION, upgradedDb.getVersion());
            } finally {
                upgradedDb.close();
            }
        }
    }

    /**
     * Copies the database from the assets directory to a temporary test file.
     *
     * @param version version of the database to copy.
     * @return the String path to the new copy of the database
     * @throws IOException if reading the existing database or writing the temporary database fails
     */
    private String copyDatabase(final int version) throws IOException {
        final InputStream inputStream = openDbFromAssets(version);
        try {
            final File dbDestination = File.createTempFile("temporaryDB-v" + version + "_", "db");
            temporaryDbFiles.add(dbDestination);
            Log.d(LOGTAG, "Moving DB from assets to " + dbDestination.getPath());

            final OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(dbDestination));
            try {
                final byte[] buffer = new byte[1024];
                int len;
                while ((len = inputStream.read(buffer)) > 0) {
                    outputStream.write(buffer, 0, len);
                }
                outputStream.flush();
            } finally {
                outputStream.close();
            }

            return dbDestination.getPath();
        } finally {
            inputStream.close();
        }
    }

    private InputStream openDbFromAssets(final int version) throws IOException {
        final String dbAssetPath = String.format("browser_db_upgrade" + File.separator + String.format("v%d.db", version));
        Log.d(LOGTAG, "Opening DB from assets: " + dbAssetPath);
        try {
            return new BufferedInputStream(getInstrumentation().getContext().getAssets().open(dbAssetPath));
        } catch (final FileNotFoundException e) {
            throw new IllegalStateException("If you're upgrading the browser.db version, " +
                    "you need to provide an old version of the database for this test! See the javadoc.", e);
        }
    }
}
