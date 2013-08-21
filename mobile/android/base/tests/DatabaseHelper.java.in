#filter substitution
package @ANDROID_PACKAGE_NAME@.tests;

import @ANDROID_PACKAGE_NAME@.*;

import android.app.Activity;
import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import java.lang.reflect.Method;
import java.util.ArrayList;

class DatabaseHelper {
    protected enum BrowserDataType {BOOKMARKS, HISTORY};
    private Activity mActivity;
    private Assert mAsserter;

    public DatabaseHelper(Activity activity, Assert asserter) {
        mActivity = activity;
        mAsserter = asserter;
    }
    /**
    * This method can be used to check if an URL is present in the bookmarks database
    */
    protected boolean isBookmark(String url) {
        try {
            ContentResolver resolver = mActivity.getContentResolver();
            ClassLoader classLoader = mActivity.getClassLoader();
            Class browserDB = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
            Method isBookmarked = browserDB.getMethod("isBookmark", ContentResolver.class, String.class);
            return (Boolean)isBookmarked.invoke(null, resolver, url);
        } catch (Exception e) {
            mAsserter.ok(false, "Exception while checking if url is bookmarked", e.toString());
            return false;
        }
    }

    protected Uri buildUri(BrowserDataType dataType) {
        Uri uri = null;
        if (dataType == BrowserDataType.BOOKMARKS || dataType == BrowserDataType.HISTORY) {
            uri = Uri.parse("content://@ANDROID_PACKAGE_NAME@.db.browser/" + dataType.toString().toLowerCase());
        } else {
           mAsserter.ok(false, "The wrong data type has been provided = " + dataType.toString(), "Please provide the correct data type");
        }
        uri = uri.buildUpon().appendQueryParameter("profile", "default")
                             .appendQueryParameter("sync", "true").build();
        return uri;
    }

    /**
     * Adds a bookmark, or updates the bookmark title if the url already exists.
     *
     * The LocalBrowserDB.addBookmark implementation handles updating existing bookmarks.
     */
    protected void addOrUpdateMobileBookmark(String title, String url) {
        try {
            ContentResolver resolver = mActivity.getContentResolver();
            ClassLoader classLoader = mActivity.getClassLoader();
            Class browserDB = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
            Method addBookmark = browserDB.getMethod("addBookmark", ContentResolver.class, String.class, String.class);
            addBookmark.invoke(null, resolver, title, url);
            mAsserter.ok(true, "Inserting/updating a new bookmark", "Inserting/updating the bookmark with the title = " + title + " and the url = " + url);
        } catch (Exception e) {
            mAsserter.ok(false, "Exception adding bookmark: ", e.toString());
        }
    }

    /**
     * Updates the title and keyword of a bookmark with the given URL.
     *
     * Warning: This method assumes that there's only one bookmark with the given URL.
     */
    protected void updateBookmark(String url, String title, String keyword) {
        try {
            ContentResolver resolver = mActivity.getContentResolver();
            ClassLoader classLoader = mActivity.getClassLoader();
            Class browserDB = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
            Method getBookmarkForUrl = browserDB.getMethod("getBookmarkForUrl", ContentResolver.class, String.class);

            // Get the id for the bookmark with the given URL.
            Cursor c = null;
            try {
                c = (Cursor) getBookmarkForUrl.invoke(null, resolver, url);
                if (!c.moveToFirst()) {
                    mAsserter.ok(false, "Getting bookmark with url", "Couldn't find bookmark with url = " + url);
                    return;
                }

                int id = c.getInt(c.getColumnIndexOrThrow("_id"));
                Method updateBookmark = browserDB.getMethod("updateBookmark", ContentResolver.class, int.class, String.class, String.class, String.class);
                updateBookmark.invoke(null, resolver, id, url, title, keyword);

                mAsserter.ok(true, "Updating bookmark", "Updating bookmark with url = " + url);
            } finally {
                if (c != null) {
                    c.close();
                }
            }
        } catch (Exception e) {
            mAsserter.ok(false, "Exception updating bookmark: ", e.toString());
        }
    }

