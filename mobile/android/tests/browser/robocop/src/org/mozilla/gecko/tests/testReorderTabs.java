package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;

import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tests.components.TabsPresenterComponent;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.GeckoHelper;

import java.util.List;

public class testReorderTabs extends UITest {
    private int expectedSelectedTabIndex;

    public void testReorderTabs() {
        GeckoHelper.blockForReady();

        loadTabs();

        testMovingTabsIn(mTabsPanel);

        if (DeviceHelper.isTablet()) {
            mTabsPanel.closePanel();

            testMovingTabsIn(mTabStrip);

            // Make sure that the tabs panel picks up the reordering just done in the tab strip.
            mTabsPanel.openPanel();
            checkTabOrderConsistency(mTabsPanel.getVisibleTabIdsInOrder());
        }
    }

    /**
     * Bring the tab count to three and then open the tabs panel.
     */
    private void loadTabs() {
        mTabsPanel.openPanel();
        mTabsPanel.clickNewTabButton();
        mTabsPanel.openPanel();
        mTabsPanel.clickNewTabButton();
        mTabsPanel.openPanel();
        expectedSelectedTabIndex = 2;
    }

    private void testMovingTabsIn(TabsPresenterComponent tabsPresenter) {
        final List<Integer> originalTabIds = tabsPresenter.getVisibleTabIdsInOrder();
        // We require all tabs to be visible on screen.
        fAssertEquals("All tabs are visible", tabsPresenter.getTabCount(), originalTabIds.size());

        checkTabOrderConsistency(originalTabIds);
        tabsPresenter.assertSelectedTabIndexIs(expectedSelectedTabIndex);

        // Move a tab up in order.
        moveTab(tabsPresenter, 0, 1);
        final List<Integer> afterFirstMoveTabIds = tabsPresenter.getVisibleTabIdsInOrder();
        fAssertEquals("Tab 0 ids", originalTabIds.get(1), afterFirstMoveTabIds.get(0));
        fAssertEquals("Tab 1 ids", originalTabIds.get(0), afterFirstMoveTabIds.get(1));
        fAssertEquals("Tab 2 ids", originalTabIds.get(2), afterFirstMoveTabIds.get(2));
        checkTabOrderConsistency(afterFirstMoveTabIds);
        tabsPresenter.assertSelectedTabIndexIs(expectedSelectedTabIndex);

        // Move a tab down in order.
        moveTab(tabsPresenter, 2, 1);
        final List<Integer> afterSecondMoveTabIds = tabsPresenter.getVisibleTabIdsInOrder();
        fAssertEquals("Tab 0 ids", afterFirstMoveTabIds.get(0), afterSecondMoveTabIds.get(0));
        fAssertEquals("Tab 1 ids", afterFirstMoveTabIds.get(2), afterSecondMoveTabIds.get(1));
        fAssertEquals("Tab 2 ids", afterFirstMoveTabIds.get(1), afterSecondMoveTabIds.get(2));
        checkTabOrderConsistency(afterSecondMoveTabIds);
        tabsPresenter.assertSelectedTabIndexIs(expectedSelectedTabIndex);
    }

    private void moveTab(TabsPresenterComponent presenter, int from, int to) {
        presenter.moveTab(from, to);

        if (expectedSelectedTabIndex == from) {
            expectedSelectedTabIndex = to;
        } else if (from < to && expectedSelectedTabIndex > from && expectedSelectedTabIndex <= to) {
            expectedSelectedTabIndex--;
        } else if (from > to && expectedSelectedTabIndex >= to && expectedSelectedTabIndex < from) {
            expectedSelectedTabIndex++;
        }
    }

    /**
     * Assert that the list of {@code tabIds} matches the ordered list of Tab ids in {@link org.mozilla.gecko.Tabs}.
     */
    private void checkTabOrderConsistency(List<Integer> tabIds) {
        final Iterable<Tab> appTabs = Tabs.getInstance().getTabsInOrder();
        int i = 0;
        for (final Tab tab : appTabs) {
            fAssertEquals("Tab ids are equal", tab.getId(), (int) tabIds.get(i));
            i++;
        }
    }
}
