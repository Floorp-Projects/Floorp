/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.mozilla.gecko.home.HomePager;

import com.robotium.solo.Condition;

public class testHistory extends AboutHomeTest {
    private View mFirstChild;

    public void testHistory() {
        blockForGeckoReady();

        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String url2 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        String url3 = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_03_URL);

        inputAndLoadUrl(url);
        verifyUrlBarTitle(url);
        inputAndLoadUrl(url2);
        verifyUrlBarTitle(url2);
        inputAndLoadUrl(url3);
        verifyUrlBarTitle(url3);

        openAboutHomeTab(AboutHomeTabs.HISTORY);

        final ListView hList = findListViewWithTag(HomePager.LIST_TAG_HISTORY);
        mAsserter.is(waitForNonEmptyListToLoad(hList), true, "list is properly loaded");

        // Click on the history item and wait for the page to load
        // wait for the history list to be populated
        mFirstChild = null;
        boolean success = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                mFirstChild = hList.getChildAt(1);
                if (mFirstChild == null) {
                    return false;
                }
                if (mFirstChild instanceof android.view.ViewGroup) {
                    ViewGroup group = (ViewGroup)mFirstChild;
                    if (group.getChildCount() < 1) {
                        return false;
                    }
                    for (int i = 0; i < group.getChildCount(); i++) {
                        View grandChild = group.getChildAt(i);
                        if (grandChild instanceof android.widget.TextView) {
                            mAsserter.ok(true, "found TextView:", ((android.widget.TextView)grandChild).getText().toString());
                        }
                    }
                } else {
                    mAsserter.dumpLog("first child not a ViewGroup: "+mFirstChild);
                    return false;
                }
                return true;
            }
        }, MAX_WAIT_MS);

        mAsserter.isnot(mFirstChild, null, "Got history item");
        mSolo.clickOnView(mFirstChild);

        // The first item here (since it was just visited) should be a "Switch to tab" item
        // i.e. don't expect a DOMContentLoaded event
        verifyUrlBarTitle(mStringHelper.ROBOCOP_BLANK_PAGE_03_URL);
        verifyUrl(url3);
    }
}
