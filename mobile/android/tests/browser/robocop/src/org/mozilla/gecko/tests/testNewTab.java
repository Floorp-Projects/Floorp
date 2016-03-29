/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

import android.app.Activity;
import android.view.View;

import com.robotium.solo.Condition;

/* A simple test that creates 2 new tabs and checks that the tab count increases. */
public class testNewTab extends BaseTest {
    private Element tabCount = null;
    private Element tabs = null;
    private final Element closeTab = null;
    private int tabCountInt = 0;

    public void testNewTab() {
        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String url2 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_02_URL);

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
