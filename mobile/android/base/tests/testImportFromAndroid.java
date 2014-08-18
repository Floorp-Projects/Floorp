package org.mozilla.gecko.tests;

import java.util.ArrayList;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoProfile;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Browser;

/**
  * This test covers the Import from Android feature
  * The test will save the existing bookmarks and history then will do an Import
  * After the import it will check that the bookmarks and history items from Android are imported
  * Then it will test that the old data from Firefox is not lost
  * At the end will test that a second import will not duplicate information
  */

public class testImportFromAndroid extends AboutHomeTest {
    private static final int MAX_WAIT_TIMEOUT = 15000;
    ArrayList<String> androidData = new ArrayList<String>();
    ArrayList<String> firefoxHistory = new ArrayList<String>();

    public void testImportFromAndroid() {
        ArrayList<String> firefoxBookmarks = new ArrayList<String>();
        ArrayList<String> oldFirefoxHistory = new ArrayList<String>();
        ArrayList<String> oldFirefoxBookmarks = new ArrayList<String>();
        blockForGeckoReady();

        // Get the Android history
        androidData = getAndroidUrls("history");

        // Add some overlapping data from the Android Stock Browser to Firefox before import
        addData();

        // Get the initial bookmarks and history
        oldFirefoxBookmarks = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.BOOKMARKS);
        oldFirefoxHistory = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.HISTORY);

        // Import the bookmarks and history
        importDataFromAndroid();

        // Get the Android history and the Firefox bookmarks and history lists
        firefoxHistory = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.HISTORY);
        firefoxBookmarks = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.BOOKMARKS);

        /**
         * Add a delay to make sure the imported items are added to the array lists 
         * if there are a lot of history items in the Android Browser database
         */
        boolean success = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                if (androidData.size() <= firefoxHistory.size()) {
                    return true;
                } else {
                    return false;
                }
            }
        }, MAX_WAIT_MS);

        /**
         * Verify the history and bookmarks are imported
         * Android history also contains the android bookmarks so we don't need to get them separately here
         */
        for (String url:androidData) {
            mAsserter.ok(firefoxHistory.contains(url)||firefoxBookmarks.contains(url), "Checking if Android" + (firefoxBookmarks.contains(url) ? " Bookmark" : " History item") + " is present", url + " was imported");
        }

        // Verify the original Firefox Bookmarks are not deleted
        for (String url:oldFirefoxBookmarks) {
             mAsserter.ok(firefoxBookmarks.contains(url), "Checking if original Firefox Bookmark is present", " Firefox Bookmark " + url + " was not removed");
        }

        // Verify the original Firefox History is not deleted
        for (String url:oldFirefoxHistory) {
             mAsserter.ok(firefoxHistory.contains(url), "Checking original Firefox History item is present", " Firefox History item " + url + " was not removed");
        }

        // Import data again and make sure bookmarks are not duplicated
        importDataFromAndroid();

        // Verify bookmarks are not duplicated
        ArrayList<String> verifiedBookmarks = new ArrayList<String>();
        firefoxBookmarks = mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.BOOKMARKS);
        for (String url:firefoxBookmarks) {
             if (verifiedBookmarks.contains(url)) {
                 mAsserter.ok(false, "Bookmark " + url + " should not be duplicated", "Bookmark is duplicated");
             } else {
                 verifiedBookmarks.add(url);
                 mAsserter.ok(true, "Bookmark " + url + " was not duplicated", "Bookmark is unique");
             }
        }

        // Verify history count is not increased after the second import
        mAsserter.ok(firefoxHistory.size() == mDatabaseHelper.getBrowserDBUrls(DatabaseHelper.BrowserDataType.HISTORY).size(), "The number of history entries was not increased", "None of the items were duplicated");
    }

    private void addData() {
        ArrayList<String> androidBookmarks = getAndroidUrls("bookmarks");

        // Add a few Bookmarks from Android to Firefox Mobile
        for (String url:androidBookmarks) {
            // Add every 3rd bookmark to Firefox Mobile
            if ((androidBookmarks.indexOf(url) % 3) == 0) {
                mDatabaseHelper.addOrUpdateMobileBookmark("Bookmark Number" + String.valueOf(androidBookmarks.indexOf(url)), url);
            }
        }

        // Add a few history items in Firefox Mobile
        ContentResolver resolver = getActivity().getContentResolver();
        Uri uri = Uri.parse("content://" + AppConstants.ANDROID_PACKAGE_NAME + ".db.browser/history");
        uri = uri.buildUpon().appendQueryParameter("profile", GeckoProfile.DEFAULT_PROFILE)
                             .appendQueryParameter("sync", "true").build();
        for (String url:androidData) {
            // Add every 3rd website from Android History to Firefox Mobile
            if ((androidData.indexOf(url) % 3) == 0) {
                 ContentValues values = new ContentValues();
                 values.put("title", "Page" + url);
                 values.put("url", url);
                 values.put("date", System.currentTimeMillis());
                 values.put("visits", androidData.indexOf(url));
                 resolver.insert(uri, values);
            }
        }
    }

    private void importDataFromAndroid() {
        waitForText(StringHelper.TITLE_PLACE_HOLDER);
        selectSettingsItem(StringHelper.CUSTOMIZE_SECTION_LABEL, StringHelper.IMPORT_FROM_ANDROID_LABEL);

        // Wait for the Import form Android pop-up to be opened. It has the same title as the option so waiting for the "Cancel" button
        waitForText("Cancel");
        mSolo.clickOnButton("Import");

        // Wait until the import pop-up is dismissed. This depending on the number of items in the android history can take up to a few seconds
        boolean importComplete = waitForTest(new BooleanTest() {
            public boolean test() {
                return !mSolo.searchText("Please wait...");
            }
        }, MAX_WAIT_TIMEOUT);

        mAsserter.ok(importComplete, "Waiting for import to finish and the pop-up to be dismissed", "Import was completed and the pop-up was dismissed");

        // Import has finished. Waiting to get back to the Settings Menu and looking for the Import&Export subsection
        if ("phone".equals(mDevice.type)) {
            // Phones don't have headers like tablets, so we need to pop up one more level.
            waitForText(StringHelper.IMPORT_FROM_ANDROID_LABEL);
            mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        }
        waitForText("Privacy"); // Settings is a header for the settings menu page. Waiting for Privacy ensures we are back in the top Settings view
        mActions.sendSpecialKey(Actions.SpecialKey.BACK); // Exit Settings
        // Make sure the settings menu has been closed.
        mAsserter.ok(mSolo.waitForText(StringHelper.TITLE_PLACE_HOLDER), "Waiting for search bar", "Search bar found");

    }

    public ArrayList<String> getAndroidUrls(String data) {
        // Return bookmarks or history depending on what the user asks for
        ArrayList<String> urls = new ArrayList<String>();
        ContentResolver resolver = getActivity().getContentResolver();
        Browser mBrowser = new Browser();
        Cursor cursor = null;
        try {
            if (data.equals("history")) {
                cursor = mBrowser.getAllVisitedUrls(resolver);
            } else if (data.equals("bookmarks")) {
                cursor = mBrowser.getAllBookmarks(resolver);
            }
            if (cursor != null) {
                cursor.moveToFirst();
                for (int i = 0; i < cursor.getCount(); i++ ) {
                     urls.add(cursor.getString(cursor.getColumnIndex("url")));
                     if(!cursor.isLast()) {
                        cursor.moveToNext();
                     }
                }
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return urls;
    }

    public void deleteImportedData() {
        // Bookmarks
        ArrayList<String> androidBookmarks = getAndroidUrls("bookmarks");
        for (String url:androidBookmarks) {
             mDatabaseHelper.deleteBookmark(url);
        }
        // History
        for (String url:androidData) {
             mDatabaseHelper.deleteHistoryItem(url);
        }
    }

    public void tearDown() throws Exception {
        deleteImportedData();
        super.tearDown();
    }
}
