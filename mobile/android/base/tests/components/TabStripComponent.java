package org.mozilla.gecko.tests.components;

import android.view.View;

import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.DeviceHelper;
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

        View tabView = getTabView(index);

        mSolo.clickOnView(tabView);
    }

    private View getTabView(int index) {
        TwoWayView tabStrip = getTabStripView();

        fAssertTrue(String.format("Tab strip contains at least %d tabs", index + 1), tabStrip.getChildCount() > index);

        View tabView = tabStrip.getChildAt(index);
        fAssertNotNull(String.format("Tab at index %d is not null", index), tabView);

        return tabView;
    }

    private TwoWayView getTabStripView() {
        TwoWayView tabStrip = (TwoWayView) mSolo.getView("tab_strip");

        fAssertNotNull("Tab strip is not null", tabStrip);

        return tabStrip;
    }
}
