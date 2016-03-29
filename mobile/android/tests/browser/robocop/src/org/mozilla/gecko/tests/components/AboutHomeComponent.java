/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.components;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertEquals;
import static org.mozilla.gecko.tests.helpers.AssertionHelper.fAssertTrue;

import java.util.Arrays;
import java.util.List;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.helpers.WaitHelper;

import android.os.Build;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.widget.TextView;

import com.robotium.solo.Condition;
import com.robotium.solo.Solo;

/**
 * A class representing any interactions that take place on the Awesomescreen.
 */
public class AboutHomeComponent extends BaseComponent {
    private static final String LOGTAG = AboutHomeComponent.class.getSimpleName();

    private static final List<PanelType> PANEL_ORDERING = Arrays.asList(
            PanelType.TOP_SITES,
            PanelType.BOOKMARKS,
            PanelType.HISTORY,
            PanelType.REMOTE_TABS,
            PanelType.RECENT_TABS,
            PanelType.READING_LIST
    );

    // The percentage of the panel to swipe between 0 and 1. This value was set through
    // testing: 0.55f was tested on try and fails on armv6 devices.
    private static final float SWIPE_PERCENTAGE = 0.70f;

    public AboutHomeComponent(final UITestContext testContext) {
        super(testContext);
    }

    private View getHomePagerContainer() {
        return mSolo.getView(R.id.home_pager_container);
    }

    private ViewPager getHomePagerView() {
        return (ViewPager) mSolo.getView(R.id.home_pager);
    }

    private View getHomeBannerView() {
        if (mSolo.waitForView(R.id.home_banner)) {
            return mSolo.getView(R.id.home_banner);
        }
        return null;
    }

    public AboutHomeComponent assertCurrentPanel(final PanelType expectedPanel) {
        assertVisible();

        final int expectedPanelIndex = PANEL_ORDERING.indexOf(expectedPanel);
        fAssertEquals("The current HomePager panel is " + expectedPanel,
                     expectedPanelIndex, getHomePagerView().getCurrentItem());
        return this;
    }

    public AboutHomeComponent assertNotVisible() {
        fAssertTrue("The HomePager is not visible",
                    getHomePagerContainer().getVisibility() != View.VISIBLE ||
                    getHomePagerView().getVisibility() != View.VISIBLE);
        return this;
    }

    public AboutHomeComponent assertVisible() {
        fAssertTrue("The HomePager is visible",
                    getHomePagerContainer().getVisibility() == View.VISIBLE &&
                    getHomePagerView().getVisibility() == View.VISIBLE);
        return this;
    }

    public AboutHomeComponent assertBannerNotVisible() {
        View banner = getHomeBannerView();
        if (Build.VERSION.SDK_INT >= 11) {
            fAssertTrue("The HomeBanner is not visible",
                        getHomePagerContainer().getVisibility() != View.VISIBLE ||
                        banner == null ||
                        banner.getVisibility() != View.VISIBLE ||
                        banner.getTranslationY() == banner.getHeight());
        } else {
            // getTranslationY is not available before api 11.
            // This check is a little less specific.
            fAssertTrue("The HomeBanner is not visible",
                        getHomePagerContainer().getVisibility() != View.VISIBLE ||
                        banner == null ||
                        banner.isShown() == false);
        }
        return this;
    }

    public AboutHomeComponent assertBannerVisible() {
        fAssertTrue("The HomeBanner is visible",
                    getHomePagerContainer().getVisibility() == View.VISIBLE &&
                    getHomeBannerView().getVisibility() == View.VISIBLE);
        return this;
    }

    public AboutHomeComponent assertBannerText(String text) {
        assertBannerVisible();

        final TextView textView = (TextView) getHomeBannerView().findViewById(R.id.text);
        fAssertEquals("The correct HomeBanner text is shown",
                     text, textView.getText().toString());
        return this;
    }

    public AboutHomeComponent clickOnBanner() {
        assertBannerVisible();

        mTestContext.dumpLog(LOGTAG, "Clicking on HomeBanner.");
        mSolo.clickOnView(getHomeBannerView());
        return this;
    }

    public AboutHomeComponent dismissBanner() {
        assertBannerVisible();

        mTestContext.dumpLog(LOGTAG, "Clicking on HomeBanner close button.");
        mSolo.clickOnView(getHomeBannerView().findViewById(R.id.close));
        return this;
    }

    public AboutHomeComponent swipeToPanelOnRight() {
        mTestContext.dumpLog(LOGTAG, "Swiping to the panel on the right.");
        swipeToPanel(Solo.RIGHT);
        return this;
    }

    public AboutHomeComponent swipeToPanelOnLeft() {
        mTestContext.dumpLog(LOGTAG, "Swiping to the panel on the left.");
        swipeToPanel(Solo.LEFT);
        return this;
    }

    private void swipeToPanel(final int panelDirection) {
        fAssertTrue("Swiping in a valid direction",
                panelDirection == Solo.LEFT || panelDirection == Solo.RIGHT);
        assertVisible();

        final int panelIndex = getHomePagerView().getCurrentItem();

        mSolo.scrollViewToSide(getHomePagerView(), panelDirection, SWIPE_PERCENTAGE);

        // The panel on the left is a lower index and vice versa.
        final int unboundedPanelIndex = panelIndex + (panelDirection == Solo.LEFT ? -1 : 1);
        final int maxPanelIndex = PANEL_ORDERING.size() - 1;
        final int expectedPanelIndex = Math.min(Math.max(0, unboundedPanelIndex), maxPanelIndex);

        waitForPanelIndex(expectedPanelIndex);
    }

    private void waitForPanelIndex(final int expectedIndex) {
        final String panelName = PANEL_ORDERING.get(expectedIndex).name();

        WaitHelper.waitFor("HomePager " + panelName + " panel", new Condition() {
            @Override
            public boolean isSatisfied() {
                return (getHomePagerView().getCurrentItem() == expectedIndex);
            }
        });
    }

    /**
     * Navigate directly to a built-in panel by its panel type.
     * <p>
     * If the panel type is not part of the active Home Panel configuration, the
     * default about:home panel is displayed. If the panel type is not a
     * built-in panel, an IllegalArgumentException is thrown.
     *
     * @param panelType to navigate to.
     * @return self, for chaining.
     */
    public AboutHomeComponent navigateToBuiltinPanelType(PanelType panelType) throws IllegalArgumentException {
        Tabs.getInstance().loadUrl(AboutPages.getURLForBuiltinPanelType(panelType));
        final int expectedPanelIndex = PANEL_ORDERING.indexOf(panelType);
        waitForPanelIndex(expectedPanelIndex);
        return this;
    }
}
