/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.concurrent.Callable;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.db.SearchHistoryProvider;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

public class testSearchHistoryProvider extends ContentProviderTest {

    // Translations of "United Kingdom" in several different languages
    private static final String[] testStrings = {
            "An Ríocht Aontaithe", // Irish
            "Angli", // Albanian
            "Britanniarum Regnum", // Latin
            "Britio", // Esperanto
            "Büyük Britanya", // Turkish
            "Egyesült Királyság", // Hungarian
            "Erresuma Batua", // Basque
            "Inggris Raya", // Indonesian
            "Ir-Renju Unit", // Maltese
            "Iso-Britannia", // Finnish
            "Jungtinė Karalystė", // Lithuanian
            "Lielbritānija", // Latvian
            "Regatul Unit", // Romanian
            "Regne Unit", // Catalan, Valencian
            "Regno Unito", // Italian
            "Royaume-Uni", // French
            "Spojené království", // Czech
            "Spojené kráľovstvo", // Slovak
            "Storbritannia", // Norwegian
            "Storbritannien", // Danish
            "Suurbritannia", // Estonian
            "Ujedinjeno Kraljevstvo", // Bosnian
            "United Alaeze", // Igbo
            "United Kingdom", // English
            "Vereinigtes Königreich", // German
            "Verenigd Koninkrijk", // Dutch
            "Verenigde Koninkryk", // Afrikaans
            "Vương quốc Anh", // Vietnamese
            "Wayòm Ini", // Haitian, Haitian Creole
            "Y Deyrnas Unedig", // Welsh
            "Združeno kraljestvo", // Slovene
            "Zjednoczone Królestwo", // Polish
            "Ηνωμένο Βασίλειο", // Greek (modern)
            "Великобритания", // Russian
            "Нэгдсэн Вант Улс", // Mongolian
            "Обединетото Кралство", // Macedonian
            "Уједињено Краљевство", // Serbian
            "Միացյալ Թագավորություն", // Armenian
            "בריטניה", // Hebrew (modern)
            "פֿאַראייניקטע מלכות", // Yiddish
            "المملكة المتحدة", // Arabic
            "برطانیہ", // Urdu
            "پادشاهی متحده", // Persian (Farsi)
            "यूनाइटेड किंगडम", // Hindi
            "संयुक्त राज्य", // Nepali
            "যুক্তরাজ্য", // Bengali, Bangla
            "યુનાઇટેડ કિંગડમ", // Gujarati
            "ஐக்கிய ராஜ்யம்", // Tamil
            "สหราชอาณาจักร", // Thai
            "ສະ​ຫະ​ປະ​ຊາ​ຊະ​ອາ​ນາ​ຈັກ", // Lao
            "გაერთიანებული სამეფო", // Georgian
            "イギリス", // Japanese
            "联合王国" // Chinese
    };


    private static final String DB_NAME = "searchhistory.db";

    /**
     * Boilerplate alert.
     * <p/>
     * Make sure this method is present and that it returns a new
     * instance of your class.
     */
    private static Callable<ContentProvider> sProviderFactory =
            new Callable<ContentProvider>() {
                @Override
                public ContentProvider call() {
                    return new SearchHistoryProvider();
                }
            };

    @Override
    public void setUp() throws Exception {
        super.setUp(sProviderFactory, BrowserContract.SEARCH_HISTORY_AUTHORITY, DB_NAME);
        mTests.add(new TestInsert());
        mTests.add(new TestUnicodeQuery());
        mTests.add(new TestTimestamp());
        mTests.add(new TestLimit());
        mTests.add(new TestDelete());
        mTests.add(new TestIncrement());
    }

    public void testSearchHistory() throws Exception {
        for (Runnable test : mTests) {
            String testName = test.getClass().getSimpleName();
            setTestName(testName);
            mAsserter.dumpLog(
                    "testBrowserProvider: Database empty - Starting " + testName + ".");
            // Clear the db
            mProvider.delete(SearchHistory.CONTENT_URI, null, null);
            test.run();
        }
    }

