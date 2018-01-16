/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.annotation.SuppressLint;
import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.support.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.HashMap;
import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class LocalBrowserDBTest {
    private static final long INVALID_ID = -1;
    private static final String BOOKMARK_URL = "https://www.mozilla.org";
    private static final String BOOKMARK_TITLE = "mozilla";

    private static final String UPDATE_URL = "https://bugzilla.mozilla.org";
    private static final String UPDATE_TITLE = "bugzilla";

    private static final String FOLDER_NAME = "folder1";

    private Context context;
    private ContentProvider provider;
    private ContentProviderClient bookmarkClient;

    @Before
    public void setUp() throws Exception {
        context = RuntimeEnvironment.application;
        provider = DelegatingTestContentProvider.createDelegatingBrowserProvider();

        ShadowContentResolver contentResolver = new ShadowContentResolver();
        bookmarkClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);
    }

    @After
    public void tearDown() {
        bookmarkClient.release();
        provider.shutdown();
    }

    @Test
    public void testRemoveBookmarkWithURL() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);
        Cursor cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNotNull(cursor);

        long parentId = INVALID_ID;
        try {
            assertTrue(cursor.moveToFirst());

            final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(title, BOOKMARK_TITLE);

            final String url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(url, BOOKMARK_URL);

            parentId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
        } finally {
            cursor.close();
        }
        assertNotEquals(parentId, INVALID_ID);

        final long lastModifiedBeforeRemove = getModifiedDate(parentId);

        // Remove bookmark record
        db.removeBookmarksWithURL(cr, BOOKMARK_URL);

        // Check the record has been removed
        cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNull(cursor);

        // Check parent's lastModified timestamp is updated
        final long lastModifiedAfterRemove = getModifiedDate(parentId);
        assertTrue(lastModifiedAfterRemove > lastModifiedBeforeRemove);
    }

    @Test
    public void testRemoveBookmarkFolder() throws Exception {
        final BrowserDB db = new LocalBrowserDB("default");
        final ContentResolver cr = context.getContentResolver();

        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        // Create a parent folder
        final Uri parentUri = db.addBookmarkFolder(cr, FOLDER_NAME, rootId);
        assertNotNull(parentUri);
        // Get id from Uri
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Create nested child bookmarks and folders
        // parent / bookmark-1
        //        / folder-1
        //        / folder-2 / bookmark-2
        insertBookmark("bookmark-1", null, "guid-bookmark-1", parentId, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        insertBookmark("folder-1", null, "guid-folder-1", parentId, BrowserContract.Bookmarks.TYPE_FOLDER);

        final Uri folderUri = insertBookmark("folder-2", null, "guid-folder-2", parentId, BrowserContract.Bookmarks.TYPE_FOLDER);
        assertNotNull(folderUri);
        final long folderId = Long.parseLong(folderUri.getLastPathSegment());
        insertBookmark("bookmark-2", null, "guid-bookmark-2", folderId, BrowserContract.Bookmarks.TYPE_BOOKMARK);

        final String selection = BrowserContract.Bookmarks.PARENT + " = ? OR " +
                                 BrowserContract.Bookmarks.PARENT + " = ?";
        final String[] selectionArgs = { String.valueOf(parentId), String.valueOf(folderId) };
        final int childCountBeforeRemove = getRowCount(selection, selectionArgs);
        assertEquals(4, childCountBeforeRemove);

        db.removeBookmarkWithId(cr, parentId);
        final Cursor cursor = db.getBookmarkById(cr, parentId);
        assertNull(cursor);

        final int childCountAfterRemove = getRowCount(selection, selectionArgs);
        assertEquals(0, childCountAfterRemove);
    }

    @Test
    public void testChunkDeleteBookmarkFolder() throws Exception {
        final BrowserDB db = new LocalBrowserDB("default");
        final ContentResolver cr = context.getContentResolver();

        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        // Create a parent folder
        final Uri parentUri = db.addBookmarkFolder(cr, FOLDER_NAME, rootId);
        assertNotNull(parentUri);
        // Get id from Uri
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Create more than DBUtils.SQLITE_MAX_VARIABLE_NUMBER child bookmarks/folders.
        for (int i = 0; i < DBUtils.SQLITE_MAX_VARIABLE_NUMBER; ++i) {
            insertBookmark("bookmark-" + i,
                           "https://www.mozilla-" + i + ".org",
                           "guid-bookmark-" + i,
                           parentId,
                           BrowserContract.Bookmarks.TYPE_BOOKMARK);
            insertBookmark("folder-" + i,
                           null,
                           "guid-folder-" + i,
                           parentId,
                           BrowserContract.Bookmarks.TYPE_FOLDER);
        }

        final String selection = BrowserContract.Bookmarks.PARENT + " = ?";
        final String[] selectionArgs = { String.valueOf(parentId) };
        final int childCountBeforeRemove = getRowCount(selection, selectionArgs);
        assertEquals(DBUtils.SQLITE_MAX_VARIABLE_NUMBER * 2, childCountBeforeRemove);

        db.removeBookmarkWithId(cr, parentId);
        final Cursor cursor = db.getBookmarkById(cr, parentId);
        assertNull(cursor);

        final int childCountAfterRemove = getRowCount(selection, selectionArgs);
        assertEquals(0, childCountAfterRemove);
    }

    @Test
    public void testDeleteBookmarkWithDifferentSize() throws Exception {
        final BrowserDB db = new LocalBrowserDB("default");
        final ContentResolver cr = context.getContentResolver();

        testDeleteBookmarkWithSize(db, cr, "guid-prefix-1", 100);
        testDeleteBookmarkWithSize(db, cr, "guid-prefix-2", DBUtils.SQLITE_MAX_VARIABLE_NUMBER - 1);
        testDeleteBookmarkWithSize(db, cr, "guid-prefix-3", DBUtils.SQLITE_MAX_VARIABLE_NUMBER + 1);
        testDeleteBookmarkWithSize(db, cr, "guid-prefix-4", DBUtils.SQLITE_MAX_VARIABLE_NUMBER);
        testDeleteBookmarkWithSize(db, cr, "guid-prefix-5", DBUtils.SQLITE_MAX_VARIABLE_NUMBER + 100);
        testDeleteBookmarkWithSize(db, cr, "guid-prefix-6", DBUtils.SQLITE_MAX_VARIABLE_NUMBER * 2);
    }

    @Test
    public void testRemoveBookmarkWithId() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);
        Cursor cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNotNull(cursor);

        long bookmarkId = INVALID_ID;
        long parentId = INVALID_ID;
        try {
            assertTrue(cursor.moveToFirst());

            bookmarkId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
            parentId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
        } finally {
            cursor.close();
        }
        assertNotEquals(bookmarkId, INVALID_ID);
        assertNotEquals(parentId, INVALID_ID);

        final long lastModifiedBeforeRemove = getModifiedDate(parentId);

        // Remove bookmark record
        db.removeBookmarkWithId(cr, bookmarkId);

        cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNull(cursor);

        // Check parent's lastModified timestamp is updated
        final long lastModifiedAfterRemove = getModifiedDate(parentId);
        assertTrue(lastModifiedAfterRemove > lastModifiedBeforeRemove);
    }

    @Test
    public void testUpdateBookmark() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);
        Cursor cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNotNull(cursor);

        long bookmarkId = INVALID_ID;
        long parentId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        try {
            assertTrue(cursor.moveToFirst());

            final String insertedUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(insertedUrl, BOOKMARK_URL);

            final String insertedTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(insertedTitle, BOOKMARK_TITLE);

            bookmarkId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
        } finally {
            cursor.close();
        }
        assertNotEquals(bookmarkId, INVALID_ID);

        final long parentLastModifiedBeforeUpdate = getModifiedDate(parentId);

        // Update bookmark record
        db.updateBookmark(cr, bookmarkId, UPDATE_URL, UPDATE_TITLE, "");
        cursor = db.getBookmarkById(cr, bookmarkId);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());

            final String updatedUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(updatedUrl, UPDATE_URL);

            final String updatedTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(updatedTitle, UPDATE_TITLE);
        } finally {
            cursor.close();
        }

        // Check parent's lastModified timestamp isn't changed
        final long parentLastModifiedAfterUpdate = getModifiedDate(parentId);
        assertTrue(parentLastModifiedAfterUpdate == parentLastModifiedBeforeUpdate);
    }

    @Test
    public void testUpdateBookmarkWithParentChange() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);
        Cursor cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNotNull(cursor);

        long bookmarkId = INVALID_ID;
        long originalParentId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        try {
            assertTrue(cursor.moveToFirst());

            final String insertedUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(insertedUrl, BOOKMARK_URL);

            final String insertedTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(insertedTitle, BOOKMARK_TITLE);

            bookmarkId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
        } finally {
            cursor.close();
        }
        assertNotEquals(bookmarkId, INVALID_ID);

        // Create a folder
        final Uri newFolderUri = db.addBookmarkFolder(cr, FOLDER_NAME, originalParentId);
        // Get id from Uri
        final long newParentId = Long.parseLong(newFolderUri.getLastPathSegment());

        final long originalParentLastModifiedBeforeUpdate = getModifiedDate(originalParentId);
        final long newParentLastModifiedBeforeUpdate = getModifiedDate(newParentId);

        // Update bookmark record
        db.updateBookmark(cr, bookmarkId, UPDATE_URL, UPDATE_TITLE, "", newParentId, originalParentId);
        cursor = db.getBookmarkById(cr, bookmarkId);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());

            final String updatedUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(updatedUrl, UPDATE_URL);

            final String updatedTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(updatedTitle, UPDATE_TITLE);

            final long parentId = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
            assertEquals(parentId, newParentId);
        } finally {
            cursor.close();
        }

        // Check parent's lastModified timestamp
        final long originalParentLastModifiedAfterUpdate = getModifiedDate(originalParentId);
        assertTrue(originalParentLastModifiedAfterUpdate > originalParentLastModifiedBeforeUpdate);

        final long newParentLastModifiedAfterUpdate = getModifiedDate(newParentId);
        assertTrue(newParentLastModifiedAfterUpdate > newParentLastModifiedBeforeUpdate);
    }

    @Test
    public void testAddBookmarkFolder() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        // Add a bookmark folder record
        final long rootFolderId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        final long lastModifiedBeforeAdd = getModifiedDate(rootFolderId);
        final Uri folderUri = db.addBookmarkFolder(cr, FOLDER_NAME, rootFolderId);
        // Get id from Uri
        long folderId = Long.parseLong(folderUri.getLastPathSegment());

        final Cursor cursor = db.getBookmarkById(cr, folderId);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());

            final String name = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(name, FOLDER_NAME);

            final long parent = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.PARENT));
            assertEquals(parent, rootFolderId);
        } finally {
            cursor.close();
        }

        // Check parent's lastModified timestamp is updated
        final long lastModifiedAfterAdd = getModifiedDate(rootFolderId);
        assertTrue(lastModifiedAfterAdd > lastModifiedBeforeAdd);
    }

    @Test
    public void testAddBookmark() throws Exception {
        BrowserDB db = new LocalBrowserDB("default");
        ContentResolver cr = context.getContentResolver();

        final long rootFolderId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        final long lastModifiedBeforeAdd = getModifiedDate(rootFolderId);

        // Add a bookmark
        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);

        final Cursor cursor = db.getBookmarkForUrl(cr, BOOKMARK_URL);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());

            final String name = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
            assertEquals(name, BOOKMARK_TITLE);

            final String url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.URL));
            assertEquals(url, BOOKMARK_URL);
        } finally {
            cursor.close();
        }

        // Check parent's lastModified timestamp is updated
        final long lastModifiedAfterAdd = getModifiedDate(rootFolderId);
        assertTrue(lastModifiedAfterAdd > lastModifiedBeforeAdd);
    }

    @Test
    public void testGetAllBookmarkFolders() throws Exception {
        final BrowserDB db = new LocalBrowserDB("default");
        final ContentResolver cr = context.getContentResolver();

        // Get folder sparse array directly through BrowserProvider
        final Map<Long, Folder> folderMap = getFolderMap();
        final int folderCount = folderMap.size();

        final long parentId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        assertNotEquals(parentId, INVALID_ID);

        // Create 2 folders into 'mobile bookmarks' and put to folderSparseArray.
        final Uri childUri1 = db.addBookmarkFolder(cr, "child-1", parentId);
        final Folder childFolder1 = getFolder(childUri1);
        assertNotNull(childFolder1);
        folderMap.put(childFolder1.id, childFolder1);

        final Uri childUri2 = db.addBookmarkFolder(cr, "child-2", parentId);
        final Folder childFolder2 = getFolder(childUri2);
        assertNotNull(childFolder2);
        folderMap.put(childFolder2.id, childFolder2);

        // Create a bookmark, adding a bookmark should not increase folder count.
        db.addBookmark(cr, BOOKMARK_TITLE, BOOKMARK_URL);

        final Cursor cursor = db.getAllBookmarkFolders(cr);
        assertNotNull(cursor);

        try {
            // Current folder count should equal (folderCount + 2) since we added 2 folders.
            final int currentFolderCount = cursor.getCount();
            assertEquals(folderCount + 2, currentFolderCount);

            while (cursor.moveToNext()) {
                final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
                final String guid = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID));
                final Folder actual = new Folder(id, title, guid);
                final Folder expected = folderMap.get(id);
                assertEquals(expected, actual);
            }
        } finally {
            cursor.close();
        }
    }

    private long getBookmarkIdFromGuid(String guid) throws RemoteException {
        Cursor cursor = bookmarkClient.query(BrowserContract.Bookmarks.CONTENT_URI,
                                             new String[] { BrowserContract.Bookmarks._ID },
                                             BrowserContract.Bookmarks.GUID + " = ?",
                                             new String[] { guid },
                                             null);
        assertNotNull(cursor);

        long id = INVALID_ID;
        try {
            assertTrue(cursor.moveToFirst());
            id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
        } finally {
            cursor.close();
        }
        assertNotEquals(id, INVALID_ID);
        return id;
    }

    private long getModifiedDate(long id) throws RemoteException {
        Cursor cursor = bookmarkClient.query(BrowserContract.Bookmarks.CONTENT_URI,
                                             new String[] { BrowserContract.Bookmarks.DATE_MODIFIED },
                                             BrowserContract.Bookmarks._ID + " = ?",
                                             new String[] { String.valueOf(id) },
                                             null);
        assertNotNull(cursor);

        long modified = -1;
        try {
            assertTrue(cursor.moveToFirst());
            modified = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.DATE_MODIFIED));
        } finally {
            cursor.close();
        }
        assertNotEquals(modified, -1);
        return modified;
    }

    private Map<Long, Folder> getFolderMap() throws RemoteException {
        final String selection = BrowserContract.Bookmarks.TYPE + " = ? AND " +
                                         BrowserContract.Bookmarks.GUID + " NOT IN (?, ?, ?, ?, ?) AND " +
                                         BrowserContract.Bookmarks.IS_DELETED + " = 0";
        final String[] selectionArgs = { String.valueOf(BrowserContract.Bookmarks.TYPE_FOLDER),
                                         BrowserContract.Bookmarks.SCREENSHOT_FOLDER_GUID,
                                         BrowserContract.Bookmarks.FAKE_READINGLIST_SMARTFOLDER_GUID,
                                         BrowserContract.Bookmarks.TAGS_FOLDER_GUID,
                                         BrowserContract.Bookmarks.PLACES_FOLDER_GUID,
                                         BrowserContract.Bookmarks.PINNED_FOLDER_GUID };

        final Cursor cursor = bookmarkClient.query(BrowserContract.Bookmarks.CONTENT_URI,
                                                   new String[] { BrowserContract.Bookmarks._ID,
                                                                  BrowserContract.Bookmarks.TITLE,
                                                                  BrowserContract.Bookmarks.GUID },
                                                   selection,
                                                   selectionArgs,
                                                   null);
        assertNotNull(cursor);

        @SuppressLint("UseSparseArrays") final Map<Long, Folder> array = new HashMap<>();
        try {
            while (cursor.moveToNext()) {
                final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
                final String guid = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID));

                final Folder folder = new Folder(id, title, guid);
                array.put(id, folder);
            }
        } finally {
            cursor.close();
        }
        return array;
    }

    @Nullable
    private Folder getFolder(Uri uri) throws RemoteException {
        final Cursor cursor = bookmarkClient.query(uri,
                                                   new String[] { BrowserContract.Bookmarks._ID,
                                                                  BrowserContract.Bookmarks.TITLE,
                                                                  BrowserContract.Bookmarks.GUID },
                                                   null,
                                                   null,
                                                   null);
        assertNotNull(cursor);
        try {
            if (cursor.moveToFirst()) {
                final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks._ID));
                final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.TITLE));
                final String guid = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.GUID));
                return new Folder(id, title, guid);
            }
        } finally {
            cursor.close();
        }
        return null;
    }

    private int getRowCount(String selection, String[] selectionArgs) throws RemoteException {
        final Cursor cursor = bookmarkClient.query(BrowserContract.Bookmarks.CONTENT_URI,
                                                   new String[] { BrowserContract.Bookmarks._ID },
                                                   selection,
                                                   selectionArgs,
                                                   null);
        assertNotNull(cursor);
        try {
            return cursor.getCount();
        } finally {
            cursor.close();
        }
    }

    private Uri insertBookmark(String title, String url, String guid, long parentId, int type) throws RemoteException {
        final ContentValues values = new ContentValues();
        if (title != null) {
            values.put(BrowserContract.Bookmarks.TITLE, title);
        }
        if (url != null) {
            values.put(BrowserContract.Bookmarks.URL, url);
        }
        if (guid != null) {
            values.put(BrowserContract.Bookmarks.GUID, guid);
        }
        values.put(BrowserContract.Bookmarks.PARENT, parentId);
        values.put(BrowserContract.Bookmarks.TYPE, type);

        final long now = System.currentTimeMillis();
        values.put(BrowserContract.Bookmarks.DATE_CREATED, now);
        values.put(BrowserContract.Bookmarks.DATE_MODIFIED, now);

        return bookmarkClient.insert(BrowserContract.Bookmarks.CONTENT_URI, values);
    }

    private void testDeleteBookmarkWithSize(BrowserDB db, ContentResolver cr, String guidPrefix, int size) throws RemoteException {
        final long rootId = getBookmarkIdFromGuid(BrowserContract.Bookmarks.MOBILE_FOLDER_GUID);
        // Create a parent folder
        final Uri parentUri = db.addBookmarkFolder(cr, FOLDER_NAME, rootId);
        assertNotNull(parentUri);
        // Get id from Uri
        final long parentId = Long.parseLong(parentUri.getLastPathSegment());

        // Create more than DBUtils.SQLITE_MAX_VARIABLE_NUMBER child bookmarks/folders.
        for (int i = 0; i < size; ++i) {
            insertBookmark("bookmark-" + i,
                           "https://www.mozilla-" + i + ".org",
                           guidPrefix + i,
                           parentId,
                           BrowserContract.Bookmarks.TYPE_BOOKMARK);
        }

        final String selection = BrowserContract.Bookmarks.PARENT + " = ?";
        final String[] selectionArgs = { String.valueOf(parentId) };
        final int childCountBeforeRemove = getRowCount(selection, selectionArgs);
        assertEquals(size, childCountBeforeRemove);

        db.removeBookmarkWithId(cr, parentId);
        final Cursor cursor = db.getBookmarkById(cr, parentId);
        assertNull(cursor);

        final int childCountAfterRemove = getRowCount(selection, selectionArgs);
        assertEquals(0, childCountAfterRemove);
    }

    /**
     * A private struct to make it easier to access folder data.
     */
    private static class Folder {
        private final long id;
        private final String title;
        private final String guid;

        private Folder(long id, String title, String guid) {
            this.id = id;
            this.title = title;
            this.guid = guid;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) {
                return false;
            }
            if (!(o instanceof Folder)) {
                return false;
            }
            if (this == o) {
                return true;
            }

            final Folder other = (Folder) o;
            if (this.id != other.id) {
                return false;
            }
            if (this.title == null) {
                if (other.title != null) {
                    return false;
                }
            } else if (!this.title.equals(other.title)) {
                return false;
            }
            if (this.guid == null) {
                if (other.guid != null) {
                    return false;
                }
            } else if (!this.guid.equals(other.guid)) {
                return false;
            }
            return true;
        }

        @Override
        public int hashCode() {
            final int prime = 31;
            int result = 1;
            result = prime * result + (int) id;
            result = prime * result + ((title == null) ? 0 : title.hashCode());
            result = prime * result + ((guid == null) ? 0 : guid.hashCode());
            return result;
        }
    }
}
