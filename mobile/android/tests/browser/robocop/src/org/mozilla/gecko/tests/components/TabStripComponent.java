package org.mozilla.gecko.tests.components;

import android.view.View;
import android.support.v7.widget.RecyclerView;

import com.robotium.solo.Condition;

import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

/**
 * A class representing any interactions that take place on the tablet tab strip.
 */
public class TabStripComponent extends BaseComponent {
    // Using a text id because the layout and therefore the id might be stripped from the (non-tablet) build
    private static final String TAB_STRIP_ID = "tab_strip";

    public TabStripComponent(final UITestContext testContext) {
        super(testContext);
    }

    public void switchToTab(int index) {
        // The tab strip is only available on tablets
        DeviceHelper.assertIsTablet();

        View tabView = waitForTabView(index);
        fAssertNotNull(String.format("Tab at index %d is not null", index), tabView);

        mSolo.clickOnView(tabView);
    }

    /**
     * Note: this currently only supports the case where the tab strip visible tabs start at tab 0
     * and the tab at {@code index} is visible in the tab strip.
     */
    private View waitForTabView(final int index) {
        final RecyclerView tabStrip = getTabStripView();
        final View[] tabView = new View[1];

        WaitHelper.waitFor(String.format("Tab at index %d to be visible", index), new Condition() {
            @Override
            public boolean isSatisfied() {
                return (tabView[0] = tabStrip.getChildAt(index)) != null;
            }
        });

        return tabView[0];
    }

    private RecyclerView getTabStripView() {
        RecyclerView tabStrip = (RecyclerView) mSolo.getView("tab_strip");

        fAssertNotNull("Tab strip is not null", tabStrip);

        return tabStrip;
    }
}