    /**
     * Verify that we can pass a LIMIT clause using a query parameter.
     */
    private class TestLimit extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues cv;
            for (int i = 0; i < testStrings.length; i++) {
                cv = new ContentValues();
                cv.put(SearchHistory.QUERY, testStrings[i]);
                mProvider.insert(SearchHistory.CONTENT_URI, cv);
            }

            final int limit = 5;

            // Test 1: Handle proper input.

            Uri uri = SearchHistory.CONTENT_URI
                                   .buildUpon()
                                   .appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(limit))
                                   .build();

            Cursor c = mProvider.query(uri, null, null, null, null);
            try {
                mAsserter.is(c.getCount(), limit,
                             String.format("Should have %d results", limit));
            } finally {
                c.close();
            }

            // Test 2: Empty input yields all results.

            uri = SearchHistory.CONTENT_URI
                                   .buildUpon()
                                   .appendQueryParameter(BrowserContract.PARAM_LIMIT, "")
                                   .build();

            c = mProvider.query(uri, null, null, null, null);
            try {
                mAsserter.is(c.getCount(), testStrings.length, "Should have all results");
            } finally {
                c.close();
            }

            // Test 3: Illegal params.

            String[] illegalParams = new String[] {"a", "-1"};
            boolean success = true;

            for (String param : illegalParams) {
                success = true;

                uri = SearchHistory.CONTENT_URI
                                   .buildUpon()
                                   .appendQueryParameter(BrowserContract.PARAM_LIMIT, param)
                                   .build();

                try {
                    c = mProvider.query(uri, null, null, null, null);
                    success = false;
                } catch(IllegalArgumentException e) {
                    // noop.
                } finally {
                    if (c != null) {
                        c.close();
                    }
                }

                mAsserter.ok(success, "LIMIT", param + " should have been an invalid argument");
            }

        }
    }

    /**
     * Verify that we can insert values into the DB, including unicode.
     */
    private class TestInsert extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues cv;
            for (int i = 0; i < testStrings.length; i++) {
                cv = new ContentValues();
                cv.put(SearchHistory.QUERY, testStrings[i]);
                mProvider.insert(SearchHistory.CONTENT_URI, cv);
            }

            final Cursor c = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                mAsserter.is(c.getCount(), testStrings.length,
                             "Should have one row for each insert");
            } finally {
                c.close();
            }
        }
    }

    /**
     * Verify that we can insert values into the DB, including unicode.
     */
    private class TestUnicodeQuery extends TestCase {
        @Override
        public void test() throws Exception {
            final String selection = SearchHistory.QUERY + " = ?";

            for (int i = 0; i < testStrings.length; i++) {
                final ContentValues cv = new ContentValues();
                cv.put(SearchHistory.QUERY, testStrings[i]);
                mProvider.insert(SearchHistory.CONTENT_URI, cv);

                final Cursor c = mProvider.query(SearchHistory.CONTENT_URI, null, selection,
                                                 new String[]{ testStrings[i] }, null);
                try {
                    mAsserter.is(c.getCount(), 1,
                                 "Should have one row for insert of " + testStrings[i]);
                } finally {
                    c.close();
                }
            }
        }
    }

    /**
     * Verify that timestamps are updated on insert.
     */
    private class TestTimestamp extends TestCase {
        @Override
        public void test() throws Exception {
            String insertedTerm = "Courtside Seats";
            long insertStart;
            long insertFinish;
            long t1Db;
            long t2Db;

            ContentValues cv = new ContentValues();
            cv.put(SearchHistory.QUERY, insertedTerm);

            // First check that the DB has a value that is close to the
            // system time.
            insertStart = System.currentTimeMillis();
            mProvider.insert(SearchHistory.CONTENT_URI, cv);
            insertFinish = System.currentTimeMillis();

            final Cursor c1 = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                c1.moveToFirst();
                t1Db = c1.getLong(c1.getColumnIndex(SearchHistory.DATE_LAST_VISITED));
            } finally {
                c1.close();
            }

            mAsserter.dumpLog("First insert:");
            mAsserter.dumpLog("  insertStart " + insertStart);
            mAsserter.dumpLog("  insertFinish " + insertFinish);
            mAsserter.dumpLog("  t1Db " + t1Db);
            mAsserter.ok(t1Db >= insertStart, "DATE_LAST_VISITED",
                         "Date last visited should be set on insert.");
            mAsserter.ok(t1Db <= insertFinish, "DATE_LAST_VISITED",
                         "Date last visited should be set on insert.");

            cv = new ContentValues();
            cv.put(SearchHistory.QUERY, insertedTerm);

            insertStart = System.currentTimeMillis();
            mProvider.insert(SearchHistory.CONTENT_URI, cv);
            insertFinish = System.currentTimeMillis();

            final Cursor c2 = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                c2.moveToFirst();
                t2Db = c2.getLong(c2.getColumnIndex(SearchHistory.DATE_LAST_VISITED));
            } finally {
                c2.close();
            }

            mAsserter.dumpLog("Second insert:");
            mAsserter.dumpLog("  insertStart " + insertStart);
            mAsserter.dumpLog("  insertFinish " + insertFinish);
            mAsserter.dumpLog("  t2Db " + t2Db);

            mAsserter.ok(t2Db >= insertStart, "DATE_LAST_VISITED",
                         "Date last visited should be set on insert.");
            mAsserter.ok(t2Db <= insertFinish, "DATE_LAST_VISITED",
                         "Date last visited should be set on insert.");
            mAsserter.ok(t2Db >= t1Db, "DATE_LAST_VISITED",
                         "Date last visited should be updated on key increment.");
        }
    }

    /**
     * Verify that sending a delete command empties the database.
     */
    private class TestDelete extends TestCase {
        @Override
        public void test() throws Exception {
            String insertedTerm = "Courtside Seats";

            ContentValues cv = new ContentValues();
            cv.put(SearchHistory.QUERY, insertedTerm);
            mProvider.insert(SearchHistory.CONTENT_URI, cv);

            final Cursor c1 = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                mAsserter.is(c1.getCount(), 1, "Should have one value");
                mProvider.delete(SearchHistory.CONTENT_URI, null, null);
            } finally {
                c1.close();
            }

            final Cursor c2 = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                mAsserter.is(c2.getCount(), 0, "Should be empty");
                mProvider.insert(SearchHistory.CONTENT_URI, cv);
            } finally {
                c2.close();
            }

            final Cursor c3 = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                mAsserter.is(c3.getCount(), 1, "Should have one value");
            } finally {
                c3.close();
            }
        }
    }


    /**
     * Ensure that we only increment when the case matches.
     */
    private class TestIncrement extends TestCase {
        @Override
        public void test() throws Exception {
            ContentValues cv = new ContentValues();
            cv.put(SearchHistory.QUERY, "omaha");
            mProvider.insert(SearchHistory.CONTENT_URI, cv);

            cv = new ContentValues();
            cv.put(SearchHistory.QUERY, "omaha");
            mProvider.insert(SearchHistory.CONTENT_URI, cv);

            Cursor c = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                c.moveToFirst();
                mAsserter.is(c.getCount(), 1, "Should have one result");
                mAsserter.is(c.getInt(c.getColumnIndex(SearchHistory.VISITS)), 2,
                             "Counter should be 2");
            } finally {
                c.close();
            }

            cv = new ContentValues();
            cv.put(SearchHistory.QUERY, "Omaha");
            mProvider.insert(SearchHistory.CONTENT_URI, cv);
            c = mProvider.query(SearchHistory.CONTENT_URI, null, null, null, null);
            try {
                mAsserter.is(c.getCount(), 2, "Should have two results");
            } finally {
                c.close();
            }
        }
    }
}
