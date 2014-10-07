package org.mozilla.gecko.tests;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

/**
 * Tests functionality related to navigating between the various about:home panels.
 *
 * TODO: Update this test to account for recent tabs panel (bug 1028727).
 */
public class testAboutHomePageNavigation extends UITest {
    // TODO: Define this test dynamically by creating dynamic representations of the Page
    // enum for both phone and tablet, then swiping through the panels. This will also
    // benefit having a HomePager with custom panels.
    public void testAboutHomePageNavigation() {
        GeckoHelper.blockForDelayedStartup();

        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.BOOKMARKS);

        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.READING_LIST);

        // Ideally these helpers would just be their own tests. However, by keeping this within
        // one method, we're saving test setUp and tearDown resources.
        if (DeviceHelper.isTablet()) {
            helperTestTablet();
        } else {
            helperTestPhone();
        }
    }

    private void helperTestTablet() {
        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.HISTORY);

        // Edge case.
        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.HISTORY);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.READING_LIST);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.BOOKMARKS);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.TOP_SITES);

        // Edge case.
        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.TOP_SITES);
    }

    private void helperTestPhone() {
        // Edge case.
        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.READING_LIST);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.BOOKMARKS);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.TOP_SITES);

        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.HISTORY);

        // Edge case.
        mAboutHome.swipeToPanelOnLeft();
        mAboutHome.assertCurrentPanel(PanelType.HISTORY);

        mAboutHome.swipeToPanelOnRight();
        mAboutHome.assertCurrentPanel(PanelType.TOP_SITES);
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
