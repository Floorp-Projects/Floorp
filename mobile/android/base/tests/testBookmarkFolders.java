package org.mozilla.gecko.tests;

import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.sync.Utils;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.view.View;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.jayway.android.robotium.solo.Condition;

public class testBookmarkFolders extends AboutHomeTest {
    private static String DESKTOP_BOOKMARK_URL;

    public void testBookmarkFolders() {
        DESKTOP_BOOKMARK_URL = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);

        setUpDesktopBookmarks();
        checkBookmarkList();
    }

    private void checkBookmarkList() {
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);
        waitForText(StringHelper.DESKTOP_FOLDER_LABEL);
        clickOnBookmarkFolder(StringHelper.DESKTOP_FOLDER_LABEL);
        waitForText(StringHelper.TOOLBAR_FOLDER_LABEL);

        // Verify the number of folders displayed in the Desktop Bookmarks folder is correct
        ListView desktopFolderContent = findListViewWithTag(HomePager.LIST_TAG_BOOKMARKS);
        ListAdapter adapter = desktopFolderContent.getAdapter();

        // Three folders and "Up to Bookmarks".
        mAsserter.is(adapter.getCount(), 4, "Checking that the correct number of folders is displayed in the Desktop Bookmarks folder");

        clickOnBookmarkFolder(StringHelper.TOOLBAR_FOLDER_LABEL);

        // Go up in the bookmark folder hierarchy
        clickOnBookmarkFolder(String.format(StringHelper.BOOKMARKS_UP_TO, StringHelper.DESKTOP_FOLDER_LABEL));
        mAsserter.ok(waitForText(StringHelper.BOOKMARKS_MENU_FOLDER_LABEL), "Going up in the folder hierarchy", "We are back in the Desktop Bookmarks folder");

        clickOnBookmarkFolder(String.format(StringHelper.BOOKMARKS_UP_TO, StringHelper.BOOKMARKS_ROOT_LABEL));
        mAsserter.ok(waitForText(StringHelper.DESKTOP_FOLDER_LABEL), "Going up in the folder hierarchy", "We are back in the main Bookmarks List View");

        clickOnBookmarkFolder(StringHelper.DESKTOP_FOLDER_LABEL);
        clickOnBookmarkFolder(StringHelper.TOOLBAR_FOLDER_LABEL);
        isBookmarkDisplayed(DESKTOP_BOOKMARK_URL);

        // Open the bookmark from a bookmark folder hierarchy
        loadBookmark(DESKTOP_BOOKMARK_URL);
        waitForText(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE, DESKTOP_BOOKMARK_URL);
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        // Check that folders don't have a context menu
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                View desktopFolder = getBookmarkFolderView(StringHelper.DESKTOP_FOLDER_LABEL);
                if (desktopFolder == null) {
                    return false;
                }
                mSolo.clickLongOnView(desktopFolder);
                return true;            }
        }, MAX_WAIT_MS);

        mAsserter.ok(success, "Trying to long click on the Desktop Bookmarks","Desktop Bookmarks folder could not be long clicked");

        final String contextMenuString = StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[0];
        mAsserter.ok(!waitForText(contextMenuString), "Folders do not have context menus", "The context menu was not opened");

        // Even if no context menu is opened long clicking a folder still opens it. We need to close it.
        clickOnBookmarkFolder(String.format(StringHelper.BOOKMARKS_UP_TO, StringHelper.BOOKMARKS_ROOT_LABEL));
    }

    private void clickOnBookmarkFolder(final String folderName) {
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                View bookmarksFolder = getBookmarkFolderView(folderName);
                if (bookmarksFolder == null) {
                    return false;
                }
                mSolo.waitForView(bookmarksFolder);
                mSolo.clickOnView(bookmarksFolder);
                return true;
            }
        }, MAX_WAIT_MS);
        mAsserter.ok(success, "Trying to click on the " + folderName + " folder","The " + folderName + " folder was clicked");
    }

    private View getBookmarkFolderView(String folderName) {
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);
        mSolo.hideSoftKeyboard();
        getInstrumentation().waitForIdleSync();

        ListView bookmarksTabList = findListViewWithTag(HomePager.LIST_TAG_BOOKMARKS);
        if (!waitForNonEmptyListToLoad(bookmarksTabList)) {
            return null;
        }

        ListAdapter adapter = bookmarksTabList.getAdapter();
        if (adapter == null) {
            return null;
        }

        for (int i = 0; i < adapter.getCount(); i++ ) {
            View bookmarkView = bookmarksTabList.getChildAt(i);
            if (bookmarkView instanceof TextView) {
                TextView folderTextView = (TextView) bookmarkView;
                if (folderTextView.getText().equals(folderName)) {
                    return bookmarkView;
                }
            }
        }

        return null;
    }

    // Add a bookmark in the Desktop folder so we can check the folder navigation in the bookmarks page
    private void setUpDesktopBookmarks() {
        blockForGeckoReady();

        // Get the folder id of the StringHelper.DESKTOP_FOLDER_LABEL folder
        Long desktopFolderId = mDatabaseHelper.getFolderIdFromGuid("toolbar");

        // Generate a Guid for the bookmark
        final String generatedGuid = Utils.generateGuid();
        mAsserter.ok((generatedGuid != null), "Generating a random Guid for the bookmark", "We could not generate a Guid for the bookmark");

        // Insert the bookmark
        ContentResolver resolver = getActivity().getContentResolver();
        Uri bookmarksUri = mDatabaseHelper.buildUri(DatabaseHelper.BrowserDataType.BOOKMARKS);

        long now = System.currentTimeMillis();
        ContentValues values = new ContentValues();
        values.put("title", StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);
        values.put("url", DESKTOP_BOOKMARK_URL);
        values.put("parent", desktopFolderId);
        values.put("modified", now);
        values.put("type", 1);
        values.put("guid", generatedGuid);
        values.put("position", 10);
        values.put("created", now);

        int updated = resolver.update(bookmarksUri,
                                      values,
                                      "url = ?",
                                      new String[] { DESKTOP_BOOKMARK_URL });
        if (updated == 0) {
            Uri uri = resolver.insert(bookmarksUri, values);
            mAsserter.ok(true, "Inserted at: ", uri.toString());
        } else {
            mAsserter.ok(false, "Failed to insert the Desktop bookmark", "Something went wrong");
        }
    }

    @Override
    public void tearDown() throws Exception {
        mDatabaseHelper.deleteBookmark(DESKTOP_BOOKMARK_URL);
        super.tearDown();
    }
}
