package org.mozilla.gecko.tests;

import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

import android.app.Activity;
import android.view.View;

import com.jayway.android.robotium.solo.Condition;

/* A simple test that creates 2 new tabs and checks that the tab count increases. */
public class testNewTab extends BaseTest {
    private Element tabCount = null;
    private Element tabs = null;
    private Element closeTab = null;
    private int tabCountInt = 0;

    public void testNewTab() {
        String url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        String url2 = getAbsoluteUrl("/robocop/robocop_blank_02.html");

        blockForGeckoReady();

        Activity activity = getActivity();
        tabCount = mDriver.findElement(activity, R.id.tabs_counter);
        tabs = mDriver.findElement(activity, R.id.tabs);
        mAsserter.ok(tabCount != null && tabs != null,
                     "Checking elements", "all elements present");

        int expectedTabCount = 1;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Initial number of tabs correct");

        addTab(url);
        expectedTabCount++;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Number of tabs increased");

        addTab(url2);
        expectedTabCount++;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Number of tabs increased");

        // cleanup: close all opened tabs
        closeAddedTabs();
    }

    private void getTabCount(final int expected) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                String newTabCountText = tabCount.getText();
                tabCountInt = Integer.parseInt(newTabCountText);
                if (tabCountInt == expected) {
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }
}
