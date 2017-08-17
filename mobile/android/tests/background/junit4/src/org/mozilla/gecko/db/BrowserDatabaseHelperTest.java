/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Random;
import java.util.UUID;

import static org.junit.Assert.*;


@RunWith(TestRunner.class)
public class BrowserDatabaseHelperTest {
    private TestSQLiteOpenHelper helper = new TestSQLiteOpenHelper();
    private final long NEVER_SYNCED_BOOKMARKS = -1;
    private final int DEFAULT_BOOKMARKS_COUNT = 7;
    private Random random;

    @Before
    public void setUp() {
        random = new Random();
    }

    @After
    public void teardDown() throws Exception {
        helper.close();
    }

    @Test
    public void testBookmarkTimestampToVersionMigrationEmpty() throws Exception {
        final SQLiteDatabase db = helper.getWritableDatabase();

        // Assert a base-line.
        assertDefaultVersionValues(db);

        // "Empty" bookmarks table scenario, present are only the default bookmarks.
        // If we never synced bookmarks, migration is essentially a no-op.
        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, NEVER_SYNCED_BOOKMARKS);
        assertDefaultVersionValues(db);

        // If we did sync bookmarks at some point, migration should mark default bookmarks as "synced".
        // This test assumes that default bookmarks were all created before we last synced bookmarks,
        // and that all default bookmarks have non-null created and modified timestamps.
        HashMap<String, String> defaultGuidToRelMap = getDefaultGuidToRelMap();

