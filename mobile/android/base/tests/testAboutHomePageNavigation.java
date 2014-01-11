package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.components.AboutHomeComponent.PanelType;
import org.mozilla.gecko.tests.helpers.*;

/**
 * Tests functionality related to navigating between the various about:home pages.
 */
public class testAboutHomePageNavigation extends UITest {
    // TODO: Define this test dynamically by creating dynamic representations of the Page
    // enum for both phone and tablet, then swiping through the pages. This will also
    // benefit having a HomePager with custom pages.
    public void testAboutHomePageNavigation() {
        GeckoHelper.blockForReady();

        mAboutHome.assertVisible()
                  .assertCurrentPage(PanelType.TOP_SITES);

        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.BOOKMARKS);

        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.READING_LIST);

        // Ideally these helpers would just be their own tests. However, by keeping this within
        // one method, we're saving test setUp and tearDown resources.
        if (DeviceHelper.isTablet()) {
            helperTestTablet();
        } else {
            helperTestPhone();
        }
    }

    private void helperTestTablet() {
        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.HISTORY);

        // Edge case.
        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.HISTORY);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.READING_LIST);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.BOOKMARKS);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.TOP_SITES);

        // Edge case.
        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.TOP_SITES);
    }

    private void helperTestPhone() {
        // Edge case.
        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.READING_LIST);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.BOOKMARKS);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.TOP_SITES);

        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.HISTORY);

        // Edge case.
        mAboutHome.swipeToPageOnLeft();
        mAboutHome.assertCurrentPage(PanelType.HISTORY);

        mAboutHome.swipeToPageOnRight();
        mAboutHome.assertCurrentPage(PanelType.TOP_SITES);
    }

    // TODO: bug 943706 - reimplement this old test code.
    /*
        //  Removed by Bug 896576 - [fig] Remove [getAllPagesList] from BaseTest
        //  ListView list = getAllPagesList("about:firefox");

        // Test normal sliding of the list left and right
        ViewPager pager = (ViewPager)mSolo.getView(ViewPager.class, 0);
        mAsserter.is(pager.getCurrentItem(), 0, "All pages is selected");

        int width = mDriver.getGeckoWidth() / 2;
        int y = mDriver.getGeckoHeight() / 2;
        mActions.drag(width, 0, y, y);
        mAsserter.is(pager.getCurrentItem(), 1, "Bookmarks page is selected");

        mActions.drag(0, width, y, y);
        mAsserter.is(pager.getCurrentItem(), 0, "All pages is selected");

        // Test tapping on the tab strip changes tabs
        TabWidget tabwidget = (TabWidget)mSolo.getView(TabWidget.class, 0);
        mSolo.clickOnView(tabwidget.getChildAt(1));
        mAsserter.is(pager.getCurrentItem(), 1, "Clicking on tab selected bookmarks page");

        // Test typing in the awesomebar changes tabs and prevents panning
        mSolo.typeText(0, "woot");
        mAsserter.is(pager.getCurrentItem(), 0, "Searching switched to all pages tab");
        mSolo.scrollToSide(Solo.LEFT);
        mAsserter.is(pager.getCurrentItem(), 0, "Dragging left is not allowed when searching");

        mSolo.scrollToSide(Solo.RIGHT);
        mAsserter.is(pager.getCurrentItem(), 0, "Dragging right is not allowed when searching");

        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
    */
}