    protected void deleteBookmark(String url) {
        try {
            ContentResolver resolver = mActivity.getContentResolver();
            ClassLoader classLoader = mActivity.getClassLoader();
            Class browserDB = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
            Method removeBookmark = browserDB.getMethod("removeBookmarksWithURL", ContentResolver.class, String.class);
            removeBookmark.invoke(null, resolver, url);
        } catch (Exception e) {
            mAsserter.ok(false, "Exception deleting bookmark", e.toString());
        }
    }

    protected void deleteHistoryItem(String url) {
        try {
            ContentResolver resolver = mActivity.getContentResolver();
            ClassLoader classLoader = mActivity.getClassLoader();
            Class browserDB = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
            Method removeHistory = browserDB.getMethod("removeHistoryEntry", ContentResolver.class, String.class);
            removeHistory.invoke(null, resolver, url);
        } catch (Exception e) {
            mAsserter.ok(false, "Exception deleting history item", e.toString());
        }
    }

    // About the same implementation as getFolderIdFromGuid from LocalBrowserDB because it is declared private and we can't use reflections to access it
    protected long getFolderIdFromGuid(String guid) {
        ContentResolver resolver = mActivity.getContentResolver();
        long folderId = Long.valueOf(-1);
        Uri bookmarksUri = buildUri(BrowserDataType.BOOKMARKS);
        Cursor c = null;
        try {
            c = resolver.query(bookmarksUri,
                               new String[] { "_id" },
                               "guid = ?",
                               new String[] { guid },
                               null);
            if (c.moveToFirst()) {
                folderId = c.getLong(c.getColumnIndexOrThrow("_id"));
            }
            if (folderId == -1) {
                mAsserter.ok(false, "Trying to get the folder id" ,"We did not get the correct folder id");
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return folderId;
    }

     /**
     * @param  a BrowserDataType value - either HISTORY or BOOKMARKS
     * @return an ArrayList of the urls in the Firefox for Android Bookmarks or History databases
     */
    protected ArrayList<String> getBrowserDBUrls(BrowserDataType dataType) {
        ArrayList<String> browserData = new ArrayList<String>();
        ContentResolver resolver = mActivity.getContentResolver();
        ClassLoader classLoader = mActivity.getClassLoader();
        Cursor cursor = null;
        Uri uri = buildUri(dataType);
        if (dataType == BrowserDataType.HISTORY) {
            try {
                Class browserDBClass = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
                Method getAllVisitedHistory = browserDBClass.getMethod("getAllVisitedHistory", ContentResolver.class);
                cursor = (Cursor)getAllVisitedHistory.invoke(null, resolver);
            } catch (Exception e) {
                mAsserter.ok(false, "Exception while getting history", e.toString());
            }
        } else if (dataType == BrowserDataType.BOOKMARKS) {
            try {
                Class browserDBClass = classLoader.loadClass("org.mozilla.gecko.db.BrowserDB");
                Method getBookmarks = browserDBClass.getMethod("getBookmarksInFolder", ContentResolver.class, Long.TYPE);
                cursor = (Cursor)getBookmarks.invoke(null, resolver, getFolderIdFromGuid("mobile"));
            } catch (Exception e) {
                mAsserter.ok(false, "Exception while getting bookmarks", e.toString());
            }
        }
        if (cursor != null) {
            cursor.moveToFirst();
            for (int i = 0; i < cursor.getCount(); i++ ) {
                 // The url field may be null for folders in the structure of the Bookmarks table for Firefox so we should eliminate those
                if (cursor.getString(cursor.getColumnIndex("url")) != null) {
                    browserData.add(cursor.getString(cursor.getColumnIndex("url")));
                }
                if(!cursor.isLast()) {
                    cursor.moveToNext();
                }
            }
        } else {
             mAsserter.ok(false, "We could not retrieve any data from the database", "The cursor was null");
        }
        if (cursor != null) {
            cursor.close();
        }
        return browserData;
    }
}
