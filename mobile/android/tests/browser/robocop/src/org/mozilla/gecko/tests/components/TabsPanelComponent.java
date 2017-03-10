package org.mozilla.gecko.tests.components;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import com.robotium.solo.Condition;

import org.mozilla.gecko.R;
import org.mozilla.gecko.tabs.TabsLayoutItemView;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.WaitHelper;

import java.util.List;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

public class TabsPanelComponent extends TabsPresenterComponent {
    public TabsPanelComponent(final UITestContext testContext) {
        super(testContext);
    }

    public void openPanel() {
        assertTabsPanelIsClosed();
        mSolo.clickOnView(mSolo.getView(R.id.tabs));
        WaitHelper.waitFor("tabs panel to open", new Condition() {
            @Override
            public boolean isSatisfied() {
                final View tabsPanel = mSolo.getView(R.id.tabs_panel);
                return tabsPanel != null && tabsPanel.getVisibility() == View.VISIBLE;
            }
        });
        waitForAnimations();
    }

    public void closePanel() {
        assertTabsPanelIsOpen();
        mSolo.clickOnView(mSolo.getView(R.id.nav_back));
        waitForTabsPanelToClose();
    }

    @Override
    public void clickNewTabButton() {
        assertTabsPanelIsOpen();
        mSolo.clickOnView(mSolo.getView(R.id.add_tab));
        waitForTabsPanelToClose();
    }

    private void waitForTabsPanelToClose() {
        WaitHelper.waitFor("tabs panel to close", new Condition() {
            @Override
            public boolean isSatisfied() {
                final View tabsPanel = mSolo.getView(R.id.tabs_panel);
                return tabsPanel.getVisibility() != View.VISIBLE;
            }
        });
        waitForAnimations();
    }

    private void waitForAnimations() {
        mSolo.sleep(1500);
    }

    @Override
    protected void addVisibleTabIdsToList(List<Integer> idList) {
        final RecyclerView tabsLayout = getPresenter();
        final int tabsLayoutChildCount = tabsLayout.getChildCount();
        for (int i = 0; i < tabsLayoutChildCount; i++) {
            final TabsLayoutItemView tabView = (TabsLayoutItemView) tabsLayout.getChildAt(i);
            idList.add(tabView.getTabId());
        }
    }

    /**
     * You should use {@link TabsPresenterComponent#getPresenter()} instead if you expect the layout to exist.
     */
    @Override
    protected RecyclerView maybeGetPresenter() {
        return (RecyclerView) mSolo.getView(R.id.normal_tabs);
    }

    private View getTabsPanel() {
        return mSolo.getView(R.id.tabs_panel);
    }

    private void assertTabsPanelIsOpen() {
        final View tabsPanel = getTabsPanel();
        fAssertTrue("Tabs panel is open", tabsPanel != null && tabsPanel.getVisibility() == View.VISIBLE);
    }

    private void assertTabsPanelIsClosed() {
        final View tabsPanel = getTabsPanel();
        fAssertTrue("Tabs panel is closed", tabsPanel == null || tabsPanel.getVisibility() != View.VISIBLE);
    }
}
