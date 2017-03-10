package org.mozilla.gecko.tests.components;

import android.app.Instrumentation;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.Checkable;

import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.RobotiumHelper;

import java.util.ArrayList;
import java.util.List;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertNotNull;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

/**
 * A component representing something that presents the list of tabs to the user along with a way
 * to add tabs etc, like the tab strip on tablets or the tabs tray on phones and tablets.
 */
public abstract class TabsPresenterComponent extends BaseComponent {
    public TabsPresenterComponent(final UITestContext testContext) {
        super(testContext);
    }

    /**
     * Assert that the tab at the given visual index is the one selected.
     * @param tabIndex is the *visual* index of the tab to check (the first tab visible on screen is
     *                 index 0).
     */
    public void assertSelectedTabIndexIs(int tabIndex) {
        final RecyclerView tabsLayout = getPresenter();
        fAssertTrue("The tab at tabIndex " + tabIndex + " is visible", tabIndex >= 0 && tabIndex < tabsLayout.getChildCount());
        final Checkable tabView = (Checkable) tabsLayout.getChildAt(tabIndex);
        fAssertTrue("The tab at tabIndex " + tabIndex + " is selected", tabView.isChecked());
    }

    public void assertTabCountIs(int count) {
        fAssertEquals("The tab count is " + count, count, getTabCount());
    }

    /**
     * Click the button used to create a new tab.
     */
    public abstract void clickNewTabButton();

    /**
     * The component must be currently active.
     * @return the total tab count (not just those visible on screen).
     */
    public int getTabCount() {
        return getPresenter().getAdapter().getItemCount();
    }

    /**
     * @param index the *visual* index of the tab view to return (the first visible tab is index 0).
     * @return the tab view at the given visual index.
     */
    public View getTabViewAt(int index) {
        final RecyclerView presenter = getPresenter();
        fAssertTrue("The tab at index " + index + " is visible", index >= 0 && index < presenter.getChildCount());
        return presenter.getChildAt(index);
    }

    public List<Integer> getVisibleTabIdsInOrder() {
        final ArrayList<Integer> tabIds = new ArrayList<>();
        addVisibleTabIdsToList(tabIds);
        return tabIds;
    }

    public void moveTab(int from, int to) {
        final View fromTab = getTabViewAt(from);
        final View toTab = getTabViewAt(to);
        RobotiumHelper.longPressDragView(fromTab, toTab, true);
    }

    protected RecyclerView getPresenter() {
        final RecyclerView tabsPresenter = maybeGetPresenter();

        fAssertNotNull("Tabs presenter is not null", tabsPresenter);

        return tabsPresenter;
    }

    /**
     * (This method shouldn't really be necessary, but the tab strip and the tabs panel item views
     * don't share a common base usable for this purpose.)
     */
    protected abstract void addVisibleTabIdsToList(List<Integer> idList);

    /**
     * Return the {@code RecyclerView} presenting tabs for this component, or {@code null} if the
     * component isn't currently active.
     */
    protected abstract RecyclerView maybeGetPresenter();
}
