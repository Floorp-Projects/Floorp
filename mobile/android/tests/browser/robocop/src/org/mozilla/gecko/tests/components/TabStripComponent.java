package org.mozilla.gecko.tests.components;

import android.view.View;

import com.robotium.solo.Condition;

import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;
import org.mozilla.gecko.widget.TwoWayView;

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

    private View waitForTabView(final int index) {
        final TwoWayView tabStrip = getTabStripView();
        final View[] tabView = new View[1];

        WaitHelper.waitFor(String.format("Tab at index %d to be visible", index), new Condition() {
            @Override
            public boolean isSatisfied() {
                return (tabView[0] = tabStrip.getChildAt(index)) != null;
            }
        });

        return tabView[0];
    }

    private TwoWayView getTabStripView() {
        TwoWayView tabStrip = (TwoWayView) mSolo.getView("tab_strip");

        fAssertNotNull("Tab strip is not null", tabStrip);

        return tabStrip;
    }
}