        // Test that if bookmarks were synced before default bookmarks were created, they're marked
        // as "needs an upload". Not sure how this might happen, but let's cover our bases!
        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, System.currentTimeMillis() - 1000L);
        assertDefaultVersionValues(db);

        // Otherwise, test that if bookmarks were synced after defaults ones were created, they're
        // marked as "don't need an upload".
        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, System.currentTimeMillis() + 1000L);
        assertCorrectVersionValues(db, defaultGuidToRelMap);
    }

    @Test
    public void testBookmarkTimestampToVersionMigrationNonEmpty() throws Exception {
        final SQLiteDatabase db = helper.getWritableDatabase();

        // Create some amount of bookmarks for all combinations of how modified and created can
        // relate to some arbitrary "last synced" timestamp.
        int createMin = 10;
        int createMax = 100;

        // Provide mapping for default auto-created bookmarks.
        HashMap<String, String> guidToRelMap = getDefaultGuidToRelMap();

        // +1s to ensure that there's a gap between when default bookmarks are inserted and lastSynced.
        long lastSynced = System.currentTimeMillis() + 1000L;
        String[] rels = new String[] {"<", "=", ">"};
        for (String rel1 : rels) {
            for (String rel2 : rels) {
                long created = boundaryAround(lastSynced, rel1);
                long modified = boundaryAround(lastSynced, rel2);

                int toCreate = random.nextInt(createMax) + createMin;
                for (int i = 0; i < toCreate; i++) {
                    String guid = UUID.randomUUID().toString();
                    guidToRelMap.put(guid, "c" + rel1 + "m" + rel2);
                    insertBookmark(db, guid, created, modified);
                }
            }
        }

        // Insert some bookmarks with some null timestamps.
        insertBookmark(db, "bad1", lastSynced, null);
        guidToRelMap.put("bad1", "c=mn");
        insertBookmark(db, "bad2", null, lastSynced);
        guidToRelMap.put("bad2", "cnm=");
        insertBookmark(db, "bad3", null, null);
        guidToRelMap.put("bad3", "cnmn");

        insertBookmark(db, "sneaky1", null, lastSynced - 10L);
        guidToRelMap.put("sneaky1", "cnm<");

        // Test that we don't actually do anything if the bookmarks were never synced.
        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, NEVER_SYNCED_BOOKMARKS);

        assertDefaultVersionValues(db);

        // Test that we map timestamps to versions correctly if we did sync.
        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, lastSynced);

        assertCorrectVersionValues(db, guidToRelMap);
    }

    @Test
    public void testBookmarkTimestampToVersionMigrationChunkingLess() throws Exception {
        final SQLiteDatabase db = helper.getWritableDatabase();

        testBookmarkVersionMigrationChunkingForCount(db, DBUtils.SQLITE_MAX_VARIABLE_NUMBER - DEFAULT_BOOKMARKS_COUNT - 1);
    }

    @Test
    public void testBookmarkTimestampToVersionMigrationChunkingEqual() throws Exception {
        final SQLiteDatabase db = helper.getWritableDatabase();

        testBookmarkVersionMigrationChunkingForCount(db, DBUtils.SQLITE_MAX_VARIABLE_NUMBER - DEFAULT_BOOKMARKS_COUNT);
    }

    @Test
    public void testBookmarkTimestampToVersionMigrationChunkingMore() throws Exception {
        final SQLiteDatabase db = helper.getWritableDatabase();

        testBookmarkVersionMigrationChunkingForCount(db, DBUtils.SQLITE_MAX_VARIABLE_NUMBER - DEFAULT_BOOKMARKS_COUNT + 1);
    }

    private void testBookmarkVersionMigrationChunkingForCount(SQLiteDatabase db, int count) {
        final long lastSynced = System.currentTimeMillis();
        ArrayList<String> guids = insertBookmarks(db, count, lastSynced - 1, lastSynced - 1);

        HashMap<String, String> guidToRelMap = getDefaultGuidToRelMap();
        for (String guid : guids) {
            guidToRelMap.put(guid, "c<m<");
        }

        BrowserDatabaseHelper.performBookmarkTimestampToVersionMigration(db, lastSynced);

        assertCorrectVersionValues(db, guidToRelMap);
    }

    private ArrayList<String> insertBookmarks(SQLiteDatabase db, int count, long created, long modified) {
        ArrayList<String> guids = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            String guid = UUID.randomUUID().toString();
            insertBookmark(db, guid, created, modified);
            guids.add(guid);
        }
        return guids;
    }

    private void assertCorrectVersionValues(SQLiteDatabase db, HashMap<String, String> guidToRelMap) {
        final Cursor cursor = db.query(
                Bookmarks.TABLE_NAME,
                new String[] {
                        Bookmarks.GUID,
                        Bookmarks.LOCAL_VERSION,
                        Bookmarks.SYNC_VERSION
                }, null, null, null, null, null
        );
        assertNotNull(cursor);

        final int guidCol = cursor.getColumnIndexOrThrow(Bookmarks.GUID);
        final int localVersionCol = cursor.getColumnIndexOrThrow(Bookmarks.LOCAL_VERSION);
        final int syncVersionCol = cursor.getColumnIndexOrThrow(Bookmarks.SYNC_VERSION);

        try {
            while (cursor.moveToNext()) {
                String guid = cursor.getString(guidCol);
                int localVersion = cursor.getInt(localVersionCol);
                int syncVersion = cursor.getInt(syncVersionCol);

                String rel = guidToRelMap.get(guid);

                // Test combinations of modified and created timestamp vs "last sync" timestamp
                // Our implementation should ignore 'created', since prior timestamp-based syncing
                // ignored it as well.
                switch (rel) {
                    // -> assume that our timestamp-based syncing did the right thing and uploaded this record
                    case "c<m<":
                    // - recordCreated < lastBookmarkSync && recordModified == lastBookmarkSync
                    // -> does not need an upload (assume that record was inserted during last sync)
                    case "c<m=":
                    // - recordCreated == lastBookmarkSync
                    case "c=m<":
                    // -> assume that record was inserted during last sync
                    case "c=m=":
                    // -> assume that record was inserted during last sync
                    case "c>m=":
                    // - recordCreated > lastBookmarkSync
                    case "c>m<":
                    // missing 'created' timestamp shouldn't matter
                    case "cnm<":
                    case "cnm=":
                        assertDoesNotNeedUpload(localVersion, syncVersion);
                        break;

                    // -> needs to be uploaded (record was modified after we last synced)
                    case "c<m>":
                    // assume that timestamp-based syncing picked this up already
                    case "c=m>":
                    // assume that timestamp-based syncing picked this up already
                    case "c>m>":
                    // if one of the timestamps is missing, assume record needs to be uploaded.
                    case "c>mn":
                    case "c=mn":
                    case "c<mn":
                    case "cnm>":
                    case "cnmn":
                        assertNeedsUpload(localVersion, syncVersion);
                        break;

                    default:
                        throw new IllegalStateException();
                }
            }
        } finally {
            cursor.close();
        }
    }

    private void assertNeedsUpload(int localVersion, int syncVersion) {
        assertEquals(1, localVersion);
        assertEquals(0, syncVersion);
    }

    private void assertDoesNotNeedUpload(int localVersion, int syncVersion) {
        assertEquals(1, localVersion);
        assertEquals(1, syncVersion);
    }

    private void assertDefaultVersionValues(SQLiteDatabase db) {
        final Cursor cursor = db.query(
                BrowserContract.Bookmarks.TABLE_NAME,
                new String[]{
                        Bookmarks.LOCAL_VERSION,
                        Bookmarks.SYNC_VERSION
                }, null, null, null, null, null);
        assertNotNull(cursor);

        try {
            final int localVersionCol = cursor.getColumnIndexOrThrow(Bookmarks.LOCAL_VERSION);
            final int syncVersionCol = cursor.getColumnIndexOrThrow(Bookmarks.SYNC_VERSION);
            while (cursor.moveToNext()) {
                assertEquals(1, cursor.getInt(localVersionCol));
                assertEquals(0, cursor.getInt(syncVersionCol));
            }
        } finally {
            cursor.close();
        }
    }

    private void insertBookmark(SQLiteDatabase db, String guid, Long created, Long modified) {
        ContentValues values = new ContentValues();
        values.put(Bookmarks.GUID, guid);
        values.put(Bookmarks.TYPE, Bookmarks.TYPE_BOOKMARK);
        values.put(Bookmarks.DATE_MODIFIED, modified);
        values.put(Bookmarks.DATE_CREATED, created);
        values.put(Bookmarks.POSITION, 0);
        values.put(Bookmarks.URL, "http://example.com/" + random.nextInt(Integer.MAX_VALUE));

        db.insertOrThrow(Bookmarks.TABLE_NAME, Bookmarks.GUID, values);
    }

    private long boundaryAround(long base, String dir) {
        switch (dir) {
            case "<":
                return base - 1;
            case "=":
                return base;
            case ">":
                return base + 1;
            default:
                throw new IllegalArgumentException();
        }
    }

    private HashMap<String, String> getDefaultGuidToRelMap() {
        HashMap<String, String> guidToRelMap = new HashMap<>();
        guidToRelMap.put("mobile", "c<m<");
        guidToRelMap.put("unfiled", "c<m<");
        guidToRelMap.put("places", "c<m<");
        guidToRelMap.put("pinned", "c<m<");
        guidToRelMap.put("tags", "c<m<");
        guidToRelMap.put("toolbar", "c<m<");
        guidToRelMap.put("menu", "c<m<");
        return guidToRelMap;
    }

    private static class TestSQLiteOpenHelper extends BrowserDatabaseHelper {
        public TestSQLiteOpenHelper() {
            super(RuntimeEnvironment.application, "test-path.db");
        }
    }
}