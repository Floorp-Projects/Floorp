package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

public class testBookmarksPanel extends AboutHomeTest {
    public void testBookmarksPanel() {
        final String BOOKMARK_URL = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);

        // Add a mobile bookmark
        mDatabaseHelper.addOrUpdateMobileBookmark(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, BOOKMARK_URL);

        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        // Check that the default bookmarks are displayed
        for (String url : StringHelper.DEFAULT_BOOKMARKS_URLS) {
            isBookmarkDisplayed(url);
        }

        // Open the context menu for the first bookmark in the list
        openBookmarkContextMenu(StringHelper.DEFAULT_BOOKMARKS_URLS[0]);

        // Test that the options are all displayed
        for (String contextMenuOption : StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS) {
            mAsserter.ok(mSolo.searchText(contextMenuOption), "Checking that the context menu option is present", contextMenuOption + " is present");
        }

        // Test that "Open in New Tab" works
        final Element tabCount = mDriver.findElement(getActivity(), R.id.tabs_counter);
        final int tabCountInt = Integer.parseInt(tabCount.getText());
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[0]);
        tabEventExpecter.blockForEvent();
        tabEventExpecter.unregisterListener();
        mAsserter.ok(mSolo.searchText(StringHelper.TITLE_PLACE_HOLDER), "Checking that the tab is not changed", "The tab was not changed");

        // Test that "Open in Private Tab" works
        openBookmarkContextMenu(StringHelper.DEFAULT_BOOKMARKS_URLS[1]);
        tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[1]);
        tabEventExpecter.blockForEvent();
        tabEventExpecter.unregisterListener();
        mAsserter.ok(mSolo.searchText(StringHelper.TITLE_PLACE_HOLDER), "Checking that the tab is not changed", "The tab was not changed");

        // Test that "Edit" works
        String[] editedBookmarkValues = new String[] { "New bookmark title", "www.NewBookmark.url", "newBookmarkKeyword" };
        editBookmark(BOOKMARK_URL, editedBookmarkValues);
        checkBookmarkEdit(editedBookmarkValues[1], editedBookmarkValues);

        // Test that "Remove" works
        openBookmarkContextMenu(editedBookmarkValues[1]);
        mSolo.clickOnText(StringHelper.BOOKMARK_CONTEXT_MENU_ITEMS[3]);
        waitForText("Bookmark removed");
        mAsserter.ok(!mDatabaseHelper.isBookmark(editedBookmarkValues[1]), "Checking that the bookmark was removed", "The bookmark was removed");
    }

   /**
    * @param bookmarkUrl URL of the bookmark to edit
    * @param values String array with the new values for all fields
    */
    private void editBookmark(String bookmarkUrl, String[] values) {
        openBookmarkContextMenu(bookmarkUrl);
        mSolo.clickOnText("Edit");
        waitForText("Edit Bookmark");

        // Update the fields with the new values
        for (int i = 0; i < values.length; i++) {
            mSolo.clearEditText(i);
            mSolo.clickOnEditText(i);
            mActions.sendKeys(values[i]);
        }

        mSolo.clickOnButton("OK");
        waitForText("Bookmark updated");
    }

   /**
    * @param bookmarkUrl String with the original url
    * @param values String array with the new values for all fields
    */
    private void checkBookmarkEdit(String bookmarkUrl, String[] values) {
        openBookmarkContextMenu(bookmarkUrl);
        mSolo.clickOnText("Edit");
        waitForText("Edit Bookmark");

        // Check the values of the fields
        for (String value : values) {
            mAsserter.ok(mSolo.searchText(value), "Checking that the value is correct", "The value = " + value + " is correct");
        }

        mSolo.clickOnButton("Cancel");
        waitForText("BOOKMARKS");
    }
}
